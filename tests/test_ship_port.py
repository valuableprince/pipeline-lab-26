"""SHIP-PORT-01: no-teleport dock departure contracts."""
from pathlib import Path
import unittest
ROOT=Path(__file__).resolve().parents[1]
SHIP=(ROOT/"gamemodes/includes/ship_studio.inc").read_text(encoding="utf-8")
PORT=(ROOT/"gamemodes/includes/ship_port.inc").read_text(encoding="utf-8")
MAIN=(ROOT/"gamemodes/signal0317.pwn").read_text(encoding="utf-8")
class ShipPortContracts(unittest.TestCase):
    def test_connected_and_controlled(self):
        self.assertIn('#include "ship_port.inc"', MAIN)
        self.assertIn("ShipPort_Init();", MAIN)
        self.assertIn('!strcmp(action, "port", true)', SHIP)
        self.assertIn("ShipPort_Command(playerid, portParams)", SHIP)
        self.assertIn("Studio_Require(playerid)", PORT)
        self.assertIn("Ship_Lock(playerid)", PORT)
    def test_no_player_or_ship_snap(self):
        self.assertIn("MoveObject(gShipRoot, ex, ey,", PORT)
        self.assertIn("SHIP_PORT_SLOW_SPEED", PORT)
        self.assertIn("gShipPortDeparture = port;", PORT)
        self.assertIn("ShipPort_Tick()", SHIP)
        self.assertNotIn("SetPlayerPos(", PORT)
        self.assertNotIn("SetObjectPos(", PORT)
        self.assertNotIn("SetObjectPos(", PORT.split('public ShipPort_Command',1)[1])
        self.assertIn("gShipPortDeparture = -1;", SHIP)
    def test_explicit_geometry_and_persistence(self):
        self.assertIn('TARKENSO_PORT|1', PORT)
        self.assertIn("SHIP_PORT_FILE", PORT)
        self.assertIn('!strcmp(operation, "berth", true)', PORT)
        self.assertIn('!strcmp(operation, "exit", true)', PORT)
        self.assertIn('!strcmp(operation, "depart", true)', PORT)
        self.assertIn("SHIP_PORT_EXIT_NEAR_ROUTE", PORT)
        self.assertIn("SHIP_PORT_MAX_APPROACH", PORT)
        self.assertIn("Ship_SetRoutePreset(port == SHIP_PORT_SF)", PORT)
if __name__=="__main__":
    unittest.main()
