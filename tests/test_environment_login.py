"""Clock, world weather and cinematic authentication lifecycle contracts."""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]
def source(rel):
    return (ROOT / rel).read_text(encoding="utf-8")

ADMIN = source("gamemodes/includes/admin_system.inc")
TOOLS = source("gamemodes/includes/admin_tools2.inc")
COMMANDS = source("gamemodes/includes/commands.inc")
HELP = source("gamemodes/includes/admin_help.inc")
UI = source("gamemodes/includes/admin_ui.inc")
WATCH = source("gamemodes/includes/rp_time.inc")
CAMERA = source("gamemodes/includes/auth_camera.inc")
GAME = source("gamemodes/signal0317.pwn")


class EnvironmentLoginContract(unittest.TestCase):
    def test_moscow_timestamp_independent_of_host_timezone(self):
        self.assertIn("(gettime() + 3 * 3600) % (24 * 3600)", ADMIN)
        self.assertIn("gAdminClockHour = daySecond / 3600;", ADMIN)
        self.assertIn("gAdminClockMinute = (daySecond % 3600) / 60;", ADMIN)
        self.assertNotIn("gettime(gAdminClockHour, gAdminClockMinute", ADMIN)
        self.assertIn('SetTimer("Admin_ClockTick", ADMIN_CLOCK_SYNC_MS, true)', ADMIN)
        self.assertIn('Admin_ClockTick();', TOOLS)
        self.assertIn("if (Admin_Ready(i)) SetPlayerTime(i, gAdminClockHour, gAdminClockMinute);", ADMIN)

    def test_settime_has_single_hour_and_explicit_auto(self):
        self.assertIn('cmd:settime(playerid, params[])', COMMANDS)
        command = TOOLS.split("stock AdminTools2_SetTime(", 1)[1].split(
            "stock AdminTools2_ClearChat(", 1)[0]
        self.assertIn("Admin_Next(params, offset, extra, sizeof(extra))", command)
        self.assertIn("!Admin_UnsignedNumber(arg)", command)
        self.assertIn("hour > 23", command)
        self.assertIn('!strcmp(arg, "auto", true)', command)
        self.assertIn("gAdminClockMinute = 0;", command)
        self.assertIn("SetPlayerTime(i, hour, 0)", command)
        self.assertNotIn("AdminTools2_ParseTwo(", command)
        self.assertIn('/settime ЧЧ | auto', HELP)
        self.assertNotIn('/settime HH MM', HELP)

    def test_rp_watch_uses_same_authoritative_clock(self):
        self.assertIn("Admin_ClockRefresh();", WATCH)
        self.assertIn("new hour = gAdminClockHour, minute = gAdminClockMinute;", WATCH)
        self.assertNotIn("GetPlayerTime(", WATCH)
        self.assertIn("RPTime_ShowClock(playerid, hour, minute)", WATCH)
        self.assertIn("SetPlayerTime(playerid, gAdminClockHour, gAdminClockMinute)", ADMIN)

    def test_world_weather_kept_and_event_shortcuts_removed(self):
        self.assertIn("gAdminWeather = 1;", ADMIN)
        self.assertIn("SetPlayerWeather(playerid, gAdminWeather);", ADMIN)
        self.assertIn('cmd:weather(playerid, params[])', COMMANDS)
        self.assertNotIn("cmd:event(", COMMANDS)
        self.assertNotIn('if (!strcmp(command, "/event", true))', ADMIN)
        self.assertNotIn('{"event",', HELP)
        self.assertNotIn("ADMIN_DLG_EVENTS", UI)
        self.assertNotIn("Сюжетные события", UI)

    def test_authorization_letterbox_is_per_player_and_cleaned_up(self):
        self.assertIn("CreatePlayerTextDraw(playerid, 0.0, y", CAMERA)
        self.assertIn("AuthCamera_CreateBar(playerid, 0.0)", CAMERA)
        self.assertIn("AuthCamera_CreateBar(playerid, 414.0)", CAMERA)
        self.assertIn("PlayerTextDrawUseBox(playerid, td, true)", CAMERA)
        self.assertIn("PlayerTextDrawBoxColour(playerid, td, 0x000000FF)", CAMERA)
        self.assertIn("PlayerTextDrawTextSize(playerid, td, 640.0, y + 40.0)", CAMERA)
        self.assertIn("AuthCamera_ShowBars(playerid);", CAMERA)
        self.assertIn("AuthCamera_HideBars(playerid);", CAMERA)
        self.assertIn("PlayerTextDrawDestroy(playerid, gAuthCameraBarTop[playerid])", CAMERA)
        self.assertIn("PlayerTextDrawDestroy(playerid, gAuthCameraBarBottom[playerid])", CAMERA)
        self.assertIn("AuthCamera_Stop(playerid);", GAME)

    def test_auth_weather_and_time_remain_untouched(self):
        self.assertIn("SetPlayerTime(playerid, 3, 17);", CAMERA)
        self.assertIn("SetPlayerWeather(playerid, 9);", CAMERA)
        self.assertIn("AUTH_READY", CAMERA)
        self.assertIn("gAuthToken[playerid] != token", CAMERA)
        self.assertIn("Admin_ApplyEvent(playerid);", GAME)

    def test_pawn_cp1251_compatibility(self):
        for name, data in (
            ("admin", ADMIN), ("tools", TOOLS), ("commands", COMMANDS),
            ("help", HELP), ("ui", UI), ("watch", WATCH), ("camera", CAMERA)
        ):
            with self.subTest(name=name):
                data.encode("cp1251", errors="strict")


if __name__ == "__main__":
    unittest.main()
