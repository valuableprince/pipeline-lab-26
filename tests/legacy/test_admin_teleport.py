"""ADMIN-TELEPORT-01: map waypoint + coordinate teleport security contracts.

Static checks do not replace a live NickNinth/open.mp client UAT.
"""
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[1]
TELEPORT = (ROOT / "gamemodes/includes/admin_teleport.inc").read_text("utf-8")
GAME = (ROOT / "gamemodes/signal0317.pwn").read_text("utf-8")
COMMANDS = (ROOT / "gamemodes/includes/commands.inc").read_text("utf-8")
ADMIN = (ROOT / "gamemodes/includes/admin_system.inc").read_text("utf-8")
UI = (ROOT / "gamemodes/includes/admin_ui.inc").read_text("utf-8")
CI = (ROOT / ".github/workflows/pawn-build.yml").read_text("utf-8")
WINDOWS = (ROOT / "tools/build_windows.py").read_text("utf-8")

class AdminTeleportContracts(unittest.TestCase):
    def test_map_callback_and_confirmation_route(self):
        self.assertRegex(
            GAME, r"public OnPlayerClickMap\(playerid, Float:fX, Float:fY, Float:fZ\)\s*\{\s*return AdminTP_OnClickMap\(playerid, fX, fY, fZ\);")
        self.assertIn("stock AdminTP_OnClickMap(playerid, Float:x, Float:y, Float:z)", TELEPORT)
        self.assertIn("AdminTP_ShowConfirmation(playerid)", TELEPORT)
        self.assertIn("ShowPlayerDialog(playerid, ADMIN_TP_DLG_CONFIRM, DIALOG_STYLE_MSGBOX", TELEPORT)
        self.assertIn('"Телепорт", "Отмена"', TELEPORT)
        self.assertIn("ADMIN_TP_DLG_HEIGHT, DIALOG_STYLE_INPUT", TELEPORT)
        self.assertIn("floatabs(z) < 0.01", TELEPORT)
        self.assertNotIn("SetPlayerPosFindZ(playerid,", TELEPORT)
        self.assertIn("AdminTP_OnDialog(playerid, dialogid, response, inputtext)", GAME)

    def test_command_and_permissions(self):
        self.assertIn("cmd:tpcor(playerid, params[])", COMMANDS)
        self.assertIn("return AdminTP_Coordinates(playerid, params);", COMMANDS)
        self.assertIn('sscanf(params, "fff", x, y, z)', TELEPORT)
        self.assertIn("AdminTP_TokenCount(params, 3)", TELEPORT)
        self.assertIn("Admin_Require(playerid, PERM_REPORT)", TELEPORT)
        self.assertIn("Race_IsParticipant(playerid)", TELEPORT)
        self.assertIn("TogglePlayerSpectating(playerid, false)", ADMIN)
        self.assertIn("playerState != PLAYER_STATE_ONFOOT && playerState != PLAYER_STATE_DRIVER", TELEPORT)
        self.assertIn("GetPlayerVehicleID(other) == vehicleid", TELEPORT)
        self.assertIn("Admin_LogPlayer(playerid, action, detail)", TELEPORT)
        self.assertRegex(TELEPORT, r"if \(!Admin_LogPlayer\(playerid, action, detail\)\)\s*return")
        self.assertIn("TP_COORDS", TELEPORT)
        self.assertIn("TP_MAP", TELEPORT)
        self.assertIn("Использование: /tpcor X Y Z", TELEPORT)
        self.assertIn("/tpcor X Y Z", COMMANDS)
        self.assertIn("SetPlayerInterior(playerid, 0)", TELEPORT)
        self.assertIn("SetPlayerVirtualWorld(playerid, 0)", TELEPORT)
        self.assertIn("SetVehiclePos(vehicleid, x, y, z)", TELEPORT)

    def test_pending_state_revoked_on_disconnect_and_aduty(self):
        self.assertIn("gAdminTPAuthToken[playerid] != gAuthToken[playerid]", TELEPORT)
        self.assertIn("gAdminTPStage[playerid] != ADMIN_TP_STAGE_HEIGHT", TELEPORT)
        self.assertIn("gAdminTPStage[playerid] != ADMIN_TP_STAGE_CONFIRM", TELEPORT)
        self.assertIn("AdminTP_Reset(playerid);", GAME)
        self.assertIn("AdminTP_Reset(playerid);", ADMIN)
        self.assertIn("gAdminDuty[playerid] = !gAdminDuty[playerid];", ADMIN)
        self.assertIn("gAdminTPPending[playerid] = false;", TELEPORT)
        self.assertIn("if (!response)", TELEPORT)
        self.assertIn("AdminTP_Valid(x, y, z)", TELEPORT)

    def test_admin_ui_and_build_staging(self):
        self.assertIn("Телепортация (карта / координаты)", UI)
        self.assertIn("case 6: return AdminTP_Info(playerid);", UI)
        self.assertIn("ADMIN_TP_DLG_CONFIRM  (6516)", TELEPORT)
        self.assertIn("ADMIN_TP_DLG_HEIGHT   (6517)", TELEPORT)
        self.assertIn("ADMIN_TP_DLG_INFO     (6518)", TELEPORT)
        self.assertIn('#include "admin_teleport.inc"', GAME)
        for source in (CI, WINDOWS):
            self.assertIn('"admin_teleport.inc"', source)
        self.assertNotIn("TextDraw", TELEPORT)

if __name__ == "__main__":
    unittest.main()
