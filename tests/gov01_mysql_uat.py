"""GOV-01 real MariaDB integration tests. Run against throwaway CI database ONLY."""
import concurrent.futures
import os
import mysql.connector

DB = dict(
    host=os.environ.get("GOV00_TEST_HOST", "127.0.0.1"),
    port=int(os.environ.get("GOV00_TEST_PORT", "3306")),
    user=os.environ.get("GOV00_TEST_USER", "root"),
    password=os.environ["GOV00_TEST_PASSWORD"],
    database=os.environ.get("GOV00_TEST_DB", "tarkenso_gov00"),
    autocommit=True,
)


def run(sql, args=()):
    with mysql.connector.connect(**DB) as conn:
        with conn.cursor() as cursor:
            cursor.execute(sql, args)
            return cursor.fetchall() if cursor.with_rows else cursor.rowcount


def call(sql, args):
    with mysql.connector.connect(**DB) as conn:
        with conn.cursor() as cursor:
            cursor.execute(sql, args)
            result = cursor.fetchone()
            while cursor.nextset():
                pass
            return tuple(result)


def fund_state():
    return tuple(map(int, run(
        "SELECT balance,bond_fund_reserved,bond_fund_committed,"
        "bond_fund_enabled,bond_fund_utilization_bps "
        "FROM treasury_accounts WHERE code='state'")[0]))


def rejects(sql, *args):
    try:
        call(sql, args)
    except mysql.connector.Error:
        return
    raise AssertionError(f"Rejected operation unexpectedly accepted: {sql} {args}")


CAP = "CALL bond_fund_capitalize(%s,%s,%s)"
COMMIT = "CALL bond_fund_commit(%s,%s,%s,%s)"
POST = "CALL treasury_post(%s,%s,%s,%s,%s)"
XFER = "CALL gov_transfer_post(%s,%s,%s,%s,%s)"


def main():
    assert run("SELECT version FROM schema_migrations WHERE version=19") == [(19,)]
    start = fund_state()
    assert start[1:] == (0,0,0,8000), start

    # Fund has no artificial starting cash and must not create cash on capitalization.
    rejects(CAP, "fund:insufficient", start[0] + 1, None)
    rejects(COMMIT, "fund:disabled", "GOV-TEST-A", 1, None)
    new_cash = call(POST, ("gov01:capital", 150000, "uat", None, "staging_seed"))
    assert int(new_cash[0]) == start[0] + 150000

    state = fund_state()
    first = call(CAP, ("fund:cap:001", 100000, None))
    assert first[0] == state[0], first  # reserve bookkeeping: no new money
    assert first[1:3] == (100000, 0), first
    assert first[-1] == 0, first
    same = call(CAP, ("fund:cap:001", 100000, None))
    assert same[1:3] == (100000, 0) and same[-1] == 1
    rejects(CAP, "fund:cap:001", 99999, None)
    assert run("SELECT COUNT(*) FROM bond_fund_ledger WHERE event_key='fund:cap:001'") == [(1,)]
    rejects(CAP, "fund:too_much", 50061, None)  # free=50060

    # No normal outflow (neither treasury_post nor GOV-00) can touch protected funds.
    rejects(POST, "gov01:blocked", -50061, "uat", None, "protected")
    run("INSERT INTO accounts(username,password_hash,background,balance) VALUES(%s,%s,1,0)",
        ("gov01uat", "$2b$12$" + "x" * 53))
    account_id = run("SELECT account_id FROM accounts WHERE username='gov01uat'")[0][0]
    rejects(XFER, "gov00:fund:protected", account_id, -50061, "uat", 0)

    # Test minimum_reserve PLUS the insurance reserve, not max(minimum,reserve).
    run("UPDATE treasury_accounts SET minimum_reserve=1000 WHERE code='state'")
    rejects(POST, "gov01:min-reserve", -49500, "uat", None, "protected")
    rejects(XFER, "gov00:fund:minreserve", account_id, -49500, "uat", 0)
    run("UPDATE treasury_accounts SET minimum_reserve=0 WHERE code='state'")
    assert fund_state()[1:3] == (100000, 0)

    # New commitments are disabled by default, then explicitly enabled in staging.
    rejects(COMMIT, "fund:disabled2", "GOV-TEST-A", 1000, None)
    run("UPDATE treasury_accounts SET bond_fund_enabled=1,"
        "bond_fund_config_version=bond_fund_config_version+1 WHERE code='state'")
    c1 = call(COMMIT, ("fund:commit:001", "GOV-TEST-A", 75000, None))
    assert c1[1:3] == (100000, 75000) and c1[-1] == 0, c1
    duplicate = call(COMMIT, ("fund:commit:001", "GOV-TEST-A", 75000, None))
    assert duplicate[1:3] == (100000, 75000) and duplicate[-1] == 1
    rejects(COMMIT, "fund:commit:001", "GOV-TEST-B", 75000, None)
    rejects(COMMIT, "fund:commit:002", "GOV-TEST-A", 1000, None)  # same issue
    rejects(COMMIT, "fund:commit:too-large", "GOV-TEST-B", 5001, None)
    c2 = call(COMMIT, ("fund:commit:002", "GOV-TEST-B", 5000, None))
    assert c2[1:3] == (100000, 80000), c2
    assert run("SELECT COUNT(*) FROM bond_fund_guarantees") == [(2,)]

    # DB constraints are last line of defense even against direct SQL writes.
    try:
        run("UPDATE treasury_accounts SET bond_fund_committed=80001 WHERE code='state'")
        raise AssertionError("capacity CHECK bypassed")
    except mysql.connector.Error:
        pass
    try:
        run("UPDATE treasury_accounts SET bond_fund_reserved=2000000 WHERE code='state'")
        raise AssertionError("balance CHECK bypassed")
    except mysql.connector.Error:
        pass

    # Stop new issue guarantees without cancelling existing commitments.
    run("UPDATE treasury_accounts SET bond_fund_enabled=0,"
        "bond_fund_config_version=bond_fund_config_version+1 WHERE code='state'")
    rejects(COMMIT, "fund:disabled3", "GOV-TEST-C", 1, None)
    assert fund_state()[1:4] == (100000, 80000, 0)

    # Spending unreserved budget remains possible. It cannot spend even $1 of F.
    free = fund_state()[0] - fund_state()[1]
    paid = call(POST, ("gov01:permitted", -free, "uat", None, "unreserved_only"))
    assert int(paid[0]) == 100000
    rejects(POST, "gov01:protected_last_dollar", -1, "uat", None, "no")
    rejects(XFER, "gov00:fund:no_free", account_id, -1, "uat", 0)
    assert fund_state()[:3] == (100000, 100000, 80000)

    print("GOV-01 MariaDB UAT: PASS (no money creation, 80% capacity, "
          "idempotency, constraints, reserve protection in both debits)")


if __name__ == "__main__":
    main()
