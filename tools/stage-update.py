#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Verify an isolated package snapshot; no downloading, elevation or installation."""
from contextlib import contextmanager
import importlib.util
import os
from pathlib import Path
import stat
import tempfile


def load(name):
    spec = importlib.util.spec_from_file_location(name, Path(__file__).with_name(name + '.py'))
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


verifier = load('verify-signed-package')
policy = load('release-policy')


def copy_regular(source, destination, limit):
    if not stat.S_ISREG(os.lstat(source).st_mode):
        raise ValueError("unsafe input type or size")
    fd = os.open(source, os.O_RDONLY | os.O_NOFOLLOW | os.O_NONBLOCK | os.O_CLOEXEC)
    with os.fdopen(fd, 'rb') as inp:
        info = os.fstat(inp.fileno())
        if not stat.S_ISREG(info.st_mode) or not 0 < info.st_size <= limit:
            raise ValueError('unsafe input type or size')
        outfd = os.open(destination, os.O_WRONLY | os.O_CREAT | os.O_EXCL | os.O_NOFOLLOW, 0o600)
        with os.fdopen(outfd, 'wb') as out:
            copied = 0
            while True:
                block = inp.read(1024 * 1024)
                if not block:
                    break
                copied += len(block)
                if copied > limit:
                    raise ValueError('input grew beyond limit')
                out.write(block)
            if copied != info.st_size:
                raise ValueError('input size changed')


@contextmanager
def verified_snapshot(*, package, manifest, signature, key, key_sha256,
                      installed, version, target, commit, channel='stable'):
    """Yield private verified bytes for inspection only; remove them on exit.

    The caller must authenticate installed version and release selection, and
    obtain the key fingerprint independently. Same-UID hostile code is outside
    this boundary. Never pass this user-writable path to a privileged installer.
    """
    with tempfile.TemporaryDirectory(prefix='syspulse-stage-') as work:
        root = Path(work)
        payload = root / 'payload'
        payload.mkdir(mode=0o700)
        staged = payload / Path(package).name
        for source, dest, cap in ((package, staged, 512 * 1024 * 1024),
                                  (manifest, root / 'manifest', 16384),
                                  (signature, root / 'signature', 8192),
                                  (key, root / 'key', 16384)):
            copy_regular(source, dest, cap)
        result = verifier.verify(staged, root / 'manifest', root / 'signature',
                                 root / 'key', key_sha256, version, target, commit)
        policy.admit(installed, result['version'], channel)
        yield staged, result
