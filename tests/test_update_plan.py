#!/usr/bin/python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Updater review policy tests; no real install or privileged mutation."""
import contextlib
import importlib.machinery
import importlib.util
import pathlib
import unittest
from unittest.mock import patch
p=pathlib.Path(__file__).resolve().parents[1]/'tools/syspulse-apt-update'
loader=importlib.machinery.SourceFileLoader('updater',str(p))
spec=importlib.util.spec_from_loader(loader.name,loader)
u=importlib.util.module_from_spec(spec);loader.exec_module(u)
OLD='3.0.0~alpha39~local1'
NEW='3.0.0~alpha39~render1'
SUMMARY='1 upgraded, 0 newly installed, 0 to remove and 0 not upgraded.'
PLAN=f'{SUMMARY}\nInst syspulse [{OLD}] ({NEW} Fixture [amd64])\nConf syspulse ({NEW} Fixture [amd64])\n'
class Tests(unittest.TestCase):
    def test_local_installed_version(self):
        with patch.object(u,'run',return_value=f'installed {OLD} amd64'):
            self.assertEqual(u.installed(),OLD)
        self.assertEqual(u.version(OLD,allow_local=True),'alpha')
        self.assertTrue(u.newer(NEW,OLD))
    def test_local_candidate_denied(self):
        with self.assertRaises(ValueError):u.version(OLD)
        with patch.object(u,'run',return_value=f'  Candidate: {OLD}\n'):
            with self.assertRaises(ValueError):u.candidate([],'alpha','3.0.0~alpha38')
    def test_local_review_denied(self):
        with patch.object(u.os,'geteuid',return_value=0), patch.object(u,'snapshot') as snap:
            with self.assertRaises(ValueError):u.install('alpha',OLD,None,None)
            snap.assert_not_called()
    def test_signed_successor_available(self):
        with patch.object(u,'installed',return_value=OLD), patch.object(u,'secure_key',return_value=True), patch.object(u,'snapshot',return_value=contextlib.nullcontext([])),patch.object(u,'run',return_value=f'  Candidate: {NEW}\n'):
            self.assertEqual(u.check('alpha',None,None),'AVAILABLE '+NEW)
    def test_legacy_versions(self):
        for v,c in [('3.0.0','stable'),('3.0.0~beta1','beta'),('3.0.0~alpha39~render1','alpha')]:
            self.assertEqual(u.version(v),c)
        for v in ('3.0.0~local1','3.0.0~alpha39~local0','3.0.0~alpha39~local01','3.0.0~alpha39~local1\n'):
            with self.assertRaises(ValueError):u.version(v,allow_local=True)
    def test_exact_plan(self):
        u.review_plan(PLAN,OLD,NEW)
        u.review_plan(PLAN.replace('syspulse ','syspulse:amd64 '),OLD,NEW)
    def test_summary_missing_duplicate_or_wrong(self):
        for plan in (PLAN.replace(SUMMARY,'',1),PLAN+SUMMARY,PLAN.replace('1 upgraded','2 upgraded'),PLAN.replace('0 newly','1 newly'),PLAN.replace('0 to remove','1 to remove')):
            with self.assertRaises(RuntimeError):u.review_plan(plan,OLD,NEW)
    def test_unrelated_actions(self):
        for line in ('Inst other (1 Fixture [amd64])',' Conf other (1 Fixture [amd64])','Remv syspulse','Purg syspulse','Inst'):
            with self.assertRaises(RuntimeError):u.review_plan(PLAN+line+'\n',OLD,NEW)
    def test_mismatch_versions(self):
        for plan in (PLAN.replace(OLD,'3.0.0~alpha38'),PLAN.replace(NEW,'3.0.0~alpha40'),PLAN.replace('syspulse ','syspulse:i386 '),PLAN.replace('[amd64]','[arm64]')):
            with self.assertRaises(RuntimeError):u.review_plan(plan,OLD,NEW)
    def test_missing_or_duplicate_action(self):
        for plan in (PLAN.split('Conf')[0],PLAN+PLAN.splitlines()[2],PLAN.replace('Conf syspulse','Inst syspulse')):
            with self.assertRaises(RuntimeError):u.review_plan(plan,OLD,NEW)
    def test_warning_and_bounds(self):
        for line in ('W: warning',' E: error','N: notice','\x00','x'*262145):
            with self.assertRaises(RuntimeError):u.review_plan(PLAN+line,OLD,NEW)
    def test_review_before_mutation(self):
        with patch.object(u.os,'geteuid',return_value=0),patch.object(u,'installed',return_value=OLD),patch.object(u,'snapshot',return_value=contextlib.nullcontext([])),patch.object(u,'candidate',return_value=NEW),patch.object(u,'run',return_value=PLAN+'Purg other\n') as run:
            with self.assertRaises(RuntimeError):u.install('alpha',NEW,None,None)
            self.assertEqual(run.call_count,1)
            self.assertIn('--simulate',run.call_args.args[0])
    def test_install_readback(self):
        with patch.object(u.os,'geteuid',return_value=0),patch.object(u,'installed',side_effect=[OLD,OLD,NEW]),patch.object(u,'snapshot',return_value=contextlib.nullcontext([])),patch.object(u,'candidate',return_value=NEW),patch.object(u,'run',side_effect=[PLAN,'']) as run:
            self.assertEqual(u.install('alpha',NEW,None,None),'INSTALLED '+NEW)
            self.assertEqual(run.call_count,2)
            self.assertIn('-y',run.call_args.args[0])
if __name__=='__main__':unittest.main(verbosity=2)
