"""DB-WORLD-03: сохранение игровых подсистем в MySQL-primary без файлов."""
from pathlib import Path
import unittest
R = Path(__file__).resolve().parents[1]
def source(path):
    return (R / path).read_text(encoding="utf-8")
D = source("gamemodes/includes/db_world.inc")
G = source("gamemodes/signal0317.pwn")
CI = source(".github/workflows/build.yml")
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
        self.assertIn('"db_world.inc"',CI)
        self.assertIn("WHERE version=4 LIMIT 1",source("gamemodes/includes/db_primary.inc"))
    def test_atomic_writes(self):
        for method in ("DBWorld_LeaderSave","DBWorld_TopSave","DBWorld_RouteSave"):
            self.assertIn(method,D)
        self.assertGreaterEqual(D.count('DBWorld_Exec("START TRANSACTION")'),3)
        self.assertGreaterEqual(D.count('DBWorld_Exec("COMMIT")'),3)
        self.assertIn('DBWorld_Exec("ROLLBACK")',D)
        self.assertIn("mysql_format(",D)
if __name__=="__main__":unittest.main()
