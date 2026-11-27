#!/usr/bin/python3
"""Adversarial tests; isolated fixtures never use the owner secret or production remote."""
import copy, datetime as dt, json, os, pathlib, subprocess, tempfile, unittest, hashlib, shutil
from unittest.mock import patch
from fleet_policy import *
import fleet_signer as f
P=pathlib.Path
class PolicyTests(unittest.TestCase):
    def setUp(self):
        self.now=dt.datetime.now(dt.timezone.utc)
        self.p={"schema":2,"project":"fixture-app","channel":"alpha","repository":"https://github.com/example/fixture.git","feed_branch":"updates-alpha","catalogue_branch":"release-candidates","package":"fixture-app","architecture":"amd64","origin":"Fixture Test","label":"Fixture","fingerprint":"A"*40,"anchor_path":"/etc/deb-release/fixture.gpg","anchor_sha256":"b"*64,"embedded_anchor":"usr/share/fixture-app/update-keys/alpha.gpg","minimum_version":"1.0.0~alpha1","minimum_sha256":"c"*64,"artifact_hosts":["fixture.example.org"],"minimum_suites":20,"require_physical":False}
        self.c={"project":"fixture-app","channel":"alpha","package":"fixture-app","architecture":"amd64","version":"1.0.0~alpha2","source":"a"*40,"deployment":"b"*40,"artifact_url":"https://fixture.example.org/build/app.deb","sha256":"d"*64,"size":1234,"prior_version":"1.0.0~alpha1","prior_sha256":"c"*64,"suites":20,"build_report_sha256":"e"*64,"install_report_sha256":"f"*64}
        self.current={"Version":self.c["prior_version"],"SHA256":self.c["prior_sha256"]}
        self.cat={"schema":2,"candidates":[copy.deepcopy(self.c)]}
        self.gate={"schema":2,"candidate":copy.deepcopy(self.c),"verified_at":(self.now-dt.timedelta(minutes=1)).isoformat(),"expires_at":(self.now+dt.timedelta(hours=1)).isoformat(),"native_regression":True,"signed_upgrade":True,"tamper_rejection":True,"runtime":True,"package_verified":True,"physical_acceptance":False}
    def accept(self):return approval(policy(self.p),self.cat,self.gate,self.current,self.now)
    def test_accept(self):self.assertEqual(self.accept(),self.c)
    def test_version_order(self):
        for a,b in (("1.0.0~alpha10","1.0.0~alpha9"),("1.0.0","1.0.0~rc1"),("1.2-2","1.2-1")):self.assertTrue(newer(a,b))
        for a,b in (("1.0.0~alpha9","1.0.0~alpha10"),("1.0.0","1.0.0")):self.assertFalse(newer(a,b))
    def test_bad_version(self):
        for v in ("../1","1:2","1\n","-1","1/2","1;id","1 "*60):
            with self.subTest(v=v),self.assertRaises(ValueError):safe_version(v)
    def test_cross_project(self):
        for k,v in (("project","other"),("package","other"),("architecture","arm64"),("channel","stable")):
            with self.subTest(k=k),self.assertRaises(ValueError):candidate(self.p,dict(self.c,**{k:v}))
    def test_feed_branch(self):
        for v in ("main","master","release-candidates","../../main","--all"):
            with self.subTest(v=v),self.assertRaises(ValueError):policy(dict(self.p,feed_branch=v))
    def test_stable_physical_required(self):
        with self.assertRaises(ValueError):policy(dict(self.p,channel="stable"))
        self.assertEqual(policy(dict(self.p,channel="stable",require_physical=True))["channel"],"stable")
    def test_process_timeout(self):
        with tempfile.TemporaryDirectory() as tmp:
            with self.assertRaisesRegex(ValueError,"timed out"):f.run(P(tmp),"/usr/bin/python3","-c","import time;time.sleep(5)",timeout=0.05)
    def test_wrong_repository(self):
        for v in ("http://github.com/a/b.git","https://github.com@evil.example/a/b.git","https://github.com/a/b.git?x=y"):
            with self.assertRaises(ValueError):policy(dict(self.p,repository=v))
    def test_url(self):
        for v in ("http://fixture.example.org/a.deb","https://fixture.example.org@evil.example/a.deb","https://fixture.example.org:443/a.deb","https://fixture.example.org/../a.deb","https://fixture.example.org/a%2fb.deb","https://fixture.example.org/a.deb?x=1","https://fixture.example.org/a.deb#x","https://evil.example/a.deb"):
            with self.subTest(v=v),self.assertRaises(ValueError):candidate(self.p,dict(self.c,artifact_url=v))
    def test_size(self):
        for v in (0,True,-1,67108865):
            with self.assertRaises(ValueError):candidate(self.p,dict(self.c,size=v))
    def test_suites(self):
        for v in (19,True,100001):
            with self.assertRaises(ValueError):candidate(self.p,dict(self.c,suites=v))
    def test_gate_failures(self):
        for k in ("native_regression","signed_upgrade","tamper_rejection","runtime","package_verified"):
            for v in (False,1,"true",None):
                self.gate[k]=v
                with self.subTest(k=k,v=v),self.assertRaises(ValueError):self.accept()
            self.gate[k]=True
    def test_physical_policy(self):
        self.p["require_physical"]=True
        with self.assertRaises(ValueError):self.accept()
        self.gate["physical_acceptance"]=True;self.accept()
    def test_expiry(self):
        self.gate["expires_at"]=self.now.isoformat()
        with self.assertRaises(ValueError):self.accept()
    def test_future(self):
        self.gate["verified_at"]=(self.now+dt.timedelta(minutes=1)).isoformat()
        with self.assertRaises(ValueError):self.accept()
    def test_long_lifetime(self):
        self.gate["expires_at"]=(self.now+dt.timedelta(days=2)).isoformat()
        with self.assertRaises(ValueError):self.accept()
    def test_exact_prior(self):
        self.current["SHA256"]="e"*64
        with self.assertRaises(ValueError):self.accept()
    def test_wrong_prior_version(self):
        self.current["Version"]="1.0.0~alpha0"
        with self.assertRaises(ValueError):self.accept()
    def test_duplicate_catalogue(self):
        self.cat["candidates"].append(copy.deepcopy(self.c))
        with self.assertRaises(ValueError):self.accept()
    def test_tampered_evidence(self):
        for k in ("sha256","build_report_sha256","install_report_sha256"):
            original=self.cat["candidates"][0][k];self.cat["candidates"][0][k]="0"*64
            with self.subTest(k=k),self.assertRaises(ValueError):self.accept()
            self.cat["candidates"][0][k]=original
    def test_duplicate_json(self):
        with self.assertRaises(ValueError):strict('{"x":1,"x":2}')
    def test_nonfinite(self):
        with self.assertRaises(ValueError):strict('{"x":NaN}')
    def test_state_binding_and_replay(self):
        with tempfile.TemporaryDirectory() as tmp:
            s=P(tmp);f.highwater(s,self.p,self.current,self.now)
            with self.assertRaises(ValueError):f.highwater(s,self.p,self.current,self.now-dt.timedelta(seconds=1))
            with self.assertRaises(ValueError):f.highwater(s,dict(self.p,project="other"),self.current,self.now)
            with self.assertRaises(ValueError):f.highwater(s,self.p,dict(self.current,SHA256="a"*64),self.now)
            f.highwater(s,self.p,{"Version":self.c["version"],"SHA256":self.c["sha256"]},self.now)
            with self.assertRaises(ValueError):f.highwater(s,self.p,self.current,self.now)
    def test_lock(self):
        with tempfile.TemporaryDirectory() as tmp:
            with f.locked(P(tmp)):
                with self.assertRaises(OSError):
                    with f.locked(P(tmp)):pass
    def test_lease_validation(self):
        self.assertEqual(f.exact_lease("a"*40,"updates-alpha"),"--force-with-lease=refs/heads/updates-alpha:"+"a"*40)
        with self.assertRaises(ValueError):f.exact_lease("a"*39,"updates-alpha")
    def test_command_output_bound(self):
        with tempfile.TemporaryDirectory() as tmp:
            with self.assertRaises(ValueError):f.run(P(tmp),"/usr/bin/python3","-c","print('x'*5000)",limit=100)
    def test_actual_git_stale_lease(self):
        # A local bare Git fixture; no network or real repository writes.
        with tempfile.TemporaryDirectory() as tmp:
            root=P(tmp);bare=root/"remote";a=root/"a";b=root/"b"
            def g(*args,cwd=None):
                return subprocess.run(["git","-c","user.name=Fixture","-c","user.email=fixture@invalid.example",*map(str,args)],cwd=cwd,stdout=subprocess.PIPE,stderr=subprocess.PIPE,check=True).stdout.decode().strip()
            g("init","--bare",bare);g("clone",bare,a);g("checkout","-b","updates-alpha",cwd=a)
            (a/"x").write_text("one");g("add","x",cwd=a);g("commit","-m","one",cwd=a);g("push","origin","updates-alpha",cwd=a)
            parent=g("rev-parse","HEAD",cwd=a)
            g("clone","-b","updates-alpha",bare,b)
            (a/"x").write_text("two");g("commit","-am","two",cwd=a)
            (b/"x").write_text("three");g("commit","-am","three",cwd=b);g("push","origin","updates-alpha",cwd=b)
            self.assertEqual(f.direct_child(root,a,parent),g("rev-parse","HEAD",cwd=a))
            with self.assertRaises(subprocess.CalledProcessError):g("push",f.exact_lease(parent,"updates-alpha"),"origin","HEAD:refs/heads/updates-alpha",cwd=a)
            self.assertEqual(g("rev-parse","refs/heads/updates-alpha",cwd=bare),g("rev-parse","HEAD",cwd=b))
    def test_actual_deb_identity_anchor(self):
        with tempfile.TemporaryDirectory() as tmp:
            home=P(tmp);tree=home/"tree";(tree/"DEBIAN").mkdir(parents=True);path=tree/self.p["embedded_anchor"];path.parent.mkdir(parents=True);key=b"fixture public anchor bytes";path.write_bytes(key)
            (tree/"DEBIAN/control").write_text("Package: fixture-app\nVersion: 1.0.0~alpha2\nArchitecture: amd64\nMaintainer: Fixture <fixture@invalid.example>\nDescription: Isolated fixture\n")
            deb=home/"test.deb";f.run(home,"dpkg-deb","--build",tree,deb)
            p=dict(self.p,anchor_sha256=hashlib.sha256(key).hexdigest())
            f.identity(home,p,deb,self.c["version"]);f.check_embedded(home,p,deb)
            with self.assertRaises(ValueError):f.identity(home,p,deb,"1.0.0~alpha3")
            with self.assertRaises(ValueError):f.check_embedded(home,dict(p,anchor_sha256="0"*64),deb)
    def test_report_digest_mismatch(self):
        with tempfile.TemporaryDirectory() as tmp,patch.object(f,"download",return_value=b"altered report"):
            with self.assertRaises(ValueError):f.fetch(P(tmp),self.p,self.c)
    def test_signed_owner_fixture(self):
        # Uses actual public owner signature, not a generated or private key.
        with tempfile.TemporaryDirectory() as tmp:
            home=P(tmp);p=policy(strict(trusted_file("/etc/deb-release/syspulse.json")))
            key=f.anchor(home,p);root=home/"feed";f.clone(home,p,p["feed_branch"],root)
            entry,start,end=f.verify(home,p,root,key,dt.datetime.now(dt.timezone.utc),True)
            self.assertEqual(entry["Package"],"syspulse")
            for file in ("Packages","Release","InRelease","Release.gpg",entry["Filename"]):
                path=root/file;old=path.read_bytes()
                try:
                    # Mutate an existing byte rather than adding ignorable signature whitespace.
                    altered=bytearray(old);altered[len(altered)//2]^=1;path.write_bytes(altered)
                    with self.subTest(file=file),self.assertRaises((ValueError,OSError,KeyError)):f.verify(home,p,root,key,dt.datetime.now(dt.timezone.utc),True)
                finally:path.write_bytes(old)
if __name__=="__main__":unittest.main(verbosity=2)
