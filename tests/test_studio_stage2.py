"""Контракты STUDIO-UI-02: меню редакторов и обратная совместимость."""

from pathlib import Path
import unittest

BASE = Path(__file__).resolve().parents[1]
MAIN = (BASE / "gamemodes/signal0317.pwn").read_text(encoding="utf-8")
UI = (BASE / "gamemodes/includes/studio_ui.inc").read_text(encoding="utf-8")
CORE = (BASE / "gamemodes/includes/studio_system.inc").read_text(encoding="utf-8")
BUILD = (BASE / "tools/build_windows.py").read_text(encoding="utf-8")
CI = (BASE / ".github/workflows/build.yml").read_text(encoding="utf-8")


class StudioStage2Contract(unittest.TestCase):
    def test_ui_integrated_in_both_builds(self):
        self.assertIn('#include "studio_ui.inc"', MAIN)
        self.assertLess(MAIN.index('#include "studio_system.inc"'),
                        MAIN.index('#include "studio_ui.inc"'))
        self.assertIn("StudioUI_OnDialog(playerid, dialogid, response, listitem, inputtext)", MAIN)
        self.assertIn("StudioUI_Reset(playerid);", MAIN)
        self.assertIn('"studio_ui.inc"', BUILD)
        self.assertIn('"gamemodes/**"', CI)

    def test_founder_and_dialog_actions(self):
        self.assertIn("if (!Studio_Require(playerid))", UI)
        self.assertIn("if (!Studio_Require(playerid)) return 1;", CORE)
        for marker in ("STUDIO_UI_ZONES", "STUDIO_UI_ZONE_ACTION", "STUDIO_UI_ZONE_RADIUS",
                       "StudioUI_ZoneBegin(playerid)", "StudioUI_ZoneOwned(playerid)",
                       "StudioZone_Show(playerid)", 'StudioZone_Command(playerid, "publish")'):
            self.assertIn(marker, UI)

    def test_checkpoint_backward_compat(self):
        self.assertIn("TARKENSO_ZONE|1", CORE)
        self.assertIn("TARKENSO_ZONE|2", CORE)
        self.assertIn('sscanf(values, "iffffiii"', CORE)
        self.assertIn('sscanf(values, "iffffii"', CORE)
        self.assertIn("gStudioZoneMode[slot] = mode;", CORE)
        self.assertIn("gStudioZoneDraftMode", CORE)
        self.assertIn("gStudioZoneMode[slot] == 1", CORE)
        self.assertNotIn("SetPlayerCheckpoint(", CORE)
        self.assertNotIn("SetPlayerCheckpoint(", UI)
        self.assertNotIn("CreateDynamicCP(", CORE)
        self.assertNotIn("CreateDynamicCP(", UI)

    def test_isolation_and_accountability(self):
        self.assertIn("gStudioZoneOwner != INVALID_PLAYER_ID && gStudioZoneOwner != playerid", UI)
        self.assertIn("StudioZone_Cancel();", UI)
        self.assertIn("Admin_LogPlayer(playerid, \"STUDIO_ZONE_PUBLISH\"", CORE)
        self.assertIn("Admin_LogPlayer(playerid, \"STUDIO_ZONE_DISABLE\"", CORE)


if __name__ == "__main__":
    unittest.main()
