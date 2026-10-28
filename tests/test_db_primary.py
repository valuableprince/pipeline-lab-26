"""DB-MIGRATION-02B: два persistence backend и контракт SQL сохранений."""
from pathlib import Path
from build_contracts import assert_staged
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
        self.assertIn("build/compiled.amx", W)
        assert_staged(self, "db_primary.inc")
        self.assertNotIn("#define TARKENSO_DB_PRIMARY", G)
        self.assertIn("#define TARKENSO_DB_PRIMARY", W)

    def test_sql_accounts_and_cas(self):
        self.assertIn("return DBPrimary_BeginAccount(playerid, fromIntro)", G)
        self.assertIn("return DBPrimary_WriteAccount(playerid, create)", G)
        self.assertIn("row_version=row_version+1", D)
        self.assertIn("WHERE account_id=%d AND row_version=%d", D)
        self.assertIn('mysql_connect_file("mysql.ini")', D)
        self.assertIn('native mysql_set_charset(const charset[], MySQL:handle);', D)
        self.assertIn('mysql_set_charset("cp1251", gDBPrimary)', D)
        self.assertIn('could not set CP1251 client charset', D)

        self.assertIn("mysql_tquery(gDBPrimary, q", D)
        self.assertIn("gAuthToken[playerid]", D)
        self.assertIn("fexist(legacyPath) || fexist(legacyBackup)", D)
        self.assertIn("name: compiled-output", W)
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
