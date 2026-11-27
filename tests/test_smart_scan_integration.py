#!/usr/bin/python3
# SPDX-License-Identifier: GPL-3.0-or-later
import json, pathlib, sys, unittest
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1] / "tools"))
import syspulse_hybrid as hybrid


def disk(name, transport="sata", removable=False):
    return {"name": name, "type": "disk", "tran": transport, "rm": removable}


def evidence(rc, passed):
    return json.dumps({"smartctl": {"exit_status": rc}, "smart_status": {"passed": passed}, "serial_number": "PRIVATE"})


class Tests(unittest.TestCase):
    def scan(self, disks, response, device_ok=lambda _: True):
        calls = []
        def command(args, **kwargs):
            calls.append((args, kwargs))
            if args[0] == "lsblk":
                return "ok", json.dumps({"blockdevices": disks}), False
            return response
        return hybrid.smart_scan(command, device_ok), calls

    def test_healthy_and_privacy(self):
        result, calls = self.scan([disk("sda"), disk("nvme0n1", "nvme")], ("ok", evidence(0, True), False))
        self.assertEqual(result["status"], "ok")
        self.assertEqual([d["health"] for d in result["disks"]], ["passed", "passed"])
        self.assertNotIn("PRIVATE", json.dumps(result))
        self.assertNotIn("sda", json.dumps(result))
        self.assertEqual(calls[2][0], ["smartctl", "-j", "-H", "-A", "-n", "standby,3", "/dev/sda"])
        self.assertEqual(calls[1][0], ["smartctl", "-j", "-H", "-A", "/dev/nvme0n1"])

    def test_failed_command_cannot_pass(self):
        result, _ = self.scan([disk("sda")], ("failed", evidence(0, True), False))
        self.assertEqual(result["status"], "partial")
        self.assertEqual(result["disks"][0]["health"], "unknown")

    def test_standby_and_permission(self):
        for reply in (("failed", evidence(3, True), False), ("permission", "", False), ("ok", evidence(0, True), True)):
            result, _ = self.scan([disk("sda")], reply)
            self.assertEqual(result["status"], "partial")
            self.assertEqual(result["disks"][0]["health"], "unknown")

    def test_current_failure_remains_visible(self):
        result, _ = self.scan([disk("sda")], ("failed", evidence(11, False), False))
        self.assertEqual(result["status"], "partial")
        self.assertEqual(result["disks"][0]["health"], "critical")

    def test_device_scope_and_limit(self):
        disks = [disk("sd" + c) for c in "abcdef"] + [disk("../sda"), disk("sdg", "usb"), disk("sdh", removable=True)]
        result, calls = self.scan(disks, ("ok", evidence(0, True), False))
        self.assertEqual(result["excluded"], 5)
        self.assertEqual(result["status"], "partial")
        self.assertEqual(len(calls), 5)
        self.assertEqual([c[0][-1] for c in calls[1:]], ["/dev/sda", "/dev/sdb", "/dev/sdc", "/dev/sdd"])

    def test_missing_device_never_probed(self):
        result, calls = self.scan([disk("sda")], ("ok", evidence(0, True), False), lambda _: False)
        self.assertEqual(result, {"status": "partial", "disks": [], "excluded": 1})
        self.assertEqual(len(calls), 1)


if __name__ == "__main__":
    unittest.main(verbosity=2)
