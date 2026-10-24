"""Начальный регион персонажа сохраняется между сессиями и не теряется при SQL-импорте."""
from pathlib import Path
import unittest
from tools.migrate_legacy import parse_account

ROOT = Path(__file__).resolve().parents[1]
M = (ROOT / "gamemodes/signal0317.pwn").read_text("utf-8")
D = (ROOT / "gamemodes/includes/auth_dialogs.inc").read_text("utf-8")
S = (ROOT / "gamemodes/includes/db_primary.inc").read_text("utf-8")
Q = (ROOT / "db/migrations/002_start_region.sql").read_text("utf-8")

class StartRegionContract(unittest.TestCase):
    def test_registration_precedes_hash_and_success(self):
        self.assertIn("AUTH_REGISTER_REGION", M)
        self.assertIn("Auth_ShowRegion(playerid);", M)
        self.assertIn("case DIALOG_REGION:", M)
        region_handler = M.split("case DIALOG_REGION:", 1)[1].split("case DIALOG_LOGIN:", 1)[0]
        self.assertIn("bcrypt_hash(playerid", region_handler)
        self.assertIn("gStartRegion[playerid] = listitem + REGION_FORT_CARSON;", region_handler)
        self.assertIn("dialogid != DIALOG_REGION) return 0;", M)
        for name in ("Форт-Карсон", "Лос-Сантос", "Паломино-Крик"):
            self.assertIn(name, D)

    def test_file_and_mysql_persistence(self):
        self.assertIn('ACCOUNT_FORMAT       "SIGNAL0317|5"', M)
        self.assertIn("gStartRegion[playerid] = region;", M)
        self.assertIn("gStartRegion[playerid]);", M)
        self.assertIn('start_region,row_version FROM accounts', S)
        self.assertIn("start_region=%d,mute_until=", S)
        self.assertIn("background,start_region) VALUES", S)
        self.assertIn("DEFAULT 1", Q)
        self.assertIn("BETWEEN 1 AND 3", Q)

    def test_spawn_is_determined_by_saved_region(self):
        self.assertIn("Auth_GetSpawnCoordinates(playerid, spawnX, spawnY, spawnZ, spawnA);", M)
        self.assertIn("SetSpawnInfo(playerid, NO_TEAM, 26, spawnX, spawnY, spawnZ, spawnA,", M)
        self.assertIn("SetPlayerPos(playerid, spawnX, spawnY, spawnZ);", M)
        self.assertIn("gStartRegion[playerid] = REGION_FORT_CARSON;", M)

    def test_legacy_import_defaults_to_fort_carson(self):
        sample = ["SIGNAL0317|1", "$2b$12$" + "A" * 53, "1"]
        self.assertEqual(parse_account("test_user", sample)["start_region"], 1)

if __name__ == "__main__":
    unittest.main()
