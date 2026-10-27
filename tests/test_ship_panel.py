"""Unified ship control UI: navigation, authorization, safety, and compatibility."""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]
PANEL = (ROOT / "gamemodes/includes/ship_panel.inc").read_text("utf-8")
SHIP = (ROOT / "gamemodes/includes/ship_studio.inc").read_text("utf-8")
MAIN = (ROOT / "gamemodes/signal0317.pwn").read_text("utf-8")
CAM = (ROOT / "gamemodes/includes/ship_camera.inc").read_text("utf-8")
COMMANDS = (ROOT / "gamemodes/includes/commands.inc").read_text("utf-8")


class ShipPanelContracts(unittest.TestCase):
    def test_one_entry_point_and_legacy_compatibility(self):
        self.assertIn('cmd:ship(playerid, params[])', COMMANDS)
        self.assertIn("return Ship_Command(playerid, params);", COMMANDS)
        self.assertIn("if (isnull(params)) return ShipPanel_Open(playerid);", SHIP)
        self.assertIn('if (!strcmp(action, "help", true)) return Ship_Help(playerid);', SHIP)
        self.assertIn('if (!strcmp(action, "stop", true))', SHIP)
        self.assertIn('cmd:shipcam(playerid, params[])', COMMANDS)
        self.assertIn('ShipCam_Command(playerid)', PANEL)
        self.assertIn('ShipEditor_Command(playerid, "")', PANEL)

    def test_unique_dialogs_and_navigation(self):
        for id_ in range(6340, 6346):
            self.assertIn(f'({id_})', PANEL)
        self.assertIn('#include "ship_panel.inc"', MAIN)
        self.assertIn("ShipPanel_OnDialog(playerid, dialogid, response, listitem)", MAIN)
        for name in ("ShipPanel_Open", "ShipPanel_Info", "ShipPanel_Voyage",
                     "ShipPanel_Route", "ShipPanel_Tools", "ShipPanel_OpenEditor"):
            self.assertIn(f"stock {name}(playerid)", PANEL)
        for section in ("Общая информация", "Управление рейсами", "Визуальный редактор",
                        "Портовые маршруты", "Наблюдение за кораблём", "Дополнительные инструменты"):
            self.assertIn(section, PANEL)
        self.assertIn("if (listitem < 0 || listitem > 5) return 1;", PANEL)
        self.assertIn("if (listitem < 0 || listitem > 3) return 1;", PANEL)
        self.assertIn("if (listitem < 0 || listitem > 6) return 1;", PANEL)
        self.assertIn("if (!response)", PANEL)

    def test_access_control_for_all_callbacks(self):
        self.assertIn("Admin_Require(playerid, PERM_MODERATE)", PANEL)
        self.assertIn("Admin_Allowed(playerid, PERM_MODERATE)", PANEL)
        self.assertIn("gAdminDuty[playerid]", PANEL)
        self.assertIn("if (!ShipPanel_Allowed(playerid)) return 1;", PANEL)
        self.assertIn("if (!Studio_Require(playerid)) return 1;", PANEL)
        self.assertIn("return Ship_Command(playerid, command);", PANEL)
        self.assertIn("gShipEditActive[playerid]", PANEL)
        self.assertIn("gShipCamActive[playerid]", PANEL)
        self.assertIn("ShipPanel_Reset(playerid);", MAIN)

    def test_safe_controls_no_unintentional_teleport(self):
        for command in ("port ls depart", "port sf depart", "stop",
                        "preset lssf", "preset sfls", "cargo", "save",
                        "load", "export", "release", "clear"):
            self.assertIn(f'"{command}"', PANEL)
        self.assertNotIn('"run"', PANEL)
        self.assertNotIn("SetObjectPos(", PANEL)
        self.assertNotIn("SetPlayerPos(", PANEL)
        self.assertIn("gShipPanelClearPending[playerid]", PANEL)
        self.assertIn("if (ShipEditor_Occupied())", PANEL)
        self.assertIn("if (!gShipPanelClearPending[playerid]) return 1;", PANEL)

    def test_camera_keeps_own_click_panel(self):
        self.assertIn("ShipCam_OnClick(playerid, playertextid)", MAIN)
        self.assertIn("ShipCam_OnEscape(playerid, clickedid)", MAIN)
        self.assertIn("SHIPCAM_EXIT", CAM)


if __name__ == "__main__":
    unittest.main()
