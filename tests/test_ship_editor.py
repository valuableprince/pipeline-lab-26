"""SHIP-EDITOR-01: safe native top-down positioning of the assembled freighter."""
from pathlib import Path
import unittest
R=Path(__file__).resolve().parents[1]
E=(R/"gamemodes/includes/ship_editor.inc").read_text(encoding="utf-8")
S=(R/"gamemodes/includes/ship_studio.inc").read_text(encoding="utf-8")
M=(R/"gamemodes/signal0317.pwn").read_text(encoding="utf-8")
P=(R/"gamemodes/includes/ship_port.inc").read_text(encoding="utf-8")
class ShipEditorContracts(unittest.TestCase):
    def test_hooks_and_authorization(self):
        for pattern in ['#include "ship_editor.inc"', "ShipEditor_Init();",
                        "ShipEditor_Shutdown();", "ShipEditor_Reset(playerid);",
                        "ShipEditor_OnClick(playerid, playertextid)",
                        "ShipEditor_OnEscape(playerid, clickedid)"]:
            self.assertIn(pattern, M)
        self.assertIn('!strcmp(action, "edit", true)', S)
        self.assertIn("ShipEditor_Command(playerid, editParams)", S)
        self.assertIn("Studio_Require(playerid)", E)
        self.assertIn("Ship_Lock(playerid)", E)
        self.assertIn("gAdminDuty[p]", E)
        self.assertIn("Admin_IsFounder(p)", E)
        self.assertIn("GetPlayerState(playerid) != PLAYER_STATE_ONFOOT", E)
        self.assertIn("gShipRunning || gShipPortDeparture >= 0", E)
    def test_ui_mouse_camera_and_exit(self):
        for phrase in ("SetPlayerCameraPos(", "SetPlayerCameraLookAt(",
                       "CreatePlayerTextDraw(", "PlayerTextDrawSetSelectable(",
                       "SelectTextDraw(", "OnClick", "SE_NORTH", "SE_EAST",
                       "SE_ROTATE_L", "SE_ROTATE_R", "SE_ZUP", "SE_ZDOWN",
                       "SE_FINE", "SE_COARSE", "SE_ZOOM_IN", "SE_ZOOM_OUT",
                       "SE_SAVE", "SE_CANCEL", "SE_LS_BERTH", "SE_SF_EXIT",
                       "SetCameraBehindPlayer(", "CancelSelectTextDraw(",
                       "TogglePlayerControllable(playerid, !gAdminFrozen[playerid])"):
            self.assertIn(phrase, E)
    def test_safety_and_rollback(self):
        self.assertIn("ShipSurf_Component(GetPlayerSurfingObjectID(playerid))", E)
        self.assertIn("GetPlayerPos(playerid", E)
        self.assertIn("ShipEditor_Occupied()", E)
        self.assertIn("if (ShipEditor_Occupied())", E)
        self.assertIn("!ShipEditor_Occupied()", E)
        self.assertIn("SetObjectPos(gShipRoot", E)
        self.assertNotIn("SetPlayerPos(", E)
        self.assertIn("gShipEditActive[playerid] = false;", E)
        self.assertIn("ShipPort_Command(playerid, op);", E)
        self.assertIn("Ship_Save()", E)
        self.assertIn('!strcmp(operation, "berth", true)', P)
        self.assertIn("SHIPEDIT_POLL_MS", E)
        self.assertIn("KillTimer(gShipEditTimer)", E)
if __name__=="__main__": unittest.main()
