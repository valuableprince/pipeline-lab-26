"""DB-OPT-012: non-destructive migration and Pawn schema compatibility."""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]
MIGRATION = (ROOT / "db/migrations/012_safe_optimization.sql").read_text("utf-8")
ROLLBACK = (ROOT / "db/migrations/012_safe_optimization_rollback.sql").read_text("utf-8")
DB = (ROOT / "gamemodes/includes/db_primary.inc").read_text("utf-8")
TRUCK = (ROOT / "gamemodes/includes/cleaner_truck_persist.inc").read_text("utf-8")
CLEANER = (ROOT / "gamemodes/includes/cleaner_container.inc").read_text("utf-8")
WORLD = (ROOT / "gamemodes/includes/db_world.inc").read_text("utf-8")


class DatabaseOptimization12(unittest.TestCase):
    def test_version_gate_and_preflight_before_ddl(self):
        self.assertIn("WHERE version=11", MIGRATION)
        self.assertIn("WHERE version>=12", MIGRATION)
        self.assertIn("HAVING COUNT(*)>1", MIGRATION)
        self.assertIn("SIGNAL SQLSTATE '45000'", MIGRATION)
        self.assertLess(MIGRATION.index("HAVING COUNT(*)>1"),
                        MIGRATION.index("ALTER TABLE bans"))
        self.assertLess(MIGRATION.index("ALTER TABLE bans"),
                        MIGRATION.index("INSERT INTO schema_migrations(version) VALUES (12)"))
        self.assertIn("WHERE version=12 LIMIT 1", DB)

    def test_indexed_case_insensitive_bans_preserve_names(self):
        self.assertIn("GENERATED ALWAYS AS (LOWER(username)) STORED", MIGRATION)
        self.assertIn("ADD UNIQUE KEY uq_bans_username_key (username_key)", MIGRATION)
        self.assertEqual(DB.count("FROM bans WHERE username_key=LOWER('%e')"), 3)
        self.assertNotIn("LOWER(username)=LOWER('%e')", DB)
        self.assertIn("INSERT INTO bans (username,banned_until,reason)", DB)
        self.assertIn("VALUES (LOWER('%e')", DB)

    def test_delivery_history_index_no_money_mutation(self):
        self.assertIn("idx_mtd_account_completed (account_id,completed_at,delivery_id)", MIGRATION)
        for forbidden in ("DROP TABLE", "TRUNCATE", "DELETE FROM accounts",
                          "UPDATE accounts", "UPDATE cleaner_profiles", "UPDATE municipal_driver_profiles"):
            self.assertNotIn(forbidden, MIGRATION)
        self.assertIn("UPDATE accounts SET balance=balance+%d", TRUCK)
        self.assertIn("UPDATE municipal_truck_cargo SET account_id=NULL", TRUCK)
        self.assertIn("INSERT INTO municipal_truck_deliveries", TRUCK)

    def test_legacy_table_distinctions_remain(self):
        self.assertIn("FROM cleaner_containers", CLEANER)
        self.assertIn("FROM cleaner_container_bag_positions", CLEANER)
        self.assertIn("FROM race_top", WORLD)
        self.assertNotIn("DROP TABLE", ROLLBACK)
        self.assertIn("DELETE FROM schema_migrations WHERE version=12", ROLLBACK)
        self.assertIn("DROP COLUMN username_key", ROLLBACK)
        self.assertIn("DROP INDEX idx_mtd_account_completed", ROLLBACK)
        self.assertIn("ADD INDEX fk_mtd_account (account_id)", ROLLBACK)

    def test_pawn_cp1251_compatible(self):
        self.assertEqual(DB, DB.encode("cp1251").decode("cp1251"))


if __name__ == "__main__":
    unittest.main()
