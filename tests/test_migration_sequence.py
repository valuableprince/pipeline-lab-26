"""Migration 012 belongs to TextDraw Studio, not LoadNet."""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]
M = ROOT / "db/migrations"

class MigrationVersionContracts(unittest.TestCase):
    def test_unique_migration_files(self):
        files = list(M.glob("[0-9][0-9][0-9]_*.sql"))
        numbers = [int(p.name[:3]) for p in files]
        self.assertEqual(len(numbers), len(set(numbers)),
                         "Two migration files claim the same version")
        self.assertEqual(sorted(numbers), list(range(1, 15)))

    def test_12_is_exact_textdraw_schema(self):
        td = (M / "012_tdstudio_projects.sql").read_text(encoding="utf-8")
        self.assertIn("CREATE TABLE tdstudio_projects", td)
        self.assertIn("CREATE TABLE tdstudio_elements", td)
        for procedure in ("tds_save", "tds_delete", "tds_list", "tds_load"):
            self.assertIn("CREATE PROCEDURE " + procedure, td)
        self.assertIn("INSERT INTO schema_migrations (version) VALUES (12);", td)
        self.assertNotIn("loadtest_jobs", td)

    def test_13_is_synthetic_accounts(self):
        text = (M / "013_loadtest_virtual_accounts.sql").read_text(encoding="utf-8")
        self.assertIn("CREATE TABLE loadtest_virtual_accounts", text)
        self.assertIn("INSERT INTO schema_migrations (version) VALUES (13);", text)

    def test_14_is_optional_loadnet_queue(self):
        text = (M / "014_loadnet_jobs.sql").read_text(encoding="utf-8")
        self.assertIn("CREATE TABLE loadtest_jobs", text)
        self.assertIn("INSERT INTO schema_migrations (version) VALUES (14);", text)
        self.assertNotIn("VALUES (12);", text)

if __name__ == "__main__":
    unittest.main()
