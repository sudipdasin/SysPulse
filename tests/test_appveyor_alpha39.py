#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Actual local Git and disposable DEB fixtures; no cloud worker or Qt build."""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
RUNNER = ROOT / 'tools/ci/appveyor-alpha39.sh'
FIXTURE = r'''import base64, hashlib, os, pathlib, runpy, subprocess, sys
pin, source = sys.argv[1:]
source = pathlib.Path(source)
mode = os.environ.get('FIXTURE_MODE', '')
if mode == 'failed': raise SystemExit(43)
res = pathlib.Path.home()/'SysPulse-alpha39-local.fixture/results'; res.mkdir(parents=True)
root = res.parent/'pkg'; root.mkdir(); root.chmod(0o755)
control = root/'DEBIAN'; control.mkdir(); control.chmod(0o755)
(control/'control').write_text('Package: syspulse\nVersion: 3.0.0~alpha39~local1\nArchitecture: amd64\nMaintainer: Fixture <fixture@example.invalid>\nDescription: Data-only test fixture\n'); (control/'control').chmod(0o644)
g = runpy.run_path(str(source/'tools/verify-alpha39-payload.py'))
key = base64.b64decode((source/'assets/update-trust/alpha-public.gpg.b64').read_bytes())
for path, perm in g['REQUIRED'].items():
    t = root/path; t.parent.mkdir(parents=True, exist_ok=True)
    t.write_bytes(key if path == g['KEY'] else (g['ANCHOR']+'\n').encode() if path == g['KEY_HASH'] else b'fixture only\n'); t.chmod(perm)
for path in g['PARENTS']: (root/path).chmod(0o755)
name = 'syspulse-3.0.0-alpha.39-ubuntu-24.04-amd64-local-eval.deb'; deb = res/name
subprocess.run(['dpkg-deb','--root-owner-group','--build',str(root),str(deb)],check=True,stdout=subprocess.DEVNULL)
report = subprocess.check_output(['python3',str(source/'tools/verify-alpha39-payload.py'),str(deb),'3.0.0~alpha39~local1'])
if mode == 'bad-deb': deb.write_bytes(b'invalid but rehashed')
(res/'SHA256SUMS').write_text(hashlib.sha256(deb.read_bytes()).hexdigest()+'  '+name+'\n')
(res/'SOURCE-CHECKOUT.txt').write_text(pin+'\n'); (res/'BUILD-STATUS.txt').write_text('Source: '+pin+'\n')
(res/'PAYLOAD-STATUS.txt').write_bytes(report if mode != 'wrong-report' else b'wrong report\n')
for item in ['CTEST.txt','OFFLINE-DIAGNOSTICS.txt','PACKAGE-VERIFY.txt','DETECTION-TUNED.json','DETECTION-REPLAY.json']:
    (res/item).write_text('fixture only\n')
if mode == 'missing-report': (res/'CTEST.txt').unlink()
'''

class Tests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory(prefix='sp-av-test-')
        self.addCleanup(self.tmp.cleanup)
        self.repo = Path(self.tmp.name)
        (self.repo/'tools/ci').mkdir(parents=True)
        (self.repo/'assets/update-trust').mkdir(parents=True)
        shutil.copy(ROOT/'tools/verify-alpha39-payload.py', self.repo/'tools/verify-alpha39-payload.py')
        shutil.copy(ROOT/'assets/update-trust/alpha-public.gpg.b64', self.repo/'assets/update-trust/alpha-public.gpg.b64')
        (self.repo/'tools/fixture.py').write_text(FIXTURE)
        (self.repo/'tools/build-alpha39-local.sh').write_text('python3 "$3/tools/fixture.py" "$2" "$3"\n')
        self.env = dict(os.environ, HOME=str(self.repo), XDG_CONFIG_HOME=str(self.repo/'config'), GIT_CONFIG_NOSYSTEM='1', GIT_CONFIG_GLOBAL='/dev/null')
        self.git('init','-q'); self.git('config','user.name','Fixture'); self.git('config','user.email','fixture@example.invalid'); self.git('add','.'); self.git('commit','-qm','fixture')
        self.env.update(APPVEYOR='true', APPVEYOR_REPO_NAME='sudipdasin/SysPulse', APPVEYOR_REPO_BRANCH='build/appveyor-alpha39', APPVEYOR_REPO_COMMIT=self.git('rev-parse','HEAD'))
        self.out = self.repo/'syspulse-ci-artifacts'
    def git(self,*args):
        return subprocess.check_output(['git','-C',str(self.repo),*args],env=self.env,text=True).strip()
    def invoke(self, **env):
        return subprocess.run(['bash',str(RUNNER),'build'],cwd=self.repo,env=dict(self.env,**env),capture_output=True,text=True,timeout=30)
    def reject(self, **env):
        r=self.invoke(**env); self.assertNotEqual(r.returncode,0,r.stdout+r.stderr); self.assertFalse(self.out.exists())
    def test_success(self):
        r=self.invoke(); self.assertEqual(r.returncode,0,r.stdout+r.stderr); self.assertEqual(len(list(self.out.iterdir())),2); self.assertTrue((self.out/'evidence.tar.gz').is_file())
    def test_builder_failure(self): self.reject(FIXTURE_MODE='failed')
    def test_bad_rehashed_deb(self): self.reject(FIXTURE_MODE='bad-deb')
    def test_wrong_report(self): self.reject(FIXTURE_MODE='wrong-report')
    def test_missing_report(self): self.reject(FIXTURE_MODE='missing-report')
    def test_wrong_pin(self): self.reject(APPVEYOR_REPO_COMMIT='0'*40)
    def test_wrong_repository(self): self.reject(APPVEYOR_REPO_NAME='other/repo')
    def test_wrong_branch(self): self.reject(APPVEYOR_REPO_BRANCH='main')
    def test_pr(self): self.reject(APPVEYOR_PULL_REQUEST_NUMBER='7')
    def test_tag(self): self.reject(APPVEYOR_REPO_TAG='true')
    def test_not_worker(self): self.reject(APPVEYOR='false')
    def test_existing_destination(self):
        self.out.mkdir(); (self.out/'keep').write_text('keep'); r=self.invoke(); self.assertNotEqual(r.returncode,0); self.assertEqual((self.out/'keep').read_text(),'keep')

if __name__=='__main__':
    if os.geteuid()==0: raise SystemExit('Run as ordinary user')
    unittest.main(verbosity=2)
