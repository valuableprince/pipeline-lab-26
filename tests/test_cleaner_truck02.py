"""JOB-TRUCK-02 safety and integration contracts; gameplay still needs UAT."""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]
MAIN = (ROOT / "gamemodes/signal0317.pwn").read_text("utf-8")
TRUCK = (ROOT / "gamemodes/includes/cleaner_truck.inc").read_text("utf-8")
OPS = (ROOT / "gamemodes/includes/cleaner_truck_ops.inc").read_text("utf-8")
COMMANDS = (ROOT / "gamemodes/includes/commands.inc").read_text("utf-8")

class TruckStageTwoTests(unittest.TestCase):
    def test_integrated_lifecycle(self):
        for symbol in ('#include "cleaner_truck_ops.inc"', "Truck_Init();",
                       "Truck_Shutdown();", "Truck_OnStateChange(playerid, newstate, oldstate);",
                       "Truck_OnVehicleDeath(vehicleid);", "Truck_Reset(playerid);"):
            self.assertIn(symbol, MAIN)
        self.assertIn("return Truck_StopShift(playerid);", TRUCK)

    def test_dedicated_vehicle_and_ownership(self):
        for symbol in ("TRUCK_VEHICLE_MODEL (408)", "TRUCK_BAYS (4)",
                       "CreateVehicle(TRUCK_VEHICLE_MODEL", "gTruckBayOwner[bay] = playerid + 1;",
                       "IsValidVehicle(gTruckVehicle[playerid])",
                       "RemovePlayerFromVehicle(playerid);", "GetPlayerVehicleID(playerid)"):
            self.assertIn(symbol, OPS)
        self.assertIn("Truck_DestroyVehicle(playerid);", OPS)
        self.assertIn("Truck_ReleaseOrder(playerid);", OPS)

    def test_unique_claims_by_immutable_container_id(self):
        self.assertIn("gTruckClaimOwner[i] = playerid + 1;", OPS)
        self.assertIn("gTruckClaimId[free] = id;", OPS)
        self.assertIn("gTruckReservedId[playerid] = id;", OPS)
        self.assertIn("Truck_ReservedByOther(id, playerid)", OPS)
        self.assertIn("gTruckClaimOwner[i] = 0;", OPS)
        self.assertIn("Truck_FindContainer(gTruckReservedId[playerid]) < 0", OPS)
        self.assertIn("Truck_ReservedByOther(gCleanerDumpId[i], playerid)", TRUCK)
        self.assertIn("TRUCK_ORDER_THRESHOLD", OPS)

    def test_reverse_dock_no_loading_yet(self):
        for symbol in ("GetVehicleZAngle(vehicleid, angle)", "GetVehicleVelocity(vehicleid, vx, vy, vz)",
                       "GetVehicleVirtualWorld(vehicleid) != 0",
                       "GetPlayerState(playerid) != PLAYER_STATE_DRIVER",
                       "backDot < TRUCK_DOCK_REAR_COS * floatsqroot(distanceSq)",
                       "TRUCK_DOCK_MIN_SQ", "TRUCK_DOCK_MAX_SQ",
                       "TRUCK_DOCK_SPEED_SQ", "Truck_CanDock(playerid)"):
            self.assertIn(symbol, OPS)
        self.assertNotIn("CleanerDump_Stage(", OPS)
        self.assertNotIn("Economy_Credit(", OPS)
        self.assertNotIn("GivePlayerMoney(", OPS)

    def test_commands_and_ui(self):
        for command in ("truckpark", "truckcancel", "truckveh"):
            self.assertIn(f"cmd:{command}(playerid, params[])", COMMANDS)
        self.assertIn("return Truck_ClaimOrder(playerid, id);", TRUCK)
        self.assertIn("DIALOG_STYLE_TABLIST_HEADERS", TRUCK)
        self.assertIn('"Принять", "Назад"', TRUCK)
        self.assertIn("TRUCK_MAP_SLOT", OPS)

    def test_cp1251(self):
        for source in (MAIN, OPS, TRUCK):
            self.assertEqual(source, source.encode("cp1251").decode("cp1251"))

if __name__ == "__main__":
    unittest.main()
