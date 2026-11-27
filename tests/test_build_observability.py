#!/usr/bin/python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Local disposable Git fixtures. No GUI compilation or package installation."""
import os
from pathlib import Path
import subprocess
import tempfile
import unittest

BUILDER = Path(__file__).resolve().parents[1] / "tools/build-alpha39-local.sh"

class Tests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name)
        self.repo = self.root / "repo"
        self.home = self.root / "home"
        self.repo.mkdir(); self.home.mkdir()
        self.env = dict(os.environ, HOME=str(self.home), XDG_CONFIG_HOME=str(self.home / "config"), GIT_CONFIG_NOSYSTEM="1", GIT_CONFIG_GLOBAL="/dev/null")
        self.env.pop("SYSPULSE_BUILD_JOBS", None)
        self.git("init", "-q")
        self.git("config", "user.name", "Fixture")
        self.git("config", "user.email", "fixture@example.invalid")
        (self.repo / "tools").mkdir()
        (self.repo / "CMakeLists.txt").write_text("set(SYSPULSE_ALPHA_NUMBER 39)\n")

    def tearDown(self):
        self.temp.cleanup()

    def git(self, *args):
        return subprocess.check_output(["git", "-C", str(self.repo), *args], env=self.env, text=True).strip()

    def launch(self, script, jobs=None):
        (self.repo / "tools/test-offline-alpha39.sh").write_text(script)
        self.git("add", "-A"); self.git("commit", "-qm", "fixture")
        env = self.env.copy()
        if jobs is not None:
            env["SYSPULSE_BUILD_JOBS"] = jobs
        result = subprocess.run(["bash", str(BUILDER), "--diagnostics", self.git("rev-parse", "HEAD"), str(self.repo)], env=env, text=True, capture_output=True, timeout=30)
        dirs = list(self.home.glob("SysPulse-alpha39-local.*"))
        return result, dirs[0] / "results" if dirs else None

    def test_default_jobs(self):
        r, out = self.launch("echo fixture\n")
        self.assertEqual(r.returncode, 0, r.stderr)
        self.assertIn("build jobs 2", r.stdout)
        self.assertTrue((out / "DIAGNOSTIC-STATUS.txt").is_file())

    def test_one_job(self):
        r, out = self.launch("echo fixture\n", "1")
        self.assertEqual(r.returncode, 0, r.stderr)
        self.assertIn("build jobs 1", r.stdout)
        self.assertTrue((out / "DIAGNOSTIC-STATUS.txt").is_file())

    def test_invalid_jobs(self):
        r, out = self.launch("exit 90\n", "3")
        self.assertEqual(r.returncode, 2)
        self.assertIsNone(out)

    def test_stdout_stderr_retained(self):
        r, out = self.launch("echo stdout-fixture; echo stderr-fixture >&2\n")
        self.assertEqual(r.returncode, 0, r.stderr)
        self.assertEqual((out / "OFFLINE-DIAGNOSTICS.txt").read_text(), "stdout-fixture\nstderr-fixture\n")

    def test_pipeline_preserves_exit(self):
        r, out = self.launch("echo failure-fixture >&2; exit 43\n")
        self.assertEqual(r.returncode, 43)
        self.assertIn("failure-fixture", (out / "OFFLINE-DIAGNOSTICS.txt").read_text())
        self.assertIn("FAILED: offline-diagnostics (exit 43)", (out / "FAILURE-STATUS.txt").read_text())
        self.assertFalse((out / "DIAGNOSTIC-STATUS.txt").exists())
        self.assertFalse(list(out.glob("*.deb")))

if __name__ == "__main__":
    if os.geteuid() == 0:
        raise SystemExit("Run as normal user")
    unittest.main(verbosity=2)
