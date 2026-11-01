"""Regression: /ship run must not jump to the legacy route start.

The integration is validated by CI compilation and source contracts; verify
the in-game path separately with a freshly built signal0317.amx.
"""
import unittest
from pathlib import Path

SHIP = (Path(__file__).resolve().parents[1] /
        "gamemodes/includes/ship_studio.inc").read_text(encoding="utf-8")


class ShipRunNoTeleport(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.run_source = SHIP.split('if (!strcmp(action, "run", true))', 1)[1].split(
            "    return Ship_Help(playerid);", 1)[0]

    def test_does_not_snap_ship_or_player(self):
        self.assertNotIn("SetObjectPos(", self.run_source)
        self.assertNotIn("SetObjectRot(", self.run_source)
        self.assertNotIn("SetPlayerPos(", self.run_source)

    def test_requires_proximity_to_first_legacy_waypoint(self):
        self.assertIn("GetObjectPos(gShipRoot, x, y, z);", self.run_source)
        self.assertIn("gShipPoint[0][0]", self.run_source)
        self.assertIn("gShipPoint[0][1]", self.run_source)
        self.assertIn("gShipPoint[0][2]", self.run_source)
        self.assertIn("floatsqroot(dx*dx + dy*dy + dz*dz)", self.run_source)
        self.assertIn("distance > 12.0", self.run_source)
        self.assertIn("Телепортации нет.", self.run_source)
        self.assertIn("/ship oceanprobe on", self.run_source)
        self.assertIn("/ship oceanroute start", self.run_source)

    def test_start_uses_current_position_and_next_sea_waypoint(self):
        self.assertIn("gShipNextPoint = 1;", self.run_source)
        self.assertIn("gShipRunning = true;", self.run_source)
        self.assertIn("Ship_Advance();", self.run_source)
        self.assertLess(self.run_source.index("distance > 12.0"),
                        self.run_source.index("gShipRunning = true;"))
        self.assertIn("без телепортации", self.run_source)

    def test_unsafe_state_refused_without_stopping_active_route(self):
        for condition in ("gShipRunning", "gShipPortDeparture >= 0",
                          "gShipManeuverActive", "gShipOceanHold",
                          "ShipEditor_Occupied()"):
            self.assertIn(condition, self.run_source)
        self.assertLess(self.run_source.index("gShipOceanHold"),
                        self.run_source.index("Ship_Stop();"))
        self.assertLess(self.run_source.index("ShipEditor_Occupied()"),
                        self.run_source.index("Ship_Stop();"))

    def test_help_matches_new_semantics(self):
        self.assertIn("/ship run (только рядом с первой морской точкой)", SHIP)


if __name__ == "__main__":
    unittest.main()
