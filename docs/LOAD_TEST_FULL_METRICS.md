# TARKENSO LOADTEST-04: in-game NPC + isolated auth/SQL stress

## Running on the hosted TEST server

Apply [migration 013](../db/migrations/013_loadtest_virtual_accounts.sql)
once, after migration 012. It creates **loadtest_virtual_accounts**, an
isolated table with no foreign keys to gameplay accounts, money, jobs,
factions or roles. Back up MySQL before applying the migration.
The main `accounts` table and bcrypt credentials belonging to players
are never changed by this feature.

Log in as the authenticated founder, enable `/aduty` and run:

```
/load start 5 120 full
/load status
/load stop
/load report
```

For a larger run, after validating results:

```
/load start 25 180 full
/load start 50 240 full
/load start 100 300 full
```

The old command `/load start 25 60` still runs **NPC-only** mode.
`/load start 25 60 npc` is an explicit equivalent. Both modes
are capped at 100 NPC, 300 seconds, four spawns per second and
auto-stop on sustained low tickrate.

### What "full" actually does

1. Spawn built-in open.mp NPCs at the Fort Carson spawn area in virtual
   world 0; keep them standing there. NPCs may appear in TAB depending
   on client/server behavior. The legacy npc-only mode still moves NPCs.
2. For each spawned NPC, invoke the same installed `samp_bcrypt` plugin
   used by TARKENSO with cost 12. This is executed asynchronously using
   the authenticated founder's player ID for the callback, **not** via
   any invented NPC account login.
3. INSERT an isolated synthetic account row with its hashed test password.
4. SELECT that row and verify the hash using `bcrypt_verify`.
5. UPDATE its synthetic balance once as a representative save operation.
6. Capture per-operation callback latency and game-loop/guard metrics.

The scenario is **not** a genuine incoming network login, and does not
exercise OnPlayerConnect/OnDialogResponse for each NPC, actual PayDay,
real client traffic, CPU%, memory bytes, UDP packet loss or RTT.
The TEST password is a fixed synthetic value, not the founder password.

An operation can remain outstanding at the end of a short run. The
summary will say `INCOMPLETE` rather than claiming that all logins
succeeded. Run 120–300 seconds for sufficient completion.

### Files created automatically in scriptfiles

One **dated revision folder** per test (not four loose files):

```text
scriptfiles/
  load_tests/
    2026-10-26_rev01/
      run.info
      load_test.csv
      metrics.csv
      advanced.csv
      summary.txt
    2026-10-26_rev02/
      ...
```

The directory and `run.info` are created **before** the first NPC.
`run.info` records the test scenario and revision. Failed or interrupted
runs retain their reserved folder, so an old run is never overwritten.
`/load status` and `/load report` show the corresponding location.

After a FULL run ends, `/load report` shows the last test's verdict,
completed operations, exact SQL P95/P99 and cycle P95 in the **game chat**
without downloading the CSV. This in-memory view is available until
server restart or the next FULL run. Files remain saved across restarts.

The date and revision match the core NPC report. Examples only;
actual date follows the server clock and test sequence.

The metrics CSV includes:
- elapsed seconds, active NPCs and counts of registration,
  verification and saving
- number of completed SQL operations and errors; mean and maximum
  callback latency (milliseconds)
- bcrypt hash/verify counts and mean duration; maximum operation time
- tickrate, Guard heartbeat (ms) and cumulative delayed-heartbeat
  delta since the beginning of the run
- sample phase/reason for stopping

The **advanced.csv** contains exact nearest-rank percentiles of completed
observations (no histogram approximation) for:
- SQL callback duration: P50, P95, P99 (all INSERT/SELECT/UPDATE combined)
- bcrypt hash duration: P95, P99
- bcrypt verification duration: P95, P99
- full synthetic account lifecycle (start hashing to completion of final
  UPDATE): completed count, average, P95 and P99, in milliseconds
- cumulative registration and authentication throughput, in 1/100 of an
  operation per second: `registration_rate_x100=125` means 1.25/s
- peak one-second registration and authentication completion counts

Percentile estimation uses **nearest rank**, with at most 300 SQL
observations (3 per 100 NPC), 100 hashes, 100 verifications and 100
completed synthetic account lifecycles. A value of `-1` means
**no completed samples**, not a zero-millisecond operation.
Cumulative throughput is the number of completed operations divided
by the time since test start; this is not the instant or sustained
capacity of the real game server. The one-second peak uses server time.

The summary retains the old `p95_bucket_upper` values for backward
compatibility but **also** appends exact percentile results, full-cycle
timings, throughput and peak rates. Legacy bucket bounds are estimates.
The summary has PASS/FAIL/INCOMPLETE:

- PASS: test reached its timeout, all requested NPCs completed
  INSERT, SELECT, bcrypt hash+verify and UPDATE, there were no
  captured errors, logging worked and minimum observed tickrate >=20.
- FAIL: SQL/benchmark/log error or observed tickrate below the
  configured threshold.
- INCOMPLETE: stopped early or not all NPCs completed within duration.

This is an operational smoke verdict, **not** a proof of maximum online
player capacity. **PASS does not currently impose an SQL/bcrypt P95/P99
threshold**: inspect these values explicitly when comparing runs.
CPU utilization (%), RAM consumption (MB), real player RTT and network
throughput cannot be collected by this Pawn module; the final summary
prints `UNAVAILABLE` for these resource measurements rather than
inventing values. Read them from the host panel if needed.

For true player concurrency, use the separate network client harness.

### Retention and cleanup

Synthetic rows persist under the unique `run_key` matching the run directory
name (with `load_tests/` prefix) to permit inspection. The benchmark deliberately does **not**
automatically DELETE rows on abort while callbacks may be in flight.
This avoids races and accidental damage to live accounts.

After confirming that no test is active and all callbacks have drained,
clean an individual run in phpMyAdmin, for example:

```sql
DELETE FROM loadtest_virtual_accounts
WHERE run_key = 'load_tests/2026-10-26_rev01';
```

This only deletes the isolated test rows for the named report. Never
delete from `accounts` when cleaning benchmark data.

During initial tests start with five NPCs and inspect tickrate, SQL
latency and failure counts before increasing load. The external
Windows Worker is not needed for this full NPC simulation.

## LOADTEST-07: paced spawning and sustained simulation

```text
/load start 25 180 full
/load start 25 180 full 1
/load start 25 180 full 2
/load start 25 180 full 4
```

The last argument controls NPC creation rate: **1–4 NPC per second**.
The default is **1 NPC per second** for both `full` and `npc` scenarios,
reducing the initial CPU burst. The rate is deliberately capped; it does
not change the number of concurrent bcrypt tasks (2) or SQL tasks (8).

Once an NPC completes its **synthetic** registration, bcrypt verification
and first save, the server keeps a low, bounded background workload
running until the test ends:

- A staggered SQL `SELECT balance` per virtual account approximately
  every ten seconds.
- A matching `UPDATE balance=balance+1` on the *same isolated test table*.
  This is a simulation of ordinary game progression, NOT a call to any
  live character, wallet, job or economy function.
- A short window at each 60-second boundary in which updates add ten
  synthetic units instead of one (PayDay-like **workload shape only**).
- No more than **four new operations per server tick**, with at most
  **eight outstanding SQL queries** across the whole benchmark.

An extra `activity.csv` appears in the same run folder as existing
reports. Its columns are cumulative successful reads, writes, PayDay-like
writes, errors, outstanding SQL and elapsed time. The `summary.txt`
includes final counts. SQL operation counts also include activity queries.

**Interpretation:** CPU spikes during creation can be observed in the
hosting panel, not inside Pawn. This benchmark does not execute actual
PayDay, player networking, dialog registration, driving or real payments.
It models a subset of computational/SQL workload safely. For sustained
SQL percentiles, the advanced report keeps the **first 600 recorded SQL
callback durations**, not an unbounded population; late-run tail
percentiles require exporting additional samples/aggregations.

Start with `/load start 5 120 full 1`, examine the CPU panel,
`activity.csv` and `summary.txt`, then increase gradually.
