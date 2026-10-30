"""CRIME-02.9: physical purchased assets with solo service missions."""
from pathlib import Path
import unittest

ROOT=Path(__file__).resolve().parents[1]
SQL=(ROOT/"db/migrations/040_crime_asset_operations.sql").read_text("utf-8")
MOD=(ROOT/"gamemodes/includes/crime_assets.inc").read_text("utf-8")
UI=(ROOT/"gamemodes/includes/crime_ui.inc").read_text("utf-8")
CORE=(ROOT/"gamemodes/includes/crime_contracts.inc").read_text("utf-8")
GAME=(ROOT/"gamemodes/signal0317.pwn").read_text("utf-8")
CI=(ROOT/".github/workflows/build.yml").read_text("utf-8")


class WorldAssetOperations(unittest.TestCase):
    def test_database_structure_and_migration(self):
        for part in ("ADD COLUMN activated_at","ADD COLUMN last_serviced_at",
                     "ADD COLUMN service_count","crime_syndicate_asset_services",
                     "PRIMARY KEY (faction_id,contact_id,asset_code,service_count)",
                     "activated_at=acquired_at", "VALUES (40)"):
            self.assertIn(part, SQL)
        self.assertIn("version=43 LIMIT 1",CORE)
        self.assertIn("040_crime_asset_operations.sql",CI)
        self.assertIn("crime029_mysql_uat.py",CI)

    def test_in_world_and_server_security(self):
        for part in ("CreatePlayerObject(", "DestroyPlayerObject(", "KEY_YES",
                     "gCrimeAssetAccount[playerid] != gDBPrimaryAccountId[playerid]",
                     "gCrimeAssetToken[playerid] != gAuthToken[playerid]",
                     "gCrimeAssetFaction[playerid] != Leader_PlayerOrg(playerid)",
                     "gCrimeUIDialog[playerid] != 0",
                     "IsPlayerInAnyVehicle(playerid)",
                     "IsPlayerInRangeOfPoint(playerid, CRIME_ASSET_RADIUS",
                     "EXISTS(SELECT 1 FROM faction_members m",
                     "Crime_Rollback()", 'Crime_Exec("START TRANSACTION")',
                     'Crime_Exec("COMMIT")', "a.service_count=a.service_count+1",
                     "CRIME_ASSET_COOLDOWN (1800)", "crime_syndicate_asset_services"):
            self.assertIn(part,MOD)
        self.assertNotIn("OnPlayerUpdate(",MOD)
        self.assertNotIn("SetPlayerCheckpoint(",MOD)
        self.assertIn('#include "crime_assets.inc"',GAME)
        self.assertIn("CrimeAssets_OnKeys(playerid, newkeys, oldkeys)",GAME)
        self.assertEqual(GAME.count("CrimeAssets_Reset(playerid);"),2)

    def test_activation_effect_and_menu(self):
        self.assertIn("AND a.asset_code=1 AND a.activated_at>0",CORE)
        self.assertIn("AND a.asset_code=2 AND a.activated_at>0",CORE)
        self.assertIn("CrimeAssets_Show(playerid, contactId, stash, network)",UI)
        self.assertIn("CrimeAssets_Status(playerid, cid, 1",UI)
        self.assertIn("CrimeAssets_Status(playerid, cid, 2",UI)
        self.assertIn("Через %d мин",UI)
        self.assertIn("Нужна активация",UI)
        self.assertNotIn("cmd:assetservice(",GAME)


if __name__=="__main__":
    unittest.main()
