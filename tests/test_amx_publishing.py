"""Contract for exact-source AMX publication to main after green CI."""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]
WORKFLOW = (ROOT / ".github/workflows/build.yml").read_text("utf-8")

class AMXPublishingContracts(unittest.TestCase):
    def test_binary_is_published_only_after_all_gates(self):
        self.assertIn("  publish_amx:", WORKFLOW)
        publisher = WORKFLOW.split("  publish_amx:", 1)[1]
        self.assertIn("needs: [sql_security, gov00_mysql_uat, compile]", publisher)
        self.assertIn("github.event_name == 'push'", publisher)
        self.assertIn("github.ref == 'refs/heads/main'", publisher)
        self.assertIn("contents: write", publisher)
        self.assertIn("name: compiled-output", publisher)
        self.assertIn("test -s build/published/compiled.amx", publisher)
        self.assertIn("magic != 0xF1E0", publisher)
        self.assertIn("declared_size != len(blob)", publisher)
        self.assertIn('git rev-parse origin/main', publisher)
        self.assertIn('BUILD_SOURCE_SHA', publisher)
        self.assertIn("git add -f gamemodes/signal0317.amx", publisher)
        self.assertIn("git push origin HEAD:main", publisher)

    def test_generated_amx_does_not_retrigger_build(self):
        self.assertIn('"gamemodes/**"',WORKFLOW)
        self.assertIn('"!gamemodes/**/*.amx"',WORKFLOW)
        self.assertLess(WORKFLOW.index('"gamemodes/**"'),
                        WORKFLOW.index('"!gamemodes/**/*.amx"'))

if __name__ == "__main__":
    unittest.main()
