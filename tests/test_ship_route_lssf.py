"""Conservative static contracts for LS <-> SF cargo ship draft routes."""
from pathlib import Path
import math
import unittest

ROOT = Path(__file__).resolve().parents[1]
SOURCE = (ROOT / "gamemodes/includes/ship_studio.inc").read_text(encoding="utf-8")
DOC = (ROOT / "docs/SHIP_ROUTE_LSSF.md").read_text(encoding="utf-8")

def parse_route():
    section = SOURCE.split("new const Float:gShipPresetLsSf[][4] = {", 1)[1].split("};", 1)[0]
    points = []
    for line in section.splitlines():
        line = line.strip().strip(",").strip("{}")
        if line:
            points.append(tuple(float(piece.strip()) for piece in line.split(",")))
    return points

class ShipRouteLssfContracts(unittest.TestCase):
    def test_commands_exist_and_dont_autostart(self):
        self.assertIn('!strcmp(action, "preset", true)', SOURCE)
        self.assertIn('!strcmp(direction, "lssf", true)', SOURCE)
        self.assertIn('!strcmp(direction, "sfls", true)', SOURCE)
        self.assertIn('return Ship_Preset(playerid, true);', SOURCE)
        self.assertIn('if (gShipRunning)', SOURCE)
        block = SOURCE.split("stock Ship_Preset(playerid, bool:reverse)", 1)[1].split("stock Ship_Help(playerid)", 1)[0]
        self.assertNotIn("Ship_Advance()", block)
        self.assertNotIn("Ship_Run()", block)
        self.assertIn('gShipNextPoint = 0;', block)

    def test_points_within_map_envelope_and_distance(self):
        pts = parse_route()
        self.assertEqual(len(pts), 36)
        self.assertLessEqual(len(pts), 80)
        for x, y, z, a in pts:
            self.assertTrue(-3000 <= x <= 3000)
            self.assertTrue(-3000 <= y <= 3000)
            self.assertEqual(z, 0)
            self.assertTrue(0 <= a < 360)
        self.assertEqual(pts[0][:3], (2640.0, -2740.0, 0.0))
        self.assertEqual(pts[-1][:3], (-1090.0, 700.0, 0.0))
        length = sum(math.dist(pts[i][:2], pts[i+1][:2]) for i in range(len(pts)-1))
        self.assertTrue(13600 < length < 13800)

    def test_ls_offshore_arc_and_coastal_clearance_draft(self):
        pts = parse_route()
        # More eastern offshore sweep before turning west avoids abrupt
        # turn at the old (2720,-2890) corner near the coastline.
        self.assertEqual(pts[1][:2], (2730.0, -2790.0))
        self.assertEqual(pts[3][:2], (2825.0, -2905.0))
        self.assertEqual(pts[6][:2], (2480.0, -2985.0))
        # Southern ocean lane moves out from -2960/-2950 closer to -3000.
        for x, y, _, _ in pts:
            if -820 <= x <= 2480 and y < -2500:
                self.assertLessEqual(y, -2985.0)
        # No waypoint may exceed documented game-map grid.
        self.assertTrue(all(-3000 <= p[0] <= 3000 and -3000 <= p[1] <= 3000 for p in pts))
        self.assertIn("UNVERIFIED IN GAME", DOC)

    def test_heading_matches_segment_and_docs_warn_unverified(self):
        pts = parse_route()
        for a, b in zip(pts, pts[1:]):
            expected = (math.degrees(math.atan2(-(b[0]-a[0]), b[1]-a[1]))+360) % 360
            self.assertLess(abs((a[3]-expected+180)%360-180), 0.02)
        self.assertIn("UNVERIFIED IN GAME", DOC)
        self.assertIn("not a berth", DOC.lower())
        self.assertIn("collision", DOC.lower())

if __name__ == "__main__":
    unittest.main()
