# DB-OPT-012 — безопасная оптимизация MariaDB TARKENSO

Target: `valuableprince/pipeline-lab-26`, migrations v1–v11. Tested against the
schema structure in a **local exported v11 dump**, not a live production database.

## Confirmed code contracts

- `cleaner_containers` stores bin state and coordinates; `cleaner_container_bag_positions`
  stores up to ten independent placement slots per bin. Do **not** merge.
- `cleaner_profiles.earned` and `accounts.cleaner_paid` represent accrued wages and
  settlement watermark respectively. Neither may be deleted.
- `race_top` is read/written by `db_world.inc`. Do **not** delete it.
- `bans` must allow banning unregistered nicknames. Keep the original username PK.
- `cleaner_container` from v6 was intentionally retained by v10 for fallback, but
  is **absent from the supplied v11 dump**; runtime v10+ reads the plural table.
  Do not recreate or drop the old table just to alter the count of tables.
- `TruckPersist_Unload` uses a guarded InnoDB transaction: cargo reset, account
  balance and `txn_serial`, driver stats and delivery history commit together.
  Do not claim duplicate payouts without a reproducer.
- `last_payday_hour` is an **epoch-hour** counter, not 0–23; leave its data untouched.

## Changes

1. `bans.username_key` generated as `LOWER(username)` with unique index
   `uq_bans_username_key`. Pawn's three case-insensitive ban lookups now use
   an indexed equality predicate; the displayed/original username is untouched.
2. `municipal_truck_deliveries` gets `idx_mtd_account_completed` on
   `(account_id, completed_at, delivery_id)` for efficient future history.
   MariaDB can automatically remove the now-redundant implicit
   `fk_mtd_account` index, which the rollback recreates before dropping
   the composite index. Foreign-key enforcement remains enabled.
3. `DBPrimary_Init` requires migration 12 for the new Pawn query.
4. All finance tables, salaries, contracts, and existing indexes are unchanged.

## Preconditions / compatibility gate

**Do not run against the live service while players are online.** Back up
MariaDB and server build first, record the backup location, and stop the game server.
Check that the current database is the expected `s28_db1`, and inspect the
following read-only queries:

```sql
SELECT version FROM schema_migrations ORDER BY version;
SELECT LOWER(username) AS normalized_name, COUNT(*) AS duplicate_count
FROM bans GROUP BY LOWER(username) HAVING COUNT(*) > 1;
SELECT COUNT(*) AS bans_before FROM bans;
SELECT COUNT(*) AS deliveries_before FROM municipal_truck_deliveries;
SELECT COUNT(*) AS accounts_before FROM accounts;
SELECT COUNT(*) AS cargo_before FROM municipal_truck_cargo;
SELECT INDEX_NAME, COLUMN_NAME, SEQ_IN_INDEX
FROM information_schema.STATISTICS
WHERE TABLE_SCHEMA=DATABASE()
  AND TABLE_NAME IN ('bans','municipal_truck_deliveries')
ORDER BY TABLE_NAME, INDEX_NAME, SEQ_IN_INDEX;
```

The migration procedure independently checks v11, absence of later versions,
required column names, case-insensitive ban collisions and conflicting target
index/column names. Any violation aborts **before data-table DDL**.

## Apply

With the game server **stopped**, run `db/migrations/012_safe_optimization.sql`
in MariaDB (requires the database owner's `CREATE ROUTINE` / `ALTER` grants).
**Do not split** the `DELIMITER` / procedure block. If the SQL tool cannot
run routines, use a suitable MariaDB administrative client. Do not manually
mark v12 as applied.

After success, verify:

```sql
SELECT version FROM schema_migrations WHERE version=12;
SHOW INDEX FROM bans WHERE Key_name='uq_bans_username_key';
SHOW INDEX FROM municipal_truck_deliveries
  WHERE Key_name='idx_mtd_account_completed';
SELECT COUNT(*) AS bans_after FROM bans;
SELECT COUNT(*) AS deliveries_after FROM municipal_truck_deliveries;
SELECT COUNT(*) AS accounts_after FROM accounts;
SELECT COUNT(*) AS cargo_after FROM municipal_truck_cargo;
```

Compare counts with the preflight snapshot; counts should match when the server
is stopped. Deploy the new, CI-compiled AMX **after** schema v12 succeeds.
Verify startup schema check, ban/unban (including case variants), cleaner
payments, PayDay, container editor and a municipal delivery on staging first.

## Rollback / partial failure

MariaDB DDL implicitly commits: migration 12 is not one atomic transaction.
If either `ALTER TABLE` fails, the migration marker is not written, but an
earlier `ALTER` may have succeeded. The migration is intentionally fail-closed
rather than guessing a repair. Keep the server stopped, inspect the schema,
and run `db/migrations/012_safe_optimization_rollback.sql`. It conditionally
restores the original FK-supporting index if MariaDB removed it, drops
the two v12 indexes and generated column, then removes only
`schema_migrations.version=12`. Data rows are not removed.

For rollback of a successfully deployed server, **restore the pre-v12 AMX before
running rollback SQL**; its ban queries do not reference `bans.username_key`.
Do not roll back v12 after a subsequent v13+ migration without a new plan.
The rollback script does not restore a DB backup or reverse gameplay writes.
