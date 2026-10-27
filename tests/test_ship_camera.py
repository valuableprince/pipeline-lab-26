"""One-command ship camera: right-side TextDraw UI, safe observer lifecycle."""
from pathlib import Path
import unittest
ROOT=Path(__file__).resolve().parents[1]
C=(ROOT/"gamemodes/includes/ship_camera.inc").read_text(encoding="utf-8")
M=(ROOT/"gamemodes/signal0317.pwn").read_text(encoding="utf-8")
CMDS=(ROOT/"gamemodes/includes/commands.inc").read_text(encoding="utf-8")
HELP=(ROOT/"gamemodes/includes/admin_help.inc").read_text(encoding="utf-8")

class ShipCameraContracts(unittest.TestCase):
    def test_single_command_and_admin_access(self):
        self.assertIn("cmd:shipcam(playerid, params[])", CMDS)
        self.assertIn("return ShipCam_Command(playerid);", CMDS)
        self.assertIn('{"shipcam", "/shipcam",', HELP)
        self.assertIn("Admin_Require(playerid, PERM_MODERATE)", C)
        self.assertIn("gAdminDuty[playerid]", C)
        self.assertIn("gShipEditActive[playerid]", C)
        self.assertIn("gSortOpen[playerid]", C)
        self.assertIn("GetPlayerState(playerid) != PLAYER_STATE_ONFOOT", C)

    def test_every_camera_mode_uses_one_right_panel(self):
        for name in ("SHIPCAM_TOP", "SHIPCAM_REAR", "SHIPCAM_FRONT",
                     "SHIPCAM_PORT", "SHIPCAM_STARBOARD", "SHIPCAM_ANGLE",
                     "SHIPCAM_ORBIT", "SHIPCAM_ROTATE_LEFT", "SHIPCAM_ROTATE_RIGHT",
                     "SHIPCAM_ZOOM_IN", "SHIPCAM_ZOOM_OUT", "SHIPCAM_EXIT"):
            self.assertIn(name, C)
        self.assertIn("CreatePlayerTextDraw(playerid, 502.0, y, title)", C)
        self.assertIn("PlayerTextDrawSetSelectable(playerid, td, true)", C)
        self.assertIn("SelectTextDraw(playerid,", C)
        self.assertIn("SetPlayerCameraPos(playerid,", C)
        self.assertIn("SetPlayerCameraLookAt(playerid,", C)
        self.assertIn("GetObjectPos(gShipRoot", C)
        self.assertIn("GetObjectRot(gShipRoot", C)
        self.assertNotIn("MoveObject(gShipRoot", C)
        self.assertNotIn("SetObjectPos(gShipRoot", C)

    def test_follow_streaming_and_restore(self):
        self.assertIn("SHIPCAM_ANCHOR_HEIGHT", C)
        self.assertIn("SetPlayerPos(playerid, x, y, z + SHIPCAM_ANCHOR_HEIGHT)", C)
        self.assertIn("gShipCamReturn[playerid][0]", C)
        self.assertIn("SetPlayerFacingAngle(playerid, gShipCamReturn[playerid][3])", C)
        self.assertIn("SetCameraBehindPlayer(playerid)", C)
        self.assertIn("TogglePlayerControllable(playerid, !gAdminFrozen[playerid])", C)
        self.assertIn("gShipCamActive[playerid] = false;", C)
        self.assertIn("CancelSelectTextDraw(playerid)", C)
        self.assertIn("ShipCam_RemoveObserver(playerid)", C)

    def test_on_demand_timing_and_lifecycle(self):
        self.assertIn('SetTimer("ShipCam_Tick", SHIPCAM_TICK_MS, true)', C)
        self.assertIn("if (gShipCamCount == 0 && gShipCamTimer)", C)
        self.assertIn("KillTimer(gShipCamTimer)", C)
        for item in ('#include "ship_camera.inc"', "ShipCam_Init();",
                     "ShipCam_Shutdown();", "ShipCam_Reset(playerid);",
                     "ShipCam_OnClick(playerid, playertextid)",
                     "ShipCam_OnEscape(playerid, clickedid)"):
            self.assertIn(item, M)

if __name__ == "__main__":
    unittest.main()
