"""Port maneuver authoring, playback and persistence contracts."""
from pathlib import Path
import unittest

ROOT=Path(__file__).resolve().parents[1]
MANEUVER=(ROOT/"gamemodes/includes/ship_port_route.inc").read_text("utf-8")
EDITOR=(ROOT/"gamemodes/includes/ship_editor.inc").read_text("utf-8")
PORT=(ROOT/"gamemodes/includes/ship_port.inc").read_text("utf-8")
SHIP=(ROOT/"gamemodes/includes/ship_studio.inc").read_text("utf-8")
MAIN=(ROOT/"gamemodes/signal0317.pwn").read_text("utf-8")

class ShipManeuverContracts(unittest.TestCase):
    def test_four_routes_independent_and_saved(self):
        self.assertIn("new gShipManeuverCount[2][2];", SHIP)
        self.assertIn("SHIP_MANEUVER_DEPART", MANEUVER)
        self.assertIn("SHIP_MANEUVER_ARRIVE", MANEUVER)
        self.assertIn("#define SHIP_MANEUVER_MAX (8)", MANEUVER)
        self.assertIn('TARKENSO_MANEUVER|1', MANEUVER)
        self.assertIn('maneuvers_01.dat', MANEUVER)
        self.assertIn('ShipManeuver_Save()', MANEUVER)
        self.assertIn('ShipManeuver_Load()', MANEUVER)
        self.assertIn('ShipManeuver_DraftLoad()', MANEUVER)
        self.assertIn('ShipManeuver_Init();', MAIN)
        self.assertIn('#include "ship_port_route.inc"', MAIN)
    def test_editor_buttons_and_draft_isolation(self):
        for word in ('SE_ROUTE_LS_OUT','SE_ROUTE_LS_IN','SE_ROUTE_SF_OUT',
                     'SE_ROUTE_SF_IN','SE_ROUTE_ADD','SE_ROUTE_UNDO',
                     'SE_ROUTE_PREVIEW','SE_ROUTE_STOP','SE_ROUTE_SAVE',
                     'SE_ROUTE_LOAD','ShipManeuver_EditorBegin(playerid)',
                     'ShipManeuver_EditorEnd(playerid)'):
            self.assertIn(word, EDITOR)
        self.assertIn('CreatePlayer3DTextLabel', MANEUVER)
        self.assertIn('DeletePlayer3DTextLabel', MANEUVER)
        self.assertIn('ShipManeuver_MarkersClear(playerid)', MANEUVER)
        self.assertIn('ShipManeuver_MarkersShow(playerid)', MANEUVER)
        self.assertIn('gShipManeuverDraftCount', MANEUVER)
        self.assertIn('ShipManeuver_DraftLoad();', MANEUVER)
        self.assertIn('ShipManeuver_EditorLoad(playerid)', EDITOR)
    def test_preview_and_collision_gate(self):
        self.assertIn('if (preview && ShipEditor_Occupied()) return false;', MANEUVER)
        self.assertIn('ShipManeuver_Validated', MANEUVER)
        self.assertIn('SHIP_MANEUVER_SEGMENT_MAX', MANEUVER)
        self.assertIn('if (gShipManeuverActive)', SHIP)
        self.assertIn('ShipManeuver_Stop();', SHIP)
        self.assertIn('MoveObject(gShipRoot, x, y, z, SHIP_MANEUVER_SPEED', MANEUVER)
        self.assertNotIn('SetPlayerPos(', MANEUVER)
        self.assertNotIn('SetObjectPos(', MANEUVER)
        self.assertIn('ShipManeuver_EditorStop(playerid)', EDITOR)
    def test_departure_and_arrival_sequencing(self):
        self.assertIn('gShipManeuverCount[port][SHIP_MANEUVER_DEPART]', PORT)
        self.assertIn('ShipManeuver_Start(port, SHIP_MANEUVER_DEPART, false)', PORT)
        self.assertIn('gShipManeuverArrivePort = port == SHIP_PORT_LS', PORT)
        self.assertIn('ShipManeuver_Start(port, 1, false)', SHIP)
        self.assertIn('gShipPortDeparture = -1;', MANEUVER)
        self.assertIn('gShipNextPoint = 0;', MANEUVER)
        self.assertIn('gShipRunning = true;', MANEUVER)

if __name__=='__main__':
    unittest.main()
