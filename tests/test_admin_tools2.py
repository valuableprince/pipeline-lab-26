"""ADMIN-TOOLS-02: command contracts, permissions, audit, hint dictionary, and isolation."""
from pathlib import Path
import unittest
import re

ROOT = Path(__file__).resolve().parents[1]
CMDS = (ROOT / "gamemodes/includes/commands.inc").read_text("utf-8")
MODULE = (ROOT / "gamemodes/includes/admin_tools2.inc").read_text("utf-8")
HELP = (ROOT / "gamemodes/includes/admin_help.inc").read_text("utf-8")
ADMIN = (ROOT / "gamemodes/includes/admin_system.inc").read_text("utf-8")
GAME = (ROOT / "gamemodes/signal0317.pwn").read_text("utf-8")
STORY = (ROOT / "gamemodes/includes/story_prologue.inc").read_text("utf-8")
RACE = (ROOT / "gamemodes/includes/race_map.inc").read_text("utf-8")


class AdminTools02Contract(unittest.TestCase):
    def test_eight_commands_are_registered_once(self):
        commands = ["unmute", "history", "respawn", "setskin", "settime",
                    "clearchat", "setvw", "setint"]
        for name in commands:
            self.assertEqual(len(re.findall(rf"^cmd:{name}\(playerid, params\[\]\)", CMDS, re.M)), 1)
            self.assertIn(f'"{name}"', HELP)
        self.assertIn('#include "admin_help.inc"', GAME)
        self.assertIn('#include "admin_tools2.inc"', GAME)

    def test_help_metadata_for_all_administrative_commands(self):
        existing = ["ahelp","aduty","admin","admins","a","reports","accept",
                    "areply","close","spec","warn","mute","ban","unban",
                    "setadmin","setevent","weather","event","veh","delveh",
                    "heal","armour","fixveh","flip","check","alog","founder"]
        for name in existing:
            self.assertIn('{"' + name + '", "/', HELP)
        self.assertIn("ahUsage", HELP)
        self.assertIn("ahPurpose", HELP)
        self.assertIn("ahExample", HELP)
        self.assertIn("AdminHelp_Access(playerid, index)", HELP)
        self.assertIn("AdminHelp_Print(playerid, index)", HELP)
        self.assertIn("ADMIN_HELP_PAGE_SIZE (9)", HELP)
        self.assertIn("AdminHelp_Usage(playerid, \"mute\")", CMDS)
        self.assertIn("AdminHelp_FromSlash(playerid, command)", ADMIN)
        self.assertIn("AdminHelp_RequiresParams(command)", CMDS)

    def test_unmute_is_hierarchy_protected_and_persistent(self):
        body = MODULE.split("stock AdminTools2_Unmute(", 1)[1].split(
            "stock bool:AdminTools2_HistoryAction", 1)[0]
        for token in ("PERM_MODERATE", "Admin_ValidTarget(playerid, target)",
                      "gAdminMuteUntil[target] <= gettime()", 'Admin_LogPlayer(playerid, "UNMUTE", detail)',
                      "gAdminMuteUntil[target] = 0", "Auth_SaveAccount(target)",
                      "gAdminMuteUntil[target] = oldUntil"):
            self.assertIn(token, body)
        self.assertLess(body.index('Admin_LogPlayer(playerid, "UNMUTE", detail)'),
                        body.index("gAdminMuteUntil[target] = 0"))

    def test_history_only_reads_records_for_exact_account(self):
        body = MODULE.split("stock AdminTools2_History(", 1)[1].split(
            "stock AdminTools2_Respawn(", 1)[0]
        for token in ('"HISTORY_VIEW"', '"/audit.log"', "Admin_Ready(target)",
                      "AdminTools2_HistoryAction(action)", "gAccountName[target]",
                      "line[detailStart + targetLength] != ':'", "fclose(log)",
                      "entries[5][140]"):
            self.assertIn(token, body)
        self.assertIn('"MUTE"', MODULE)
        self.assertIn('"UNMUTE"', MODULE)
        self.assertIn('"BAN"', MODULE)

    def test_player_tools_protect_story_race_hierarchy(self):
        for token in ("AdminTools_CanEditPlayer(playerid, target)",
                      "gStoryStage[target] < STORY_DONE",
                      "GetPlayerState(target) != PLAYER_STATE_ONFOOT",
                      "skin > 311 || skin == 74", "SpawnPlayer(target)",
                      "SetPlayerSkin(target, skin)", "dimension == RACE_WORLD_ID",
                      "dimension > (world ? 65535 : 255)", "SetPlayerVirtualWorld(target, dimension)",
                      "SetPlayerInterior(target, dimension)"):
            self.assertIn(token, MODULE)
        self.assertIn("#define STORY_DONE", STORY)
        self.assertIn("#define RACE_WORLD_ID", RACE)
        for action in ("RESPAWN", "SETSKIN", "SETVW", "SETINT"):
            self.assertIn('"' + action + '"', MODULE)

    def test_time_and_clearchat_are_global_and_audited(self):
        for token in ("Admin_Require(playerid, PERM_BAN_DAY)",
                      "Admin_Require(playerid, PERM_BAN_MONTH)",
                      "gAdminClockHour = hour", "gAdminClockMinute = minute",
                      "SetPlayerTime(i, hour, minute)", 'Admin_LogPlayer(playerid, "SETTIME", detail)',
                      'Admin_LogPlayer(playerid, "CLEAR_CHAT", "global")',
                      "gAdminChatClearNext = gettime() + 30"):
            self.assertIn(token, MODULE)
        self.assertIn("SetPlayerTime(playerid, gAdminClockHour, gAdminClockMinute)", ADMIN)
        self.assertIn("gAdminClockAuto = true;", ADMIN)
        self.assertIn("gettime(gAdminClockHour, gAdminClockMinute, second)", ADMIN)
        self.assertIn("gAdminClockHour = 0;", ADMIN)
        self.assertIn('!strcmp(params, "auto", true)', MODULE)

    def test_cp1251_source_compatibility(self):
        for file in (CMDS, MODULE, HELP, ADMIN, GAME):
            self.assertEqual(file, file.encode("cp1251").decode("cp1251"))


if __name__ == "__main__":
    unittest.main()
