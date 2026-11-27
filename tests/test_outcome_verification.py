#!/usr/bin/python3
"""Pure evidence-policy tests; no hardware writes or administrator actions."""
import pathlib,sys,unittest
sys.path.insert(0,str(pathlib.Path(__file__).resolve().parents[1]/"tools"))
import syspulse_outcomes as o
def source(name,status="ok",events=None,**kw):
    return {"name":name,"status":status,"events":events or {},"bounded":False,**kw}
class Tests(unittest.TestCase):
    def setUp(self):
        self.before={"memory_available_kib":1000,"load1":1.0}
        for a in ("root","home","apt"):
            self.before.update({a+"_device":10,a+"_total_bytes":1000,a+"_free_bytes":100,a+"_readonly":False})
        self.after=dict(self.before,memory_available_kib=1100,load1=0.9)
        self.sources=[source("kernel-ring"),source("dpkg-audit","empty"),source("smart-disk-health",smart={"status":"ok"})]
        self.receipt={"thumbnails":{"status":"success","bytes":100}}
    def verify(self):return o.verification(self.before,self.after,self.sources,self.receipt)
    def test_observed(self):
        r=self.verify();self.assertEqual(r["status"],"observed");self.assertEqual(r["deltas"]["memory_available_kib"]["change"],100)
        self.assertIn("not proof of causal speed gain",r["interpretation"])
    def test_device_replaced(self):
        self.after["home_device"]=11;self.after["home_free_bytes"]=999
        r=self.verify();self.assertEqual(r["status"],"limited");self.assertIn("home",r["uncomparable_storage"]);self.assertNotIn("home_free_bytes",r["deltas"])
    def test_capacity_changed(self):
        self.after["apt_total_bytes"]=2000
        self.assertNotIn("apt_free_bytes",self.verify()["deltas"])
    def test_kernel_unknown(self):
        self.sources[0]["status"]="permission"
        self.assertEqual(self.verify()["status"],"limited")
    def test_package_unknown(self):
        self.sources[1]["status"]="unavailable";self.assertEqual(self.verify()["status"],"limited")
    def test_smart_unknown(self):
        self.sources[2]["smart"]["status"]="partial";self.assertEqual(self.verify()["status"],"limited")
    def test_current_fault(self):
        self.sources[0]["events"]={"storage_io":2}
        self.assertEqual(self.verify()["status"],"review");self.assertIn("storage_io",self.verify()["risks"])
    def test_action_failure(self):
        self.receipt["thumbnails"]={"status":"failure","bytes":0};self.assertIn("action_failure",self.verify()["risks"])
    def test_readonly_and_temperature(self):
        self.after["home_readonly"]=True;self.after["max_temperature_c"]=95
        r=self.verify();self.assertEqual(r["status"],"review");self.assertIn("readonly_filesystem",r["risks"]);self.assertIn("high_temperature",r["risks"])
    def test_nonfinite(self):
        self.after["load1"]=float("nan");self.assertNotIn("load1",self.verify()["deltas"])
    def test_previously_available_smart(self):
        self.receipt={"own_memory":{"status":"no_effect","bytes":0}};prior=self.sources[:]
        r=o.verification(self.before,self.after,self.sources[:2],self.receipt,prior);self.assertEqual(r["status"],"limited")
    def test_no_storage_action_unknown_smart(self):
        self.receipt={"own_memory":{"status":"no_effect","bytes":0}};self.sources=self.sources[:2]
        self.assertEqual(self.verify()["status"],"observed")
if __name__=="__main__":unittest.main(verbosity=2)
