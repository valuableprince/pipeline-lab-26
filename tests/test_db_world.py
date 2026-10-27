"""DB-WORLD-03: сохранение игровых подсистем в MySQL-primary без файлов."""
from pathlib import Path
from build_contracts import assert_staged
import unittest
R = Path(__file__).resolve().parents[1]
def source(path):
    return (R / path).read_text(encoding="utf-8")
D = source("gamemodes/includes/db_world.inc")
G = source("gamemodes/signal0317.pwn")
SQL = source("db/migrations/003_world_persistence.sql")
HOST_SQL = source("db/hosting_from_v1_to_v3.sql")
class DatabaseWorldTests(unittest.TestCase):
    def test_sql_entities(self):
        for table in ("studio_zones","npc_routes","npc_route_points","factions","cleaner_districts"):
            self.assertIn(table, SQL)
        self.assertIn("INSERT INTO schema_migrations (version) VALUES (3)", SQL)
        self.assertIn("INSERT INTO schema_migrations (version) VALUES (2)", HOST_SQL)
        self.assertIn("INSERT INTO schema_migrations (version) VALUES (3)", HOST_SQL)
    def test_world_routing(self):
        for file,funcs in {
            "gamemodes/includes/leader_system.inc": ("DBWorld_LeaderSave(org)","DBWorld_LeaderLoad(org)"),
            "gamemodes/includes/cleaner_job.inc": ("DBWorld_DistrictSave()","DBWorld_DistrictLoad()"),
            "gamemodes/includes/race_progress.inc": ("DBWorld_TopSave()","DBWorld_TopLoad()"),
            "gamemodes/includes/studio_system.inc": ("DBWorld_ZoneSave(slot","DBWorld_ZoneLoad(slot)")
        }.items():
            code=source(file)
            self.assertIn("#if defined TARKENSO_DB_PRIMARY",code)
            for func in funcs:
                self.assertIn(func,code)
    def test_compilation_gate(self):
        self.assertIn('#include "db_world.inc"',G)
        assert_staged(self, "db_world.inc")
        self.assertIn("WHERE version=10 LIMIT 1",source("gamemodes/includes/db_primary.inc"))
    def test_atomic_writes(self):
        for method in ("DBWorld_LeaderSave", "DBWorld_TopSave"):
            body = D.split("stock bool:" + method + "(", 1)[1].split("\n    stock ", 1)[0]
            with self.subTest(method=method):
                self.assertIn('DBWorld_Exec("START TRANSACTION")', body)
                self.assertIn('DBWorld_Exec("COMMIT")', body)
                self.assertIn("DBWorld_Rollback();", body)
                self.assertIn("mysql_format(", body)
        self.assertIn('DBWorld_Exec("ROLLBACK")', D)
        self.assertNotIn("DBWorld_RouteSave", D)
if __name__=="__main__":unittest.main()
