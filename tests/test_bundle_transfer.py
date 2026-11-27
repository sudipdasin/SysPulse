#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Test real Git bundle round-trips; no application or network execution."""
import os
from pathlib import Path
import subprocess
import tempfile
import unittest

TOOL = Path(__file__).resolve().parents[1] / 'tools/transfer-integration-bundle.py'
REF = 'integration/alpha39'

class Tests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.root = Path(self.tmp.name)
        self.src = self.root / 'source'; self.src.mkdir()
        self.dest = self.root / 'restored'; self.bundle = self.root / 'source.bundle'
        self.env = dict(os.environ, HOME=str(self.root), XDG_CONFIG_HOME=str(self.root / 'config'), GIT_CONFIG_NOSYSTEM='1', GIT_CONFIG_GLOBAL='/dev/null')
        self.git(self.src, 'init', '-q', '--initial-branch=' + REF)
        self.git(self.src, 'config', 'user.name', 'Fixture')
        self.git(self.src, 'config', 'user.email', 'fixture@example.invalid')
        (self.src / 'source.txt').write_text('reviewed\n')
        self.git(self.src, 'add', '.')
        self.git(self.src, 'commit', '-qm', 'first')
        self.first = self.git(self.src, 'rev-parse', 'HEAD')
        (self.src / 'source.txt').write_text('second\n')
        self.git(self.src, 'commit', '-qam', 'second')
        self.pin = self.git(self.src, 'rev-parse', 'HEAD')

    def tearDown(self): self.tmp.cleanup()
    def git(self, path, *args):
        return subprocess.check_output(['git', '-C', str(path), *args], env=self.env, text=True).strip()
    def run_tool(self, *args):
        return subprocess.run(['python3', str(TOOL), *map(str,args)], env=self.env, text=True, capture_output=True, timeout=30)
    def export(self, pin=None, source=None):
        return self.run_tool('export', self.bundle, pin or self.pin, '--local-repo', source or self.src)
    def restore(self, pin=None, bundle=None):
        return self.run_tool('import', bundle or self.bundle, self.dest, pin or self.pin)
    def make(self):
        r=self.export(); self.assertEqual(r.returncode,0,r.stdout+r.stderr)
    def test_round_trip(self):
        self.make(); r=self.restore(); self.assertEqual(r.returncode,0,r.stdout+r.stderr)
        self.assertEqual(self.git(self.dest,'rev-parse','HEAD'),self.pin)
        self.assertEqual(self.git(self.dest,'rev-parse','HEAD^'),self.first)
        self.assertEqual(self.git(self.dest,'rev-parse','--is-shallow-repository'),'false')
        self.assertEqual(self.git(self.dest,'remote','get-url','origin'),'https://github.com/sudipdasin/SysPulse.git')
    def test_dirty_work_excluded(self):
        (self.src/'source.txt').write_text('private changes'); (self.src/'private.txt').write_text('private')
        self.make(); r=self.restore(); self.assertEqual(r.returncode,0,r.stderr)
        self.assertEqual((self.dest/'source.txt').read_text(),'second\n'); self.assertFalse((self.dest/'private.txt').exists())
        self.assertEqual((self.src/'source.txt').read_text(),'private changes')
    def test_extra_branch_excluded(self):
        self.git(self.src,'switch','-qc','private-topic'); (self.src/'private.txt').write_text('private'); self.git(self.src,'add','.'); self.git(self.src,'commit','-qm','private')
        self.make(); r=self.restore(); self.assertEqual(r.returncode,0,r.stderr)
        self.assertFalse((self.dest/'private.txt').exists()); self.assertEqual(self.git(self.dest,'branch','--list'),'* '+REF)
    def test_stale_pin(self):
        self.assertNotEqual(self.export(self.first).returncode,0); self.assertFalse(self.bundle.exists())
    def test_wrong_import_pin(self):
        self.make(); self.assertNotEqual(self.restore(self.first).returncode,0); self.assertFalse(self.dest.exists())
    def test_existing_bundle_preserved(self):
        self.bundle.write_text('keep'); self.assertNotEqual(self.export().returncode,0); self.assertEqual(self.bundle.read_text(),'keep')
    def test_existing_checkout_preserved(self):
        self.make(); self.dest.mkdir(); (self.dest/'keep').write_text('keep')
        self.assertNotEqual(self.restore().returncode,0); self.assertEqual((self.dest/'keep').read_text(),'keep')
    def test_corrupted_bundle(self):
        self.make(); data=self.bundle.read_bytes(); self.bundle.write_bytes(data[:len(data)//2])
        self.assertNotEqual(self.restore().returncode,0); self.assertFalse(self.dest.exists())
    def test_symlink_bundle(self):
        self.make(); link=self.root/'link.bundle'; link.symlink_to(self.bundle)
        self.assertNotEqual(self.restore(bundle=link).returncode,0); self.assertFalse(self.dest.exists())
    def test_shallow_source_refused(self):
        shallow=self.root/'shallow'; subprocess.run(['git','clone','-q','--depth','1','--branch',REF,self.src.as_uri(),str(shallow)],env=self.env,check=True)
        self.assertNotEqual(self.export(source=shallow).returncode,0); self.assertFalse(self.bundle.exists())
    def test_incremental_bundle_rejected(self):
        self.git(self.src,'bundle','create',str(self.bundle),REF,'^'+self.first)
        self.assertNotEqual(self.restore().returncode,0); self.assertFalse(self.dest.exists())
    def test_multi_ref_bundle_rejected(self):
        self.git(self.src,'branch','other'); self.git(self.src,'bundle','create',str(self.bundle),REF,'other')
        self.assertNotEqual(self.restore().returncode,0); self.assertFalse(self.dest.exists())

if __name__ == '__main__': unittest.main(verbosity=2)
