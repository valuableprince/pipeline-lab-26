"""Контрактные проверки системы ADMIN-01; не заменяют игровой UAT."""

from pathlib import Path
from build_contracts import assert_staged
import unittest

ROOT = Path(__file__).resolve().parents[1]
GAME = (ROOT / "gamemodes/signal0317.pwn").read_text(encoding="utf-8")
ADMIN = (ROOT / "gamemodes/includes/admin_system.inc").read_text(encoding="utf-8")
COMMANDS = (ROOT / "gamemodes/includes/commands.inc").read_text(encoding="utf-8")
UI = (ROOT / "gamemodes/includes/admin_ui.inc").read_text(encoding="utf-8")
DOC = (ROOT / "docs/ADMIN_SYSTEM.md").read_text(encoding="utf-8")


class AdminContractTests(unittest.TestCase):
    def test_auth_and_lifecycle_integration(self):
        for token in (
            "bcrypt_verify(", "if (!Admin_OnReady(playerid) ||",
            "Admin_Init();", "Admin_OnConnect(playerid);",
            "Admin_OnDisconnect(playerid);", "return Admin_Execute(playerid, input);",
            "Admin_UI_OnDialog(playerid, dialogid, response, listitem, inputtext)",
        ):
            self.assertIn(token, GAME + COMMANDS)
        self.assertIn('"SIGNAL0317|1"', GAME)
        self.assertIn("Auth_IsLocked(playerid)", GAME)

    def test_six_levels_and_independent_event_flag(self):
        for token in ("PERM_REPORT", "PERM_MODERATE", "PERM_BAN_DAY",
                      "PERM_BAN_MONTH", "PERM_BAN_PERMANENT",
                      "PERM_MANAGE_STAFF", "PERM_OWNER", "PERM_EVENT",
                      "gAdminEvent[playerid]", "gAdminDuty[playerid]"):
            self.assertIn(token, ADMIN)
        self.assertIn("if (level < 0 || level > 6 || Admin_IsFounderName(gAccountName[playerid]))", ADMIN)
        self.assertIn("gAdminLevel[playerid] == 6", ADMIN)
        self.assertIn("gAdminLevel[target] >= gAdminLevel[playerid]", ADMIN)
        self.assertIn("return gAdminLevel[playerid] >= gAdminLevel[targetid];", ADMIN)
        self.assertIn("new limit = gAdminLevel[playerid] == 6 ? 5 : 4;", ADMIN)

    def test_higher_rank_punishment_guard_is_shared(self):
        self.assertIn("stock bool:Admin_CanPunish(playerid, targetid)", ADMIN)
        self.assertIn("if (Admin_IsFounderName(gAccountName[targetid])) return false;", ADMIN)
        self.assertIn("return gAdminLevel[playerid] >= gAdminLevel[targetid];", ADMIN)
        self.assertIn("if (!Admin_CanPunish(playerid, targetid))", ADMIN)
        self.assertIn("if (!Admin_CanPunish(playerid, i)) continue;", UI)
        self.assertIn("if (!Admin_ValidTarget(playerid, target)) return 1;", ADMIN)

    def test_roles_bans_and_audit(self):
        for token in ("SIGNALADMIN|1", "SIGNALBAN|1", "Admin_ReadRole",
                      "Admin_WriteRole", "Admin_IsBanned", "Admin_WriteBan",
                      "fremove(path)", "Admin_ReplaceFile(path, tmp, backup)",
                      'ADMIN_DIRECTORY "/audit.log"', "Admin_LogPlayer"):
            self.assertIn(token, ADMIN)

    def test_player_staff_and_moderation_commands(self):
        for cmd in ("/report", "/admins", "/aduty", "/admin", "/a", "/reports",
                    "/accept", "/areply", "/close", "/spec", "/warn", "/mute",
                    "/kick", "/freeze", "/goto", "/gethere", "/ban", "/unban",
                    "/setadmin", "/setevent", "/event", "/weather", "/alog"):
            self.assertIn('"' + cmd + '"', ADMIN)
            self.assertIn("cmd:" + cmd.lstrip("/") + "(", COMMANDS)
        self.assertIn("ADMIN_REPORT_COOLDOWN", ADMIN)
        self.assertIn("ADMIN_REPORT_LIMIT", ADMIN)
        self.assertIn("gAdminMuteUntil[playerid] > gettime()", GAME)

    def test_dialog_and_session_protection(self):
        for token in ("ShowPlayerDialog", "ADMIN_DLG_MAIN",
                      "ADMIN_DLG_REPORTS", "ADMIN_DLG_PLAYERS",
                      "ADMIN_DLG_PLAYER_ACTIONS", "ADMIN_DLG_PLAYER_REASON",
                      "gAdminUIDialogMode[playerid]", "gAdminUIPlayerTokens",
                      "gAdminUIReportSerial", "gAdminUITargetToken"):
            self.assertIn(token, UI)
        self.assertNotIn("CreatePlayerTextDraw(", UI)
        self.assertNotIn("TextDraw", UI)
        self.assertIn("DIALOG_STYLE_LIST", UI)

    def test_build_and_bootstrap_documentation(self):
        assert_staged(self, "admin_system.inc", "admin_ui.inc")
        for token in ("Ручная приёмка", "SIGNALADMIN|1", "Уровень", "/setadmin",
                      "bcrypt", "обращений", "Windows"):
            self.assertIn(token, DOC)


if __name__ == "__main__":
    unittest.main()
