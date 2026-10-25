"""UAT regression: truck ALT, independently unloadable bins and reduced lift."""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]
TRUCK = (ROOT / "gamemodes/includes/cleaner_truck.inc").read_text("utf-8")
LOAD = (ROOT / "gamemodes/includes/cleaner_truck_loading.inc").read_text("utf-8")
OPS = (ROOT / "gamemodes/includes/cleaner_truck_ops.inc").read_text("utf-8")
DUMP = (ROOT / "gamemodes/includes/cleaner_container.inc").read_text("utf-8")
MIGRATION = (ROOT / "db/migrations/011_truck_container_independent_overflow.sql").read_text("utf-8")

class TruckLoadHotfix(unittest.TestCase):
    def test_in_vehicle_left_alt_and_onfoot_alt_actions(self):
        self.assertIn("new KEY:action = (state == PLAYER_STATE_DRIVER) ? KEY_FIRE : KEY_WALK;", TRUCK)
        self.assertIn("if (!(newkeys & action) || (oldkeys & action)) return 0;", TRUCK)
        self.assertIn("TruckLoad_HandleKey(playerid);", TRUCK)
        self.assertIn("if (state != PLAYER_STATE_ONFOOT) return 0;", TRUCK)

    def test_migration_preserves_bag_inventory_after_emptying_bin(self):
        self.assertIn("ALTER TABLE cleaner_containers DROP CONSTRAINT chk_cc_full;", MIGRATION)
        self.assertIn("INSERT INTO schema_migrations (version) VALUES (11);", MIGRATION)
        self.assertNotIn("(overflow > 0 && bin != CLEANER_DUMP_CAP)", DUMP)
        self.assertIn("overflow < 0 || overflow > CLEANER_DUMP_OVERFLOW_CAP", DUMP)
        self.assertIn("bin < 0 || bin > CLEANER_DUMP_CAP", DUMP)
        self.assertIn("SET bin_units=%d WHERE container_id=%d", LOAD)
        self.assertIn("AND overflow_units=%d AND enabled=1", LOAD)

    def test_missing_schema_fails_before_animation(self):
        self.assertIn("TruckLoad_CheckSchema();", OPS)
        self.assertIn("WHERE version=11 LIMIT 1", LOAD)
        self.assertIn("if (!gTruckLoadSchemaReady)", LOAD)
        self.assertIn("миграцию БД v11", LOAD)
        self.assertIn("!gTruckLoadSchemaReady) return false;", LOAD)

    def test_more_natural_lift_and_diagnostic_errors(self):
        self.assertIn("#define TRUCK_HIGH_CLEARANCE (4.0)", LOAD)
        self.assertNotIn("#define TRUCK_HIGH_CLEARANCE (5.5)", LOAD)
        self.assertIn("gTruckLoadHighZ[playerid]", LOAD)
        self.assertIn("gTruckLoadError[playerid] = 2;", LOAD)
        self.assertIn("gTruckLoadError[playerid] = 3;", LOAD)
        self.assertIn("mysql_errno(gDBPrimary)", LOAD)
        self.assertIn("TruckLoad_Reset(playerid);", LOAD)

    def test_pawn_encoding(self):
        for contents in (TRUCK, LOAD, OPS, DUMP):
            self.assertEqual(contents.encode("cp1251").decode("cp1251"), contents)

if __name__ == "__main__":
    unittest.main()
