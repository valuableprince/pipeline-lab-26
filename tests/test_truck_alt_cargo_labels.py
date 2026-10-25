"""Regression contracts for one-key municipal loading and vehicle cargo HUD."""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]
OPS = (ROOT / "gamemodes/includes/cleaner_truck_ops.inc").read_text("utf-8")
LOAD = (ROOT / "gamemodes/includes/cleaner_truck_loading.inc").read_text("utf-8")
PERSIST = (ROOT / "gamemodes/includes/cleaner_truck_persist.inc").read_text("utf-8")
TRUCK = (ROOT / "gamemodes/includes/cleaner_truck.inc").read_text("utf-8")
MAIN = (ROOT / "gamemodes/signal0317.pwn").read_text("utf-8")

class MunicipalAltCargoTests(unittest.TestCase):
    def test_all_steps_dispatch_through_left_alt(self):
        self.assertIn("(playerState == PLAYER_STATE_DRIVER) ? KEY_FIRE : KEY_WALK", TRUCK)
        self.assertIn("(gTruckHeldUnits[playerid] || !Cleaner_NearDepot(playerid))", TRUCK)
        self.assertIn("TruckLoad_HandleKey(playerid);", TRUCK)
        self.assertIn("if (!gTruckDockReady[playerid])", LOAD)
        self.assertIn("return Truck_ParkCommand(playerid);", LOAD)
        self.assertIn("return TruckLoad_Bin(playerid);", LOAD)
        self.assertIn("if (gTruckHeldUnits[playerid]) return TruckLoad_Throw(playerid);", LOAD)
        self.assertIn("return TruckLoad_Bag(playerid);", LOAD)
        self.assertIn("if (gTruckLoadPhase[playerid] != TRUCK_PHASE_IDLE) return 1;", LOAD)
        self.assertIn("Truck_OnKeyStateChange(playerid, newkeys, oldkeys)", MAIN)

    def test_vehicle_bound_live_cargo_label(self):
        self.assertIn("new Text3D:gTruckCargoLabel[TRUCK_BAYS];", OPS)
        self.assertIn('Create3DTextLabel(', OPS)
        self.assertIn("Attach3DTextLabelToVehicle(gTruckCargoLabel[i]", OPS)
        self.assertIn('format(title, sizeof(title), "Мусоровоз #%d\\nЗагрузка: %d/%d"', OPS)
        self.assertIn("Update3DTextLabelText(gTruckCargoLabel[bay], color, title);", OPS)
        self.assertIn("Delete3DTextLabel(gTruckCargoLabel[bay]);", OPS)
        self.assertIn("TRUCK_CARGO_CAP (200)", OPS)
        self.assertIn("gTruckCargo[bay] += amount;", PERSIST)
        self.assertIn("Truck_UpdateCargoLabel(bay);", PERSIST)
        # SQL success must precede any visible cargo credit.
        self.assertLess(PERSIST.index("if (!DBPrimary_Execute(\"COMMIT\", affected))"), PERSIST.index("gTruckCargo[bay] += amount;"))
        self.assertLess(LOAD.index("gTruckCargo[bay] += amount;"), PERSIST.index("Truck_UpdateCargoLabel(bay);"))

    def test_shorter_lift_and_rear_position(self):
        self.assertIn("TRUCK_HIGH_CLEARANCE (3.0)", LOAD)
        self.assertIn("TRUCK_REAR_LOAD_OFFSET (4.5)", LOAD)
        self.assertIn("x += floatsin(angle, degrees) * TRUCK_REAR_LOAD_OFFSET;", LOAD)
        self.assertIn("y -= floatcos(angle, degrees) * TRUCK_REAR_LOAD_OFFSET;", LOAD)
        self.assertIn("gTruckLoadHighZ[playerid]", LOAD)
        self.assertIn("TRUCK_PHASE_LOWER", LOAD)

    def test_bag_first_and_server_safety_unchanged(self):
        self.assertIn("if (gCleanerDumpOverflow[slot] > 0)", LOAD)
        self.assertIn("if (!overflow && gCleanerDumpOverflow[slot] > 0) return false;", LOAD)
        self.assertIn("TruckPersist_AddCargo(playerid, slot, overflow, amount)", LOAD)
        self.assertNotIn("Economy_Credit(", LOAD)

    def test_pawn_encoding(self):
        for src in (OPS, LOAD, TRUCK):
            self.assertEqual(src.encode("cp1251").decode("cp1251"), src)

if __name__ == "__main__":
    unittest.main()
