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

```
load_test_01_26.10.2026.csv
load_test_01_26.10.2026_metrics.csv
load_test_01_26.10.2026_summary.txt
```

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

The summary also reports the **p95 histogram bucket upper bounds**
for SQL and bcrypt. These are estimates, *not* exact 95th percentiles:
SQL bucket marker 501 means >500ms; bcrypt marker 1001 means >1000ms.
The summary has PASS/FAIL/INCOMPLETE:

- PASS: test reached its timeout, all requested NPCs completed
  INSERT, SELECT, bcrypt hash+verify and UPDATE, there were no
  captured errors, logging worked and minimum observed tickrate >=20.
- FAIL: SQL/benchmark/log error or observed tickrate below the
  configured threshold.
- INCOMPLETE: stopped early or not all NPCs completed within duration.

This is an operational smoke verdict, **not** a proof of maximum online
player capacity. For true player concurrency, use LOADNET-03.

### Retention and cleanup

Synthetic rows persist under the unique `run_key` matching the NPC CSV
name to permit inspection. The benchmark deliberately does **not**
automatically DELETE rows on abort while callbacks may be in flight.
This avoids races and accidental damage to live accounts.

After confirming that no test is active and all callbacks have drained,
clean an individual run in phpMyAdmin, for example:

```sql
DELETE FROM loadtest_virtual_accounts
WHERE run_key = 'load_test_01_26.10.2026.csv';
```

This only deletes the isolated test rows for the named report. Never
delete from `accounts` when cleaning benchmark data.

During initial tests start with five NPCs and inspect tickrate, SQL
latency and failure counts before increasing load. The external
Windows Worker is not needed for this full NPC simulation.
