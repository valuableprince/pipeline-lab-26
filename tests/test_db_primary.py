"""DB-MIGRATION-02B: два persistence backend и контракт SQL сохранений."""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]
G = (ROOT / "gamemodes/signal0317.pwn").read_text("utf-8")
D = (ROOT / "gamemodes/includes/db_primary.inc").read_text("utf-8")
C = (ROOT / "gamemodes/includes/cleaner_job.inc").read_text("utf-8")
A = (ROOT / "gamemodes/includes/admin_system.inc").read_text("utf-8")
W = (ROOT / ".github/workflows/build.yml").read_text("utf-8")

class PrimaryDatabase(unittest.TestCase):
    def test_opt_in_and_build(self):
        self.assertIn("#if defined TARKENSO_DB_PRIMARY", D)
        self.assertIn("signal0317_db_primary.amx", W)
        self.assertIn("db_primary.inc", W)
        self.assertNotIn("#define TARKENSO_DB_PRIMARY", G)

    def test_sql_accounts_and_cas(self):
        self.assertIn("return DBPrimary_BeginAccount(playerid, fromIntro)", G)
        self.assertIn("return DBPrimary_WriteAccount(playerid, create)", G)
        self.assertIn("row_version=row_version+1", D)
        self.assertIn("WHERE account_id=%d AND row_version=%d", D)
        self.assertIn('mysql_connect_file("mysql.ini")', D)
        self.assertIn("mysql_tquery(gDBPrimary, q", D)
        self.assertIn("gAuthToken[playerid]", D)
        self.assertIn("fexist(legacyPath) || fexist(legacyBackup)", D)
        self.assertIn("signal0317-mysql-uat-amx", W)
        self.assertIn("cache_affected_rows()", D)

    def test_jobs_admin_and_bans(self):
        self.assertIn("DBPrimary_LoadCleaner(playerid)", C)
        self.assertIn("DBPrimary_SaveCleaner(playerid)", C)
        self.assertIn("DBPrimary_WriteRole(playerid, level, eventRole)", A)
        self.assertIn("DBPrimary_WriteBan(playerid, until, reason)", A)
        self.assertIn("DBPrimary_UnbanName(name)", A)
        self.assertIn("ON DUPLICATE KEY UPDATE", D)
        self.assertIn("mysql_format(", D)

if __name__ == "__main__":
    unittest.main()
