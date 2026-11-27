#!/usr/bin/python3
"""Offline adversarial tests of external investigation adapters; no API keys."""
import json,pathlib,sys,threading,unittest,urllib.error
sys.path.insert(0,str(pathlib.Path(__file__).resolve().parents[1]/"tools"))
import syspulse_research as r
class Tests(unittest.TestCase):
    def setUp(self):
        self.p=r.profile({"findings":[{"code":"package_failure"},{"code":"low_disk_space"}],"context":"disk"})
        self.good={"investigate":["package-audit"],"caution":["archives"],"prioritize":["thumbnails"]}
    def test_privacy(self):
        p=r.profile({"findings":[{"code":"oom","message":"user@example.org /home/private IP 192.0.2.1"},{"code":"secret /etc/passwd"},{"code":{}}],"context":"secret","metrics":{"serial":"sensitive"},"boot":"private"})
        self.assertEqual(p["codes"],["oom"]);self.assertEqual(p["context"],"normal")
        for v in ("example.org","/home","192.0.2.1","serial","sensitive","private","/etc/passwd"):self.assertNotIn(v,r.prompt(p))
    def test_strict_schema(self):self.assertEqual(r.advice(json.dumps(self.good),self.p),self.good)
    def test_reject_command(self):
        for v in ({"investigate":["sudo apt --fix-broken install"],"caution":[],"prioritize":[]},{"investigate":["kernel"],"caution":[],"prioritize":[]},dict(self.good,command="rm -rf /"),dict(self.good,prioritize=["archives"]),dict(self.good,caution=["archives","archives"])):
            with self.assertRaises(ValueError):r.advice(json.dumps(v),self.p)
    def test_duplicate_and_nonfinite(self):
        for raw in ('{"investigate":[],"investigate":[],"caution":[],"prioritize":[]}', '{"investigate":[],"caution":[],"prioritize":[NaN]}'):
            with self.assertRaises(ValueError):r.advice(raw,self.p)
    def test_no_key_no_request(self):
        def fail(*args):self.fail("request without credential")
        for key in (None,"", "unsafe\nheader", "short"):
            self.assertEqual(r.ai("groq",self.p,key,fail)["status"],"not_configured")
    def test_groq_request(self):
        def fake(url,payload,headers):
            self.assertEqual(url,"https://api.groq.com/openai/v1/chat/completions")
            self.assertEqual(headers["Authorization"],"Bearer "+ "x"*20)
            self.assertNotIn("tools",payload)
            return {"choices":[{"message":{"content":json.dumps(self.good)}}]}
        self.assertEqual(r.ai("groq",self.p,"x"*20,fake)["advice"],self.good)
    def test_gemini_request(self):
        def fake(url,payload,headers):
            self.assertEqual(url,"https://generativelanguage.googleapis.com/v1beta/models/gemini-3.1-flash-lite:generateContent")
            self.assertEqual(headers["x-goog-api-key"],"x"*20)
            self.assertNotIn("tools",payload)
            return {"candidates":[{"content":{"parts":[{"text":json.dumps(self.good)}]}}]}
        self.assertEqual(r.ai("gemini",self.p,"x"*20,fake)["advice"],self.good)
    def test_web_typed_evidence(self):
        def fake(url):
            if "/search/" in url:return {"items":[{"question_id":42,"accepted_answer_id":51,"title":"APT &amp; dpkg","link":"https://evil.invalid"},{"question_id":43,"accepted_answer_id":51}],"quota_remaining":10}
            self.assertIn("/answers/51?",url)
            return {"items":[{"answer_id":51,"body":"<p>Use dpkg and backup</p><script>arbitrary command</script>","owner":{"display_name":"Public author"},"content_license":"CC BY-SA 4.0"}]}
        a=r.web(self.p,fake);ref=a["references"][0]
        self.assertEqual(len(a["references"]),1);self.assertEqual(ref["url"],"https://askubuntu.com/questions/42")
        self.assertEqual(ref["concepts"],["backup","package_review"]);self.assertNotIn("arbitrary command",json.dumps(a))
    def test_backoff(self):
        calls=[]
        def fake(url):
            calls.append(url);return {"items":[{"question_id":1,"accepted_answer_id":2}],"backoff":120,"quota_remaining":10}
        self.assertEqual(r.web(self.p,fake)["backoff"],120);self.assertEqual(len(calls),1)
    def test_offline_quota(self):
        def fake(*args):raise urllib.error.HTTPError("https://api.groq.com",429,"quota",{},None)
        self.assertEqual(r.run(self.p,["groq"],lambda _: "x"*20,fake)[0]["status"],"quota")
    def test_parallel(self):
        barrier=threading.Barrier(3,timeout=3)
        def fake(url,*args):
            barrier.wait()
            if "stackexchange" in url:return {"items":[],"quota_remaining":0}
            if "groq" in url:return {"choices":[{"message":{"content":json.dumps(self.good)}}]}
            return {"candidates":[{"content":{"parts":[{"text":json.dumps(self.good)}]}}]}
        result=r.run(self.p,["web","groq","gemini"],lambda _: "x"*20,fake)
        self.assertEqual([x["status"] for x in result],["no_results","ok","ok"])
    def test_disagreement(self):
        a={"provider":"groq","status":"ok","advice":self.good}
        b={"provider":"gemini","status":"ok","advice":{"investigate":[],"caution":[],"prioritize":["archives"]}}
        self.assertEqual(r.combine(self.p,[a,b])["consensus_caution"],[])
    def test_corroboration(self):
        a={"provider":"groq","status":"ok","advice":self.good};b=dict(a,provider="gemini")
        w={"provider":"web","status":"ok","references":[{"concepts":["package_review"]}]}
        g=r.combine(self.p,[a,b,w]);self.assertEqual(g["corroborated_routes"],["package-audit"])
        self.assertEqual(g["investigate"][0],"package-audit")
    def test_never_grant(self):
        a={"recommendations":{"archives":{"allowed":False,"recommended":False,"reason":"blocked"},"thumbnails":{"allowed":True,"recommended":False,"reason":"history"}}}
        rec={"guidance":{"consensus_caution":["archives"],"consensus_priority":["archives","thumbnails"]}}
        result=r.apply_research(a,rec)
        self.assertFalse(result["recommendations"]["archives"]["allowed"]);self.assertFalse(result["recommendations"]["thumbnails"]["recommended"])
    def test_reject_url_before_network(self):
        for url in ("http://api.groq.com","https://api.groq.com@evil.invalid/a","https://127.0.0.1","https://api.groq.com:443/a"):
            with self.assertRaises(ValueError):r.request(url)
    def test_malformed_provider(self):
        self.assertEqual(r.run(self.p,["groq"],lambda _:"x"*20,lambda *a:{"choices":[]})[0]["status"],"invalid_response")
if __name__=="__main__":unittest.main(verbosity=2)
