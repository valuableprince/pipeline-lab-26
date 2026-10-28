"""GOV-07.2 regression: UTC epoch hour works under a +03:00 SQL session.

Run ONLY after GOV-07/07.1 MariaDB UAT on disposable tarkenso_gov00.
A replay of the existing coupon event must succeed independently of SQL
session time zone without creating new ledger entries or coupon money.
"""
import os
from datetime import datetime, timezone

import mysql.connector

database = os.environ.get("GOV00_TEST_DB", "tarkenso_gov00")
assert database == "tarkenso_gov00", "Refuse non-disposable GOV-07 timezone UAT"
db = mysql.connector.connect(
    host=os.environ.get("GOV00_TEST_HOST", "127.0.0.1"),
    port=int(os.environ.get("GOV00_TEST_PORT", "3306")),
    user=os.environ.get("GOV00_TEST_USER", "root"),
    password=os.environ["GOV00_TEST_PASSWORD"],
    database=database,
    autocommit=True,
)

def rows(sql, args=()):
    with db.cursor() as cursor:
        cursor.execute(sql, args)
        return cursor.fetchall()

def call_accrue(issue_id, hour_key):
    with db.cursor() as cursor:
        cursor.execute("CALL gov_bond_coupon_accrue(%s,%s)", (issue_id, hour_key))
        result = cursor.fetchone()
        while cursor.nextset():
            pass
        return result

def main():
    assert rows("SELECT version FROM schema_migrations WHERE version=28") == [(28,)]
    event = rows(
        "SELECT issue_id,hour_key FROM government_bond_coupon_events "
        "ORDER BY hour_key DESC LIMIT 1"
    )
    assert event, "GOV-07/07.1 UAT must run before timezone regression"
    issue_id, hour_key = event[0]
    snapshot = rows(
        "SELECT account_id,accrued,collected,converted FROM "
        "government_bond_coupon_wallets WHERE issue_id=%s ORDER BY account_id",
        (issue_id,),
    )
    events_before = rows(
        "SELECT COUNT(*) FROM government_bond_coupon_events WHERE issue_id=%s",
        (issue_id,),
    )[0][0]
    notices_before = rows(
        "SELECT COUNT(*) FROM government_bond_coupon_notice_queue WHERE issue_id=%s",
        (issue_id,),
    )[0][0]

    # Reproduce the reported MySQL environment: SYSTEM/+03:00 vs UTC.
    with db.cursor() as cursor:
        cursor.execute("SET SESSION time_zone = '+03:00'")
    actual, broken, session_tz = rows(
        "SELECT UNIX_TIMESTAMP() DIV 3600, "
        "UNIX_TIMESTAMP(UTC_TIMESTAMP()) DIV 3600, @@session.time_zone"
    )[0]
    assert session_tz == "+03:00", session_tz
    assert actual - broken == 3, (actual, broken)
    assert hour_key <= actual, (hour_key, actual)

    # Old migration 027 rejects the valid hour with 'invalid coupon payday'.
    replay = call_accrue(issue_id, hour_key)
    assert replay[0] == issue_id and replay[-1] == 1, replay

    # A future hour must still be rejected (UTC-based guard remains fail-closed).
    future = int(datetime.now(timezone.utc).timestamp()) // 3600 + 1
    try:
        call_accrue(issue_id, future)
    except mysql.connector.Error as err:
        assert "invalid coupon payday" in str(err), err
    else:
        raise AssertionError("Future PayDay was not rejected")

    assert rows(
        "SELECT COUNT(*) FROM government_bond_coupon_events WHERE issue_id=%s",
        (issue_id,),
    ) == [(events_before,)], "Replay created another coupon event"
    assert rows(
        "SELECT account_id,accrued,collected,converted FROM "
        "government_bond_coupon_wallets WHERE issue_id=%s ORDER BY account_id",
        (issue_id,),
    ) == snapshot, "Replay modified coupon balances"
    assert rows(
        "SELECT COUNT(*) FROM government_bond_coupon_notice_queue WHERE issue_id=%s",
        (issue_id,),
    ) == [(notices_before,)], "Replay created duplicate notifications"
    with db.cursor() as cursor:
        cursor.execute("CALL gov_finance_assert_integrity()")
        integrity = cursor.fetchone()
        while cursor.nextset():
            pass
    assert integrity[0] == "PASS", integrity
    print("GOV-07.2 PASS: +03 SQL timezone, UTC PayDay hour, replay, future guard, integrity")

if __name__ == "__main__":
    try:
        main()
    finally:
        db.close()
