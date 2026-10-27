"""AUTH-PROFILE-01: auth/login phase timing must be safe and additive."""
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
GAME = (ROOT / "gamemodes/signal0317.pwn").read_text(encoding="utf-8")
DB = (ROOT / "gamemodes/includes/db_primary.inc").read_text(encoding="utf-8")
PROF = (ROOT / "gamemodes/includes/auth_profiler.inc").read_text(encoding="utf-8")

class AuthProfilerContracts(unittest.TestCase):
    def test_include_and_lifecycle(self):
        self.assertIn('#include "auth_profiler.inc"', GAME)
        self.assertLess(GAME.index('#include "auth_profiler.inc"'),
                        GAME.index('#include "db_primary.inc"'))
        self.assertIn("AuthProfiler_EnsureFile();", GAME)
        self.assertIn("AuthProfiler_Reset(playerid);", GAME)
        self.assertIn("AuthProfiler_WorldStart(playerid);", GAME)
        self.assertIn("AuthProfiler_WorldEnd(playerid);", GAME)

    def test_real_async_account_db_and_bcrypt_callbacks(self):
        self.assertIn("AuthProfiler_DBStart(playerid);", DB)
        self.assertIn("AuthProfiler_DBEnd(playerid);", DB)
        self.assertIn("AuthProfiler_DBEnd(playerid, false);", DB)
        self.assertIn("AuthProfiler_BcryptStart(playerid);", GAME)
        self.assertIn("AuthProfiler_BcryptEnd(playerid, false, success);", GAME)
        self.assertIn("AuthProfiler_BcryptEnd(playerid, true, strlen(hash) == 60);", GAME)
        self.assertIn("account_sql_callback", PROF)
        self.assertIn("bcrypt_verify_callback", PROF)
        self.assertIn("bcrypt_hash_callback", PROF)
        self.assertIn("account_row_decode", DB)
        self.assertIn("GetTickCount()", PROF)

    def test_every_world_stage_profiled(self):
        for stage in (
            "camera_stop", "admin_ready", "punishment_load",
            "legacy_account_save", "leader_ready", "cleaner_ready",
            "truck_profile_load", "race_ready", "payday_ready",
            "spawn_request", "first_spawn_handler", "world_enter_sync_total"
        ):
            self.assertIn(stage, GAME + PROF)
        self.assertIn("AuthProfiler_FirstSpawnBegin(playerid);", GAME)
        self.assertIn("AuthProfiler_FirstSpawnEnd(playerid);", GAME)
        self.assertIn("gAuthProfSpawnPending[playerid] = true;", GAME)

    def test_no_sensitive_data_in_report(self):
        self.assertIn("auth_profiles", PROF)
        self.assertIn("session_token", PROF)
        self.assertIn("duration_ms,ok,tickrate", PROF)
        self.assertNotIn("gAccountHash", PROF)
        self.assertNotIn("gPasswordPending", PROF)
        self.assertNotIn("inputtext", PROF)
        self.assertNotIn("password_hash", PROF)
        self.assertNotIn("mysql_query(", PROF)
        self.assertNotIn("bcrypt_verify(", PROF)
        self.assertNotIn("Kick(", PROF)
        self.assertIn("gAuthProfDbPending[playerid] = false;", PROF)
        self.assertIn("gAuthProfHashPending[playerid] = false;", PROF)

if __name__ == "__main__":
    unittest.main()
