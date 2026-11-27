#!/usr/bin/python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Disposable signed APT repository, isolated status; simulation only."""
import datetime
import email.utils
import hashlib
import importlib.machinery
import importlib.util
import os
import pathlib
import subprocess
import tempfile
import unittest
p=pathlib.Path(__file__).resolve().parents[1]/'tools/syspulse-apt-update'
loader=importlib.machinery.SourceFileLoader('updater',str(p))
spec=importlib.util.spec_from_loader(loader.name,loader)
u=importlib.util.module_from_spec(spec);loader.exec_module(u)
OLD='3.0.0~alpha39~local1'
NEW='3.0.0~alpha39~render1'
class Tests(unittest.TestCase):
    def test_real_signed_simulation(self):
        self.assertNotEqual(os.geteuid(),0,'Run as ordinary user')
        with tempfile.TemporaryDirectory(prefix='syspulse-test-apt-') as d:
            root=pathlib.Path(d); home=root/'gnupg'; home.mkdir(mode=0o700)
            repo=root/'feed';repo.mkdir(); data=root/'pkg';(data/'DEBIAN').mkdir(parents=True)
            (data/'DEBIAN/control').write_text(f'Package: syspulse\nVersion: {NEW}\nArchitecture: amd64\nMaintainer: Test <fixture@example.invalid>\nDescription: Disposable test-only package\n')
            (data/'DEBIAN').chmod(0o755)
            (data/'DEBIAN/control').chmod(0o644)
            data.chmod(0o755)
            env={'PATH':'/usr/bin:/bin','HOME':d,'LANG':'C','LC_ALL':'C'}
            def run(args,**kw):
                return subprocess.run(args,env=env,check=True,capture_output=True,timeout=30,**kw).stdout
            gpg=['gpg','--no-options','--batch','--homedir',str(home),'--pinentry-mode','loopback','--passphrase','']
            try:
                run(gpg+['--quick-generate-key','SysPulse disposable isolated test key','ed25519','sign','1d'])
                key=root/'public.gpg';key.write_bytes(run(gpg+['--export']))
                run(['dpkg-deb','--root-owner-group','--build',str(data),str(repo/'syspulse.deb')])
                packages=run(['dpkg-scanpackages','.','/dev/null'],cwd=repo);(repo/'Packages').write_bytes(packages)
                now=datetime.datetime.now(datetime.timezone.utc)
                release=f'Origin: SysPulseFixture\nLabel: SysPulseFixture\nDate: {email.utils.format_datetime(now,usegmt=True)}\nValid-Until: {email.utils.format_datetime(now+datetime.timedelta(days=1),usegmt=True)}\nArchitectures: amd64\nDescription: Disposable test only\nSHA256:\n {hashlib.sha256(packages).hexdigest()} {len(packages)} Packages\n'
                (repo/'Release').write_text(release)
                run(gpg+['--clearsign','--output',str(repo/'InRelease'),str(repo/'Release')])
                status=root/'status';status.write_text(f'Package: syspulse\nStatus: install ok installed\nPriority: optional\nSection: admin\nInstalled-Size: 1\nMaintainer: Test <fixture@example.invalid>\nArchitecture: amd64\nVersion: {OLD}\nDescription: Disposable test-only status\n\n')
                original=status.read_bytes()
                (root/'lists/partial').mkdir(parents=True);(root/'archives/partial').mkdir(parents=True)
                source=root/'sources.list';source.write_text(f'deb [arch=amd64 signed-by={key}] {repo.as_uri()} /\n')
                cfg=root/'apt.conf';cfg.write_text('Dir::Etc::main "/dev/null";\nDir::Etc::parts "-";\n')
                opts=['-c',str(cfg)]
                for setting in ('Dir::Etc::sourcelist='+str(source),'Dir::Etc::sourceparts=-','Dir::Etc::trusted=/dev/null','Dir::Etc::trustedparts=-','Dir::State::lists='+str(root/'lists'),'Dir::State::status='+str(status),'Dir::State::extended_states='+str(root/'extended_states'),'Dir::Cache::archives='+str(root/'archives'),'Dir::Cache::pkgcache=','Dir::Cache::srcpkgcache=','APT::Architecture=amd64','Acquire::Languages=none','Acquire::AllowInsecureRepositories=false','APT::Get::AllowUnauthenticated=false','APT::Update::Error-Mode=any'):
                    opts+=['-o',setting]
                env['APT_CONFIG']=str(cfg)
                run(['apt-get',*opts,'update'])
                policy=run(['apt-cache',*opts,'policy','syspulse']).decode()
                self.assertIn('Candidate: '+NEW,policy)
                plan=run(['apt-get',*opts,'--simulate','--no-remove','--only-upgrade','--no-install-recommends','--no-install-suggests','install','syspulse='+NEW]).decode()
                print('Actual APT simulation:\n'+plan)
                u.review_plan(plan,OLD,NEW)
                self.assertEqual(status.read_bytes(),original)
                # Tamper with signed metadata and require fresh APT update to fail.
                (repo/'InRelease').write_bytes((repo/'InRelease').read_bytes().replace(b'Origin: SysPulseFixture',b'Origin: AlteredFixture'))
                import shutil
                shutil.rmtree(root/'lists');(root/'lists/partial').mkdir(parents=True)
                bad=subprocess.run(['apt-get',*opts,'update'],env=env,capture_output=True,timeout=30)
                self.assertNotEqual(bad.returncode,0)
                self.assertEqual(status.read_bytes(),original)
            finally:
                subprocess.run(['gpgconf','--homedir',str(home),'--kill','gpg-agent'],env=env,capture_output=True,timeout=10)
if __name__=='__main__':unittest.main(verbosity=2)
