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
        cls.run = SHIP.split('if (!strcmp(action, "run", true))', 1)[1].split(
            "    return Ship_Help(playerid);", 1)[0]

    def test_does_not_snap_ship_or_player(self):
        self.assertNotIn("SetObjectPos(", self.run)
        self.assertNotIn("SetObjectRot(", self.run)
        self.assertNotIn("SetPlayerPos(", self.run)

    def test_requires_proximity_to_first_legacy_waypoint(self):
        self.assertIn("GetObjectPos(gShipRoot, x, y, z);", self.run)
        self.assertIn("gShipPoint[0][0]", self.run)
        self.assertIn("gShipPoint[0][1]", self.run)
        self.assertIn("gShipPoint[0][2]", self.run)
        self.assertIn("floatsqroot(dx*dx + dy*dy + dz*dz)", self.run)
        self.assertIn("distance > 12.0", self.run)
        self.assertIn("Телепортации нет.", self.run)
        self.assertIn("/ship oceanprobe on", self.run)
        self.assertIn("/ship oceanroute start", self.run)

    def test_start_uses_current_position_and_next_sea_waypoint(self):
        self.assertIn("gShipNextPoint = 1;", self.run)
        self.assertIn("gShipRunning = true;", self.run)
        self.assertIn("Ship_Advance();", self.run)
        self.assertLess(self.run.index("distance > 12.0"),
                        self.run.index("gShipRunning = true;"))
        self.assertIn("без телепортации", self.run)

    def test_unsafe_state_refused_without_stopping_active_route(self):
        for condition in ("gShipRunning", "gShipPortDeparture >= 0",
                          "gShipManeuverActive", "gShipOceanHold",
                          "ShipEditor_Occupied()"):
            self.assertIn(condition, self.run)
        self.assertLess(self.run.index("gShipOceanHold"),
                        self.run.index("Ship_Stop();"))
        self.assertLess(self.run.index("ShipEditor_Occupied()"),
                        self.run.index("Ship_Stop();"))

    def test_help_matches_new_semantics(self):
        self.assertIn("/ship run (только рядом с первой морской точкой)", SHIP)


if __name__ == "__main__":
    unittest.main()
