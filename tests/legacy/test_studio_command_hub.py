"""Studio command hub and role-aware help contracts, without launching open.mp."""
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[1]
CORE = (ROOT / "gamemodes/includes/studio_system.inc").read_text("utf-8")
COMMANDS = (ROOT / "gamemodes/includes/commands.inc").read_text("utf-8")
UI = (ROOT / "gamemodes/includes/studio_ui.inc").read_text("utf-8")
ROUTES = (ROOT / "gamemodes/includes/npc_route_editor.inc").read_text("utf-8")

class StudioCommandHub(unittest.TestCase):
    def test_editor_entrypoints_and_legacy_shortcuts(self):
        self.assertIn("stock Studio_Command(playerid, const params[])", CORE)
        self.assertIn("if (!Studio_Require(playerid)) return 1;", CORE)
        self.assertIn("if (isnull(params)) return Studio_Open(playerid);", CORE)
        self.assertIn("return Studio_Command(playerid, params);", COMMANDS)
        self.assertIn("return NPCRoute_Command(playerid, params);", COMMANDS)
        self.assertIn("return StudioZone_Command(playerid, params);", COMMANDS)
        self.assertIn("return NPCRoute_Command(playerid, tail);", CORE)
        self.assertIn("return StudioZone_Command(playerid, tail);", CORE)

    def test_help_categories(self):
        for category in ("player", "jobs", "race", "factions", "admin", "studio"):
            self.assertIn(f'!strcmp(category, "{category}", true)', COMMANDS)
        self.assertIn('if (!Admin_IsFounder(playerid))', COMMANDS)
        self.assertIn('if (gAdminLevel[playerid] == 0)', COMMANDS)

    def test_routes_are_data_not_old_npcs(self):
        self.assertNotIn('"NPC 0\\nNPC 1', UI)
        self.assertIn("gNPCRouteSlot != slot", UI)
        self.assertIn('"Маршрут %d | %s%s\\n"', UI)
        self.assertNotIn("CityNPC_Init()", ROUTES)
        self.assertNotIn("CityNPC_Shutdown()", ROUTES)
        for subcommand in ("record", "crossing", "stop", "undo", "show", "test", "save", "apply"):
            self.assertIn(f'"{subcommand}"', ROUTES)
if __name__ == "__main__":
    unittest.main()
