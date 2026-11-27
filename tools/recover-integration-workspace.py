#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Recover or inspect an exact SysPulse integration checkout without destructive resets."""
import argparse
import os
from pathlib import Path
import re
import subprocess
import sys

BRANCH = 'integration/alpha39'
REMOTE = 'https://github.com/sudipdasin/SysPulse.git'
REF = 'refs/remotes/origin/' + BRANCH


def git(path, *args, timeout=30):
    env = {k: v for k, v in os.environ.items() if not k.startswith('GIT_')}
    env.update(PATH='/usr/bin:/bin', LC_ALL='C', GIT_CONFIG_NOSYSTEM='1',
               GIT_CONFIG_GLOBAL='/dev/null', GIT_TERMINAL_PROMPT='0',
               GIT_NO_REPLACE_OBJECTS='1', GIT_OPTIONAL_LOCKS='0')
    cmd = ['/usr/bin/git', '-C', str(path), '-c', 'core.hooksPath=/dev/null',
           '-c', 'core.fsmonitor=false', '-c', 'core.excludesFile=/dev/null',
           '-c', 'core.attributesFile=/dev/null', '-c', 'protocol.ext.allow=never',
           '-c', 'http.sslVerify=true',
           '-c', 'http.followRedirects=false', *args]
    result = subprocess.run(cmd, env=env, text=True, stdout=subprocess.PIPE,
                            stderr=subprocess.PIPE, timeout=timeout)
    if result.returncode:
        raise RuntimeError(f'Git {args[0]} failed (exit {result.returncode}): {result.stderr.strip()[:1000]}')
    return result.stdout.strip()


def inspect(path, pin, remote):
    if path.is_symlink() or not path.is_dir():
        raise ValueError('Workspace must be an existing ordinary directory.')
    if git(path, 'rev-parse', '--show-toplevel') != str(path.resolve()):
        raise ValueError('Path is not the checkout root.')
    if git(path, 'rev-parse', '--is-shallow-repository') != 'false':
        raise ValueError('Shallow checkout: complete history required.')
    if git(path, 'config', '--local', '--get', 'remote.origin.url') != remote:
        raise ValueError('Unexpected origin; no changes made.')
    if git(path, 'status', '--porcelain=v1', '--untracked-files=all'):
        raise ValueError('Workspace contains local changes; no changes made.')
    if git(path, 'rev-parse', 'HEAD') != pin:
        raise ValueError('HEAD differs from reviewed pin; no changes made.')
    if git(path, 'symbolic-ref', '--short', 'HEAD') != BRANCH:
        raise ValueError('Not on the integration branch; no changes made.')
    if git(path, 'rev-parse', REF) != pin:
        raise ValueError('Local tracking ref differs from reviewed pin; no changes made.')
    git(path, 'fsck', '--full', '--no-dangling', timeout=120)


def create(path, pin, remote, local):
    # Exclusive creation prevents overwriting even an empty existing directory.
    path.mkdir(mode=0o700, exist_ok=False)
    print('New workspace reserved:', path, flush=True)
    try:
        git(path, 'init', '--quiet', '--initial-branch=workspace-init')
        git(path, 'config', '--local', 'core.hooksPath', '/dev/null')
        git(path, 'remote', 'add', '-t', BRANCH, 'origin', remote)
        git(path, '-c', 'protocol.file.allow=' + ('always' if local else 'never'),
            'fetch', '--no-tags', '--no-recurse-submodules', 'origin',
            'refs/heads/' + BRANCH + ':' + REF, timeout=180)
        if git(path, 'rev-parse', REF) != pin:
            raise ValueError('Source branch advanced or differs from reviewed pin; checkout refused.')
        git(path, 'switch', '--quiet', '--track', '-c', BRANCH, 'origin/' + BRANCH)
        inspect(path, pin, remote)
    except (OSError, ValueError, RuntimeError, subprocess.SubprocessError):
        print('Incomplete new workspace retained for inspection; do not use it as a verified checkout.', file=sys.stderr)
        raise


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('mode', choices=('create', 'check'))
    ap.add_argument('destination', type=Path)
    ap.add_argument('commit', help='Exact reviewed 40-character commit SHA')
    ap.add_argument('--local-repo', type=Path, help='Trusted local repository instead of canonical HTTPS')
    a = ap.parse_args()
    if os.geteuid() == 0:
        ap.error('Run as an ordinary user.')
    if not re.fullmatch(r'[0-9a-f]{40}', a.commit):
        ap.error('Exact lowercase full commit SHA required.')
    try:
        dest = a.destination.absolute()
        remote = REMOTE
        if a.local_repo is not None:
            remote = str(a.local_repo.resolve(strict=True))
            if not Path(remote).is_dir():
                raise ValueError('Local repository must be a directory.')
        if a.mode == 'create':
            create(dest, a.commit, remote, a.local_repo is not None)
        else:
            inspect(dest, a.commit, remote)
        print('VERIFIED workspace:', dest)
        print('Integration commit:', a.commit)
        print('Local checkout integrity only; no build or release validation claimed.')
        return 0
    except (OSError, ValueError, RuntimeError, subprocess.SubprocessError) as e:
        print('BLOCKED:', str(e), file=sys.stderr)
        return 1


if __name__ == '__main__':
    sys.exit(main())
