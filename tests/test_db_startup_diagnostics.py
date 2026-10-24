from pathlib import Path
import unittest

SRC = (Path(__file__).resolve().parents[1] / "gamemodes/includes/db_primary.inc").read_text("utf-8")

class DBStartupDiagnostics(unittest.TestCase):
    def test_failure_stages(self):
        for message in ("Opening mysql.ini", "mysql_connect_file returned invalid handle",
                        "mysql_connect_file failed; MySQL errno=",
                        "schema query failed; MySQL errno=",
                        "expected schema version 4"):
            self.assertIn(message, SRC)
    def test_safe_logging(self):
        self.assertNotIn("password = ", SRC)
        self.assertNotIn("mysql.ini contents", SRC)

if __name__ == "__main__":
    unittest.main()
