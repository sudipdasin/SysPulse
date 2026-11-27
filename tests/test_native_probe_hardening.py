#!/usr/bin/python3
# SPDX-License-Identifier: GPL-3.0-or-later
import json,pathlib,sys,unittest
sys.path.insert(0,str(pathlib.Path(__file__).resolve().parents[1]/"tools"))
import syspulse_native_probes as p
SUMMARY="0 upgraded, 0 newly installed, 0 to remove and 0 not upgraded.\n"
HEADER="r b swpd free buff cache si so bi bo in cs us sy id wa st\n"
ROW="2 1 0 0 0 0 5 7 0 0 0 0 10 5 80 5 0\n"
class Tests(unittest.TestCase):
    def test_notice_and_indented_warning(self):
        for line in ("N: advisory", "  W: warning", "\tE: failure"):
            with self.subTest(line=line):self.assertEqual(p.apt_result("ok",SUMMARY+line,False)["status"],"unknown")
    def test_removal_disabled(self):
        for status in ("ok","failed"):
            self.assertEqual(p.apt_result(status,"E: Packages need to be removed but remove is disabled.",False)["status"],"removal_blocked")
    def test_purge_is_removal(self):
        self.assertEqual(p.apt_result("ok",SUMMARY+"Purg secret",False)["status"],"removal_blocked")
    def test_malformed_actions(self):
        self.assertEqual(p.apt_result("ok",SUMMARY+"Conf\n",False)["status"],"unknown")
    def test_large_numbers(self):
        self.assertEqual(p.apt_result("ok",SUMMARY.replace("0 not upgraded","100001 not upgraded"),False)["status"],"unknown")
    def test_raw_bounds(self):
        for raw in (SUMMARY+"x"*262144,SUMMARY+"\x00"):
            self.assertEqual(p.apt_result("ok",raw,False)["status"],"unknown")
    def test_normal_nonroot_notice(self):
        self.assertEqual(p.apt_result("ok","NOTE: This is only a simulation!\n"+SUMMARY,False)["status"],"no_changes")
    def test_privacy_and_no_authority(self):
        result=p.apt_result("ok",SUMMARY+"Conf private-name",False)
        self.assertEqual(result["status"],"review");self.assertFalse(result["execution_authorized"]);self.assertNotIn("private",json.dumps(result))
    def test_vm_bounds(self):
        for raw in ("x"*16384+HEADER+ROW*3,HEADER+ROW*3+"\x00"):
            self.assertEqual(p.vm_result("ok",raw,False)["status"],"unknown")
    def test_vm_compatible_extra_column(self):
        result=p.vm_result("ok",HEADER.rstrip()+" gu\n"+(ROW.rstrip()+" 0\n")*3,False)
        self.assertEqual(result["status"],"sampled");self.assertEqual(result["si_mean"],5)
if __name__=="__main__":unittest.main(verbosity=2)
