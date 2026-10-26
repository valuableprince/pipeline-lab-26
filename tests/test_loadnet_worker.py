"""LOADNET-01: regressions for Pawn queue, MySQL worker, scenarios and safe limits."""
import importlib.util
from pathlib import Path
import tempfile
import unittest
from unittest.mock import MagicMock, patch

ROOT = Path(__file__).resolve().parents[1]
PAWN = (ROOT / "gamemodes/includes/loadnet_jobs.inc").read_text(encoding="utf-8")
GAME = (ROOT / "gamemodes/signal0317.pwn").read_text(encoding="utf-8")
COMMANDS = (ROOT / "gamemodes/includes/commands.inc").read_text(encoding="utf-8")
MIGRATION = (ROOT / "db/migrations/012_loadnet_jobs.sql").read_text(encoding="utf-8")
MODULE = ROOT / "tools/loadnet_worker.py"
spec = importlib.util.spec_from_file_location("loadnet_worker", MODULE)
worker = importlib.util.module_from_spec(spec)
spec.loader.exec_module(worker)


class PawnQueueContract(unittest.TestCase):
    def test_protected_game_integration(self):
        self.assertIn('#include "loadnet_jobs.inc"', GAME)
        self.assertIn("cmd:loadnet(playerid, params[])", COMMANDS)
        self.assertIn("return LoadNet_Command(playerid, params);", COMMANDS)
        self.assertIn("Admin_IsFounder(playerid) && gAdminDuty[playerid]", PAWN)
        self.assertIn("gAuthToken[playerid] != authToken", PAWN)
        self.assertIn("gDBPrimaryLoaded[playerid]", PAWN)

    def test_asynchronous_sql_and_safe_stop(self):
        self.assertIn("mysql_tquery(gDBPrimary", PAWN)
        self.assertNotIn("mysql_query(gDBPrimary", PAWN)
        self.assertIn("INSERT IGNORE INTO loadtest_jobs", PAWN)
        self.assertIn("state=IF(state='PENDING','STOPPED','STOP_REQUESTED')", PAWN)
        self.assertIn("cache_affected_rows()", PAWN)
        self.assertNotIn("SendRconCommand", PAWN)
        self.assertNotIn("EXEC ", PAWN)

    def test_mysql_schema_one_active_job(self):
        self.assertIn("CREATE TABLE loadtest_jobs", MIGRATION)
        self.assertIn("UNIQUE KEY uq_loadtest_one_active (active_slot)", MIGRATION)
        self.assertIn("WHEN state IN ('PENDING','RUNNING','STOP_REQUESTED')", MIGRATION)
        self.assertIn("FOREIGN KEY (account_id) REFERENCES accounts(account_id)", MIGRATION)
        self.assertIn("VALUES (12)", MIGRATION)


class WorkerTests(unittest.TestCase):
    def sample_job(self, **overrides):
        job = {"job_id": 7, "target_clients": 5, "duration_seconds": 90,
               "auth_mode": "register", "scenario": "idle"}
        job.update(overrides)
        return job

    def test_rejects_invalid_jobs(self):
        for change in (
            {"target_clients": 51}, {"target_clients": 0}, {"duration_seconds": 601},
            {"auth_mode": "anything"}, {"scenario": "shell"},
        ):
            with self.subTest(change=change), self.assertRaises(ValueError):
                worker.validate_job(self.sample_job(**change))

    def test_server_target_fixed_on_pc_and_no_shell(self):
        job = self.sample_job()
        cmd = worker.make_command(
            Path("python"), Path("loadtest_stage2.py"), Path("rakclient.exe"),
            "owned.example:7777", job, Path("local_reports"))
        self.assertIn("owned.example:7777", cmd)
        self.assertIn("--confirm-target", cmd)
        self.assertEqual(cmd[cmd.index("--rate")+1], "0.5")
        self.assertIn("LT_R7", cmd)
        self.assertNotIn("--shell", cmd)
        self.assertEqual(cmd[cmd.index("--mode")+1], "register")

    def test_login_fails_without_known_registered_source(self):
        job = self.sample_job(auth_mode="login")
        with self.assertRaisesRegex(ValueError, "completed registration"):
            worker.make_command(Path("python"), Path("stage2.py"),
                                Path("rakclient.exe"), "owned.example:7777",
                                job, Path("reports"))
        cmd = worker.make_command(Path("python"), Path("stage2.py"),
                                  Path("rakclient.exe"), "owned.example:7777",
                                  job, Path("reports"), login_source=5)
        self.assertIn("LT_R5", cmd)

    def test_only_actual_csv_samples_are_counted(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp) / "load_net_01_26.10.2026"
            root.mkdir()
            csv = root / "samples.csv"
            csv.write_text(
                "seconds,started,transport_connected,game_joined,authenticated,spawned,exited,failed,alive\n"
                "10,5,4,3,2,1,0,0,5\n", encoding="utf-8")
            self.assertEqual(worker.last_sample(Path(tmp)),
                             {"launched": 5, "joined": 3,
                              "authenticated": 2, "spawned": 1})

    def test_job_claim_compare_and_swap(self):
        db = MagicMock()
        cursor = db.cursor.return_value.__enter__.return_value
        cursor.execute.return_value = 1
        self.assertTrue(worker.claim_job(db, 7))
        sql, arguments = cursor.execute.call_args.args
        self.assertIn("WHERE job_id=%s AND state='PENDING'", sql)
        self.assertEqual(arguments, (7,))
        cursor.execute.return_value = 0
        self.assertFalse(worker.claim_job(db, 7))

    def test_stop_state_wins_in_finish(self):
        db = MagicMock()
        cursor = db.cursor.return_value.__enter__.return_value
        cursor.fetchone.return_value = {"state": "STOP_REQUESTED"}
        worker.finish_job(db, 7, "COMPLETE", "Finished", {"spawned": 5})
        last_sql, arguments = cursor.execute.call_args.args
        self.assertIn("state=%s", last_sql)
        self.assertEqual(arguments[0], "STOPPED")
        self.assertEqual(arguments[-1], 7)

    def test_dry_run_does_not_connect_database_or_launch_process(self):
        args = worker.parse_args([
            "--server", "127.0.0.1:7777", "--confirm-target", "127.0.0.1:7777",
            "--client-bin", "missing-client", "--dry-run"])
        with patch.object(worker, "connect_db") as db, patch.object(worker.subprocess, "Popen") as popen:
            self.assertEqual(worker.main([
                "--server", "127.0.0.1:7777", "--confirm-target", "127.0.0.1:7777",
                "--client-bin", "missing-client", "--dry-run"]), 0)
            db.assert_not_called()
            popen.assert_not_called()

if __name__ == "__main__":
    unittest.main()
