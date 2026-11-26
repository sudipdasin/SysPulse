import importlib.util, json, pathlib, unittest
root=pathlib.Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location("relay",root/"tools/reporting/relay.py")
r=importlib.util.module_from_spec(spec);spec.loader.exec_module(r)
def encode(x): return json.dumps(x).encode()
class RelayTest(unittest.TestCase):
 def setUp(self):
    self.base={"schema":1,"release":"3.0.0-alpha.20","level":"min","counts":{"PASS":1}}
 def test_min(self):
    self.assertEqual(r.validate(encode(self.base)),self.base)
 def test_unknown_fields(self):
    for k,v in [("email","secret"),("hostname","secret"),("checks",{}),("ip","private")]:
      with self.assertRaises(ValueError): r.validate(encode(dict(self.base,**{k:v})))
 def test_levels(self):
    med=dict(self.base,level="med",checks={"run/storage":"PASS"})
    self.assertEqual(r.validate(encode(med)),med)
    with self.assertRaises(ValueError): r.validate(encode(dict(med,checks={"native/Cleaner/cache-access":"PASS"})))
    high=dict(med,level="max",checks={"native/Cleaner/cache-access":"PASS"})
    self.assertEqual(r.validate(encode(high)),high)
    for bad in ({"run/storage":"secret@example.test"},{"private/path":"PASS"},{"run/storage":"FAILED"},{}):
      with self.assertRaises(ValueError): r.validate(encode(dict(high,checks=bad)))
 def test_audit(self):
    x=dict(self.base,level="max",checks={"audit/Cleaner/quick-cache":"PASS"},audit_target="3.0.0-alpha.16",audit_reference="3.0.0-alpha.19")
    self.assertEqual(r.validate(encode(x)),x)
    with self.assertRaises(ValueError): r.validate(encode(dict(x,audit_target="private")))
 def test_bounds(self):
    for counts in ({"PASS":True},{"PASS":-1},{"PASS":99999},{"private":1},{}):
      with self.assertRaises(ValueError): r.validate(encode(dict(self.base,counts=counts)))
    with self.assertRaises(ValueError): r.validate(b'{"schema":1,"schema":1}')
    with self.assertRaises(ValueError): r.validate(b'x'*(r.MAX_BODY+1))
 def test_triage(self):
    x=dict(self.base,level="max",counts={"DISCREPANCY":1},checks={"native/Cleaner/cache-redirection":"DISCREPANCY"})
    self.assertIn("preserve deletion guards",r.issue_for(r.validate(encode(x)))["body"])
 def test_rate(self):
    r.recent.clear();r.attempts.clear()
    self.assertIsNotNone(r.admit(self.base,100))
    self.assertIsNone(r.admit(self.base,101))
    self.assertIsNotNone(r.admit(self.base,3701))
if __name__=="__main__": unittest.main()
