#!/usr/bin/python3
import pathlib, sys, unittest
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1]/"tools"))
import syspulse_package_triage as p

class Tests(unittest.TestCase):
    def test_no_changes(self):
        self.assertEqual(p.solver_result("ok", "0 upgraded, 0 newly installed, 0 to remove and 7 not upgraded.", False)["status"], "no_changes_proposed")
    def test_install(self):
        r=p.solver_result("ok", "1 upgraded, 1 newly installed, 0 to remove and 0 not upgraded.\nInst a\nInst b\nConf a\nConf b", False)
        self.assertEqual((r["status"],r["configurations"]), ("changes_proposed",2))
        self.assertNotIn("Inst a", str(r))
    def test_configuration_only(self):
        self.assertEqual(p.solver_result("ok", "0 upgraded, 0 newly installed, 0 to remove and 0 not upgraded.\nConf a",False)["status"], "changes_proposed")
    def test_removal(self):
        for raw in ("Remv a", "E: Packages need to be removed but remove is disabled.", "0 upgraded, 0 newly installed, 1 to remove and 0 not upgraded."):
            self.assertEqual(p.solver_result("ok",raw,False)["status"],"blocked_removal")
    def test_unresolved(self):
        self.assertEqual(p.solver_result("failed","E: Unable to correct problems, you have held broken packages.",False)["status"],"unresolved")
    def test_unknown(self):
        clean="0 upgraded, 0 newly installed, 0 to remove and 0 not upgraded."
        for status,raw,bounded in (("ok",clean,True),("timeout",clean,False),("ok","",False),("ok",clean+"\nInst a",False),("ok",clean+"\n"+clean,False),("ok",clean+"\nE: access denied",False)):
            self.assertEqual(p.solver_result(status,raw,bounded)["status"],"unknown")
    def test_integrity(self):
        raw="missing     /usr/share/doc/dpkg/AUTHORS\n??5?????? c /etc/example\n??5??????   /usr/bin/dpkg\nmissing     /var/lib/example"
        r=p.integrity_result("ok",raw,False)
        self.assertEqual(r,{"status":"differences","core_file_changes":1,"configuration_changes":1,"documentation_changes":1,"other_changes":1})
        self.assertNotIn("/etc/",str(r))
    def test_integrity_unknown(self):
        for status,raw,bounded in (("ok","invalid output",False),("failed","",False),("ok","",True)):
            self.assertEqual(p.integrity_result(status,raw,bounded)["status"],"unknown")
    def test_fixed_solver_command(self):
        calls=[]
        def run(args,**kw):
            calls.append(args);return "ok","0 upgraded, 0 newly installed, 0 to remove and 0 not upgraded.",False
        p.solver(run)
        self.assertEqual(calls,[list(p.SOLVER)])
        self.assertTrue(all(x in calls[0] for x in ("-s","--no-download","--no-remove")))
    def test_fixed_integrity_scope(self):
        calls=[]
        def run(args,**kw):
            calls.append(args);return "ok","installed" if args[0]=="dpkg-query" else "",False
        r=p.integrity(run)
        self.assertEqual([x["package"] for x in r],list(p.CORE))
        self.assertEqual([x for x in calls if x[0]=="dpkg"],[["dpkg","--verify",x] for x in p.CORE])
    def test_missing_package(self):
        r=p.integrity(lambda *a,**kw:("failed","",False))
        self.assertTrue(all(x["status"]=="unknown" for x in r))
if __name__=="__main__":unittest.main(verbosity=2)
