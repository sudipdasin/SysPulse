import importlib.machinery, importlib.util, pathlib, unittest, tempfile, hashlib
from unittest.mock import patch
p=pathlib.Path(__file__).parents[1]/"tools/syspulse-apt-update"
loader=importlib.machinery.SourceFileLoader("update",str(p))
spec=importlib.util.spec_from_loader("update",loader)
u=importlib.util.module_from_spec(spec);loader.exec_module(u)
class Update(unittest.TestCase):
    def test_versions(self):
        self.assertEqual(u.version("3.0.0~alpha22~render1"),"alpha")
        self.assertEqual(u.version("3.0.0~beta1"),"beta")
        self.assertEqual(u.version("3.0.0"),"stable")
        for v in ("3.0.0\n","1:3.0.0","3.0.0;sh","3.0.0~alpha02","--allow-downgrades"):
            with self.assertRaises(ValueError):u.version(v)
        self.assertTrue(u.newer("3.0.0~alpha22~render1","3.0.0~alpha21~render1"))
        self.assertFalse(u.newer("3.0.0~alpha21~render1","3.0.0~alpha22~render1"))
    def test_unconfigured(self):
        with patch.object(u,"installed",return_value="3.0.0~alpha21~render1"),patch.object(u,"secure_key",return_value=False),patch.object(u,"snapshot") as snap:
            self.assertEqual(u.check("alpha",pathlib.Path("/missing"),"https://example.invalid"),"UNCONFIGURED 3.0.0~alpha21~render1")
            snap.assert_not_called()
    def test_reject_channel_crossing(self):
        with patch.object(u,"run",return_value="  Candidate: 3.0.0~alpha22~render1\n"):
            with self.assertRaises(RuntimeError):u.candidate([],"stable","3.0.0~alpha21~render1")
            self.assertEqual(u.candidate([],"alpha","3.0.0~alpha21~render1"),"3.0.0~alpha22~render1")
    def test_no_untrusted_key(self):
        with tempfile.TemporaryDirectory() as d:
            key=pathlib.Path(d)/"test.gpg";key.write_bytes(b"fake")
            self.assertFalse(u.secure_key(key)) # writable /tmp ancestor
    def test_package_managed_trust(self):
        with tempfile.TemporaryDirectory() as d:
            bundled=pathlib.Path(d)/"bundled";bundled.mkdir()
            legacy=pathlib.Path(d)/"legacy"
            self.assertEqual(u.resolve_key("alpha",bundled,legacy),legacy/"alpha.gpg")
            key=bundled/"alpha.gpg";pin=bundled/"alpha.sha256"
            key.write_bytes(b"synthetic public fixture")
            with patch.object(u,"secure_key",side_effect=lambda p:p.is_file() and not p.is_symlink()):
                with self.assertRaises(ValueError):u.resolve_key("alpha",bundled,legacy)
                pin.write_text(hashlib.sha256(key.read_bytes()).hexdigest()+"\n")
                self.assertEqual(u.resolve_key("alpha",bundled,legacy),key)
                self.assertEqual(u.resolve_key("beta",bundled,legacy),legacy/"beta.gpg")
                key.write_bytes(b"tampered")
                with self.assertRaises(ValueError):u.resolve_key("alpha",bundled,legacy)
                key.unlink();key.symlink_to(pin)
                with self.assertRaises(ValueError):u.resolve_key("alpha",bundled,legacy)
    def test_public_key_build_gate(self):
        import subprocess, os
        validator=p.parent/"validate-update-key.py"
        with tempfile.TemporaryDirectory() as temp:
            home=pathlib.Path(temp)/"gpg";home.mkdir(mode=0o700)
            args=["/usr/bin/gpg","--no-options","--batch","--homedir",str(home),"--pinentry-mode","loopback","--passphrase",""]
            try:
                subprocess.run(args+["--quick-generate-key","SysPulse disposable test key","ed25519","sign","1d"],check=True,capture_output=True,timeout=30)
                public=pathlib.Path(temp)/"public.gpg";secret=pathlib.Path(temp)/"secret.gpg"
                public.write_bytes(subprocess.check_output(args+["--export"],timeout=15))
                secret.write_bytes(subprocess.check_output(args+["--export-secret-keys"],timeout=15))
                for path,accepted in ((public,True),(secret,False)):
                    result=subprocess.run(["/usr/bin/python3",str(validator),str(path),hashlib.sha256(path.read_bytes()).hexdigest()],capture_output=True,timeout=30)
                    self.assertEqual(result.returncode==0,accepted)
                result=subprocess.run(["/usr/bin/python3",str(validator),str(public),"0"*64],capture_output=True,timeout=30)
                self.assertNotEqual(result.returncode,0)
            finally:
                subprocess.run(["/usr/bin/gpgconf","--homedir",str(home),"--kill","gpg-agent"],capture_output=True,timeout=10)
    def test_nonroot_install_guard(self):
        with patch.object(u.os,"geteuid",return_value=1000):
            with self.assertRaisesRegex(ValueError,"authorization"):u.install("alpha","3.0.0~alpha22~render1",None,None)
if __name__=="__main__":unittest.main()
