#!/usr/bin/python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Real disposable .deb fixtures, never installed or executed."""
import hashlib
import importlib.machinery
import importlib.util
import pathlib
import subprocess
import tempfile
import unittest

loader = importlib.machinery.SourceFileLoader("gate", str(pathlib.Path(__file__).resolve().parents[1] / "tools/verify-alpha39-payload.py"))
spec = importlib.util.spec_from_loader(loader.name, loader)
gate = importlib.util.module_from_spec(spec)
loader.exec_module(gate)
KEY = b"DISPOSABLE TEST DATA, NOT AN OWNER KEY\n"
DIGEST = hashlib.sha256(KEY).hexdigest()
VERSION = gate.VERSIONS[0]


class Tests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.base = pathlib.Path(self.temp.name)
        self.root = self.base / "root"
        self.root.mkdir()
        (self.root / "DEBIAN").mkdir(mode=0o755)
        (self.root / "DEBIAN").chmod(0o755)
        self.control = self.root / "DEBIAN/control"
        self.control.write_text(f"Package: syspulse\nVersion: {VERSION}\nArchitecture: amd64\nMaintainer: Fixture <fixture@example.invalid>\nDescription: Test data only\n")
        self.control.chmod(0o644)
        self.deb = self.base / "fixture.deb"
        for name, mode in gate.REQUIRED.items():
            p = self.root / name
            p.parent.mkdir(parents=True, exist_ok=True)
            p.write_bytes(KEY if name == gate.KEY else (DIGEST + "\n").encode() if name == gate.KEY_HASH else b"fixture, not executable application\n")
            p.chmod(mode)
        for name in gate.PARENTS:
            (self.root / name).chmod(0o755)

    def tearDown(self):
        self.temp.cleanup()

    def build(self):
        subprocess.run(["dpkg-deb", "--root-owner-group", "--build", str(self.root), str(self.deb)], check=True, stdout=subprocess.DEVNULL)

    def check(self):
        self.build()
        return gate.verify_deb(self.deb, VERSION, DIGEST)

    def test_valid(self):
        self.assertEqual(self.check(), len(gate.REQUIRED))

    def test_missing_smart(self):
        (self.root / "usr/bin/syspulse_smart_evidence.py").unlink()
        with self.assertRaisesRegex(ValueError, "Missing payload entries"):
            self.check()

    def test_missing_updater(self):
        (self.root / "usr/bin/syspulse-apt-update").unlink()
        with self.assertRaisesRegex(ValueError, "Missing payload entries"):
            self.check()

    def test_insecure_mode(self):
        (self.root / "usr/bin/syspulse-admin-session").chmod(0o4755)
        with self.assertRaisesRegex(ValueError, "Invalid required payload entry"):
            self.check()

    def test_module_executable(self):
        (self.root / "usr/bin/syspulse_smart_evidence.py").chmod(0o755)
        with self.assertRaisesRegex(ValueError, "Invalid required payload entry"):
            self.check()

    def test_symlink(self):
        p = self.root / "usr/bin/syspulse-apt-update"
        p.unlink()
        p.symlink_to("syspulse-diagnostics")
        with self.assertRaisesRegex(ValueError, "Invalid required payload entry"):
            self.check()

    def test_writable_parent(self):
        (self.root / "usr/bin").chmod(0o777)
        with self.assertRaisesRegex(ValueError, "Unsafe required parent"):
            self.check()

    def test_inaccessible_parent(self):
        (self.root / "usr/bin").chmod(0o700)
        with self.assertRaisesRegex(ValueError, "Unsafe required parent"):
            self.check()

    def test_bad_anchor(self):
        (self.root / gate.KEY).write_bytes(b"tampered")
        with self.assertRaisesRegex(ValueError, "public anchor mismatch"):
            self.check()

    def test_bad_anchor_record(self):
        (self.root / gate.KEY_HASH).write_text("0" * 64)
        with self.assertRaisesRegex(ValueError, "public anchor mismatch"):
            self.check()

    def test_wrong_architecture(self):
        self.control.write_text(self.control.read_text().replace("amd64", "arm64"))
        with self.assertRaisesRegex(ValueError, "Wrong package Architecture"):
            self.check()

    def test_wrong_version(self):
        self.control.write_text(self.control.read_text().replace(VERSION, "1.0"))
        with self.assertRaisesRegex(ValueError, "Wrong package Version"):
            self.check()

    def test_production_anchor_cannot_be_overridden_on_cli(self):
        self.build()
        result = subprocess.run(["python3", gate.__file__, str(self.deb), VERSION], capture_output=True, text=True)
        self.assertEqual(result.returncode, 1)
        self.assertIn("public anchor mismatch", result.stderr)


if __name__ == "__main__":
    unittest.main(verbosity=2)
