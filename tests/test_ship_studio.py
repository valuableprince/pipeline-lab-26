"""Contract checks for the developer-only moving ship prototype."""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]
MAIN = (ROOT / "gamemodes/signal0317.pwn").read_text(encoding="utf-8")
COMMANDS = (ROOT / "gamemodes/includes/commands.inc").read_text(encoding="utf-8")
SHIP = (ROOT / "gamemodes/includes/ship_studio.inc").read_text(encoding="utf-8")


class ShipStudioStage1(unittest.TestCase):
    def test_lifecycle_and_command(self):
        self.assertIn('#include "ship_studio.inc"', MAIN)
        self.assertIn("Ship_Init();", MAIN)
        self.assertIn("Ship_Shutdown();", MAIN)
        self.assertIn("Ship_OnDisconnect(playerid);", MAIN)
        self.assertIn("cmd:ship(playerid, params[])", COMMANDS)
        self.assertIn("return Ship_Command(playerid, params);", COMMANDS)

    def test_object_assembly_and_admin_gate(self):
        self.assertIn("Studio_Require(playerid)", SHIP)
        self.assertIn("AttachObjectToObject(id, gShipRoot", SHIP)
        self.assertIn("CreateObject(model,", SHIP)
        self.assertIn("SHIP_MAX_PARTS     (48)", SHIP)
        self.assertIn("Ship_Lock(playerid)", SHIP)
        self.assertIn("IsValidObject(gShipRoot)", SHIP)

    def test_route_workflow_and_cleanup(self):
        for token in ("Ship_Mark()", "Ship_Advance()", "Ship_Stop()",
                      "Ship_Tick()", "MoveObject(gShipRoot",
                      "SetObjectRot(gShipRoot", "Ship_Reset()"):
            self.assertIn(token, SHIP)
        self.assertIn("TARKENSO_SHIP|1", SHIP)
        self.assertIn("frename(temp, SHIP_DATA)", SHIP)
        self.assertIn("gCargoShipRoute[][4]", SHIP)
        self.assertIn("KillTimer(gShipTimer)", SHIP)

    def test_prototype_does_not_claim_collision_prevention(self):
        self.assertIn("collision is NOT checked", SHIP)
        self.assertNotIn("SetPlayerPos(", SHIP)


if __name__ == "__main__":
    unittest.main()
