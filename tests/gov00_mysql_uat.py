"""Real MariaDB/MySQL GOV-00 UAT. Run only on an isolated throwaway database."""
import os
import concurrent.futures
import mysql.connector

CONNECTION = {
    "host": os.environ.get("GOV00_TEST_HOST", "127.0.0.1"),
    "port": int(os.environ.get("GOV00_TEST_PORT", "3306")),
    "user": os.environ.get("GOV00_TEST_USER", "root"),
    "password": os.environ["GOV00_TEST_PASSWORD"],
    "database": os.environ.get("GOV00_TEST_DB", "tarkenso_gov00"),
    "autocommit": True,
}


def query(statement, params=()):
    with mysql.connector.connect(**CONNECTION) as db:
        with db.cursor() as c:
            c.execute(statement, params)
            return c.fetchall() if c.with_rows else c.rowcount


def transfer(key, account, delta, source, version):
    with mysql.connector.connect(**CONNECTION) as db:
        with db.cursor() as c:
            c.execute("CALL gov_transfer_post(%s,%s,%s,%s,%s)",
                      (key, account, delta, source, version))
            result = c.fetchone()
            while c.nextset():
                pass
            return tuple(result)


def rejects(key, account, delta, source, version):
    try:
        transfer(key, account, delta, source, version)
    except mysql.connector.Error:
        return
    raise AssertionError(f"Unsafe transfer accepted: {key}")


def state(account):
    a = query("SELECT balance,row_version,txn_serial FROM accounts WHERE account_id=%s", (account,))[0]
    t = query("SELECT balance,row_version FROM treasury_accounts WHERE code='state'")[0]
    return tuple(map(int, a + t))


def ledger_count(key):
    return int(query("SELECT COUNT(*) FROM treasury_ledger WHERE event_key=%s", (key,))[0][0])


def main():
    assert query("SELECT version FROM schema_migrations WHERE version=18") == [(18,)]
    query("INSERT INTO accounts(username,password_hash,background,balance) VALUES(%s,%s,1,1000)",
          ("gov00uat", "$2b$12$" + "x" * 53))
    account = int(query("SELECT account_id FROM accounts WHERE username='gov00uat'")[0][0])
    assert state(account) == (1000, 0, 0, 0, 0)

    first = transfer("gov00:uat:1", account, 100, "uat", 0)
    assert first == (900, 100, 1, 1, 1, 0), first
    assert state(account) == (900, 1, 1, 100, 1)
    assert ledger_count("gov00:uat:1") == 1
    assert query("SELECT COUNT(*) FROM gov_transfers") == [(1,)]

    replay = transfer("gov00:uat:1", account, 100, "uat", 0)
    assert replay == (900, 100, 1, 1, 1, 1), replay
    assert state(account) == (900, 1, 1, 100, 1)
    assert ledger_count("gov00:uat:1") == 1

    rejects("gov00:uat:1", account, 101, "uat", 1)  # collision
    rejects("gov00:uat:2", account, 100, "uat", 0)  # stale row version
    rejects("gov00:uat:3", account, 2000, "uat", 1)  # no funds
    assert state(account) == (900, 1, 1, 100, 1)

    paid = transfer("gov00:uat:4", account, -50, "uat", 1)
    assert paid == (950, 50, 2, 2, 2, 0), paid
    assert state(account) == (950, 2, 2, 50, 2)

    query("UPDATE treasury_accounts SET minimum_reserve=50 WHERE code='state'")
    rejects("gov00:uat:5", account, -1, "uat", 2)
    query("UPDATE treasury_accounts SET minimum_reserve=0,expense_enabled=0 WHERE code='state'")
    rejects("gov00:uat:6", account, -1, "uat", 2)
    query("UPDATE treasury_accounts SET expense_enabled=1 WHERE code='state'")
    assert state(account) == (950, 2, 2, 50, 2)

    # Two simultaneous requests with same event key post once, even on separate connections.
    with concurrent.futures.ThreadPoolExecutor(max_workers=2) as pool:
        results = list(pool.map(lambda _: transfer("gov00:uat:concurrent", account, 10, "uat", 2), range(2)))
    assert sorted(x[-1] for x in results) == [0, 1], results
    assert ledger_count("gov00:uat:concurrent") == 1
    assert state(account) == (940, 3, 3, 60, 3)

    # Guard wallet cap when paying players from treasury.
    query("UPDATE accounts SET balance=1000000000 WHERE account_id=%s", (account,))
    rejects("gov00:uat:cap", account, -1, "uat", 3)
    assert ledger_count("gov00:uat:cap") == 0

    # Verify conservation of money for all successful transfers, excluding
    # explicit UAT setup/change of account balance above.
    assert query("SELECT SUM(treasury_delta),COUNT(*) FROM gov_transfers") == [(60, 3)]
    print("GOV-00 real DB UAT: OK (atomicity, replay, collisions, reserve, caps, concurrency)")


if __name__ == "__main__":
    main()
