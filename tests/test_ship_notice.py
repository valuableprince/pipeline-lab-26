"""SHIP-NOTICE-01: announcement is tied to a successful manual departure."""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]
NOTICE = (ROOT / "gamemodes/includes/ship_notice.inc").read_text(encoding="utf-8")
PORT = (ROOT / "gamemodes/includes/ship_port.inc").read_text(encoding="utf-8")
SHIP = (ROOT / "gamemodes/includes/ship_studio.inc").read_text(encoding="utf-8")
MAIN = (ROOT / "gamemodes/signal0317.pwn").read_text(encoding="utf-8")


class ShipNoticeContracts(unittest.TestCase):
    def test_connected_to_both_real_departure_paths(self):
        self.assertIn('#include "ship_notice.inc"', MAIN)
        self.assertIn("forward ShipNotice_Depart(port);", SHIP)
        self.assertEqual(PORT.count("ShipNotice_Depart(port);"), 2)
        self.assertIn("if (!ShipManeuver_Start(port, SHIP_MANEUVER_DEPART, false))", PORT)
        self.assertIn("ShipNotice_Depart(port);", PORT.split("if (gShipManeuverCount[port][SHIP_MANEUVER_DEPART] > 0)", 1)[1])
        self.assertIn("ShipNotice_Depart(port);", PORT.split("MoveObject(gShipRoot, ex, ey,", 1)[1])
        self.assertNotIn("ShipNotice_Depart(", SHIP.split('if (!strcmp(action, "run", true))', 1)[1])
        self.assertNotIn("ShipNotice_Depart(", (ROOT / "gamemodes/includes/ship_port_route.inc").read_text(encoding="utf-8"))

    def test_eta_is_computed_from_real_segments_and_speeds(self):
        for token in (
            "GetObjectPos(gShipRoot, x, y, z);",
            "gShipManeuverPoint[port][SHIP_MANEUVER_DEPART]",
            "SHIP_MANEUVER_SPEED",
            "SHIP_PORT_SLOW_SPEED",
            "gShipPortCoord[port][SHIP_PORT_EXIT]",
            "for (new i = 0; i < gShipPointCount; i++)",
            "gShipPoint[i][0]",
            "SHIP_CRUISE_SPEED",
            "gShipManeuverPoint[destination][SHIP_MANEUVER_ARRIVE]",
            "gShipPortCoord[destination][SHIP_PORT_BERTH]",
            "ShipManeuver_Validated(-1, port, SHIP_MANEUVER_ARRIVE, false)",
            "SHIP_PORT_EXIT_NEAR_ROUTE",
            "floatround(seconds / 60.0, floatround_ceil)",
            "gettime(hour, minute, second);",
        ):
            with self.subTest(token=token):
                self.assertIn(token, NOTICE)
        self.assertNotIn("SetTimer(", NOTICE)
        self.assertNotIn("SetObjectPos(", NOTICE)
        self.assertNotIn("MoveObject(", NOTICE)
        self.assertNotIn("SetPlayerPos(", NOTICE)

    def test_public_notification_is_relative_and_time_zone_explicit(self):
        self.assertIn("SendClientMessageToAll(SHIP_COLOR, message);", NOTICE)
        self.assertIn("прибытие через ~%d ч %d мин", NOTICE)
        self.assertIn("около %02d:%02d (время сервера)", NOTICE)
        self.assertIn("рейда у порта", NOTICE)
        self.assertIn("ShipNotice_HasBerthArrival(destination)", NOTICE)
        self.assertIn("if (!toBerth)", NOTICE)
        self.assertNotIn("CMD:", NOTICE)


if __name__ == "__main__":
    unittest.main()
