# TARKENSO LoadNet / PC Worker

Game command -> MySQL job queue -> Windows Worker -> external RakClient headless clients.
No executable commands are run by Pawn. Existing /load NPC testing remains independent.

## Server setup

1. Back up the game database and files.
2. Apply db/migrations/012_loadnet_jobs.sql in phpMyAdmin. The new table is loadtest_jobs.
3. Install the AMX produced by the verified Github Actions build.
4. Create a separate MySQL account with SELECT and UPDATE on loadtest_jobs only.
5. Ensure the worker computer can reach MySQL over a protected connection. Shared game
   hosting sometimes blocks all external database access: in that case this direct
   MySQL Worker will require an SSH tunnel or a separately developed HTTPS gateway.

Suggested SQL for a dedicated worker account (replace placeholders):

    CREATE USER 'tarkenso_loadnet'@'CLIENT_IP' IDENTIFIED BY 'LONG_RANDOM_PASSWORD';
    GRANT SELECT, UPDATE ON your_database.loadtest_jobs TO 'tarkenso_loadnet'@'CLIENT_IP';

## Windows prerequisites

- Python 3.10+ and PyMySQL: python -m pip install PyMySQL
- An independently downloaded compatible rakclient.exe next to the tools folder.
- Candidate RakClient release: https://github.com/Newmcpe/RakClient/releases/tag/0.2.0
- Compatibility with the current TARKENSO open.mp server needs a live 1-client check.

Set environment variables in PowerShell (do not commit credentials):

    $env:TARKENSO_GAME_SERVER = "YOUR_GAME_HOST:PORT"
    $env:TARKENSO_DB_HOST = "YOUR_DB_HOST"
    $env:TARKENSO_DB_PORT = "3306"
    $env:TARKENSO_DB_NAME = "YOUR_DB_NAME"
    $env:TARKENSO_DB_USER = "tarkenso_loadnet"
    $env:TARKENSO_DB_PASSWORD = "YOUR_WORKER_PASSWORD"
    $env:TARKENSO_LOAD_PASSWORD = "SeparateTestAccountPassword123"

Validate locally without connecting:

    python tools/loadnet_worker.py --server $env:TARKENSO_GAME_SERVER --confirm-target $env:TARKENSO_GAME_SERVER --client-bin "./rakclient.exe" --dry-run

Run in that same terminal (or double-click the BAT after environment is configured):

    .\tools\run_loadnet_worker.bat

## In-game controls

Login as authenticated founder and enable /aduty, then:

    /loadnet start 1 90 register
    /loadnet status
    /loadnet report
    /loadnet stop

After verifying a real registration and spawn, increase to 5, 10, 25 and 50.
Use /loadnet start 5 90 login to log in using the most recent completed
registration test with at least 5 successfully spawned accounts and the SAME password.
Only one job can be active at once. Limits: 1..50 clients; 10..600 seconds;
client launch rate fixed at 0.5 per second.

## Reports, recovery and security

Worker output is stored ON THE PC under loadtest_results/job_<id>/, not the
game server scriptfiles. Includes worker.log, per-client transcripts and samples.csv.
Metrics distinguish launched, network joined, authenticated, and spawned.

Stop requests set STOP_REQUESTED in MySQL and worker terminates its subprocess
tree. Worker Ctrl+C also interrupts the test. If Worker crashes before it
can update MySQL, a job may remain RUNNING. First verify NO game test clients
are still running on the PC, then manually mark the stale job FAILED in SQL.
Never reset active jobs while another worker or test is running.

Never use the founder account password for test clients. Prefer MySQL TLS
or SSH tunnel, restrict database access by IP, and do not expose DB credentials.
The database never stores arbitrary executables, target hosts or commands.
Use only for servers you own and where your provider permits load testing.
