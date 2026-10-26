"""DB-MIGRATION-02A — optional MySQL shadow adapter safety contracts."""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]
GAME = (ROOT / "gamemodes/signal0317.pwn").read_text(encoding="utf-8")
INC = (ROOT / "gamemodes/includes/db_shadow.inc").read_text(encoding="utf-8")
CI = (ROOT / ".github/workflows/build.yml").read_text(encoding="utf-8")
WIN = (ROOT / "tools/build_windows.py").read_text(encoding="utf-8")


class ShadowBridge(unittest.TestCase):
    def test_optional_mode(self):
        self.assertIn("#if defined TARKENSO_DB_SHADOW", INC)
        self.assertIn('mysql_connect_file("mysql.ini")', INC)
        self.assertIn('mysql_tquery(gDBShadowHandle, query', INC)
        self.assertIn('DBShadow_OnAccountResult', INC)
        self.assertIn("token != gDBShadowToken[playerid]", INC)
        self.assertIn('strcmp(gDBShadowName[playerid], gAccountName[playerid], false)', INC)

    def test_read_only_by_construction(self):
        for forbidden in ("INSERT INTO", "UPDATE accounts", "DELETE FROM",
                          "REPLACE INTO", "mysql_query(", "mysql_pquery("):
            self.assertNotIn(forbidden, INC)
        self.assertIn("SELECT balance,txn_serial", INC)
        self.assertNotIn("password_hash, ", INC)
        self.assertIn("strcmp(hash, gAccountHash[playerid], false)", INC)
        self.assertIn("values and hashes omitted", INC)

    def test_lifecycle_and_legacy_compatibility(self):
        for method in ("DBShadow_Init();", "DBShadow_Shutdown();",
                       "DBShadow_OnConnect(playerid);",
                       "DBShadow_OnDisconnect(playerid);",
                       "DBShadow_CheckAccount(playerid);"):
            self.assertIn(method, GAME)
        self.assertIn('#include "db_shadow.inc"', GAME)
        self.assertIn('"db_shadow.inc"', CI)
        self.assertIn('"db_shadow.inc"', WIN)
        self.assertIn("Auth_ReadAccount(playerid)", GAME)
        self.assertIn("Auth_WriteData(playerid", GAME)


if __name__ == "__main__":
    unittest.main()
