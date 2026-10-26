# TARKENSO Load Tester — Stage 1 (NPC world-load)

This is a **controlled NPC workload probe**, not a real-player or RakNet
load generator. NPCs do not register accounts, authenticate via bcrypt,
run gameplay commands as a real player, or issue ordinary client traffic.
Do **not** use NPC counts as proof that the same number of human players
will work without lag.

## Prerequisites

- Test-only deployment using open.mp **v1.5.8.3079+**, with NPC natives.
- Separate test database and backed-up game data.
- The server operator/hosting provider must permit the test.
- A real, provisioned and authenticated founder account `Veyran` and `/aduty`.
- No manual enable file is necessary. Only the founder in /aduty can start a test.

The earlier scriptfiles/load_test.enabled flag is no longer required.

## Commands

```
/aduty
/load start
/load start 25 60
/load start 100 120
/load status
/load stop
```

`/loadtest` is also supported as an alias. The shorter `/load start`
starts a 25-NPC, 60-second run. Valid explicit limits are 1–100 NPCs
and 10–300 seconds. The founder must be spawned outdoors.

- At most four NPCs spawn per second; movement is also bounded.
- NPCs appear in the founder's virtual world near the founder.
- Only one run can be active, with automatic stop on timeout,
  founder disconnect or permission loss, spawn failure, or low tickrate.
- Only probe-created NPCs are cleaned up.

## Per-run reports

Every run first reserves a separate dated directory inside `scriptfiles/load_tests`.
For example:

```text
scriptfiles/
  load_tests/
    2026-10-26_rev01/
      run.info
      load_test.csv
    2026-10-26_rev02/
      run.info
      load_test.csv
    2026-10-27_rev01/
      run.info
      load_test.csv
```

`run.info` records the scenario and run revision. In `full` mode, the
same directory also contains `metrics.csv`, `advanced.csv` and
`summary.txt`. The folder is created **before any NPC is spawned**.
Even partially written or aborted directories are reserved, never reused.

The date is read from the game server clock, which might have a different
timezone than the operator. The revision is the daily test-run number,
not a Git/software version. Revisions run from 01 through 999 (numbers beyond
99 use three digits). Previously existing folders are skipped, never replaced.
If report initialization fails, no NPCs spawn; the reserved directory may remain
for diagnostics. Older flat CSV files are neither moved nor deleted.

## Telemetry

New log file per run: `scriptfiles/load_tests/YYYY-MM-DD_revNN/load_test.csv` (with CSV header).

Each row contains 10 comma-separated columns:

```
run_id,elapsed_sec,active_npcs,target_npcs,tickrate,min_tickrate,
worst_timer_interval_ms,guard_heartbeat_ms,guard_delayed_count,state
```

Rows are sampled every ~5 seconds and on start/stop; a timer delayed by
an overloaded process may skip sample points. Tickrate and heartbeat are
server-reported values, **not** end-to-end network RTT or CPU/RAM metrics.
`guard_delayed_count` is cumulative since server startup, not per run.

Available `state` examples: `start`, `running`, `complete`,
`manual`, `low_tickrate`, `owner_disconnect`, `owner_unavailable`.

To summarize recorded runs:

```bash
python3 tools/loadtest_report.py scriptfiles/load_tests/2026-10-26_rev01/load_test.csv
python3 tools/loadtest_report.py scriptfiles/load_tests/2026-10-26_rev01/load_test.csv --json
```

## Recommended procedure

1. Verify healthy tickrate and database on the isolated test server.
2. Start 25 NPCs for 60 seconds, observe logs and in-game movement.
3. Repeat at 50 and 100 NPCs with 120-second durations.
4. Collect CSV, server logs, CPU/RAM observations and SQL latencies separately.
5. Read the per-run CSV. Former combined `loadtest.csv` remains readable by the CLI.

**Limitations:** Stage 1 tests server NPC/world processing and associated
game-loop responsiveness only. It does not load bcrypt, `mysql_tquery`,
registration/PayDay as real players, the client UDP handshake, or genuine
RakNet synchronization per connected player. A later stage needs an independently
implemented and compatibility-tested headless client for *authorized test
instances*; it must include connection pacing, hard limits, and teardown.
