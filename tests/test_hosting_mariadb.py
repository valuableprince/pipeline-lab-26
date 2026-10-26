"""HOSTING-DB-01: безопасная поставка схемы и теневой проверки."""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]
GAME = (ROOT / "gamemodes/signal0317.pwn").read_text(encoding="utf-8")
INC = (ROOT / "gamemodes/includes/db_shadow.inc").read_text(encoding="utf-8")
WORKFLOW = (ROOT / ".github/workflows/pawn-build.yml").read_text(encoding="utf-8")
INI = (ROOT / "config/mysql.ini.example").read_text(encoding="utf-8")

class HostingReadiness(unittest.TestCase):
    def test_mysql_shadow_disabled_by_default(self):
        self.assertIn("#if defined TARKENSO_DB_SHADOW", INC)
        self.assertIn('mysql_connect_file("mysql.ini")', INC)
        self.assertIn('DBShadow_Init();', GAME)
        self.assertIn('DBShadow_Shutdown();', GAME)
        self.assertIn('DBShadow_CheckAccount(playerid);', GAME)
        self.assertIn('DBShadow_OnConnect(playerid);', GAME)
        self.assertIn('DBShadow_OnDisconnect(playerid);', GAME)
        self.assertIn('return Auth_WriteData(playerid);', GAME)
        self.assertNotIn('#define TARKENSO_DB_SHADOW', GAME)

    def test_both_pawn_configs_are_built(self):
        self.assertIn('signal0317_db_shadow.pwn', WORKFLOW)
        self.assertIn('signal0317_db_shadow.amx', WORKFLOW)
        self.assertIn('db_shadow.inc', WORKFLOW)

    def test_no_credential_or_config_leak(self):
        self.assertIn('password = CHANGE_ME', INI)
        self.assertIn('hostname = 127.0.0.1', INI)
        self.assertIn('server_port = 3306', INI)
        self.assertNotIn('scriptfiles/mysql.ini', INC)

if __name__ == "__main__":
    unittest.main()
