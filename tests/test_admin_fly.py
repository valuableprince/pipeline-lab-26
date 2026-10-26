"""Native staff-flight feature contracts."""
from pathlib import Path
import unittest
R=Path(__file__).resolve().parents[1]
FLY=(R/"gamemodes/includes/admin_fly.inc").read_text(encoding="utf-8")
MAIN=(R/"gamemodes/signal0317.pwn").read_text(encoding="utf-8")
CMD=(R/"gamemodes/includes/commands.inc").read_text(encoding="utf-8")
ADMIN=(R/"gamemodes/includes/admin_system.inc").read_text(encoding="utf-8")
class FlightContracts(unittest.TestCase):
    def test_wiring(self):
        for item in ('#include "admin_fly.inc"','AdminFly_Init();','AdminFly_Shutdown();','AdminFly_Reset(playerid);','AdminFly_Stop(playerid);'):
            self.assertIn(item, MAIN)
        self.assertIn('cmd:fly(playerid, params[])',CMD)
        self.assertIn('AdminFly_Stop(playerid);',ADMIN)
    def test_authorization_and_cleanup(self):
        self.assertIn('Admin_Require(playerid, PERM_MODERATE)',FLY)
        self.assertIn('GetPlayerState(playerid) != PLAYER_STATE_ONFOOT',FLY)
        self.assertIn('!gAdminDuty[playerid]',FLY)
        self.assertIn('SetPlayerGravity(playerid, gAdminFlyOldGravity[playerid]);',FLY)
        self.assertIn('GetPlayerGravity(playerid)',FLY)
        self.assertIn('KillTimer(gAdminFlyTimer)',FLY)
    def test_no_third_party_client_or_hot_callback(self):
        self.assertIn('GetPlayerKeys(playerid, keys, updown, leftright)',FLY)
        self.assertIn('GetPlayerCameraFrontVector(playerid, fx, fy, fz)',FLY)
        self.assertIn('SetPlayerVelocity(playerid, vx, vy, verticalMove * ADMIN_FLY_LIFT)',FLY)
        self.assertNotIn('public OnPlayerUpdate(',FLY)
if __name__=='__main__': unittest.main()
