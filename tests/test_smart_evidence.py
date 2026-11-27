#!/usr/bin/python3
# SPDX-License-Identifier: GPL-3.0-or-later
import json, pathlib, sys, unittest
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1] / "tools"))
from syspulse_smart_evidence import smart_result


class Tests(unittest.TestCase):
    def parse(self, rc=0, passed=True, status=None, **extra):
        return smart_result(json.dumps({"smartctl": {"exit_status": rc}, "smart_status": {"passed": passed}, **extra}), status or ("ok" if rc == 0 else "failed"), False)

    def test_all_exit_masks(self):
        for rc in range(256):
            with self.subTest(rc=rc):
                want = "critical" if rc & 0x18 else "warning" if rc & 0xe0 else "unknown" if rc & 7 else "passed"
                actual = self.parse(rc)
                self.assertEqual(actual["health"], want)
                self.assertEqual(actual["status"], "partial" if rc & 7 else "ok")

    def test_contradictory_command_status(self):
        for rc, status in ((0, "failed"), (3, "ok")):
            self.assertEqual(self.parse(rc, status=status)["health"], "unknown")
        self.assertEqual(self.parse(8, status="ok")["health"], "critical")

    def test_unavailable_and_bounds(self):
        raw = '{"smartctl":{"exit_status":0},"smart_status":{"passed":true}}'
        for status in ("permission", "timeout", "unavailable", "unexpected"):
            self.assertEqual(smart_result(raw, status, False)["health"], "unknown")
        self.assertEqual(smart_result(raw, "ok", True)["health"], "unknown")
        self.assertEqual(smart_result(raw + " " * 131072, "ok", False)["health"], "unknown")

    def test_duplicate_json(self):
        raw = '{"smartctl":{"exit_status":0},"smart_status":{"passed":false,"passed":true}}'
        self.assertEqual(smart_result(raw, "ok", False)["health"], "unknown")

    def test_bad_counter_values(self):
        for value in (True, -1, "0", None, 2**64, 256):
            self.assertEqual(self.parse(nvme_smart_health_information_log={"critical_warning": value})["health"], "unknown")

    def test_bad_health_values(self):
        for value in (0, 1, "true", None):
            self.assertEqual(self.parse(passed=value)["health"], "unknown")

    def test_bad_table(self):
        for table in (None, {}, [None], [{"id": 197, "raw": {"value": "0"}}], [{"id": 197, "raw": {"value": False}}], [{}] * 257):
            self.assertEqual(self.parse(ata_smart_attributes={"table": table})["health"], "unknown")

    def test_duplicate_sector_attribute(self):
        table = [{"id": 197, "raw": {"value": v}} for v in (4, 0)]
        result = self.parse(ata_smart_attributes={"table": table})
        self.assertEqual(result, {"status": "partial", "health": "critical", "pending_sectors": 4})

    def test_partial_keeps_positive_evidence(self):
        self.assertEqual(self.parse(8, ata_smart_attributes=None)["health"], "critical")
        self.assertEqual(self.parse(passed=False, nvme_smart_health_information_log=[])["health"], "critical")
        self.assertEqual(self.parse(nvme_smart_health_information_log={"critical_warning": 1, "media_errors": "bad"})["health"], "critical")

    def test_historical_media_errors(self):
        self.assertEqual(self.parse(nvme_smart_health_information_log={"media_errors": 2})["health"], "warning")
        self.assertEqual(self.parse(ata_smart_attributes={"table": [{"id": 5, "raw": {"value": 2}}]})["health"], "warning")

    def test_missing_health_and_standby(self):
        self.assertEqual(smart_result('{"smartctl":{"exit_status":0}}', "ok", False)["health"], "unknown")
        self.assertEqual(self.parse(3)["health"], "unknown")

    def test_privacy(self):
        result = self.parse(serial_number="PRIVATE", model_name="SECRET", device={"name": "/dev/private"})
        self.assertEqual(result, {"status": "ok", "health": "passed"})

    def test_malformed_json(self):
        for raw in ("[]", "{", '{"smartctl":{"exit_status":NaN}}', '{"smartctl":{"exit_status":false}}'):
            self.assertEqual(smart_result(raw, "ok", False)["health"], "unknown")


if __name__ == "__main__":
    unittest.main(verbosity=2)
