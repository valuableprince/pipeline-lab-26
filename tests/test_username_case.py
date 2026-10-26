"""Preserve exact display case while enforcing case-insensitive SQL identity."""
from pathlib import Path
import unittest
ROOT=Path(__file__).resolve().parents[1]
GAME=(ROOT/"gamemodes/signal0317.pwn").read_text("utf-8")
DB=(ROOT/"gamemodes/includes/db_primary.inc").read_text("utf-8")
WORLD=(ROOT/"gamemodes/includes/db_world.inc").read_text("utf-8")
SQL=(ROOT/"db/migrations/004_username_case.sql").read_text("utf-8")
class UsernameCase(unittest.TestCase):
    def test_canonical_key(self):
        self.assertIn("GENERATED ALWAYS AS (LOWER(username)) STORED",SQL)
        self.assertIn("uq_accounts_username_key UNIQUE",SQL)
        self.assertIn("VALUES (4)",SQL)
        self.assertIn("WHERE version=10 LIMIT 1",DB)
    def test_case_preservation(self):
        self.assertNotIn("if (c >= 'A' && c <= 'Z') c += ('a' - 'A');",GAME)
        self.assertIn("c >= 'A' && c <= 'Z'",GAME)
        self.assertIn('format(gAccountName[playerid], MAX_PLAYER_NAME, "%s", savedName)',DB)
        self.assertGreaterEqual(DB.count("username_key=LOWER('%e')"),2)
        self.assertIn("username_key=LOWER('%e')",WORLD)
    def test_bans_not_case_sensitive(self):
        self.assertIn("LOWER(username)=LOWER('%e')",DB)
if __name__=="__main__":
    unittest.main()
