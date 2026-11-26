#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
import hashlib
import importlib.util
import json
from pathlib import Path
import subprocess
import tempfile
import unittest

spec = importlib.util.spec_from_file_location("signed_package", Path(__file__).parents[1] / "tools/verify-signed-package.py")
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)


class SignedPackage(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.work = tempfile.TemporaryDirectory()
        cls.addClassCleanup(cls.work.cleanup)
        cls.private = Path(cls.work.name) / "private.pem"
        cls.public = Path(cls.work.name) / "public.pem"
        subprocess.run(["/usr/bin/openssl", "genpkey", "-algorithm", "RSA", "-pkeyopt", "rsa_keygen_bits:2048", "-out", str(cls.private)], check=True, stderr=subprocess.DEVNULL)
        subprocess.run(["/usr/bin/openssl", "pkey", "-in", str(cls.private), "-pubout", "-out", str(cls.public)], check=True)

    def setUp(self):
        work = tempfile.TemporaryDirectory()
        self.addCleanup(work.cleanup)
        root = Path(work.name)
        self.package = root / "syspulse-fixture.deb"
        self.package.write_bytes(b"synthetic package fixture")
        self.manifest = root / "manifest.json"
        self.signature = root / "manifest.sig"
        self.kwargs = dict(package=self.package, manifest=self.manifest, signature=self.signature, key=self.public, key_sha256=hashlib.sha256(self.public.read_bytes()).hexdigest(), version="3.0.0-alpha.2", target="linux-mint-22.3-amd64", commit="a" * 40)
        self.record = dict(schema=1, version=self.kwargs["version"], target=self.kwargs["target"], commit=self.kwargs["commit"], filename=self.package.name, size=self.package.stat().st_size, sha256=hashlib.sha256(self.package.read_bytes()).hexdigest())
        self.sign()

    def sign(self, raw=None):
        self.manifest.write_bytes(json.dumps(self.record).encode() if raw is None else raw)
        subprocess.run(["/usr/bin/openssl", "dgst", "-sha256", "-sign", str(self.private), "-out", str(self.signature), str(self.manifest)], check=True)

    def check(self):
        return module.verify(**self.kwargs)

    def test_valid(self):
        self.assertEqual(self.check()["version"], self.record["version"])

    def test_tampered_package(self):
        self.package.write_bytes(b"X" + self.package.read_bytes()[1:])
        with self.assertRaisesRegex(ValueError, "integrity mismatch"):
            self.check()

    def test_tampered_manifest(self):
        self.manifest.write_bytes(self.manifest.read_bytes() + b" ")
        with self.assertRaisesRegex(ValueError, "signature rejected"):
            self.check()

    def test_wrong_trust(self):
        self.kwargs["key_sha256"] = "0" * 64
        with self.assertRaisesRegex(ValueError, "untrusted public key"):
            self.check()

    def test_wrong_target(self):
        self.record["target"] = "ubuntu-24.04-amd64"
        self.sign()
        with self.assertRaisesRegex(ValueError, "identity mismatch"):
            self.check()

    def test_duplicate_key(self):
        self.sign(b'{"schema":1,"schema":1}')
        with self.assertRaisesRegex(ValueError, "duplicate manifest key"):
            self.check()

    def test_path_escape(self):
        self.record["filename"] = "../syspulse-fixture.deb"
        self.sign()
        with self.assertRaisesRegex(ValueError, "filename mismatch"):
            self.check()

    def test_oversized_manifest(self):
        self.manifest.write_bytes(b"x" * 16385)
        with self.assertRaisesRegex(ValueError, "file size rejected"):
            self.check()

    def test_symlink_package(self):
        old = self.package.with_name("original.deb")
        self.package.rename(old)
        self.package.symlink_to(old)
        with self.assertRaisesRegex(ValueError, "regular package"):
            self.check()

    def test_wrong_commit(self):
        self.record["commit"] = "b" * 40
        self.sign()
        with self.assertRaisesRegex(ValueError, "identity mismatch"):
            self.check()

    def test_package_growth(self):
        with self.package.open("ab") as stream:
            stream.write(b"more")
        with self.assertRaisesRegex(ValueError, "length mismatch"):
            self.check()


if __name__ == "__main__":
    unittest.main()
