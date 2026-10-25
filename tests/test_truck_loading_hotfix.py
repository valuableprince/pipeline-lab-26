"""Regression: driver sees bag-first workflow, and the database v10 invariant remains."""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]
TRUCK = (ROOT / "gamemodes/includes/cleaner_truck.inc").read_text("utf-8")
LOAD = (ROOT / "gamemodes/includes/cleaner_truck_loading.inc").read_text("utf-8")
OPS = (ROOT / "gamemodes/includes/cleaner_truck_ops.inc").read_text("utf-8")
DUMP = (ROOT / "gamemodes/includes/cleaner_container.inc").read_text("utf-8")
MIGRATION_V10 = (ROOT / "db/migrations/010_cleaner_container_editor.sql").read_text("utf-8")

class TruckLoadHotfix(unittest.TestCase):
    def test_alt_inside_vehicle_and_onfoot(self):
        self.assertIn("new PLAYER_STATE:playerState = GetPlayerState(playerid);", TRUCK)
        self.assertIn("new KEY:action = (playerState == PLAYER_STATE_DRIVER) ? KEY_FIRE : KEY_WALK;", TRUCK)
        self.assertIn("if (!(newkeys & action) || (oldkeys & action)) return 0;", TRUCK)
        self.assertIn("TruckLoad_HandleKey(playerid);", TRUCK)

    def test_bin_unloading_rejects_remaining_bags_before_animation(self):
        self.assertIn("if (gCleanerDumpOverflow[slot] > 0)", LOAD)
        self.assertIn("Сначала соберите все мешки с площадки", LOAD)
        self.assertLess(LOAD.index("Сначала соберите все мешки с площадки"),
                        LOAD.index("gTruckLoadPhase[playerid] = TRUCK_PHASE_LIFT;"))
        self.assertIn("if (!overflow && gCleanerDumpOverflow[slot] > 0) return false;", LOAD)
        self.assertIn("gCleanerDumpOverflow[slot] -= amount;", LOAD)
        self.assertIn("gTruckLoadBinUnits[playerid]", LOAD)

    def test_no_schema_v11_migration_needed(self):
        self.assertIn("CONSTRAINT chk_cc_full CHECK (overflow_units=0 OR bin_units=60)", MIGRATION_V10)
        self.assertIn("(overflow > 0 && bin != CLEANER_DUMP_CAP)", DUMP)
        self.assertNotIn("TruckLoad_CheckSchema()", OPS)
        self.assertNotIn("gTruckLoadSchemaReady", LOAD)
        self.assertFalse((ROOT / "db/migrations/011_truck_container_independent_overflow.sql").exists())

    def test_drivers_are_told_the_correct_order(self):
        self.assertIn("СНАЧАЛА мешки: /truckbag, /truckthrow. ПОТОМ /truckload.", OPS)
        self.assertIn("Порядок: сначала все мешки с площадки, затем выгрузка контейнера.", OPS)
        self.assertNotIn("контейнер, затем /truckbag", OPS)

    def test_lower_lift_and_database_diagnostics(self):
        self.assertIn("#define TRUCK_HIGH_CLEARANCE (3.0)", LOAD)
        self.assertIn("gTruckLoadError[playerid] = 2;", LOAD)
        self.assertIn("gTruckLoadError[playerid] = 3;", LOAD)
        self.assertIn("mysql_errno(gDBPrimary)", LOAD)
        self.assertIn("TruckLoad_Reset(playerid);", LOAD)

    def test_pawn_encoding(self):
        for contents in (TRUCK, LOAD, OPS, DUMP):
            self.assertEqual(contents.encode("cp1251").decode("cp1251"), contents)

if __name__ == "__main__":
    unittest.main()
