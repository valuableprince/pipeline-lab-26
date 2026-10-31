"""RP-TIME-01: watch gesture, world time and gameplay safety contracts."""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]
GAME = (ROOT / "gamemodes/signal0317.pwn").read_text(encoding="utf-8")
TIME = (ROOT / "gamemodes/includes/rp_time.inc").read_text(encoding="utf-8")
COMMANDS = (ROOT / "gamemodes/includes/commands.inc").read_text(encoding="utf-8")
HELP = (ROOT / "gamemodes/includes/player_command_help.inc").read_text(encoding="utf-8")
MENU = (ROOT / "gamemodes/includes/player_menu.inc").read_text(encoding="utf-8")
DOC = (ROOT / "docs/RP_COMMANDS.md").read_text(encoding="utf-8")


class RPTimeTests(unittest.TestCase):
    def test_world_time_not_unix_timestamp(self):
        self.assertIn("GetPlayerTime(playerid, hour, minute)", TIME)
        self.assertIn('"~w~TIME: ~y~%02d:%02d"', TIME)
        self.assertIn("RPTime_ShowClock(playerid, hour, minute)", TIME)
        self.assertIn("CreatePlayerTextDraw(playerid,", TIME)
        self.assertIn("618.0, 374.0, clockText", TIME)
        self.assertIn("PlayerTextDrawAlignment(playerid, gRPTimeDraw[playerid], TEXT_DRAW_ALIGN_RIGHT)", TIME)
        self.assertIn("PlayerTextDrawShow(playerid, gRPTimeDraw[playerid])", TIME)
        self.assertNotIn("GameTextForPlayer(", TIME)
        self.assertNotIn("gettime(hour,", TIME)
        self.assertNotIn("SetPlayerTime(", TIME)

    def test_watch_gesture_label_and_local_chat(self):
        self.assertIn('"* %s посмотрел(а) на часы"', TIME)
        self.assertIn("RP_ShowActionLabel(playerid, label)", TIME)
        self.assertIn("RP_SendLocal(playerid, RP_COLOR_ACTION, label, RP_RADIUS_ACTION)", TIME)
        self.assertIn('"COP_AMBIENT", "Coplook_watch"', TIME)
        self.assertIn("RP_TIME_ANIM_MS, SYNC_ALL", TIME)
        self.assertIn("false, false, false, false", TIME)
        self.assertNotIn("ClearAnimations(", TIME)
        self.assertIn('SetTimerEx("RPTime_HideLater", RP_TIME_TEXT_MS, false,', TIME)
        self.assertIn("gRPTimeSerial[playerid] == serial", TIME)
        self.assertIn("PlayerTextDrawDestroy(playerid, gRPTimeDraw[playerid])", TIME)
        self.assertNotIn("ClearAnimations(playerid", TIME)
        self.assertNotIn("TogglePlayerControllable(", TIME)

    def test_other_animations_are_not_interrupted_and_cooldown(self):
        for piece in ("IsPlayerConnected(playerid)", "!IsPlayerNPC(playerid)",
                      "AUTH_READY", "gAccountSpawned[playerid]", "!gAccountDead[playerid]",
                      "!gAdminFrozen[playerid]", "!gAdminFly[playerid]", "TDS_Active(playerid)",
                      "Race_IsParticipant(playerid)", "gCleanerDuty[playerid]",
                      "!RPAnim_Active(playerid)", "PLAYER_STATE_ONFOOT",
                      "SPECIAL_ACTION_NONE", "GetPlayerVelocity(playerid",
                      "GetPlayerAnimationIndex(playerid)", "GetAnimationName(index",
                      "gettime() < gRPTimeNextUse[playerid]",
                      "gRPTimeNextUse[playerid] = gettime() + RP_TIME_COOLDOWN"):
            self.assertIn(piece, TIME)
        self.assertEqual(GAME.count("RPTime_Reset(playerid);"), 4)
        self.assertIn("RPTime_Init();", GAME)
        self.assertIn("PlayerText:INVALID_TEXT_DRAW", TIME)
        self.assertIn("#define RP_TIME_COOLDOWN (4)", TIME)

    def test_wired_in_command_and_help(self):
        self.assertIn('#include "rp_time.inc"', GAME)
        self.assertIn("cmd:time(playerid, params[])", COMMANDS)
        self.assertIn("return RPTime_Command(playerid);", COMMANDS)
        self.assertIn('{"time", "/time",', HELP)
        self.assertIn("/time", MENU)
        self.assertIn("Часы: /time", COMMANDS)
        self.assertIn("COP_AMBIENT / Coplook_watch", DOC)
        self.assertIn("правом нижнем углу", DOC)
        for source in (GAME, TIME, COMMANDS, HELP, MENU):
            source.encode("cp1251", errors="strict")


if __name__ == "__main__":
    unittest.main()
