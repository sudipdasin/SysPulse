#!/usr/bin/python3
"""Hybrid engine adversarial tests. No real disk writes or cleaner execution."""
import importlib.machinery,importlib.util,json,pathlib,sys,tempfile,unittest
P=pathlib.Path
TOOLS=P(__file__).resolve().parents[1]/"tools"
sys.path.insert(0,str(TOOLS))
import syspulse_hybrid as h
loader=importlib.machinery.SourceFileLoader("diag",str(TOOLS/"syspulse-diagnostics"))
spec=importlib.util.spec_from_loader(loader.name,loader);d=importlib.util.module_from_spec(spec);loader.exec_module(d)
class Tests(unittest.TestCase):
    def test_signature_privacy(self):
        x=h.signature("Buffer I/O error on /home/alice/private@example.org device sda sector 123456")
        for word in ("alice","private","example","sda","123456","/home"):
            self.assertNotIn(word,x)
        self.assertIn("error",x)
    def test_variable_template(self):
        self.assertEqual(h.signature("I/O error device sda sector 123"),h.signature("I/O error device sdb sector 456"))
    def test_no_unknown_literals(self):
        self.assertEqual(h.signature("uniquePRIVATEsecret 234567"),"unclassified error")
    def test_miner_bound(self):
        m=h.Miner()
        for i in range(10000):m.add("other_error","error "+(" ".join(["kernel","memory","disk","network","service","failure","warning"][int(x)] for x in f"{i%10000:04o}" if int(x)<7)))
        self.assertLessEqual(len(m.counts),128);self.assertLessEqual(len(m.result()),6)
    def test_recurrence(self):
        m=h.Miner();m.add("storage_io","I/O error device sda");m.add("storage_io","I/O error device sdb")
        a=h.patterns([{"patterns":m.result()}],[])
        self.assertEqual(a[0]["observations"],2);self.assertTrue(a[0]["new_to_recent_history"])
        b=h.patterns([{"patterns":m.result()}],[{"patterns":a}])
        self.assertEqual(b[0]["previous_analyses"],1);self.assertFalse(b[0]["new_to_recent_history"])
    def test_journal_privacy(self):
        m=h.Miner();raw=json.dumps({"MESSAGE":"Failed password for alice@example.org","PRIORITY":"3","__CURSOR":"secret"})
        counts,*_=d.event_counts(raw+"\n"+raw,True,m)
        self.assertEqual(counts,{"authentication":1});self.assertNotIn("alice",json.dumps(m.result()))
        self.assertEqual(m.result()[0]["count"],1)
    def test_health_pass(self):
        r=h.smart_result('{"smartctl":{"exit_status":0},"smart_status":{"passed":true}}',"ok",False)
        self.assertEqual(r["health"],"passed")
    def test_unknown_never_pass(self):
        for raw,status,bounded in [('{}',"ok",False),('{"smartctl":{"exit_status":3},"smart_status":{"passed":true}}',"failed",False),('{}',"permission",False),('{}',"ok",True)]:
            self.assertEqual(h.smart_result(raw,status,bounded)["health"],"unknown")
    def test_failed_health_nonzero(self):
        self.assertEqual(h.smart_result('{"smartctl":{"exit_status":8},"smart_status":{"passed":false}}',"failed",False)["health"],"critical")
    def test_pending_sectors(self):
        row={"smartctl":{"exit_status":0},"smart_status":{"passed":True},"ata_smart_attributes":{"table":[{"id":197,"raw":{"value":4}}]}}
        self.assertEqual(h.smart_result(json.dumps(row),"ok",False)["health"],"critical")
    def test_nvme_warning(self):
        row={"smartctl":{"exit_status":0},"smart_status":{"passed":True},"nvme_smart_health_information_log":{"critical_warning":2}}
        self.assertEqual(h.smart_result(json.dumps(row),"ok",False)["health"],"critical")
    def test_historical_warning(self):
        self.assertEqual(h.smart_result('{"smartctl":{"exit_status":64},"smart_status":{"passed":true}}',"failed",False)["health"],"warning")
    def test_no_serial(self):
        raw='{"smartctl":{"exit_status":0},"smart_status":{"passed":true},"serial_number":"private","model_name":"private"}'
        self.assertNotIn("private",json.dumps(h.smart_result(raw,"ok",False)))
    def test_device_allowlist_and_standby(self):
        calls=[]
        def cmd(args,**kw):
            calls.append(args)
            if args[0]=="lsblk":
                return "ok",json.dumps({"blockdevices":[{"name":"sda","type":"disk","tran":"sata","rm":False},{"name":"nvme0n1","type":"disk","tran":"nvme","rm":False},{"name":"../../etc/passwd","type":"disk","tran":"sata","rm":False},{"name":"sdb","type":"disk","tran":"usb","rm":False},{"name":"sdc","type":"disk","tran":"sata","rm":True}]}),False
            return "ok",'{"smartctl":{"exit_status":0},"smart_status":{"passed":true}}',False
        data=h.smart_scan(cmd,lambda _:True)
        self.assertEqual(len(data["disks"]),2);self.assertEqual(data["excluded"],3)
        self.assertEqual(calls[2],["smartctl","-j","-H","-A","-n","standby,3","/dev/sda"])
        self.assertNotIn("sda",json.dumps(data))
    def test_device_bound(self):
        calls=[]
        def cmd(args,**kw):
            calls.append(args)
            if args[0]=="lsblk":return "ok",json.dumps({"blockdevices":[{"name":"sd"+x,"type":"disk","tran":"sata","rm":False} for x in "abcdefgh"]}),False
            return "permission","",False
        data=h.smart_scan(cmd,lambda _:True)
        self.assertEqual(len(calls),5);self.assertEqual(data["excluded"],4)
        self.assertEqual(data["status"],"partial")
    def test_recipe_no_execution(self):
        a=h.recipes([{"code":"storage_io"},{"code":"package_failure"},{"code":"unknown arbitrary command"}])
        self.assertEqual(len(a),2);self.assertTrue(all(x["automatic_repair"] is False for x in a))
    def test_catalog_untrusted(self):
        with tempfile.TemporaryDirectory() as tmp:
            self.assertIn(h.cleaner_catalog(P(tmp))["status"],("untrusted","unavailable"))
    def test_catalog_installed_fixture(self):
        root=P("/usr/share/syspulse-fixtures/cleaners")
        if not root.exists():self.skipTest("root-owned catalog fixture only in evaluation")
        r=h.cleaner_catalog(root)
        self.assertEqual(r,{"status":"metadata_only","options":["fixture.cache"]})
    def test_db_patterns_and_storage_gate(self):
        c=d.connect(":memory:")
        m={"memory_total_kib":1000,"memory_available_kib":100,"root_total_bytes":1000,"root_free_bytes":100,"root_readonly":False,"home_total_bytes":1000,"home_free_bytes":500,"home_readonly":False,"apt_total_bytes":1000,"apt_free_bytes":100,"apt_readonly":False}
        miner=h.Miner();miner.add("disk_health","SMART pending sectors 12")
        s=d.source("smart-disk-health","ok",{"disk_health":1},1);s["patterns"]=miner.result()
        rows=[d.source("kernel-ring","ok",{},1),d.source("dpkg-audit","empty"),s]
        a=d.record_analysis(c,m,rows,"boot",10,1000)
        self.assertFalse(a["recommendations"]["archives"]["allowed"]);self.assertEqual(len(a["patterns"]),1)
        self.assertTrue(any(x["code"]=="disk_health" for x in a["recipes"]))
        b=d.record_analysis(c,m,rows,"boot",11,1001)
        self.assertEqual(b["patterns"][0]["previous_analyses"],1);c.close()
if __name__=="__main__":unittest.main(verbosity=2)
