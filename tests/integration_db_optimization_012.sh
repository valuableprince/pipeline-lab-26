#!/usr/bin/env bash
# DB-OPT-012: disposable MariaDB 10.11 migration compatibility test.
set -euo pipefail

db() {
    mariadb --host=127.0.0.1 --port=3306 --user=root \
        --database=tarkenso_ci --batch --skip-column-names "$@"
}
scalar() { db --execute "$1"; }

# Apply the real committed migrations in order, not an invented fixture schema.
for n in $(seq 1 11); do
    printf -v version "%03d" "$n"
    matches=(db/migrations/${version}_*.sql)
    if [[ ${#matches[@]} -ne 1 || ! -f ${matches[0]} ]]; then
        echo "Missing or ambiguous migration $version" >&2
        exit 1
    fi
    db < "${matches[0]}"
done
[[ "$(scalar 'SELECT MAX(version) FROM schema_migrations')" == "11" ]]

db <<'SQL'
INSERT INTO accounts (username,password_hash,background)
VALUES ('Test_012',REPEAT('a',60),1);
INSERT INTO bans (username,banned_until,reason)
VALUES ('Case_Ban',0,'CI casefold fixture'),('case_ban',0,'CI duplicate fixture');
INSERT INTO municipal_truck_deliveries
(bay_id,account_id,cargo_units,salary,experience)
VALUES (0,1,10,250,10);
SQL

# The preflight MUST reject ambiguous legacy bans before any ALTER TABLE.
if db < db/migrations/012_safe_optimization.sql; then
    echo "v12 unexpectedly accepted casefold-duplicate bans" >&2
    exit 1
fi
[[ "$(scalar 'SELECT COUNT(*) FROM schema_migrations WHERE version=12')" == "0" ]]
[[ "$(scalar "SELECT COUNT(*) FROM information_schema.COLUMNS
    WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='bans'
      AND COLUMN_NAME='username_key'")" == "0" ]]

# The abort may leave only a temporary stored procedure; its next run
# explicitly drops/recreates it. Preserve the original mixed-case ban.
db --execute "DELETE FROM bans WHERE username='case_ban'"
accounts_before="$(scalar 'SELECT COUNT(*) FROM accounts')"
bans_before="$(scalar 'SELECT COUNT(*) FROM bans')"
deliveries_before="$(scalar 'SELECT COUNT(*) FROM municipal_truck_deliveries')"
cargo_before="$(scalar 'SELECT COUNT(*) FROM municipal_truck_cargo')"

db < db/migrations/012_safe_optimization.sql
[[ "$(scalar 'SELECT MAX(version) FROM schema_migrations')" == "12" ]]
[[ "$(scalar "SELECT username FROM bans WHERE username_key=LOWER('CASE_BAN')")" == "Case_Ban" ]]
[[ "$(scalar "SELECT COUNT(*) FROM information_schema.STATISTICS
    WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='municipal_truck_deliveries'
      AND INDEX_NAME='idx_mtd_account_completed'")" == "3" ]]

# Case-insensitive uniqueness must reject a new alias without changing old data.
if db --execute "INSERT INTO bans(username,banned_until,reason)
    VALUES ('CASE_BAN',0,'duplicate')"; then
    echo "v12 unique casefold ban index did not reject a duplicate" >&2
    exit 1
fi

[[ "$(scalar 'SELECT COUNT(*) FROM accounts')" == "$accounts_before" ]]
[[ "$(scalar 'SELECT COUNT(*) FROM bans')" == "$bans_before" ]]
[[ "$(scalar 'SELECT COUNT(*) FROM municipal_truck_deliveries')" == "$deliveries_before" ]]
[[ "$(scalar 'SELECT COUNT(*) FROM municipal_truck_cargo')" == "$cargo_before" ]]
[[ "$(scalar 'SELECT salary FROM municipal_truck_deliveries WHERE delivery_id=1')" == "250" ]]
[[ "$(scalar 'SELECT balance FROM accounts WHERE account_id=1')" == "0" ]]

db < db/migrations/012_safe_optimization_rollback.sql
[[ "$(scalar 'SELECT MAX(version) FROM schema_migrations')" == "11" ]]
[[ "$(scalar "SELECT COUNT(*) FROM information_schema.COLUMNS
    WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='bans'
      AND COLUMN_NAME='username_key'")" == "0" ]]
[[ "$(scalar "SELECT COUNT(*) FROM information_schema.STATISTICS
    WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='municipal_truck_deliveries'
      AND INDEX_NAME='idx_mtd_account_completed'")" == "0" ]]
[[ "$(scalar "SELECT username FROM bans WHERE LOWER(username)=LOWER('case_ban')")" == "Case_Ban" ]]
[[ "$(scalar 'SELECT COUNT(*) FROM accounts')" == "$accounts_before" ]]
[[ "$(scalar 'SELECT COUNT(*) FROM bans')" == "$bans_before" ]]
[[ "$(scalar 'SELECT COUNT(*) FROM municipal_truck_deliveries')" == "$deliveries_before" ]]
[[ "$(scalar 'SELECT COUNT(*) FROM municipal_truck_cargo')" == "$cargo_before" ]]
echo "DB-OPT-012: v11 -> v12 -> v11, no gameplay rows lost."
