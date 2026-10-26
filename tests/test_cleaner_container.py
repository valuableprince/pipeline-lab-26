"""JOB-CLEANER-04: persistent Fort Carson dumpster, overflow, and chat security."""
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[1]
MAIN = (ROOT / "gamemodes/signal0317.pwn").read_text("utf-8")
JOB = (ROOT / "gamemodes/includes/cleaner_job.inc").read_text("utf-8")
DUMP = (ROOT / "gamemodes/includes/cleaner_container.inc").read_text("utf-8")
COMMANDS = (ROOT / "gamemodes/includes/commands.inc").read_text("utf-8")
SQL = (ROOT / "db/migrations/006_cleaner_container.sql").read_text("utf-8")
CI = (ROOT / ".github/workflows/build.yml").read_text("utf-8")
BUILD = (ROOT / "tools/build_windows.py").read_text("utf-8")

class CleanerContainerContracts(unittest.TestCase):
    def test_sql_v6_additive_constraints(self):
        for value in ("CREATE TABLE cleaner_container", "container_id = 1",
                      "bin_units BETWEEN 0 AND 60",
                      "overflow_units BETWEEN 0 AND 40",
                      "overflow_units = 0 OR bin_units = 60",
                      "INSERT INTO schema_migrations (version) VALUES (6)"):
            self.assertIn(value, SQL)
        self.assertNotRegex(SQL, r"(?i)\b(DROP\s+TABLE|DELETE\s+FROM|TRUNCATE)\b")

    def test_dump_coords_and_tiered_objects(self):
        for value in ("(-227.1175)", "(1066.0662)", "(19.7422)",
                      "CLEANER_DUMP_CAP            (60)",
                      "CLEANER_DUMP_OVERFLOW_CAP   (40)",
                      "CLEANER_DUMP_BAG_OBJECTS    (10)",
                      "gCleanerDumpBin, CLEANER_DUMP_CAP",
                      "Update3DTextLabelText(", "CreateObject(CLEANER_DUMP_MODEL",
                      "Мусорный контейнер %d/%d",
                      "CreateObject(CLEANER_DUMP_BAG_MODEL"):
            self.assertIn(value, DUMP)
        for rule in ("if (overflow > 30) return 10;",
                     "if (overflow >= 20) return 4;",
                     "if (overflow >= 10) return 2;",
                     "return 0;"):
            self.assertIn(rule, DUMP)
        self.assertNotIn("Create3DTextLabel", DUMP[DUMP.index("stock CleanerDump_VisualBagCount"):DUMP.index("stock bool:CleanerDump_Load")])

    def test_state_persisted_safe_and_atomic(self):
        self.assertIn("gCleanerDumpBin + gCleanerDumpOverflow + count <=", DUMP)
        self.assertIn("gCleanerDumpReady && !gCleanerDumpStaged", DUMP)
        self.assertIn("count > 0 && count <= 15", DUMP)
        self.assertIn('"START TRANSACTION"', DUMP)
        self.assertIn('"COMMIT"', DUMP)
        self.assertIn("DBWorld_Rollback();", DUMP)
        self.assertIn("changed != 1", DUMP)
        self.assertIn("WHERE container_id=1 AND bin_units=%d AND overflow_units=%d", DUMP)
        self.assertIn("container.env", DUMP)
        self.assertIn("container.bak", DUMP)
        self.assertIn("TARKENSODUMP|1", DUMP)
        self.assertIn("gCleanerDumpReady = false; // uncertain transaction state: fail closed", DUMP)
        deposit = JOB[JOB.index("stock Cleaner_Deposit(playerid)"):]
        ordered = ["CleanerDump_CanAccept(count)", "CleanerDump_Stage(count)",
                   "gCleanerXP[playerid] += count;",
                   "if (!Cleaner_Save(playerid))", "if (!CleanerDump_Commit())",
                   "gCleanerBag[playerid] = 0;", "if (!Cleaner_SettlePending(playerid))"]
        self.assertEqual([deposit.index(x) for x in ordered],
                         sorted(deposit.index(x) for x in ordered))
        self.assertIn("CleanerDump_Rollback();", deposit)
        self.assertIn("gCleanerEarned[playerid] -= wage;", deposit)
        self.assertIn("!CleanerDump_Near(playerid)", deposit)
        self.assertIn("if (gCleanerBag[playerid] > 0)", JOB)
        self.assertIn("CleanerDump_IsReady()", JOB)

    def test_account_integrity_simulated_ranges(self):
        # Model the same capacity split, checking every feasible transfer:
        # no units lost, bin <=60, overflow <=40, overflow only when bin full.
        for bin_units in range(61):
            for overflow in range(41):
                if overflow and bin_units != 60:
                    continue
                for count in range(1, 16):
                    if bin_units + overflow + count > 100:
                        continue
                    inside = min(count, 60 - bin_units)
                    next_bin = bin_units + inside
                    next_overflow = overflow + count - inside
                    self.assertLessEqual(next_bin, 60)
                    self.assertLessEqual(next_overflow, 40)
                    self.assertEqual(next_bin+next_overflow,bin_units+overflow+count)
                    self.assertTrue(next_overflow == 0 or next_bin == 60)

    def test_authorization_and_lifecycle(self):
        self.assertIn("!Admin_IsFounder(playerid) || !gAdminDuty[playerid]", DUMP)
        self.assertIn('Admin_LogPlayer(playerid, "DUMP_SERVICE", info)', DUMP)
        self.assertIn("cmd:trashservice(playerid, params[])", COMMANDS)
        self.assertIn("CleanerDump_Service(playerid)", COMMANDS)
        for name in ("CleanerDump_Init();", "CleanerDump_Shutdown();",
                     '#include "cleaner_container.inc"'):
            self.assertIn(name, MAIN)
        self.assertLess(MAIN.index("CleanerDump_Shutdown();"),
                        MAIN.index("DBPrimary_Shutdown();"))
        for build in (CI, BUILD):
            self.assertIn('"cleaner_container.inc"', build)

    def test_general_chat_id_and_guard_stay_intact(self):
        body = MAIN[MAIN.index("public OnPlayerText(playerid, text[])"):]
        self.assertIn("if (!Guard_AllowChat(playerid)) return 0;", body)
        self.assertIn("if (gAdminMuteUntil[playerid] > gettime())", body)
        self.assertIn('"%s(%d): %s", nickname, playerid, cleanText', body)
        self.assertIn("GetPlayerName(playerid, nickname", body)
        self.assertIn("if (cleanText[n] == '{') cleanText[n] = '(';", body)
        self.assertIn("else if (cleanText[n] == '}') cleanText[n] = ')';", body)
        self.assertIn("cleanText[n] = EOS;", body)
        self.assertIn("SendClientMessageToAll(COLOR_WHITE, message);", body)
        self.assertRegex(body, r"SendClientMessageToAll\(COLOR_WHITE, message\);\s*return 0;")

if __name__ == "__main__":
    unittest.main()
