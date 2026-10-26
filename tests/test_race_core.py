"""RACE-CORE-01: контракт виртуального мира, гонок и жизненного цикла."""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]
MAIN = (ROOT / "gamemodes/signal0317.pwn").read_text(encoding="utf-8")
CORE = (ROOT / "gamemodes/includes/race_core.inc").read_text(encoding="utf-8")
COMMANDS = (ROOT / "gamemodes/includes/commands.inc").read_text(encoding="utf-8")
BUILD = (ROOT / "tools/build_windows.py").read_text(encoding="utf-8")
CI = (ROOT / ".github/workflows/build.yml").read_text(encoding="utf-8")


class RaceCoreIntegrationTest(unittest.TestCase):
    def test_separate_world(self):
        self.assertIn("#define RACE_WORLD_ID            (1800)", (ROOT / "gamemodes/includes/race_map.inc").read_text(encoding="utf-8"))
        self.assertIn("GetPlayerVirtualWorld(playerid) != 0", CORE)
        self.assertIn("SetPlayerVirtualWorld(playerid, RACE_WORLD_ID);", CORE)
        self.assertIn("SetVehicleVirtualWorld(vehicle, RACE_WORLD_ID);", CORE)
        self.assertIn("SetPlayerVirtualWorld(playerid, gRaceReturnWorld[playerid]);", CORE)
        self.assertIn("SetPlayerInterior(playerid, gRaceReturnInterior[playerid]);", CORE)

    def test_hooks(self):
        self.assertIn('#include "race_core.inc"', MAIN)
        for code in ("Race_Init();", "Race_Shutdown();", "Race_OnConnect(playerid);",
                     "Race_OnDisconnect(playerid);", "Race_OnSpawn(playerid);",
                     "Race_OnDeath(playerid)", "Race_OnCheckpoint(playerid);",
                     "Race_OnStateChange(playerid, newstate, oldstate);"):
            self.assertIn(code, MAIN)
        self.assertIn("public OnPlayerEnterRaceCheckpoint(playerid)", MAIN)
        self.assertIn("public OnPlayerStateChange(", MAIN)
        self.assertIn("Race_IsParticipant(playerid) || !gAccountSpawned[playerid]", MAIN)

    def test_registration_and_cleanup(self):
        self.assertIn("cmd:race(playerid, params[])", CORE)
        for command in ('"join"', '"leave"', '"status"'):
            self.assertIn(command, CORE)
        self.assertIn("gRaceCount >= RACE_MAX_PLAYERS", CORE)
        self.assertIn("gCleanerDuty[playerid]", CORE)
        self.assertIn("gCleanerBag[playerid] > 0", CORE)
        self.assertIn("gRaceJoined[playerid] = false;", CORE)
        self.assertIn("DestroyVehicle(gRaceCars[playerid]);", CORE)
        self.assertIn("DisablePlayerRaceCheckpoint(playerid);", CORE)
        self.assertIn("Cleaner_SyncTrashLabels(playerid);", CORE)
        self.assertIn("Race_ReconcileSession();", CORE)
        self.assertIn('SetTimer("Race_Tick", 1000, true)', CORE)
        self.assertIn("KillTimer(gRaceTimer);", CORE)

    def test_start_and_finish(self):
        self.assertIn("#define RACE_MIN_PLAYERS       (1)", CORE)
        self.assertIn("gRaceCount < RACE_MIN_PLAYERS", CORE)
        self.assertIn("gRaceStartAt = gettime() + RACE_COUNTDOWN", CORE)
        self.assertIn("gRaceMode = RACE_IN_PROGRESS;", CORE)
        self.assertIn("TogglePlayerControllable(playerid, false);", CORE)
        self.assertIn("TogglePlayerControllable(playerid, true);", CORE)
        self.assertIn("CP_TYPE_GROUND_NORMAL", CORE)
        self.assertIn("CP_TYPE_GROUND_FINISH", CORE)
        self.assertIn("GetPlayerVehicleID(playerid) != gRaceCars[playerid]", CORE)
        self.assertIn("dx * dx + dy * dy + dz * dz > RACE_VERIFY_SQ", CORE)
        self.assertIn("gRaceProgress[playerid]++;", CORE)
        self.assertIn("gRacePlaces++;", CORE)
        self.assertIn("gettime() >= gRaceFinishAt", CORE)
        self.assertIn("Race_Remove(playerid);", CORE)

    def test_no_economy_side_effects(self):
        self.assertNotIn("Economy_Credit(", CORE)
        self.assertNotIn("Economy_Debit(", CORE)
        self.assertIn("SetPlayerHealth(playerid, gRaceReturnHealth[playerid]);", CORE)
        self.assertIn("SetPlayerArmour(playerid, gRaceReturnArmour[playerid]);", CORE)
        self.assertIn("gRaceRespawnRestore[playerid] = true;", CORE)
        self.assertIn("Race_IsParticipant(playerid)", COMMANDS)

    def test_build_and_encoding(self):
        self.assertIn('"race_core.inc"', BUILD)
        self.assertIn('"race_core.inc"', CI)
        CORE.encode("cp1251")
        self.assertIn("/race join", COMMANDS)


if __name__ == "__main__":
    unittest.main()
