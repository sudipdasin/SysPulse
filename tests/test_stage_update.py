#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
import importlib.util
import os
from pathlib import Path
import unittest
import test_signed_package as fixtures

spec = importlib.util.spec_from_file_location('stage_update', Path(__file__).parents[1] / 'tools/stage-update.py')
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)


class StagedUpdate(fixtures.SignedPackage):
    def stage(self, **changes):
        args = dict(self.kwargs, installed='3.0.0-alpha.1', channel='preview')
        args.update(changes)
        return module.verified_snapshot(**args)

    def test_private_snapshot_cleanup(self):
        original = self.package.read_bytes()
        with self.stage() as (path, info):
            self.assertEqual(path.read_bytes(), original)
            self.assertEqual(path.stat().st_mode & 0o777, 0o600)
            self.assertEqual(path.parent.parent.stat().st_mode & 0o777, 0o700)
            self.package.write_bytes(b'changed after verification')
            self.assertEqual(path.read_bytes(), original)
            self.assertEqual(info['version'], self.record['version'])
        self.assertFalse(path.exists())

    def test_consumer_exception_cleanup(self):
        with self.assertRaisesRegex(RuntimeError, 'fixture'):
            with self.stage() as (path, _):
                raise RuntimeError('fixture')
        self.assertFalse(path.exists())

    def test_downgrade_blocked(self):
        with self.assertRaisesRegex(ValueError, 'not newer'):
            with self.stage(installed='3.0.0'):
                self.fail('yielded rejected payload')

    def test_stable_channel_blocks_preview(self):
        with self.assertRaisesRegex(ValueError, 'preview channel'):
            with self.stage(channel='stable'):
                self.fail('yielded rejected payload')

    def test_tamper_no_yield(self):
        self.package.write_bytes(b'corrupt')
        with self.assertRaises(ValueError):
            with self.stage():
                self.fail('yielded rejected payload')

    def test_symlink_no_yield(self):
        self.package.unlink()
        self.package.symlink_to(self.manifest)
        with self.assertRaises(ValueError):
            with self.stage():
                self.fail('yielded rejected payload')

    def test_fifo_no_yield(self):
        self.package.unlink()
        os.mkfifo(self.package)
        with self.assertRaisesRegex(ValueError, 'unsafe input'):
            with self.stage():
                self.fail('yielded rejected payload')

    def test_bad_key_no_yield(self):
        with self.assertRaisesRegex(ValueError, 'untrusted public key'):
            with self.stage(key_sha256='0' * 64):
                self.fail('yielded rejected payload')


if __name__ == '__main__':
    unittest.main()
