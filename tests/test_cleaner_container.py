"""Container contracts: v6 migration, current v10 multi-container runtime, safe local chat."""
from pathlib import Path
from build_contracts import assert_staged
import re
import unittest

ROOT = Path(__file__).resolve().parents[1]
MAIN = (ROOT / "gamemodes/signal0317.pwn").read_text("utf-8")
JOB = (ROOT / "gamemodes/includes/cleaner_job.inc").read_text("utf-8")
DUMP = (ROOT / "gamemodes/includes/cleaner_container.inc").read_text("utf-8")
COMMANDS = (ROOT / "gamemodes/includes/commands.inc").read_text("utf-8")
SQL = (ROOT / "db/migrations/006_cleaner_container.sql").read_text("utf-8")

class CleanerContainerContracts(unittest.TestCase):
    def test_sql_v6_additive_constraints(self):
        for value in ("CREATE TABLE cleaner_container", "container_id = 1",
                      "bin_units BETWEEN 0 AND 60",
                      "overflow_units BETWEEN 0 AND 40",
                      "overflow_units = 0 OR bin_units = 60",
                      "INSERT INTO schema_migrations (version) VALUES (6)"):
            self.assertIn(value, SQL)
        self.assertNotRegex(SQL, r"(?i)\b(DROP\s+TABLE|DELETE\s+FROM|TRUNCATE)\b")

    def test_mapped_containers_and_bounded_bag_objects(self):
        # Coordinates moved from constants into the v10 container data model.
        migration = (ROOT / "db/migrations/010_cleaner_container_editor.sql").read_text("utf-8")
        self.assertIn("SELECT container_id,-227.1175,1066.0662,19.7422", migration)
        self.assertIn("FROM cleaner_container WHERE container_id=1", migration)
        for value in ("CLEANER_DUMP_LIMIT (24)", "CLEANER_DUMP_CAP (60)",
                      "CLEANER_DUMP_OVERFLOW_CAP (40)", "CLEANER_DUMP_BAG_OBJECTS (10)",
                      "gCleanerDumpPos[CLEANER_DUMP_LIMIT][4]",
                      "gCleanerDumpBin[slot], CLEANER_DUMP_CAP",
                      "Update3DTextLabelText(", "CreateObject(CLEANER_DUMP_MODEL",
                      "Мусорный контейнер %d/%d", "CreateObject(CLEANER_DUMP_BAG_MODEL",
                      "gCleanerDumpBagDefined[slot][j]",
                      "gCleanerDumpBagPos[slot][j][2] - CLEANER_DUMP_GROUND_OFFSET"):
            self.assertIn(value, DUMP)
        visual = DUMP.split("stock CleanerDump_VisualBagCount(", 1)[1].split("stock CleanerDump_Refresh(", 1)[0]
        self.assertIn("overflow * CLEANER_DUMP_BAG_OBJECTS + CLEANER_DUMP_OVERFLOW_CAP - 1", visual)
        self.assertIn("if (needed > CLEANER_DUMP_BAG_OBJECTS) needed = CLEANER_DUMP_BAG_OBJECTS;", visual)
        self.assertIn("if (overflow <= 0) return 0;", visual)
        self.assertNotIn("Create3DTextLabel", visual)

    def test_state_persisted_safe_and_atomic(self):
        for token in ("gCleanerDumpBin[slot] + gCleanerDumpOverflow[slot] + count <=",
                      "gCleanerDumpReady && !gCleanerDumpStaged", "count > 0 && count <= 15",
                      '"START TRANSACTION"', '"COMMIT"', "DBWorld_Rollback();",
                      "changed != 1", "slot >= 0 && slot < gCleanerDumpCount",
                      "WHERE container_id=%d AND bin_units=%d AND overflow_units=%d AND enabled=1",
                      "gCleanerDumpReady = false; // unknown transaction state: fail closed"):
            self.assertIn(token, DUMP)
        commit = DUMP.split("stock bool:CleanerDump_Commit()", 1)[1].split("stock bool:CleanerDump_SetFill(", 1)[0]
        self.assertLess(commit.index('DBWorld_Exec("COMMIT")'), commit.index("gCleanerDumpBin[slot] = gCleanerDumpNextBin;"))
        self.assertIn("CleanerDump_Rollback();", commit)
        deposit = JOB[JOB.index("stock Cleaner_Deposit(playerid)"):]
        ordered = ["CleanerDump_CanAccept(playerid, count)", "CleanerDump_Stage(playerid, count)",
                   "gCleanerXP[playerid] += count;", "if (!Cleaner_Save(playerid))",
                   "if (!CleanerDump_Commit())", "gCleanerBag[playerid] = 0;",
                   "if (!Cleaner_SettlePending(playerid))"]
        self.assertEqual([deposit.index(x) for x in ordered], sorted(deposit.index(x) for x in ordered))
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
        self.assertIn('Admin_LogPlayer(playerid, "DUMP_SERVICE", detail)', DUMP)
        self.assertIn("cmd:trashservice(playerid, params[])", COMMANDS)
        self.assertIn("CleanerDump_Service(playerid)", COMMANDS)
        for name in ("CleanerDump_Init();", "CleanerDump_Shutdown();",
                     '#include "cleaner_container.inc"'):
            self.assertIn(name, MAIN)
        self.assertLess(MAIN.index("CleanerDump_Shutdown();"),
                        MAIN.index("DBPrimary_Shutdown();"))
        assert_staged(self, "cleaner_container.inc")

    def test_general_chat_id_and_guard_stay_intact(self):
        body = MAIN[MAIN.index("public OnPlayerText(playerid, text[])"):]
        self.assertIn("if (!Guard_AllowChat(playerid)) return 0;", body)
        self.assertIn("if (gAdminMuteUntil[playerid] > gettime())", body)
        self.assertIn('"%s(%d): %s", nickname, playerid, cleanText', body)
        self.assertIn("GetPlayerName(playerid, nickname", body)
        self.assertIn("if (cleanText[n] == '{') cleanText[n] = '(';", body)
        self.assertIn("else if (cleanText[n] == '}') cleanText[n] = ')';", body)
        self.assertIn("cleanText[n] = EOS;", body)
        self.assertIn("RP_SendLocal(playerid, COLOR_WHITE, message, RP_RADIUS_SPEECH);", body)
        self.assertNotIn("SendClientMessageToAll", body)
        self.assertRegex(body, r"RP_SendLocal\(playerid, COLOR_WHITE, message, RP_RADIUS_SPEECH\);\s*return 0;")

if __name__ == "__main__":
    unittest.main()
