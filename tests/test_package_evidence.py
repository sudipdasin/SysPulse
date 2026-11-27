#!/usr/bin/python3
# SPDX-License-Identifier: GPL-3.0-or-later
import copy, pathlib, sys, unittest
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1] / "tools"))
from syspulse_package_evidence import package_evidence_ok


def fixture():
    base = {"status": "ok", "bounded": False, "events": {}, "lines": 0}
    return [dict(base, name="dpkg-audit", status="empty"),
            dict(base, name="apt-repair-simulation", apt_simulation={"status": "no_changes", "execution_authorized": False, "install_or_upgrade": 0, "configure": 0, "remove": 0, "held_back": 3}),
            dict(base, name="dpkg-core-integrity", lines=3, integrity=[{"package": p, "status": "no_reported_differences", "core_file_changes": 0} for p in ("apt", "dpkg", "systemd")])]


class Tests(unittest.TestCase):
    def test_clear(self):
        self.assertTrue(package_evidence_ok(fixture()))
    def test_missing_each(self):
        for idx in range(3):
            rows = fixture(); rows.pop(idx)
            self.assertFalse(package_evidence_ok(rows))
    def test_no_evidence(self):
        self.assertFalse(package_evidence_ok([]))
    def test_duplicate_each(self):
        for idx in range(3):
            rows = fixture(); rows.append(copy.deepcopy(rows[idx]))
            self.assertFalse(package_evidence_ok(rows))
    def test_bad_coverage(self):
        for idx in range(3):
            for status in ("partial", "failed", "timeout", "permission", "unavailable"):
                rows = fixture(); rows[idx]["status"] = status
                self.assertFalse(package_evidence_ok(rows))
    def test_bounded_or_events(self):
        for idx in range(3):
            for key, value in (("bounded", True), ("events", {"package_failure": 1})):
                rows = fixture(); rows[idx][key] = value
                self.assertFalse(package_evidence_ok(rows))
    def test_nonempty_audit(self):
        rows = fixture(); rows[0]["lines"] = 1
        self.assertFalse(package_evidence_ok(rows))
    def test_solver_status(self):
        for status in ("review", "removal_blocked", "solver_failed", "unknown"):
            rows = fixture(); rows[1]["apt_simulation"]["status"] = status
            self.assertFalse(package_evidence_ok(rows))
    def test_solver_counts_and_authority(self):
        for key in ("install_or_upgrade", "configure", "remove", "execution_authorized"):
            for value in (1, True, None, "0"):
                rows = fixture(); rows[1]["apt_simulation"][key] = value
                self.assertFalse(package_evidence_ok(rows))
    def test_integrity_incomplete(self):
        rows = fixture(); rows[2]["integrity"].pop()
        self.assertFalse(package_evidence_ok(rows))
        rows = fixture(); rows[2]["integrity"][0]["package"] = "dpkg"
        self.assertFalse(package_evidence_ok(rows))
    def test_integrity_changes(self):
        rows = fixture(); rows[2]["integrity"][0].update(status="differences", core_file_changes=1)
        self.assertFalse(package_evidence_ok(rows))
        rows[2]["integrity"][0]["core_file_changes"] = 0
        self.assertTrue(package_evidence_ok(rows))
    def test_malformed(self):
        for rows in (None, [None], {}, ["bad"]):
            self.assertFalse(package_evidence_ok(rows))
        rows = fixture(); del rows[1]["apt_simulation"]
        self.assertFalse(package_evidence_ok(rows))


if __name__ == "__main__":
    unittest.main(verbosity=2)
