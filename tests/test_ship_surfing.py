"""SHIP-SURF-01 contract tests: founder gating, no forced position edits."""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]
SURF = (ROOT / "gamemodes/includes/ship_passenger_sync.inc").read_text(encoding="utf-8")
MAIN = (ROOT / "gamemodes/signal0317.pwn").read_text(encoding="utf-8")
COMMANDS = (ROOT / "gamemodes/includes/commands.inc").read_text(encoding="utf-8")

class ShipSurfDiagnosticsContracts(unittest.TestCase):
    def test_lifecycle(self):
        for expected in (
            '#include "ship_passenger_sync.inc"',
            "ShipSurf_Init();", "ShipSurf_Shutdown();",
            "ShipSurf_Reset(playerid);",
        ):
            self.assertIn(expected, MAIN)
        self.assertIn("cmd:shipsurf(playerid, params[])", COMMANDS)
        self.assertIn("return ShipSurf_Command(playerid, params);", COMMANDS)

    def test_no_player_coercion_and_bounded_timer(self):
        self.assertIn('SetTimer("ShipSurf_Poll", SHIPSURF_INTERVAL_MS, true)', SURF)
        self.assertIn('#define SHIPSURF_INTERVAL_MS   (400)', SURF)
        self.assertIn("if (!gShipSurfWatch[playerid]) continue;", SURF)
        self.assertIn("GetPlayerSurfingObjectID(playerid)", SURF)
        self.assertIn("GetPlayerSurfingOffsets(playerid", SURF)
        self.assertIn("ShipSurf_Component(objectid)", SURF)
        self.assertIn("GetPlayerState(playerid) != PLAYER_STATE_ONFOOT", SURF)
        self.assertIn("GetPlayerVirtualWorld(playerid) != 0", SURF)
        for unsafe in (
            "SetPlayerPos(", "SetPlayerVelocity(",
            "SetPlayerFacingAngle(", "PutPlayerInVehicle(",
            "public OnPlayerUpdate(", "SetPlayerHealth(",
        ):
            self.assertNotIn(unsafe, SURF)

    def test_authorized_controls_and_player_cleanup(self):
        self.assertIn("Studio_Require(playerid)", SURF)
        for action in ("watch", "off", "status"):
            self.assertIn(f'!strcmp(action, "{action}", true)', SURF)
        self.assertIn("gShipSurfWatch[targetid] = true;", SURF)
        self.assertIn("ShipSurf_Reset(targetid);", SURF)
        self.assertIn("IsPlayerConnected(targetid)", SURF)
        self.assertIn("gShipSurfWasAttached[playerid] = attached;", SURF)
        self.assertIn("gShipSurfLost[playerid]++;", SURF)
        self.assertIn("KillTimer(gShipSurfTimer)", SURF)

if __name__ == "__main__":
    unittest.main()
