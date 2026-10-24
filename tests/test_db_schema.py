"""DB-MIGRATION-01: целостность обязательных структур схемы."""
from pathlib import Path
import unittest

SCHEMA = (Path(__file__).resolve().parents[1] / "db/migrations/001_initial.sql").read_text(encoding="utf-8")


class DatabaseSchema(unittest.TestCase):
    def test_normalized_entity_tables(self):
        for table in ("accounts", "cleaner_profiles", "admin_roles", "bans",
                      "factions", "faction_members", "cleaner_districts",
                      "race_top", "legacy_imports", "schema_migrations"):
            self.assertIn(f"CREATE TABLE", SCHEMA)
            self.assertIn(table, SCHEMA)
        self.assertGreaterEqual(SCHEMA.count("ENGINE=InnoDB"), 10)

    def test_constraints_and_atomicity(self):
        self.assertIn("UNIQUE(username)", SCHEMA)
        self.assertIn("PRIMARY KEY", SCHEMA)
        self.assertIn("FOREIGN KEY", SCHEMA)
        self.assertIn("chk_accounts_race", SCHEMA)
        self.assertIn("chk_cleaner_values", SCHEMA)
        self.assertIn("uq_legacy_import_source", SCHEMA)
        self.assertIn("is_enabled BOOLEAN", SCHEMA)
        self.assertNotIn("DROP TABLE", SCHEMA)
        self.assertNotIn("DELETE FROM", SCHEMA)


if __name__ == "__main__":
    unittest.main()
