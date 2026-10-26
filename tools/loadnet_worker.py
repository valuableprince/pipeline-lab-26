#!/usr/bin/env python3
"""LOADNET-01: PC Worker. Polls founder-issued MySQL jobs and drives stage2 runner.

Never accepts executable commands or target hosts from the database.
The target address and account credentials are local-only environment variables.
"""
from __future__ import annotations

import argparse
import csv
from datetime import datetime
import os
from pathlib import Path
import signal
import subprocess
import sys
import time

MAX_CLIENTS = 50
MAX_SECONDS = 600
MAX_LAUNCH_RATE = 0.5
DEFAULT_POLL = 2.0

def validate_job(job: dict) -> tuple[int, int, str, str]:
    count = int(job["target_clients"])
    seconds = int(job["duration_seconds"])
    mode = job["auth_mode"]
    scenario = job["scenario"]
    if not (1 <= count <= MAX_CLIENTS and 10 <= seconds <= MAX_SECONDS):
        raise ValueError("job limit violation")
    if mode not in ("register", "login") or scenario not in ("idle", "commands"):
        raise ValueError("unexpected job scenario")
    return count, seconds, mode, scenario


def client_prefix(job_id: int) -> str:
    """Stable prefix for a prior registration job or new registration job."""
    if not 1 <= job_id <= 999999:
        raise ValueError("job id exceeds account prefix range")
    return f"LT_R{job_id}"


def last_sample(output_dir: Path) -> dict[str, int]:
    """Collect only figures actually emitted by the downstream client runner."""
    samples = sorted(output_dir.glob("load_net_*/samples.csv"), key=lambda p: p.stat().st_mtime)
    if not samples:
        return {}
    try:
        with samples[-1].open(encoding="utf-8", newline="") as file:
            rows = list(csv.DictReader(file))
        if not rows:
            return {}
        row = rows[-1]
        values = {
            "launched": int(row["started"]),
            "joined": int(row["game_joined"]),
            "authenticated": int(row["authenticated"]),
            "spawned": int(row["spawned"]),
        }
        if any(n < 0 or n > MAX_CLIENTS for n in values.values()):
            return {}
        return values
    except (OSError, ValueError, KeyError, csv.Error):
        return {}


def make_command(
    python: Path, runner: Path, client: Path, server: str,
    job: dict, output: Path, login_source: int | None = None
) -> list[str]:
    count, seconds, mode, scenario = validate_job(job)
    if mode == "login" and login_source is None:
        raise ValueError("login requires a completed registration job")
    prefix = client_prefix(int(login_source) if mode == "login" else int(job["job_id"]))
    # Everything executable is fixed on the PC. MySQL stores only scalar job settings.
    return [
        str(python), str(runner), "--server", server,
        "--confirm-target", server, "--remote-approved",
        "--client-bin", str(client), "--count", str(count),
        "--seconds", str(seconds), "--rate", str(MAX_LAUNCH_RATE),
        "--prefix", prefix, "--mode", mode, "--scenario", scenario,
        "--output", str(output),
    ]


def terminate_tree(process: subprocess.Popen) -> None:
    """Ask stage2 to clean up its clients; kill the process tree if necessary."""
    if process.poll() is not None:
        return
    if os.name == "nt":
        try:
            process.send_signal(signal.CTRL_BREAK_EVENT)
        except OSError:
            pass
    else:
        try:
            os.killpg(process.pid, signal.SIGINT)
        except (OSError, ProcessLookupError):
            pass
    try:
        process.wait(timeout=8)
        return
    except subprocess.TimeoutExpired:
        pass
    if os.name == "nt":
        subprocess.run(["taskkill", "/PID", str(process.pid), "/T", "/F"],
                       check=False, capture_output=True, timeout=10)
    else:
        try:
            os.killpg(process.pid, signal.SIGKILL)
        except (OSError, ProcessLookupError):
            pass
    try:
        process.wait(timeout=4)
    except subprocess.TimeoutExpired:
        process.kill()
        process.wait(timeout=4)


def connect_db():
    try:
        import pymysql
    except ImportError as exc:
        raise RuntimeError("Install PyMySQL: python -m pip install PyMySQL") from exc
    required = [
        "TARKENSO_DB_HOST", "TARKENSO_DB_USER",
        "TARKENSO_DB_PASSWORD", "TARKENSO_DB_NAME",
    ]
    missing = [name for name in required if not os.environ.get(name)]
    if missing:
        raise RuntimeError(f"Missing worker MySQL variables: {', '.join(missing)}")
    port_raw = os.getenv("TARKENSO_DB_PORT", "3306")
    if not port_raw.isascii() or not port_raw.isdecimal() or not 1 <= int(port_raw) <= 65535:
        raise RuntimeError("Invalid TARKENSO_DB_PORT")
    return pymysql.connect(
        host=os.environ["TARKENSO_DB_HOST"],
        port=int(port_raw),
        user=os.environ["TARKENSO_DB_USER"],
        password=os.environ["TARKENSO_DB_PASSWORD"],
        database=os.environ["TARKENSO_DB_NAME"],
        charset="utf8mb4",
        autocommit=True,
        connect_timeout=5,
        read_timeout=5,
        write_timeout=5,
        cursorclass=pymysql.cursors.DictCursor,
    )


def execute(db, statement: str, params: tuple = ()) -> int:
    with db.cursor() as cur:
        return cur.execute(statement, params)


def select_one(db, statement: str, params: tuple = ()) -> dict | None:
    with db.cursor() as cur:
        cur.execute(statement, params)
        return cur.fetchone()


def get_next_job(db) -> dict | None:
    return select_one(db,
        "SELECT job_id,target_clients,duration_seconds,auth_mode,scenario "
        "FROM loadtest_jobs WHERE state='PENDING' ORDER BY job_id LIMIT 1")


def claim_job(db, job_id: int) -> bool:
    return execute(db,
        "UPDATE loadtest_jobs SET state='RUNNING',started_at=NOW(),heartbeat_at=NOW(),"
        "status_message='Worker started' WHERE job_id=%s AND state='PENDING'",
        (job_id,)) == 1


def finish_job(db, job_id: int, state: str, note: str, counts: dict) -> None:
    if state not in ("COMPLETE", "FAILED", "STOPPED"):
        raise ValueError("bad terminal state")
    # A stop request always wins over a concurrent successful finish.
    row = select_one(db, "SELECT state FROM loadtest_jobs WHERE job_id=%s", (job_id,))
    if not row:
        return
    final_state = "STOPPED" if row["state"] == "STOP_REQUESTED" else state
    execute(db,
        "UPDATE loadtest_jobs SET state=%s,finished_at=NOW(),heartbeat_at=NOW(),"
        "status_message=%s,launched_clients=%s,joined_clients=%s,"
        "authenticated_clients=%s,spawned_clients=%s "
        "WHERE job_id=%s AND state IN ('RUNNING','STOP_REQUESTED')",
        (final_state, note[:120],
         counts.get("launched", 0), counts.get("joined", 0),
         counts.get("authenticated", 0), counts.get("spawned", 0),
         job_id))


def update_progress(db, job_id: int, counts: dict) -> bool:
    """Returns False when the game requested a stop."""
    row = select_one(db,
        "SELECT state FROM loadtest_jobs WHERE job_id=%s", (job_id,))
    if not row or row["state"] != "RUNNING":
        return False
    execute(db,
        "UPDATE loadtest_jobs SET heartbeat_at=NOW(),launched_clients=%s,"
        "joined_clients=%s,authenticated_clients=%s,spawned_clients=%s "
        "WHERE job_id=%s AND state='RUNNING'",
        (counts.get("launched", 0), counts.get("joined", 0),
         counts.get("authenticated", 0), counts.get("spawned", 0),
         job_id))
    return True


def run_job(db, job: dict, args) -> None:
    job_id = int(job["job_id"])
    output = args.output / f"job_{job_id}"
    output.mkdir(parents=True, exist_ok=True)
    log_path = output / "worker.log"
    flags = subprocess.CREATE_NEW_PROCESS_GROUP if os.name == "nt" else 0
    counts = {}
    proc = None
    status = "FAILED"
    reason = "Worker could not start"
    try:
        source_id = None
        if job["auth_mode"] == "login":
            prior = select_one(db,
                "SELECT job_id FROM loadtest_jobs WHERE auth_mode='register' "
                "AND state='COMPLETE' AND spawned_clients >= %s "
                "ORDER BY job_id DESC LIMIT 1", (int(job["target_clients"]),))
            if not prior:
                raise ValueError("No successful registration run with enough accounts")
            source_id = int(prior["job_id"])
        command = make_command(
            Path(sys.executable),
            Path(__file__).resolve().with_name("loadtest_stage2.py"),
            args.client_bin.resolve(), args.server, job, output, source_id)
        with log_path.open("wb") as writer:
            proc = subprocess.Popen(
                command, stdout=writer, stderr=subprocess.STDOUT,
                stdin=subprocess.DEVNULL,
                creationflags=flags,
                start_new_session=(os.name != "nt"),
                env=os.environ.copy(),
            )
            start = time.monotonic()
            while proc.poll() is None:
                counts = last_sample(output)
                if not update_progress(db, job_id, counts):
                    reason = "Stopped from game"
                    status = "STOPPED"
                    break
                # Stage2 also enforces its own duration. Extra grace prevents hangs.
                if time.monotonic() - start > int(job["duration_seconds"]) + 30:
                    reason = "Runner exceeded deadline"
                    break
                time.sleep(args.poll)
            if proc.poll() is None:
                terminate_tree(proc)
            counts = last_sample(output)
            if status != "STOPPED":
                status = "COMPLETE" if proc.returncode == 0 else "FAILED"
                reason = "Finished" if status == "COMPLETE" else f"Runner exit {proc.returncode}"
    except (OSError, RuntimeError, ValueError) as exc:
        reason = f"Worker error: {type(exc).__name__}"
        print(f"[loadnet] job #{job_id}: {reason}: {exc}", file=sys.stderr)
        if proc is not None:
            terminate_tree(proc)
    finally:
        # A DB loss can prevent status persistence. The worker refuses new jobs
        # until connectivity is restored, and does not leave an active client process.
        if proc is not None and proc.poll() is None:
            terminate_tree(proc)
        finish_job(db, job_id, status, reason, counts)
        print(f"[loadnet] #{job_id}: {status}, {reason}, counters={counts}")


def run_loop(args) -> int:
    import importlib.util
    runner = Path(__file__).resolve().with_name("loadtest_stage2.py")
    if not runner.is_file() or not args.client_bin.is_file():
        raise RuntimeError("loadtest_stage2.py or RakClient binary is missing")
    if not 0.5 <= args.poll <= 10:
        raise RuntimeError("poll must be 0.5..10 seconds")
    if not os.getenv("TARKENSO_LOAD_PASSWORD"):
        raise RuntimeError("Set TARKENSO_LOAD_PASSWORD for test accounts")
    from loadtest_stage2 import parse_target
    parse_target(args.server)
    if args.confirm_target != args.server:
        raise RuntimeError("game target confirmation mismatch")
    args.output.mkdir(parents=True, exist_ok=True)
    print("[loadnet] Worker started. Polling MySQL; Ctrl+C stops the worker.")
    while True:
        db = connect_db()
        try:
            while True:
                db.ping(reconnect=True)
                # A cancellation before claim needs no client process.
                execute(db,
                    "UPDATE loadtest_jobs SET state='STOPPED',finished_at=NOW(),"
                    "status_message='Stopped before Worker start' "
                    "WHERE state='STOP_REQUESTED' AND started_at IS NULL")
                job = get_next_job(db)
                if job and claim_job(db, job["job_id"]):
                    print(f"[loadnet] claimed #{job['job_id']}: "
                          f"{job['target_clients']} clients / {job['duration_seconds']} sec")
                    run_job(db, job, args)
                time.sleep(args.poll)
        finally:
            db.close()


def parse_args(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--server", required=True, help="fixed permitted game server host:port")
    parser.add_argument("--confirm-target", required=True, help="exact same host:port")
    parser.add_argument("--client-bin", type=Path, required=True, help="RakClient executable")
    parser.add_argument("--output", type=Path, default=Path("loadtest_results"))
    parser.add_argument("--poll", type=float, default=DEFAULT_POLL)
    parser.add_argument("--dry-run", action="store_true",
                        help="validate paths/target without SQL connections or clients")
    return parser.parse_args(argv)


def main(argv=None):
    args = parse_args(argv)
    try:
        from loadtest_stage2 import parse_target
        parse_target(args.server)
        if args.server != args.confirm_target:
            raise ValueError("game target confirmation mismatch")
        if args.dry_run:
            print(f"[loadnet] DRY RUN: target={args.server}, worker={Path(__file__).name}, "
                  f"client={args.client_bin} (no MySQL or network access)")
            return 0
        return run_loop(args)
    except KeyboardInterrupt:
        print("[loadnet] Stopped by operator")
        return 130
    except (ValueError, RuntimeError, OSError) as exc:
        print(f"[loadnet] Error: {exc}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
