"""JOB-CLEANER-05: world-editor, persisted multi-bin, exact deposit safety and navigation."""
from pathlib import Path
import unittest

ROOT=Path(__file__).resolve().parents[1]
DUMP=(ROOT/"gamemodes/includes/cleaner_container.inc").read_text("utf-8")
EDITOR=(ROOT/"gamemodes/includes/cleaner_editor.inc").read_text("utf-8")
JOB=(ROOT/"gamemodes/includes/cleaner_job.inc").read_text("utf-8")
STUDIO=(ROOT/"gamemodes/includes/studio_system.inc").read_text("utf-8")
MAIN=(ROOT/"gamemodes/signal0317.pwn").read_text("utf-8")
DB=(ROOT/"gamemodes/includes/db_primary.inc").read_text("utf-8")
SQL=(ROOT/"db/migrations/010_cleaner_container_editor.sql").read_text("utf-8")

class CleanerEditor05(unittest.TestCase):
    def test_migration_retains_old_bin_and_quantities(self):
        self.assertIn("INSERT INTO cleaner_containers",SQL)
        self.assertIn("SELECT container_id,-227.1175,1066.0662,19.7422",SQL)
        self.assertIn("bin_units,overflow_units",SQL)
        self.assertIn("FROM cleaner_container WHERE container_id=1",SQL)
        self.assertIn("FOREIGN KEY (container_id)",SQL)
        self.assertNotIn("DROP TABLE",SQL)
        self.assertIn("VALUES (10)",SQL)
        self.assertIn("WHERE version=10 LIMIT 1",DB)

    def test_editor_is_private_and_audited(self):
        for term in ("EditPlayerObject(playerid, gCleanerEditorObject)",
                     "EditPlayerObject(playerid, gCleanerEditorBagObject[listitem])",
                     "CreatePlayerObject(playerid, CLEANER_DUMP_MODEL",
                     "CreatePlayerObject(owner, CLEANER_DUMP_BAG_MODEL",
                     "DestroyPlayerObject(owner,", "EDIT_RESPONSE_FINAL",
                     "EDIT_RESPONSE_CANCEL", "Admin_LogPlayer(owner, \"DUMP_EDITOR_SAVE\"",
                     "Admin_LogPlayer(playerid, \"DUMP_EDITOR_DELETE\"",
                     "DBWorld_Exec(\"START TRANSACTION\")",
                     "DBWorld_Exec(\"COMMIT\")","DBWorld_Rollback()",
                     "INSERT INTO cleaner_container_bag_positions", "DELETE FROM cleaner_container_bag_positions"):
            self.assertIn(term,EDITOR)
        for term in ("Studio_Allowed(playerid)","!Race_IsParticipant(playerid)",
                     "!AdminPunish_IsJailed(playerid)", "gStoryStage[playerid] >= STORY_DONE",
                     "GetPlayerInterior(playerid) == 0", "gCleanerDumpBin[slot] || gCleanerDumpOverflow[slot]"):
            self.assertIn(term,EDITOR)
        self.assertIn("CleanerEditor_OnEdit(playerid, playerobject, objectid, response,",MAIN)
        self.assertIn("CleanerEditor_OnDialog(playerid, dialogid, response, listitem)",MAIN)
        self.assertIn("CleanerEditor_List(playerid)",STUDIO)
        self.assertIn('/studio containers', STUDIO)

    def test_multiple_bins_and_atomic_job_payment(self):
        for term in ("CLEANER_DUMP_LIMIT (24)","gCleanerDumpId[CLEANER_DUMP_LIMIT]",
                     "gCleanerDumpBin[CLEANER_DUMP_LIMIT]",
                     "SELECT container_id,x,y,z,rz", "FROM cleaner_containers",
                     "gCleanerDumpTarget[playerid]","CleanerDump_CanAccept(playerid, count)",
                     "CleanerDump_Stage(playerid, count)","DBWorld_Exec(\"START TRANSACTION\")",
                     "DBWorld_Exec(\"COMMIT\")", "DBWorld_Rollback()",
                     "changed != 1","WHERE container_id=%d AND bin_units=%d AND overflow_units=%d"):
            self.assertIn(term,DUMP)
        body=JOB.split("stock Cleaner_Deposit(playerid)",1)[1]
        order=["CleanerDump_CanAccept(playerid, count)","CleanerDump_Stage(playerid, count)",
               "gCleanerXP[playerid] += count;", "if (!Cleaner_Save(playerid))",
               "if (!CleanerDump_Commit())", "gCleanerBag[playerid] = 0;",
               "if (!Cleaner_SettlePending(playerid))"]
        self.assertEqual([body.index(s) for s in order], sorted(body.index(s) for s in order))

    def test_navigation_is_per_player_and_cleans_up(self):
        for needle in ("SetPlayerMapIcon(playerid, CLEANER_DUMP_MAP_SLOT",
                       "CLEANER_DUMP_MAP_COLOR (0xEE3333FF)",
                       "RemovePlayerMapIcon(playerid, CLEANER_DUMP_MAP_SLOT)",
                       "CleanerDump_Nearest(playerid, count)",
                       "gCleanerDumpBin[i] - gCleanerDumpOverflow[i] < count"):
            self.assertIn(needle,DUMP)
        self.assertIn("CleanerDump_Navigate(playerid, gCleanerBag[playerid]);",JOB)
        self.assertIn("CleanerDump_NavClear(playerid);",JOB)
        self.assertIn("CleanerDump_NavReset(playerid);",JOB)
        self.assertNotIn("SetPlayerCheckpoint(",DUMP+EDITOR)

    def test_bag_rendering_depends_on_overflow_only(self):
        self.assertIn("if (overflow <= 0) return 0;",DUMP)
        self.assertIn("CleanerDump_VisualBagCount(slot)",DUMP)
        self.assertIn("gCleanerDumpBagDefined[slot][j]",DUMP)
        self.assertIn("CreateObject(CLEANER_DUMP_BAG_MODEL",DUMP)
        self.assertIn("gCleanerEditorBagEnabled[i]",EDITOR)

    def test_navigation_survives_vehicle_state_change(self):
        self.assertIn("stock bool:CleanerDump_CanNavigate(playerid)",DUMP)
        self.assertNotIn("!Cleaner_InWorld(playerid) return -1",DUMP)
        can=DUMP.split("stock bool:CleanerDump_CanNavigate(playerid)",1)[1].split("stock CleanerDump_Nearest",1)[0]
        self.assertNotIn("IsPlayerInAnyVehicle",can)
        self.assertIn("CleanerDump_NavClear(playerid);\n        CleanerDump_Navigate(playerid, gCleanerBag[playerid]);",MAIN)
        self.assertIn("CleanerDump_Nearest(playerid, count)",DUMP)

    def test_editor_fill_has_strict_bounds_audit_and_compare_swap(self):
        for value in ("CLEANER_EDITOR_FILL_INPUT", "Опустошить контейнер",
                      "Установить заполнение (0-100)", "strlen(inputtext) > 3",
                      "inputtext[i] < '0' || inputtext[i] > '9'",
                      "total > CLEANER_DUMP_CAP + CLEANER_DUMP_OVERFLOW_CAP",
                      "CleanerDump_SetFill(playerid, gCleanerEditorSelection[playerid], total)",
                      "CleanerDump_SetFill(playerid, slot, 0)"):
            self.assertIn(value, EDITOR)
        for value in ("Admin_LogPlayer(playerid, \"DUMP_EDITOR_FILL\"",
                      "bin = CLEANER_DUMP_CAP", "overflow = total - bin",
                      "bin_units=%d AND overflow_units=%d AND enabled=1",
                      "DBPrimary_Execute(q, changed) || changed != 1",
                      "CleanerDump_Refresh(slot)", "CleanerDump_Navigate(p, gCleanerBag[p])"):
            self.assertIn(value,DUMP)

    def test_cp1251(self):
        for content in (MAIN,EDITOR,JOB,DUMP,STUDIO):
            self.assertEqual(content,content.encode("cp1251").decode("cp1251"))

if __name__=="__main__":
    unittest.main()
