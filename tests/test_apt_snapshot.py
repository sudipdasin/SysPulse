#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
import hashlib
import importlib.util
import os
from pathlib import Path
import subprocess
import tempfile
import unittest

spec = importlib.util.spec_from_file_location('snapshot', Path(__file__).parents[1] / 'tools/build-apt-snapshot.py')
m = importlib.util.module_from_spec(spec)
spec.loader.exec_module(m)


class Snapshot(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        # GnuPG agent Unix sockets have a short path limit; do not inherit nested TMPDIR.
        if os.geteuid() == 0:
            raise RuntimeError("Run snapshot tests as an ordinary user")
        cls.work = tempfile.TemporaryDirectory(prefix="sp-apt-", dir="/tmp")
        cls.addClassCleanup(cls.work.cleanup)
        cls.root = Path(cls.work.name)
        keyring = cls.root / 'gnupg'
        keyring.mkdir(mode=0o700)
        old = os.environ.get('GNUPGHOME')
        os.environ['GNUPGHOME'] = str(keyring)
        cls.addClassCleanup(lambda: os.environ.pop('GNUPGHOME', None) if old is None else os.environ.__setitem__('GNUPGHOME', old))
        cls.addClassCleanup(subprocess.run, ['gpgconf', '--homedir', str(keyring), '--kill', 'gpg-agent'], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, timeout=10)
        subprocess.run(['gpg', '--no-options', '--batch', '--pinentry-mode', 'loopback', '--passphrase', '', '--quick-gen-key', 'Synthetic test <test@example.invalid>', 'rsa2048', 'sign', '1d'], check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        cls.fpr = subprocess.check_output(['gpg', '--with-colons', '--list-secret-keys']).decode().split('fpr:::::::::')[1].split(':')[0]
        cls.pub = cls.root / 'test.gpg'
        cls.pub.write_bytes(subprocess.check_output(['gpg', '--export', cls.fpr]))
        tree = cls.root / 'fixture'
        (tree / 'DEBIAN').mkdir(parents=True)
        (tree / 'DEBIAN/control').write_text('Package: syspulse\nVersion: 3.0.0~alpha2\nArchitecture: amd64\nMaintainer: Synthetic <test@example.invalid>\nDescription: Synthetic fixture only\n')
        tree.chmod(0o755)
        (tree / 'DEBIAN').chmod(0o755)
        (tree / 'DEBIAN/control').chmod(0o644)
        cls.deb = cls.root / 'fixture.deb'
        subprocess.run(['dpkg-deb', '--root-owner-group', '--build', tree, cls.deb], check=True, stdout=subprocess.DEVNULL)
        cls.sha = hashlib.sha256(cls.deb.read_bytes()).hexdigest()

    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(dir=self.root)
        self.addCleanup(self.temp.cleanup)
        self.out = Path(self.temp.name) / 'repo'
        self.kw = dict(package=self.deb, expected_sha256=self.sha, expected_version='3.0.0~alpha2', output=self.out, fingerprint=self.fpr)

    def test_signed_metadata(self):
        m.build(**self.kw)
        subprocess.run(['gpgv', '--keyring', self.pub, self.out / 'InRelease'], check=True, stderr=subprocess.DEVNULL)
        subprocess.run(['gpgv', '--keyring', self.pub, self.out / 'Release.gpg', self.out / 'Release'], check=True, stderr=subprocess.DEVNULL)
        self.assertIn(self.sha, (self.out / 'Packages').read_text())
        self.assertIn('Valid-Until:', (self.out / 'Release').read_text())
        self.assertEqual(m.digest(next((self.out / 'pool').iterdir())), self.sha)
        self.assertEqual(self.out.stat().st_mode & 0o777, 0o755)

    def apt_options(self, key):
        state = Path(self.temp.name) / 'apt'
        for name in ('lists/partial', 'archives/partial', 'empty'):
            (state / name).mkdir(parents=True)
        (state / 'status').write_text('Package: syspulse\nStatus: install ok installed\nArchitecture: amd64\nVersion: 3.0.0~alpha1\nDescription: Synthetic installed fixture\nMaintainer: Synthetic <test@example.invalid>\n')
        source = state / 'sources.list'
        source.write_text(f'deb [arch=amd64 signed-by={key}] {self.out.as_uri()} ./\n')
        cfg = state / 'apt.conf'
        cfg.write_text('Dir::Etc::main "/dev/null";\nDir::Etc::parts "-";\n')
        self.apt_env = {'PATH': '/usr/bin:/bin', 'HOME': str(self.root), 'LANG': 'C', 'LC_ALL': 'C', 'APT_CONFIG': str(cfg)}
        opts = ['-c', str(cfg), '-o', f'Dir::Etc::sourcelist={source}', '-o', f'Dir::Etc::sourceparts={state / "empty"}', '-o', f'Dir::Etc::main=/dev/null', '-o', f'Dir::Etc::parts={state / "empty"}', '-o', f'Dir::State::lists={state / "lists"}', '-o', f'Dir::State::status={state / "status"}', '-o', f'Dir::Cache::archives={state / "archives"}', '-o', 'Dir::Cache::pkgcache=', '-o', 'Dir::Cache::srcpkgcache=', '-o', 'APT::Architecture=amd64', '-o', 'Acquire::Languages=none', '-o', 'Dir::Etc::trusted=/dev/null', '-o', 'Dir::Etc::trustedparts=-', '-o', f'Dir::State::extended_states={state / "extended_states"}', '-o', 'Acquire::AllowInsecureRepositories=false', '-o', 'APT::Get::AllowUnauthenticated=false', '-o', 'APT::Update::Error-Mode=any']
        return opts

    def test_apt_accepts_signed_snapshot(self):
        m.build(**self.kw)
        opts = self.apt_options(self.pub)
        status = Path(self.temp.name) / 'apt/status'
        original = status.read_bytes()
        subprocess.run(['apt-get', *opts, 'update'], env=self.apt_env, timeout=30, check=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        self.assertEqual(status.read_bytes(), original)
        policy = subprocess.check_output(['apt-cache', *opts, 'policy', 'syspulse'], env=self.apt_env, timeout=15).decode()
        self.assertIn('Installed: 3.0.0~alpha1', policy)
        self.assertIn('Candidate: 3.0.0~alpha2', policy)

    def test_apt_rejects_wrong_trust(self):
        m.build(**self.kw)
        wrong = Path(self.temp.name) / 'empty.gpg'
        wrong.touch()
        opts = self.apt_options(wrong)
        result = subprocess.run(['apt-get', *opts, 'update'], env=self.apt_env, timeout=30, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        self.assertNotEqual(result.returncode, 0)

    def test_apt_rejects_changed_index(self):
        m.build(**self.kw)
        with (self.out / 'Packages').open('ab') as stream:
            stream.write(b'\n')
        opts = self.apt_options(self.pub)
        result = subprocess.run(['apt-get', *opts, 'update'], env=self.apt_env, timeout=30, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        self.assertNotEqual(result.returncode, 0)

    def test_apt_config_isolated(self):
        opts = self.apt_options(self.pub)
        dump = subprocess.check_output(['apt-config', *opts, 'dump'], env=self.apt_env, timeout=15, text=True)
        self.assertIn('Dir::Etc::trusted "/dev/null";', dump)
        self.assertIn('Dir::Etc::trustedparts "-";', dump)
        self.assertIn('APT::Update::Error-Mode "any";', dump)
        self.assertIn('Acquire::AllowInsecureRepositories "false";', dump)
        self.assertIn('APT::Get::AllowUnauthenticated "false";', dump)
        self.assertNotIn('Post-Invoke', dump)
        self.assertNotIn('Pre-Invoke', dump)

    def test_bad_digest(self):
        self.kw['expected_sha256'] = '0' * 64
        with self.assertRaisesRegex(ValueError, 'digest mismatch'): m.build(**self.kw)
        self.assertFalse(self.out.exists())

    def test_wrong_version(self):
        self.kw['expected_version'] = '3.0.1'
        with self.assertRaisesRegex(ValueError, 'wrong package Version'): m.build(**self.kw)
        self.assertFalse(self.out.exists())

    def test_existing_output(self):
        self.out.mkdir()
        with self.assertRaisesRegex(ValueError, 'new directory'): m.build(**self.kw)

    def test_invalid_key(self):
        self.kw['fingerprint'] = 'short'
        with self.assertRaisesRegex(ValueError, 'fingerprint'): m.build(**self.kw)

    def test_unknown_key(self):
        self.kw['fingerprint'] = '0' * 40
        with self.assertRaises(subprocess.CalledProcessError): m.build(**self.kw)
        self.assertFalse(self.out.exists())

    def test_tampered_metadata(self):
        m.build(**self.kw)
        p = self.out / 'InRelease'
        p.write_bytes(p.read_bytes().replace(b'Label: SysPulse', b'Label: Tampered'))
        self.assertNotEqual(subprocess.run(['gpgv', '--keyring', self.pub, p], stderr=subprocess.DEVNULL).returncode, 0)


if __name__ == '__main__':
    unittest.main()
