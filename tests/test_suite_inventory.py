"""Require modernized coverage and keep historical contracts traceable."""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]
ARCHIVED = [
    "test_account_v2.py",
    "test_admin_system.py",
    "test_admin_teleport.py",
    "test_audit_fix_03.py",
    "test_audit_qa_01.py",
    "test_auth_scaffold.py",
    "test_auth_ui.py",
    "test_cleaner_container.py",
    "test_cleaner_job.py",
    "test_command_router.py",
    "test_db_primary.py",
    "test_db_shadow.py",
    "test_db_world.py",
    "test_economy.py",
    "test_hosting_mariadb.py",
    "test_leader_system.py",
    "test_race_audio.py",
    "test_race_core.py",
    "test_race_event.py",
    "test_race_playlist.py",
    "test_server_guard.py",
    "test_streamer_integration.py",
    "test_studio_command_hub.py"
]


class RegressionInventory(unittest.TestCase):
    def test_archived_sources_and_replacements_both_exist(self):
        for filename in ARCHIVED:
            with self.subTest(file=filename):
                self.assertTrue((ROOT / "tests" / filename).is_file())
                self.assertTrue((ROOT / "tests" / "legacy" / filename).is_file())
        self.assertFalse((ROOT / "tests" / "legacy" / "__init__.py").exists())

    def test_all_active_tests_are_mandatory_in_ci(self):
        workflow = (ROOT / ".github/workflows/build.yml").read_text(encoding="utf-8")
        self.assertIn("python3 -m unittest discover -s tests -p 'test_*.py' -v", workflow)
        self.assertNotIn("continue-on-error:", workflow)
        self.assertIn("python3 tools/check_pawn_output.py build/compiler.log", workflow)

    def test_deployment_secrets_are_not_committed(self):
        ignored = (ROOT / ".gitignore").read_text(encoding="utf-8")
        for sensitive in ("mysql.ini", "config.json", "*.key", "*.pem"):
            self.assertIn(sensitive, ignored)


if __name__ == "__main__":
    unittest.main()
