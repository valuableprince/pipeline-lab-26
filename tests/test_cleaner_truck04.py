"""JOB-TRUCK-04: crash-safe MariaDB hauling and exactly-once salary contracts."""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]
OPS = (ROOT/"gamemodes/includes/cleaner_truck_ops.inc").read_text("utf-8")
PERSIST = (ROOT/"gamemodes/includes/cleaner_truck_persist.inc").read_text("utf-8")
LOAD = (ROOT/"gamemodes/includes/cleaner_truck_loading.inc").read_text("utf-8")
GAME = (ROOT/"gamemodes/signal0317.pwn").read_text("utf-8")
COLLECTOR = (ROOT/"gamemodes/includes/cleaner_job.inc").read_text("utf-8")
CMDS = (ROOT/"gamemodes/includes/commands.inc").read_text("utf-8")
SQL = (ROOT/"db/migrations/011_municipal_truck_deliveries.sql").read_text("utf-8")

class TruckStageFour(unittest.TestCase):
    def test_required_migration_and_no_legacy_invariant_change(self):
        self.assertIn("CREATE TABLE municipal_truck_cargo", SQL)
        self.assertIn("CREATE TABLE municipal_driver_profiles", SQL)
        self.assertIn("CREATE TABLE municipal_truck_deliveries", SQL)
        self.assertIn("INSERT INTO schema_migrations (version) VALUES (11);", SQL)
        self.assertIn("INSERT INTO municipal_truck_cargo (bay_id) VALUES (0),(1);", SQL)
        self.assertNotIn("DROP CONSTRAINT chk_cc_full", SQL)
        self.assertIn("WHERE version=11 LIMIT 1", PERSIST)
        self.assertIn("if (!gTruckPersistenceReady", OPS)

    def test_loading_and_truck_storage_are_one_transaction(self):
        func = PERSIST.split("stock bool:TruckPersist_AddCargo(",1)[1].split(
            "stock bool:TruckPersist_NearDepot",1)[0]
        self.assertIn('DBPrimary_Execute("START TRANSACTION", affected)', func)
        self.assertIn("UPDATE cleaner_containers", func)
        self.assertIn("UPDATE municipal_truck_cargo", func)
        self.assertIn('DBPrimary_Execute("COMMIT", affected)', func)
        self.assertLess(func.index('DBPrimary_Execute("COMMIT", affected)'),
                        func.index("gTruckCargo[bay] += amount;"))
        self.assertIn("TruckPersist_Rollback()", func)
        self.assertIn("TruckPersist_AddCargo(playerid, slot, overflow, amount)", LOAD)
        self.assertIn("TruckPersist_Init();", OPS)
        self.assertNotIn("gTruckCargo[i] = 0;", OPS)

    def test_cash_xp_and_ledger_commit_once(self):
        func = PERSIST.split("stock TruckPersist_Unload(",1)[1]
        for key in ('START TRANSACTION', 'UPDATE municipal_truck_cargo',
                    'UPDATE accounts SET balance=balance+%d',
                    'municipal_driver_profiles', 'municipal_truck_deliveries',
                    'COMMIT', 'gDBPrimaryVersion[playerid]++',
                    'gAccountMoney[playerid] += pay;', 'gTruckDriverXP[playerid] += units;',
                    'Economy_SyncHUD(playerid);', 'Truck_UpdateCargoLabel(bay);'):
            self.assertIn(key, func)
        self.assertLess(func.index('DBPrimary_Execute("COMMIT", affected)'),
                        func.index("gAccountMoney[playerid] += pay;"))
        self.assertIn("gTruckDriverXP[playerid]", COLLECTOR)
        self.assertIn("TruckPersist_LoadProfile(playerid)", GAME)
        self.assertIn("cmd:truckunload(playerid, params[])", CMDS)
        self.assertIn("return TruckPersist_Unload(playerid);", LOAD)

    def test_cargo_account_owner_persists_until_paid(self):
        self.assertIn("gTruckCargoAccount[TRUCK_BAYS]", PERSIST)
        self.assertIn("TruckPersist_CargoAllowed(playerid, bay)", OPS)
        self.assertIn("gTruckCargoAccount[bay] == gDBPrimaryAccountId[playerid]", PERSIST)
        self.assertIn("TRUCK_CARGO_CAP (200)", OPS)
        self.assertIn("gTruckCargoRevision[bay]++", PERSIST)

    def test_recover_own_truck_and_partial_run_alt_unload(self):
        self.assertIn("for (new pass = 0; pass < 2; pass++)", OPS)
        self.assertIn("if (pass == 0 && gTruckCargo[bay] == 0) continue;", OPS)
        self.assertIn("if (gTruckCargo[bay] > 0) TruckPersist_ReturnGPS(playerid);", OPS)
        self.assertIn("TruckPersist_NearDepot(playerid)", LOAD)
        self.assertIn("(!gTruckReservedId[playerid] || !Truck_CanDock(playerid))", LOAD)
        self.assertIn("if (gTruckCargo[bay] >= TRUCK_CARGO_CAP)", LOAD)
        self.assertIn("return TruckPersist_Unload(playerid);", LOAD)

    def test_temporary_yard_and_return_navigation(self):
        self.assertIn("TRUCK_UNLOAD_RADIUS_SQ (64.0)", PERSIST)
        self.assertIn("TruckPersist_ReturnGPS(playerid);", LOAD)
        self.assertIn("return TruckPersist_Unload(playerid);", LOAD)
        self.assertIn("TRUCK_UNLOAD_MAP_SLOT", OPS)
        self.assertIn("Create3DTextLabel(", PERSIST)

    def test_pawn_encoding(self):
        for text in (OPS,PERSIST,LOAD,GAME,COLLECTOR):
            self.assertEqual(text, text.encode("cp1251").decode("cp1251"))

if __name__ == "__main__":
    unittest.main()
