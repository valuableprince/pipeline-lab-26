"""AUDIT-QA-01: проверка warning gate и связности критических подсистем."""
from contextlib import redirect_stderr, redirect_stdout
from io import StringIO
from pathlib import Path
from build_contracts import assert_staged
import re
import tempfile
import unittest

from tools.check_pawn_output import analyze_pawn_output, main as verify_compiler_log

ROOT = Path(__file__).resolve().parents[1]
GAME = (ROOT / "gamemodes/signal0317.pwn").read_text(encoding="utf-8")
INC = ROOT / "gamemodes/includes"
CI = (ROOT / ".github/workflows/build.yml").read_text(encoding="utf-8")
WIN = (ROOT / "tools/build_windows.py").read_text(encoding="utf-8")
HUD = (INC / "orvelar_hud.inc").read_text(encoding="utf-8")
ECONOMY = (INC / "economy.inc").read_text(encoding="utf-8")
CLEANER = (INC / "cleaner_job.inc").read_text(encoding="utf-8")
LEADER = (INC / "leader_system.inc").read_text(encoding="utf-8")


class CompilerQualityGate(unittest.TestCase):
    SUCCESS = "Pawn compiler 3.10.11\nCopyright (c) 1997-2006, ITB CompuPhase\n"

    def test_clean_and_zero_warning_logs_pass(self):
        self.assertEqual(analyze_pawn_output(self.SUCCESS), [])
        self.assertEqual(analyze_pawn_output(self.SUCCESS + "0 Warnings.\n"), [])

    def test_warning_even_with_successful_compiler_exit_is_failure(self):
        msg = "file.inc(23) : warning 234: function is deprecated (symbol X)"
        self.assertIn(msg, analyze_pawn_output(self.SUCCESS + msg))
        self.assertTrue(analyze_pawn_output(self.SUCCESS + "6 Warnings.\n"))

    def test_error_and_absent_compiler_are_fail_closed(self):
        self.assertTrue(analyze_pawn_output(self.SUCCESS + "source.pwn(4): error 017: undefined symbol"))
        self.assertTrue(analyze_pawn_output(self.SUCCESS + "1 Error.\n"))
        self.assertTrue(analyze_pawn_output(""))
        self.assertTrue(analyze_pawn_output("build step skipped"))

    def test_file_cli_rejects_warning_and_missing_log(self):
        with tempfile.TemporaryDirectory() as temp:
            log = Path(temp) / "pawn.log"
            with redirect_stdout(StringIO()), redirect_stderr(StringIO()):
                self.assertEqual(verify_compiler_log([str(log)]), 1)
                log.write_text(self.SUCCESS, encoding="utf-8")
                self.assertEqual(verify_compiler_log([str(log)]), 0)
                log.write_text(self.SUCCESS + "2 Warnings.", encoding="utf-8")
                self.assertEqual(verify_compiler_log([str(log)]), 1)

    def test_ci_and_windows_both_block_warnings(self):
        self.assertIn("set -euo pipefail", CI)
        self.assertIn("2>&1 | tee build/compiler.log", CI)
        self.assertIn("python3 tools/check_pawn_output.py build/compiler.log", CI)
        self.assertIn("python3 -m unittest discover -s tests -v", CI)
        self.assertIn("python3 -m compileall -q tools tests", CI)
        self.assertIn("from check_pawn_output import analyze_pawn_output", WIN)
        self.assertIn("stderr=subprocess.STDOUT", WIN)
        self.assertIn("issues = analyze_pawn_output(output_text)", WIN)
        self.assertIn("if amx.is_file():", WIN)
        self.assertIn("amx.unlink()", WIN)

    def test_all_main_includes_are_staged_for_ci_and_windows(self):
        modules = re.findall(r'^#include\s+"([^"]+)"', GAME, flags=re.M)
        self.assertGreaterEqual(len(modules), 10)
        self.assertEqual(len(modules), len(set(modules)))
        for module in modules:
            with self.subTest(include=module):
                self.assertTrue((INC / module).is_file())
                assert_staged(self, module)

    def test_hud_uses_supported_openmp_apis(self):
        self.assertEqual(HUD.count("TextDrawColour("), 3)
        self.assertEqual(HUD.count("TextDrawBackgroundColour("), 3)
        self.assertNotIn("TextDrawColor(", HUD)
        self.assertNotIn("TextDrawBackgroundColor(", HUD)


class CriticalSmokeContracts(unittest.TestCase):
    """Контракты исходного кода; не имитируют работу GTA/OpenMP."""

    def test_money_is_authoritative_and_persisted(self):
        self.assertIn('ACCOUNT_FORMAT       "SIGNAL0317|5"', GAME)
        for name in ("Economy_Credit", "Economy_Debit", "Economy_CleanerSettle"):
            self.assertIn("stock bool:" + name + "(", ECONOMY)
        self.assertIn("if (!Auth_SaveAccount(playerid))", ECONOMY)
        self.assertIn("Cleaner_SettlePending(playerid)", CLEANER)
        self.assertNotIn("GivePlayerMoney(", CLEANER)
        # Client money is observed only to repair HUD, never used as account authority.
        reconcile = ECONOMY.split("public Economy_ReconcileTick()", 1)[1].split("stock Economy_Log(", 1)[0]
        self.assertIn("GetPlayerMoney(playerid) != gAccountMoney[playerid]", reconcile)
        self.assertIn("Economy_SyncHUD(playerid);", reconcile)
        self.assertNotRegex(ECONOMY, r"gAccountMoney\[playerid\]\s*=\s*GetPlayerMoney\(")
        change = ECONOMY.split("stock bool:Economy_Change(", 1)[1].split("stock bool:Economy_Credit(", 1)[0]
        self.assertNotIn("GetPlayerMoney(", change)

    def test_leader_cache_and_worker_only_zone_tick(self):
        self.assertIn("gFactionCachedOrg[playerid]", LEADER)
        self.assertIn("gFactionCachedRank[playerid]", LEADER)
        self.assertIn("Leader_RefreshAccount(name)", LEADER)
        zone = CLEANER.split("public Cleaner_ZoneTick()", 1)[1].split("stock Cleaner_Deposit(", 1)[0]
        self.assertIn("i < gCleanerWorkerCount", zone)
        self.assertNotIn("MAX_PLAYERS", zone)

    def test_remaining_modules_avoid_pool_scans(self):
        self.assertNotIn("GetPlayerPoolSize()", GAME + CLEANER + LEADER)


if __name__ == "__main__":
    unittest.main()
