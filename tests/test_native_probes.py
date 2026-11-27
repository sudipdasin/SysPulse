#!/usr/bin/python3
import json,pathlib,sys,unittest
sys.path.insert(0,str(pathlib.Path(__file__).resolve().parents[1]/"tools"))
import syspulse_native_probes as p
SUMMARY="0 upgraded, 0 newly installed, 0 to remove and 0 not upgraded.\n"
HEADER="r b swpd free buff cache si so bi bo in cs us sy id wa st\n"
BOOT="100 100 0 0 0 0 900 900 0 0 0 0 100 0 0 0 0\n"
ROW="2 1 0 0 0 0 5 7 0 0 0 0 10 5 80 5 0\n"
class Tests(unittest.TestCase):
    def test_clean(self):self.assertEqual(p.apt_result("ok",SUMMARY,False)["status"],"no_changes")
    def test_proposal(self):
        r=p.apt_result("ok","1 upgraded, 1 newly installed, 0 to remove and 3 not upgraded.\nInst private-a\nInst private-b\nConf private-a\n",False)
        self.assertEqual(r["status"],"review");self.assertEqual(r["install_or_upgrade"],2);self.assertEqual(r["held_back"],3);self.assertFalse(r["execution_authorized"]);self.assertNotIn("private",json.dumps(r))
    def test_removal(self):
        for raw in ("Remv private\n", "0 upgraded, 0 newly installed, 1 to remove and 0 not upgraded.\n"):
            self.assertEqual(p.apt_result("ok",raw,False)["status"],"removal_blocked")
    def test_unknown(self):
        for st,raw,b in (("timeout",SUMMARY,False),("ok",SUMMARY,True),("ok","",False),("ok",SUMMARY+SUMMARY,False),("ok",SUMMARY+"Inst abc\n",False),("ok",SUMMARY+"W: warning\n",False)):
            self.assertEqual(p.apt_result(st,raw,b)["status"],"unknown")
    def test_failure(self):self.assertEqual(p.apt_result("failed","E: dependency problem",False)["status"],"solver_failed")
    def test_configuration(self):self.assertEqual(p.apt_result("ok",SUMMARY+"Conf package\n",False)["status"],"review")
    def test_vm_boot_excluded(self):
        r=p.vm_result("ok",HEADER+BOOT+ROW+ROW,False);self.assertEqual(r["samples"],2);self.assertEqual(r["si_mean"],5);self.assertEqual(r["id_mean"],80)
    def test_vm_unknown(self):
        for raw in (HEADER+ROW,HEADER+BOOT+ROW+ROW+ROW,HEADER+BOOT+ROW+ROW.replace("80","180"),HEADER+BOOT+ROW+ROW.replace("80","nan")):
            self.assertEqual(p.vm_result("ok",raw,False)["status"],"unknown")
    def test_vm_bounded(self):self.assertEqual(p.vm_result("ok",HEADER+BOOT+ROW+ROW,True)["status"],"unknown")
    def test_fixed_commands(self):
        calls=[]
        def fake(args,**kw):calls.append((args,kw));return "ok",SUMMARY if args[0]=="apt-get" else HEADER+BOOT+ROW+ROW,False
        p.collect_apt(fake);p.collect_vm(fake)
        self.assertEqual(calls[0][0],["apt-get","-s","--no-download","--no-remove","-f","install"]);self.assertEqual(calls[1][0],["vmstat","-w","1","3"])
if __name__=="__main__":unittest.main(verbosity=2)
