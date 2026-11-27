#!/usr/bin/python3
"""Recovery recognition boundary tests; no logs from a real machine."""
import pathlib,sys,unittest
sys.path.insert(0,str(pathlib.Path(__file__).resolve().parents[1]/"tools"))
import syspulse_recovery as r
class Tests(unittest.TestCase):
    examples=(
      ("SMART: 0 pending sectors, 0 offline uncorrectable sectors","storage_health_report"),
      ("SMART health passed; 0 pending sectors","storage_health_report"),
      ("No errors detected in the RAM test.","memory_check_clear"),
      ("Memory pressure has returned to normal.","memory_recovery"),
      ("Previous error resolved; service started successfully","service_recovery"),
      ("Previous package error was resolved during recovery","package_recovery"),
      ("Connection timed out earlier; link restored","network_recovery"),
      ("GPU reset completed successfully; display restored","gpu_recovery"),
    )
    def test_recognize(self):
        for message,expected in self.examples:
            with self.subTest(message=message):self.assertEqual(r.observation(message,6),expected)
    def test_priority(self):
        for message,_ in self.examples:
            for priority in (0,1,2,3,True,"6"):
                with self.subTest(message=message,priority=priority):self.assertIsNone(r.observation(message,priority))
    def test_mixed_fault_never_suppressed(self):
        for message,_ in self.examples:
            for suffix in ("; I/O error","; GPU hang","; pending sectors 5","; kernel panic","; temperature above threshold"):
                self.assertIsNone(r.observation(message+suffix,6))
    def test_nonzero_smart(self):
        for message in ("SMART: 1 pending sectors, 0 offline uncorrectable sectors","SMART: 0 pending sectors, 2 offline uncorrectable sectors","SMART health passed; 7 pending sectors"):
            self.assertIsNone(r.observation(message,6))
    def test_uncertain_recovery(self):
        for message in ("Memory pressure may return to normal","GPU reset not completed successfully","Previous package error was not resolved during recovery","Connection timed out earlier; link not restored"):
            self.assertIsNone(r.observation(message,6))
    def test_bounded(self):
        self.assertIsNone(r.observation("GPU reset completed successfully"+" "*9000+"; GPU hang",6))
        self.assertIsNone(r.observation(None,6))
if __name__=="__main__":unittest.main(verbosity=2)
