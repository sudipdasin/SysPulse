#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Build a signed, immutable flat APT snapshot from a previously verified DEB.
No hosting, key enrollment, installation or network access is performed.
"""
import argparse
from datetime import datetime, timedelta, timezone
from email.utils import format_datetime
import hashlib
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile


def require(ok, message):
    if not ok:
        raise ValueError(message)


def run(*args, cwd=None):
    return subprocess.check_output(args, cwd=cwd, timeout=60)


def digest(path):
    with path.open('rb') as stream:
        h = hashlib.sha256()
        for block in iter(lambda: stream.read(1024 * 1024), b''):
            h.update(block)
        return h.hexdigest()


def build(package, expected_sha256, expected_version, output, fingerprint):
    require(os.geteuid() != 0, 'run as a normal user')
    require(re.fullmatch(r'[0-9a-f]{64}', expected_sha256), 'invalid expected digest')
    require(re.fullmatch(r'[0-9A-F]{40}', fingerprint), 'expected full uppercase OpenPGP fingerprint')
    package, output = Path(package), Path(output).absolute()
    require(package.is_file() and not package.is_symlink(), 'invalid package file')
    require(0 < package.stat().st_size <= 512 * 1024 * 1024, 'package size rejected')
    require(re.fullmatch(r'[0-9]+\.[0-9]+\.[0-9]+(?:~(?:alpha|beta)[0-9]+)?', expected_version), 'invalid release version')
    require(output.parent.is_dir() and not output.exists() and not output.is_symlink(), 'output must be a new directory')
    # The operator must use a trusted private working parent and keyring.
    with tempfile.TemporaryDirectory(prefix='.syspulse-apt-', dir=output.parent) as temp:
        root = Path(temp) / 'snapshot'
        root.mkdir()
        (root / 'pool').mkdir()
        target = root / 'pool' / f'syspulse_{expected_version}_amd64.deb'
        shutil.copyfile(package, target)
        require(target.stat().st_size <= 512 * 1024 * 1024 and digest(target) == expected_sha256, 'package digest mismatch')
        for field, expected in [('Package', 'syspulse'), ('Architecture', 'amd64'), ('Version', expected_version)]:
            require(run('/usr/bin/dpkg-deb', '--field', str(target), field).decode().strip() == expected, 'wrong package ' + field)
        index = root / 'Packages'
        index.write_bytes(run('/usr/bin/dpkg-scanpackages', '--arch', 'amd64', 'pool', '/dev/null', cwd=root))
        require(index.stat().st_size > 0, 'empty package index')
        now = datetime.now(timezone.utc).replace(microsecond=0)
        release = root / 'Release'
        release.write_text('Origin: SysPulse Community\nLabel: SysPulse\nArchitectures: amd64\nDate: ' + format_datetime(now, usegmt=True) + '\nValid-Until: ' + format_datetime(now + timedelta(days=7), usegmt=True) + '\nSHA256:\n ' + digest(index) + ' ' + str(index.stat().st_size) + ' Packages\n', encoding='ascii')
        cmd = ['/usr/bin/gpg', '--batch', '--no-tty', '--local-user', fingerprint + '!', '--digest-algo', 'SHA256']
        run(*cmd, '--output', str(root / 'InRelease'), '--clearsign', str(release))
        run(*cmd, '--output', str(root / 'Release.gpg'), '--detach-sign', str(release))
        for p in root.rglob('*'):
            p.chmod(0o755 if p.is_dir() else 0o644)
        root.chmod(0o755)
        require(not output.exists() and not output.is_symlink(), 'output appeared during build')
        root.rename(output)
    return output


def main():
    p = argparse.ArgumentParser(description=__doc__)
    for arg in ('package', 'expected-sha256', 'expected-version', 'output', 'fingerprint'):
        p.add_argument('--' + arg, required=True)
    try:
        print(build(**vars(p.parse_args())))
    except (OSError, ValueError, subprocess.SubprocessError) as error:
        p.exit(1, f'Snapshot rejected: {error}\n')


if __name__ == '__main__':
    main()
