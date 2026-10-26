"""Assembly regression: grounded freight model offsets and safe startup policy."""
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[1]
SOURCE = (ROOT / "gamemodes/includes/ship_studio.inc").read_text(encoding="utf-8")
MAIN = (ROOT / "gamemodes/signal0317.pwn").read_text(encoding="utf-8")
COMMANDS = (ROOT / "gamemodes/includes/commands.inc").read_text(encoding="utf-8")

# Original GTA SA placements of the freighter group (source: base SFw IPL;
# corroborated against Prineside & CIT Tools object model placements).
HULL = (-2409.84, 1544.95, 7.0)
PARTS = [
    (9586, -2412.12, 1544.95, 17.05),
    (9761, -2411.39, 1544.95, 27.0781),
    (9584, -2485.08, 1544.95, 26.1953),
    (9590, -2403.51, 1544.95, 8.7188),
    (9587, -2401.14, 1544.94, 23.5938),
    (9588, -2404.21, 1544.94, 7.5859),
    (9589, -2389.40, 1544.88, 4.03),
    (9604, -2392.59, 1545.91, 24.8594),
    (9698, -2473.59, 1543.77, 29.0781),
    (9819, -2470.45, 1551.12, 33.1406),
    (9821, -2474.36, 1547.24, 24.7500),
    (9822, -2470.94, 1550.75, 32.9062),
]

class CargoFreighterTests(unittest.TestCase):
    def test_grounded_assembly_offsets_and_bounds(self):
        block = SOURCE.split("stock bool:Ship_CreateCargoAssembly()", 1)[1].split("// SHIP-ROUTE-01", 1)[0]
        rows = re.findall(
            r"Ship_AddPart\((\d+),\s*(-?\d+\.\d+),\s*(-?\d+\.\d+),\s*"
            r"(-?\d+\.\d+), 0\.0, 0\.0, 0\.0\)",
            block,
        )
        # Match the source with direct substring checks; regex also validates arity and precision.
        self.assertEqual(len(rows), len(PARTS))
        for (model, x, y, z), (got_id, dx, dy, dz) in zip(PARTS, rows):
            self.assertEqual(int(got_id), model)
            for index, got in enumerate((dx, dy, dz)):
                self.assertAlmostEqual(float(got), (x - HULL[0], y - HULL[1], z - HULL[2])[index], places=3)
        self.assertIn("SHIP_CARGO_ROOT_ID  (9585)", SOURCE)
        self.assertIn("SHIP_CARGO_ROOT_Z   (7.0)", SOURCE)
        self.assertIn("SHIP_CARGO_START_X  (2640.0)", SOURCE)
        self.assertIn("SHIP_CARGO_START_Y  (-2740.0)", SOURCE)

    def test_autospawn_respects_existing_data_and_no_autostart(self):
        init = SOURCE.split("stock Ship_Init()", 1)[1].split("stock Ship_OnDisconnect", 1)[0]
        self.assertIn("if (fexist(SHIP_DATA))", init)
        self.assertIn("if (!Ship_Load())", init)
        self.assertIn("if (!Ship_CreateCargoAssembly() || !Ship_SetRoutePreset(false))", init)
        self.assertNotIn("Ship_Advance()", init)
        self.assertNotIn("gShipRunning = true", init)
        self.assertIn("if (gShipModel == SHIP_CARGO_ROOT_ID)", SOURCE)
        self.assertIn("gShipPoint[i][2] = SHIP_CARGO_ROOT_Z;", SOURCE)

    def test_longitudinal_axis_matches_navigation_bearing(self):
        self.assertIn("#define SHIP_CARGO_HEADING_OFFSET (90.0)", SOURCE)
        self.assertIn("gShipPoint[i][3] += SHIP_CARGO_HEADING_OFFSET;", SOURCE)
        self.assertIn("angle -= SHIP_CARGO_HEADING_OFFSET;", SOURCE)
        self.assertIn("SHIP_CARGO_ROOT_Z, 298.07, SHIP_CARGO_ROOT_ID", SOURCE)
        # Route's first navigation heading 208.07 => 298.07 model yaw.
        # Straight north is navigation 0 => model yaw 90.
        self.assertAlmostEqual((208.07 + 90.0) % 360.0, 298.07)
        self.assertAlmostEqual((0.0 + 90.0) % 360.0, 90.0)

    def test_founder_command_and_backward_compatibility(self):
        self.assertIn("Studio_Require(playerid)", SOURCE)
        self.assertIn('!strcmp(action, "cargo", true)', SOURCE)
        self.assertIn('cmd:ship(playerid, params[])', COMMANDS)
        self.assertIn("Ship_Init();", MAIN)
        self.assertIn("Ship_Shutdown();", MAIN)
        self.assertIn('if (Ship_Ready()) return SendClientMessage(playerid, ADMIN_ERROR,', SOURCE)

if __name__ == "__main__":
    unittest.main()
