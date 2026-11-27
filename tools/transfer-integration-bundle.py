#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Transfer reviewed, committed integration history through an offline Git bundle."""
import argparse
import hashlib
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile

BRANCH = 'integration/alpha39'
REF = 'refs/heads/' + BRANCH
TRACK = 'refs/remotes/origin/' + BRANCH
REMOTE = 'https://github.com/sudipdasin/SysPulse.git'
MAX_BYTES = 128 * 1024 * 1024


def git(path, *args, local=False, timeout=180):
    env = {k: v for k, v in os.environ.items() if not k.startswith('GIT_')}
    env.update(PATH='/usr/bin:/bin', LC_ALL='C', GIT_CONFIG_NOSYSTEM='1',
               GIT_CONFIG_GLOBAL='/dev/null', GIT_NO_REPLACE_OBJECTS='1',
               GIT_TERMINAL_PROMPT='0', GIT_OPTIONAL_LOCKS='0')
    command = ['/usr/bin/git', '-C', str(path), '-c', 'core.hooksPath=/dev/null',
               '-c', 'core.fsmonitor=false', '-c', 'core.attributesFile=/dev/null',
               '-c', 'protocol.ext.allow=never', '-c',
               'protocol.file.allow=' + ('always' if local else 'never'),
               '-c', 'http.sslVerify=true', '-c', 'http.followRedirects=false', *args]
    p = subprocess.run(command, env=env, text=True, capture_output=True, timeout=timeout)
    if p.returncode:
        raise RuntimeError(f'Git {args[0]} failed (exit {p.returncode}): {p.stderr.strip()[:1000]}')
    return p.stdout.strip()


def verify_repo(repo, ref, pin):
    if git(repo, 'rev-parse', '--is-shallow-repository') != 'false':
        raise ValueError('Full history required; shallow source rejected.')
    if git(repo, 'rev-parse', '--verify', ref + '^{commit}') != pin:
        raise ValueError('Source branch differs from the reviewed commit.')
    git(repo, 'fsck', '--full', '--no-dangling')


def verify_bundle(repo, bundle, pin):
    if bundle.is_symlink() or not bundle.is_file() or not 0 < bundle.stat().st_size <= MAX_BYTES:
        raise ValueError('Bundle must be an ordinary file of at most 128 MiB.')
    if git(repo, 'bundle', 'list-heads', str(bundle)) != pin + ' ' + REF:
        raise ValueError('Bundle does not contain exactly the reviewed integration ref.')
    # An empty repository cannot satisfy missing bundle prerequisites.
    git(repo, 'bundle', 'verify', str(bundle))


def digest(path):
    with path.open('rb') as f:
        return hashlib.file_digest(f, 'sha256').hexdigest() if hasattr(hashlib, 'file_digest') else _digest(f)


def _digest(f):
    h = hashlib.sha256()
    for block in iter(lambda: f.read(65536), b''):
        h.update(block)
    return h.hexdigest()


def export_bundle(destination, pin, source):
    if os.path.lexists(destination):
        raise ValueError('Destination already exists; not overwritten.')
    with tempfile.TemporaryDirectory(prefix='.syspulse-transfer-', dir=destination.parent) as tmp:
        root = Path(tmp)
        repo = root / 'source.git'; repo.mkdir()
        git(repo, 'init', '--bare', '--quiet')
        git(repo, 'fetch', '--no-tags', '--no-recurse-submodules', source or REMOTE,
            REF + ':' + REF, local=source is not None)
        verify_repo(repo, REF, pin)
        bundle = root / 'source.bundle'
        git(repo, 'bundle', 'create', str(bundle), REF)
        empty = root / 'verify.git'; empty.mkdir()
        git(empty, 'init', '--bare', '--quiet')
        verify_bundle(empty, bundle, pin)
        git(empty, 'fetch', '--no-tags', str(bundle), REF + ':' + REF, local=True)
        verify_repo(empty, REF, pin)
        bundle.chmod(0o600)
        # Same filesystem, atomic no-overwrite publication.
        os.link(bundle, destination)
    print('Bundle:', destination)
    print('SHA256:', digest(destination))
    print('Commit:', pin)


def import_bundle(bundle, destination, pin):
    if os.path.lexists(destination):
        raise ValueError('Destination already exists; not overwritten.')
    if bundle.is_symlink() or not bundle.is_file():
        raise ValueError('Bundle must be an ordinary file.')
    # Validate and materialize in a private temporary repository before checkout.
    with tempfile.TemporaryDirectory(prefix='syspulse-verify-') as tmp:
        repo = Path(tmp)
        git(repo, 'init', '--bare', '--quiet')
        verify_bundle(repo, bundle, pin)
        git(repo, 'fetch', '--no-tags', str(bundle), REF + ':' + REF, local=True)
        verify_repo(repo, REF, pin)
        destination.mkdir(mode=0o700, exist_ok=False)
        git(destination, 'init', '--quiet', '--initial-branch=workspace-init')
        git(destination, 'config', '--local', 'core.hooksPath', '/dev/null')
        git(destination, 'remote', 'add', '-t', BRANCH, 'origin', REMOTE)
        git(destination, 'fetch', '--no-tags', '--no-recurse-submodules', str(repo),
            REF + ':' + TRACK, local=True)
        verify_repo(destination, TRACK, pin)
        git(destination, 'switch', '--quiet', '--track', '-c', BRANCH, 'origin/' + BRANCH)
        if git(destination, 'rev-parse', 'HEAD') != pin or git(destination, 'status', '--porcelain=v1', '--untracked-files=all'):
            raise RuntimeError('Imported checkout verification failed.')
    print('Verified offline checkout:', destination)
    print('Commit:', pin)
    print('No native build or release acceptance claimed.')


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    sub = ap.add_subparsers(dest='mode', required=True)
    ex = sub.add_parser('export'); ex.add_argument('bundle', type=Path); ex.add_argument('commit'); ex.add_argument('--local-repo', type=Path)
    im = sub.add_parser('import'); im.add_argument('bundle', type=Path); im.add_argument('destination', type=Path); im.add_argument('commit')
    a = ap.parse_args()
    if os.geteuid() == 0:
        ap.error('Run as an ordinary user.')
    if not re.fullmatch(r'[0-9a-f]{40}', a.commit):
        ap.error('Exact lowercase full commit SHA required.')
    os.umask(0o077)
    try:
        if a.mode == 'export':
            source = str(a.local_repo.resolve(strict=True)) if a.local_repo is not None else None
            if source is not None and not Path(source).is_dir():
                raise ValueError('Local source must be a repository directory.')
            export_bundle(a.bundle.absolute(), a.commit, source)
        else:
            import_bundle(a.bundle.absolute(), a.destination.absolute(), a.commit)
        return 0
    except (OSError, ValueError, RuntimeError, subprocess.SubprocessError) as e:
        print('BLOCKED:', str(e), file=sys.stderr)
        return 1


if __name__ == '__main__':
    sys.exit(main())
