#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
import importlib.util
from pathlib import Path
import unittest

spec = importlib.util.spec_from_file_location("release_policy", Path(__file__).parents[1] / "tools/release-policy.py")
policy = importlib.util.module_from_spec(spec)
spec.loader.exec_module(policy)


class ReleasePolicy(unittest.TestCase):
    def test_numeric_versions(self):
        self.assertEqual(policy.admit("3.0.0-alpha.2", "3.0.0-alpha.10", "preview"), "3.0.0-alpha.10")
        self.assertEqual(policy.admit("3.9.0", "3.10.0", "stable"), "3.10.0")

    def test_stable_after_preview(self):
        self.assertEqual(policy.admit("3.0.0-beta.999", "3.0.0", "stable"), "3.0.0")

    def test_beta_after_alpha(self):
        self.assertEqual(policy.admit("3.0.0-alpha.99", "3.0.0-beta.1", "preview"), "3.0.0-beta.1")

    def test_preview_requires_opt_in(self):
        with self.assertRaisesRegex(ValueError, "preview channel"):
            policy.admit("3.0.0", "3.1.0-alpha.1", "stable")

    def test_equal_and_older_rejected(self):
        for candidate in ("3.0.0", "2.99.99", "3.0.0-beta.99"):
            with self.subTest(candidate=candidate), self.assertRaisesRegex(ValueError, "not newer"):
                policy.admit("3.0.0", candidate, "preview")

    def test_unknown_channel(self):
        with self.assertRaisesRegex(ValueError, "channel"):
            policy.admit("3.0.0", "3.0.1", "nightly")

    def test_malformed_rejected(self):
        for value in (None, "v3.0.0", "03.0.0", "3.0.0-alpha.01", "3.0.0+foo", "3.0.0\n", "3.0.0-rc.1", "1234567.0.0", "3.٠.0"):
            with self.subTest(value=value), self.assertRaisesRegex(ValueError, "version"):
                policy.parse_version(value)


if __name__ == "__main__":
    unittest.main()
