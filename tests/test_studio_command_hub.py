"""Studio command hub and role-aware help contracts, without launching open.mp."""
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[1]
CORE = (ROOT / "gamemodes/includes/studio_system.inc").read_text("utf-8")
COMMANDS = (ROOT / "gamemodes/includes/commands.inc").read_text("utf-8")
UI = (ROOT / "gamemodes/includes/studio_ui.inc").read_text("utf-8")
MAIN = (ROOT / "gamemodes/signal0317.pwn").read_text("utf-8")

class StudioCommandHub(unittest.TestCase):
    def test_editor_entrypoints_and_legacy_shortcuts(self):
        self.assertIn("stock Studio_Command(playerid, const params[])", CORE)
        self.assertIn("if (!Studio_Require(playerid)) return 1;", CORE)
        self.assertIn("if (isnull(params)) return Studio_Open(playerid);", CORE)
        self.assertIn("return Studio_Command(playerid, params);", COMMANDS)
        self.assertIn("return StudioZone_Command(playerid, params);", COMMANDS)
        self.assertIn("return StudioZone_Command(playerid, tail);", CORE)

    def test_help_categories(self):
        for category in ("player", "jobs", "race", "factions", "admin", "studio"):
            self.assertIn(f'!strcmp(category, "{category}", true)', COMMANDS)
        self.assertIn('if (!Admin_IsFounder(playerid))', COMMANDS)
        self.assertIn('if (gAdminLevel[playerid] == 0)', COMMANDS)

    def test_current_menu_routes_zones_and_containers(self):
        self.assertIn("Зоны / контрольные точки\\nМусорные контейнеры\\nСостояние Studio", CORE)
        self.assertIn('!strcmp(action, "containers", true)', CORE)
        self.assertIn("return CleanerEditor_List(playerid);", CORE)
        self.assertIn("if (!Studio_Require(playerid)) return 1;", UI)
        self.assertNotIn("NPCRoute_", CORE + UI + COMMANDS)
        self.assertNotIn('"npc_route_editor.inc"', MAIN)
        self.assertNotIn("cmd:npcroute(", COMMANDS)
        self.assertNotIn("cmd:nset(", COMMANDS)

if __name__ == "__main__":
    unittest.main()
