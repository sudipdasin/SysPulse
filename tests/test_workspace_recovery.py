#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Actual disposable Git repositories, no network or application build."""
import os
from pathlib import Path
import subprocess
import tempfile
import unittest

TOOL = Path(__file__).resolve().parents[1] / 'tools/recover-integration-workspace.py'


class Tests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.root = Path(self.tmp.name)
        self.src = self.root / 'source'
        self.dest = self.root / 'checkout'
        self.src.mkdir()
        self.env = dict(os.environ, HOME=str(self.root), XDG_CONFIG_HOME=str(self.root / 'config'), GIT_CONFIG_NOSYSTEM='1', GIT_CONFIG_GLOBAL='/dev/null')
        self.git(self.src, 'init', '-q', '--initial-branch=integration/alpha39')
        self.git(self.src, 'config', 'user.name', 'Fixture')
        self.git(self.src, 'config', 'user.email', 'fixture@example.invalid')
        (self.src / 'file.txt').write_text('reviewed\n')
        self.git(self.src, 'add', '.')
        self.git(self.src, 'commit', '-qm', 'first')
        self.pin = self.git(self.src, 'rev-parse', 'HEAD')

    def tearDown(self):
        self.tmp.cleanup()

    def git(self, path, *args):
        return subprocess.check_output(['git', '-C', str(path), *args], text=True, env=self.env).strip()

    def run_tool(self, mode='create', pin=None, path=None):
        return subprocess.run(['python3', str(TOOL), mode, str(path or self.dest), pin or self.pin, '--local-repo', str(self.src)], env=self.env, text=True, capture_output=True, timeout=30)

    def make(self):
        r = self.run_tool()
        self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
        self.assertEqual(self.git(self.dest, 'rev-parse', 'HEAD'), self.pin)
        self.assertEqual(self.git(self.dest, 'rev-parse', '--is-shallow-repository'), 'false')

    def test_create_and_check(self):
        self.make()
        self.assertEqual(self.run_tool('check').returncode, 0)

    def test_existing_directory_preserved(self):
        self.dest.mkdir()
        (self.dest / 'sentinel').write_text('keep')
        self.assertNotEqual(self.run_tool().returncode, 0)
        self.assertEqual((self.dest / 'sentinel').read_text(), 'keep')

    def test_existing_empty_directory_refused(self):
        self.dest.mkdir()
        self.assertNotEqual(self.run_tool().returncode, 0)
        self.assertEqual(list(self.dest.iterdir()), [])

    def test_dirty_checkout_preserved(self):
        self.make()
        (self.dest / 'file.txt').write_text('local work')
        self.assertNotEqual(self.run_tool('check').returncode, 0)
        self.assertEqual((self.dest / 'file.txt').read_text(), 'local work')

    def test_untracked_checkout_preserved(self):
        self.make()
        (self.dest / 'new.txt').write_text('new work')
        self.assertNotEqual(self.run_tool('check').returncode, 0)
        self.assertTrue((self.dest / 'new.txt').exists())

    def test_advanced_source_refused(self):
        (self.src / 'file.txt').write_text('second\n')
        self.git(self.src, 'commit', '-qam', 'second')
        r = self.run_tool()
        self.assertNotEqual(r.returncode, 0)
        self.assertIn('checkout refused', r.stderr)
        self.assertFalse((self.dest / 'file.txt').exists())

    def test_wrong_pin(self):
        self.assertNotEqual(self.run_tool(pin='0' * 40).returncode, 0)
        self.assertFalse((self.dest / 'file.txt').exists())

    def test_invalid_pin_no_workspace(self):
        self.assertNotEqual(self.run_tool(pin='short').returncode, 0)
        self.assertFalse(self.dest.exists())

    def test_wrong_origin(self):
        self.make()
        self.git(self.dest, 'remote', 'set-url', 'origin', 'https://example.invalid/')
        self.assertNotEqual(self.run_tool('check').returncode, 0)

    def test_wrong_branch(self):
        self.make()
        self.git(self.dest, 'switch', '-qc', 'topic')
        self.assertNotEqual(self.run_tool('check').returncode, 0)
        self.assertEqual(self.git(self.dest, 'branch', '--show-current'), 'topic')

    def test_full_history_preserved(self):
        old = self.pin
        (self.src / 'file.txt').write_text('second\n')
        self.git(self.src, 'commit', '-qam', 'second')
        self.pin = self.git(self.src, 'rev-parse', 'HEAD')
        self.make()
        self.assertEqual(self.git(self.dest, 'rev-parse', 'HEAD^'), old)

    def test_tracking_ref_drift(self):
        self.make()
        self.git(self.dest, 'update-ref', '-d', 'refs/remotes/origin/integration/alpha39')
        self.assertNotEqual(self.run_tool('check').returncode, 0)
        self.assertEqual(self.git(self.dest, 'rev-parse', 'HEAD'), self.pin)

    def test_symlink_refused(self):
        self.dest.symlink_to(self.src, target_is_directory=True)
        self.assertNotEqual(self.run_tool().returncode, 0)
        self.assertNotEqual(self.run_tool('check').returncode, 0)
        self.assertTrue(self.dest.is_symlink())


if __name__ == '__main__':
    unittest.main(verbosity=2)
