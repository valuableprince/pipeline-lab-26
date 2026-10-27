"""Scaling harness extraction and strict result-validation contracts."""
import unittest
from pathlib import Path

from tools.run_pawn_scaling import (
    CASE_NAMES, DONE, HARNESS, extract_function, parse_samples, stage_sources,
)


class ScalingContractTests(unittest.TestCase):
    def test_extraction_uses_original_pawn_signature_and_brace_scope(self):
        src = '''
// stock bool:Leader_ValidateRoster(org) { return false; }
stock bool:Leader_ValidateRoster(org)
{
    if (org) { printf(" } fake"); }
    return true;
}
stock Other() { return 1; }
'''
        extracted = extract_function(src, "Leader_ValidateRoster")
        self.assertIn('printf(" } fake")', extracted)
        self.assertNotIn("stock Other", extracted)

    def test_absent_routine_is_an_error(self):
        with self.assertRaises(ValueError):
            extract_function('stock SomethingElse() { return true; }', "Leader_ValidateRoster")

    def test_production_benchmark_call_is_literally_in_harness(self):
        code = HARNESS.read_text(encoding="utf-8")
        for fragment in (
            '#include "payday.inc"',
            '#include "perf_leader_validate.inc"',
            '#include "perf_studio_poll.inc"',
            "PayDay_Grant(i, 1000)",
            "Leader_ValidateRoster((i % FACTION_COUNT)+1)",
            "StudioZone_Poll();",
            "cases[] = {100, 250, 500}",
            "PERF_SCALE_DONE:cases=9,samples=7",
        ):
            self.assertIn(fragment, code)

    def test_parser_requires_complete_samples_with_stable_checksums(self):
        lines = []
        for name in CASE_NAMES:
            for i in range(7):
                family, count = name.rsplit("_", 1)
                lines.append(f"PERF_SCALE:name={family}_{count},pass={i},ms={8+i},checksum=12")
        lines.append(DONE)
        res = parse_samples("\n".join(lines))
        self.assertEqual(len(res), 9)
        self.assertEqual(res["payday_100"]["median_ms"], 11)
        with self.assertRaises(ValueError):
            parse_samples("\n".join(lines[:-2]))
        with self.assertRaises(ValueError):
            parse_samples("\n".join(lines+[lines[0]]))

    def test_parser_rejects_checksum_drift_and_premature_abort(self):
        lines = []
        for name in CASE_NAMES:
            for i in range(7):
                family, count = name.rsplit("_", 1)
                lines.append(f"PERF_SCALE:name={family}_{count},pass={i},ms=10,checksum={i if name=='zones_500' else 10}")
        lines.append(DONE)
        with self.assertRaises(ValueError):
            parse_samples("\n".join(lines))
        with self.assertRaises(ValueError):
            parse_samples("\n".join(lines) + "\nPERF_SCALE_ERROR: failure")

    def test_stage_extracts_real_Pawn_code(self):
        from tempfile import TemporaryDirectory
        inputs = {
            "gamemodes/includes/payday.inc": b"stock PayDay_Grant() { return 1; }",
            "gamemodes/includes/leader_system.inc":
                b"stock bool:Leader_ValidateRoster(org) { return org != 0; }",
            "gamemodes/includes/studio_system.inc":
                b"public StudioZone_Poll() { return 1; }",
        }
        with TemporaryDirectory() as t:
            stage = Path(t)
            stage_sources(stage, inputs)
            self.assertIn("Leader_ValidateRoster", (stage/"perf_leader_validate.inc").read_text())
            self.assertIn("StudioZone_Poll", (stage/"perf_studio_poll.inc").read_text())
            self.assertIn("PayDay_Grant", (stage/"payday.inc").read_text())

if __name__ == "__main__":
    unittest.main()
