"""Admin /veh contracts: permissions, input bounds, cleanup and logging."""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]
VEH = (ROOT / "gamemodes/includes/admin_vehicle.inc").read_text("utf-8")
MAIN = (ROOT / "gamemodes/signal0317.pwn").read_text("utf-8")
CMDS = (ROOT / "gamemodes/includes/commands.inc").read_text("utf-8")


class AdminVehicleContract(unittest.TestCase):
    def test_admin_only_and_strict_parameters(self):
        self.assertIn("cmd:veh(playerid, params[])", CMDS)
        self.assertIn("return AdminVeh_Command(playerid, params);", CMDS)
        self.assertIn("Admin_Require(playerid, PERM_REPORT)", VEH)
        self.assertIn('sscanf(params, "iI(-1)I(-1)"', VEH)
        for term in ("modelid < 400", "modelid > 611",
                     "color1 < -1", "color1 > 255",
                     "color2 < -1", "color2 > 255",
                     "PLAYER_STATE_ONFOOT", "Race_IsParticipant(playerid)"):
            self.assertIn(term, VEH)

    def test_temp_vehicle_lifecycle(self):
        self.assertIn("new gAdminVehVehicle[MAX_PLAYERS]", VEH)
        self.assertIn("AdminVeh_Destroy(playerid);", VEH)
        self.assertIn("AdminVeh_Reset(playerid);", MAIN)
        self.assertIn("AdminVeh_Destroy(playerid);", MAIN)
        self.assertIn("AdminVeh_Shutdown();", MAIN)
        self.assertIn("AdminVeh_OnVehicleDeath(vehicleid);", MAIN)
        self.assertIn("IsValidVehicle(vehicleid)", VEH)
        self.assertIn("DestroyVehicle(vehicleid)", VEH)
        self.assertIn("CreateVehicle(modelid", VEH)
        self.assertIn("SetVehicleVirtualWorld(vehicleid, world)", VEH)
        self.assertIn("LinkVehicleToInterior(vehicleid, interior)", VEH)
        self.assertIn("PutPlayerInVehicle(playerid, vehicleid, 0)", VEH)
        self.assertNotIn("mysql_", VEH)

    def test_audit_first_and_no_unlimited_spawn(self):
        self.assertIn('Admin_LogPlayer(playerid, "VEH_CREATE", detail)', VEH)
        self.assertIn('Admin_LogPlayer(playerid, "VEH_DELETE", detail)', VEH)
        self.assertLess(VEH.index('Admin_LogPlayer(playerid, "VEH_CREATE", detail)'),
                        VEH.index("CreateVehicle(modelid"))
        self.assertLess(VEH.index("CreateVehicle(modelid"),
                        VEH.rindex("AdminVeh_Destroy(playerid);"))
        self.assertIn("У вас нет созданного", VEH)
        self.assertIn("/veh del", VEH)


if __name__ == "__main__":
    unittest.main()
