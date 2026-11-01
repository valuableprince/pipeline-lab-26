"""SHIP-LS-OCEAN-01: geometry and routing contracts for offshore trial.

This is a deterministic geometry test, not in-game collision certification.
"""
import math
import re
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SHIP = (ROOT / "gamemodes/includes/ship_studio.inc").read_text(encoding="utf-8")
ROUTE = (ROOT / "gamemodes/includes/ship_ocean_route.inc").read_text(encoding="utf-8")
MAIN = (ROOT / "gamemodes/signal0317.pwn").read_text(encoding="utf-8")
PORT = (ROOT / "gamemodes/includes/ship_port.inc").read_text(encoding="utf-8")

def define(name):
    value = re.search(r"^#define\s+" + name + r"\s+\(?(-?\d+(?:\.\d+)?)\)?\s*$",
                      ROUTE, re.M)
    if value is None:
        raise AssertionError(f"Missing numeric definition: {name}")
    return float(value.group(1))

def route_coords():
    # Mirrors the Pawn trigonometric expressions, including the confirmed LS
    # saved berth yaw (0.9), without changing live ship objects.
    x0, y0, a = 2682.0, -2273.0, 0.9
    tx = define("SHIP_OCEAN_TURN_X")
    r1 = define("SHIP_OCEAN_TURN_RADIUS")
    xline = define("SHIP_OCEAN_EAST_LANE_X")
    yturn = define("SHIP_OCEAN_SOUTH_TURN_Y")
    yline = define("SHIP_OCEAN_SOUTH_LANE_Y")
    steps = int(define("SHIP_OCEAN_TURN_STEPS"))
    first_y = y0 + (tx - x0) * math.tan(math.radians(a))
    points = [(tx, first_y, a)]
    for i in range(1, steps + 1):
        t = math.radians(i * 90 / steps)
        points.append((tx + r1 * math.sin(t),
                       first_y - r1 * (1 - math.cos(t)),
                       a - i*90/steps))
    points.append((xline, -2980.0, a - 90))
    points.append((xline, yturn, a - 90))
    for i in range(1, steps + 1):
        t = math.radians(i * 90 / steps)
        points.append((xline - 300.0 * (1 - math.cos(t)),
                       yturn - 300.0 * math.sin(t),
                       a - 90 - i*90/steps))
    points.append((define("SHIP_OCEAN_WEST_LANE_X"), yline, a - 180))
    return points


class ShipOffshoreRouteContracts(unittest.TestCase):
    def test_included_and_controller_tick_first(self):
        self.assertIn('#include "ship_ocean_route.inc"', MAIN)
        self.assertIn('if (!strcmp(action, "oceanroute", true))', SHIP)
        self.assertIn("return ShipOcean_Command(playerid, arguments);", SHIP)
        self.assertIn("if (gShipOceanActive) return ShipOcean_Tick();", SHIP)
        self.assertIn("ShipOcean_Stop();\n    ShipManeuver_Stop();", SHIP)
        self.assertIn("if (gShipOceanActive)", SHIP)
        self.assertIn("gShipOceanActive || gShipOceanHold", SHIP)
        self.assertIn('!strcmp(arguments, "start", true)', ROUTE)
        self.assertIn('!strcmp(arguments, "continue", true)', ROUTE)
        self.assertIn('!strcmp(arguments, "stop", true)', ROUTE)
        self.assertIn('!strcmp(arguments, "status", true)', ROUTE)

    def test_continuous_tangent_arcs_and_coast_clearance(self):
        pts = route_coords()
        self.assertEqual(40, len(pts))
        self.assertAlmostEqual(3092.0, pts[0][0])
        self.assertAlmostEqual(3400.0, pts[18][0], places=4)
        self.assertAlmostEqual(-89.1, pts[18][2], places=4)
        self.assertEqual((3400.0, -2980.0), pts[19][:2])
        self.assertEqual((3400.0, -3100.0), pts[20][:2])
        self.assertAlmostEqual(3100.0, pts[38][0], places=4)
        self.assertAlmostEqual(-3400.0, pts[38][1], places=4)
        self.assertEqual((2400.0, -3400.0), pts[39][:2])
        self.assertAlmostEqual(-179.1, pts[39][2], places=4)
        self.assertTrue(all(2400 <= x <= 3400.001 for x, _, _ in pts))
        self.assertTrue(all(-3400.001 <= y <= -2260 for _, y, _ in pts))
        self.assertTrue(all(abs(pts[i+1][2]-pts[i][2]) <= 5.01
                            for i in range(len(pts)-1)))
        self.assertTrue(all(math.hypot(pts[i+1][0]-pts[i][0],
                                      pts[i+1][1]-pts[i][1]) < 701
                            for i in range(len(pts)-1)))

    def test_no_snap_and_pause_before_southern_boundary(self):
        self.assertIn("MoveObject(gShipRoot, x, y, z, speed, 0.0, 0.0, yaw);", ROUTE)
        self.assertNotIn("SetObjectPos(", ROUTE)
        self.assertNotIn("SetPlayerPos(", ROUTE)
        self.assertIn("ShipOcean_Pause()", ROUTE)
        self.assertIn("gShipOceanNext - 1 == SHIP_OCEAN_HOLD_STEP", ROUTE)
        self.assertIn("gShipOceanActive = false;", ROUTE)
        self.assertIn("gShipOceanHold = true;", ROUTE)
        self.assertIn("gShipOceanNext = 20;", ROUTE)
        self.assertIn("floatabs(y + 2980.0) > 2.0", ROUTE)

    def test_trial_only_and_saved_berth_unchanged(self):
        self.assertIn("gShipOceanProbe && gShipModel == SHIP_CARGO_ROOT_ID", ROUTE)
        self.assertIn("!gShipPortSaved[SHIP_PORT_LS][SHIP_PORT_BERTH]", ROUTE)
        self.assertIn("ShipEditor_Occupied()", ROUTE)
        self.assertIn("gShipPortDeparture >= 0", ROUTE)
        self.assertIn("gShipEditOwner != INVALID_PLAYER_ID", ROUTE)
        self.assertIn("!Ship_ValidCoord(x, y, z)", PORT)
        self.assertIn("if (!Ship_ValidCoord(x, y, z)) return false;", SHIP)
        self.assertIn("y >= -3500.0", ROUTE)
        self.assertIn("x <= SHIP_OCEAN_TEST_MAX_X", ROUTE)
        self.assertIn("ShipOcean_Stop();", SHIP)
        self.assertIn("gShipOceanProbe = false;", SHIP)
        self.assertNotIn("ShipPort_Save()", ROUTE)
        self.assertNotIn("Ship_Save()", ROUTE)

    def test_run_must_be_opt_in_and_never_started_by_port_depart(self):
        self.assertNotIn("ShipOcean_", PORT)
        self.assertIn("if (!strcmp(action, \"oceanroute\", true))", SHIP)
        self.assertIn("if (gShipOceanActive)", SHIP)
        self.assertIn("gShipOceanActive || gShipOceanHold", SHIP)
        self.assertIn("Ship_Stop();", ROUTE)


if __name__ == "__main__":
    unittest.main()
