#!/usr/bin/python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Real local Git archive and failure reporting fixtures; no GUI or system changes."""
import os
import pathlib
import subprocess
import tempfile
import unittest

BUILDER = pathlib.Path(__file__).resolve().parents[1] / 'tools/build-alpha39-local.sh'


class Tests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.base = pathlib.Path(self.tmp.name)
        self.home = self.base / 'home'
        self.home.mkdir()
        self.repo = self.base / 'local repo'
        self.repo.mkdir()
        self.env = dict(os.environ, HOME=str(self.home), XDG_CONFIG_HOME=str(self.home / 'config'), GIT_CONFIG_NOSYSTEM='1', GIT_CONFIG_GLOBAL='/dev/null')
        self.git('init', '-q')
        self.git('config', 'user.name', 'Fixture')
        self.git('config', 'user.email', 'fixture@example.invalid')
        (self.repo / 'tools').mkdir()
        (self.repo / 'CMakeLists.txt').write_text('set(SYSPULSE_ALPHA_NUMBER 39)\n')
        self.runner = self.repo / 'tools/test-offline-alpha39.sh'
        self.runner.write_text('''#!/usr/bin/env bash
set -euo pipefail
[[ $1 == --full ]]
cd "$(dirname "$0")/.."
[[ ! -e untracked.txt && ! -e .git ]]
grep -Fxq 'set(SYSPULSE_ALPHA_NUMBER 39)' CMakeLists.txt
[[ $GIT_NO_REPLACE_OBJECTS == 1 && $GIT_TERMINAL_PROMPT == 0 ]]
printf 'FIXTURE ONLY: source pin and test dispatch verified\\n'
''')
        self.commit = self.save('initial')

    def tearDown(self):
        self.tmp.cleanup()

    def git(self, *args):
        return subprocess.check_output(['git', '-C', str(self.repo), *args], env=self.env, text=True).strip()

    def save(self, label):
        self.git('add', '-A')
        self.git('commit', '-qm', label)
        return self.git('rev-parse', 'HEAD')

    def launch(self, *args):
        result = subprocess.run(['bash', str(BUILDER), *map(str, args)], env=self.env, text=True, capture_output=True, timeout=30)
        folders = list(self.home.glob('SysPulse-alpha39-local.*'))
        return result, folders[0] / 'results' if folders else None

    def success(self, pin):
        r, out = self.launch('--diagnostics', pin, self.repo)
        self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
        self.assertEqual((out / 'SOURCE-CHECKOUT.txt').read_text().strip(), pin)
        self.assertIn('FIXTURE ONLY', (out / 'OFFLINE-DIAGNOSTICS.txt').read_text())
        self.assertTrue((out / 'DIAGNOSTIC-STATUS.txt').is_file())
        self.assertFalse((out / 'FAILURE-STATUS.txt').exists())
        self.assertFalse(list(out.glob('*.deb')))

    def failure(self, pin, phase):
        r, out = self.launch('--diagnostics', pin, self.repo)
        self.assertNotEqual(r.returncode, 0)
        self.assertIn('FAILED: ' + phase, (out / 'FAILURE-STATUS.txt').read_text())
        self.assertFalse((out / 'DIAGNOSTIC-STATUS.txt').exists())
        self.assertFalse(list(out.glob('*.deb')))
        return out

    def test_local_no_qt(self):
        self.success(self.commit)

    def test_dirty_worktree_ignored(self):
        self.runner.write_text('exit 41\n')
        (self.repo / 'untracked.txt').touch()
        self.success(self.commit)

    def test_older_pin_not_head(self):
        self.runner.write_text('exit 42\n')
        self.save('new head')
        self.success(self.commit)

    def test_nonexistent_commit(self):
        self.failure('0' * 40, 'source')

    def test_wrong_alpha(self):
        (self.repo / 'CMakeLists.txt').write_text('set(SYSPULSE_ALPHA_NUMBER 38)\n')
        self.failure(self.save('wrong alpha'), 'source')

    def test_failed_suite_retains_log(self):
        self.runner.write_text('echo fixture-failed; exit 43\n')
        out = self.failure(self.save('failing suite'), 'offline-diagnostics')
        self.assertIn('fixture-failed', (out / 'OFFLINE-DIAGNOSTICS.txt').read_text())
        self.assertIn('exit 43', (out / 'FAILURE-STATUS.txt').read_text())

    def test_missing_runner(self):
        self.runner.unlink()
        self.failure(self.save('missing runner'), 'offline-diagnostics')

    def test_bad_pin(self):
        r, out = self.launch('--diagnostics', 'short', self.repo)
        self.assertEqual(r.returncode, 2)
        self.assertIsNone(out)

    def test_missing_repo(self):
        r, out = self.launch('--diagnostics', self.commit, self.base / 'missing')
        self.assertEqual(r.returncode, 2)
        self.assertIsNone(out)

    def test_replace_ref_ignored(self):
        self.runner.write_text('exit 44\n')
        replacement = self.save('replacement')
        self.git('replace', self.commit, replacement)
        self.success(self.commit)


if __name__ == '__main__':
    if os.geteuid() == 0:
        raise SystemExit('Run as normal user')
    unittest.main(verbosity=2)
