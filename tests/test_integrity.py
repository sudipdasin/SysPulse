#!/usr/bin/python3
import pathlib, sys, unittest
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1]/"tools"))
import syspulse_integrity as p

class Tests(unittest.TestCase):
    def test_integrity(self):
        raw="missing     /usr/share/doc/dpkg/AUTHORS\n??5?????? c /etc/example\n??5??????   /usr/bin/dpkg\nmissing     /var/lib/example"
        r=p.integrity_result("ok",raw,False)
        self.assertEqual(r,{"status":"differences","core_file_changes":1,"configuration_changes":1,"documentation_changes":1,"other_changes":1})
        self.assertNotIn("/etc/",str(r))
    def test_integrity_unknown(self):
        for status,raw,bounded in (("ok","invalid output",False),("failed","",False),("ok","",True)):
            self.assertEqual(p.integrity_result(status,raw,bounded)["status"],"unknown")
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
