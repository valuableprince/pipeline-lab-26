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
- Manually create an empty file `scriptfiles/load_test.enabled` on the
  **test server**. This file is deliberately absent from the repository.

`/loadtest` is disabled unless this file exists; removing it stops
an active run on the next timer tick.

## Commands

```
/aduty
/loadtest status
/loadtest start 25 60
/loadtest start 100 120
/loadtest stop
```

- Allowed range: **1–100 NPCs**, **10–300 seconds**.
- The founder must be spawned outdoors before starting.
- At most four NPCs are spawned per second; movement is also bounded.
- NPCs are spawned in the founder's virtual world, close to their location.
  Use an empty test world: this is intentionally visible to clients in
  that world and may reduce their FPS.
- At most one run is active. Auto-stop occurs on timeout, owner disconnect,
  removed flag, failed creation/spawn, or three consecutive tickrate readings
  below 20. The server stopping also cleans up the probe.
- Only NPC IDs created by the probe are removed. Other game NPCs are untouched.

## Telemetry

Log file: `scriptfiles/loadtest.csv` (append-only, no header).

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
`manual`, `low_tickrate`, `owner_disconnect`, `flag_removed`.

To summarize recorded runs:

```bash
python3 tools/loadtest_report.py scriptfiles/loadtest.csv
python3 tools/loadtest_report.py scriptfiles/loadtest.csv --json
```

## Recommended procedure

1. Verify healthy tickrate and database on the isolated test server.
2. Start 25 NPCs for 60 seconds, observe logs and in-game movement.
3. Repeat at 50 and 100 NPCs with 120-second durations.
4. Collect CSV, server logs, CPU/RAM observations and SQL latencies separately.
5. Remove `load_test.enabled` after testing.

**Limitations:** Stage 1 tests server NPC/world processing and associated
game-loop responsiveness only. It does not load bcrypt, `mysql_tquery`,
registration/PayDay as real players, the client UDP handshake, or genuine
RakNet synchronization per connected player. A later stage needs an independently
implemented and compatibility-tested headless client for *authorized test
instances*; it must include connection pacing, hard limits, and teardown.
