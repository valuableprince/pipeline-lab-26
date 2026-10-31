"""CRIME-02.1: isolated MariaDB transactional acceptance tests.

Run only against disposable CI database with migrations 001, 004, 031, 032.
No real player data is read or modified.
"""
import os
import mysql.connector

CFG = {
    "host": os.environ.get("CRIME_TEST_HOST", "127.0.0.1"),
    "user": os.environ.get("CRIME_TEST_USER", "root"),
    "password": os.environ["CRIME_TEST_PASSWORD"],
    "database": os.environ.get("CRIME_TEST_DB", "tarkenso_crime02"),
    "autocommit": True,
}


def run(con, sql, params=()):
    cur = con.cursor()
    try:
        cur.execute(sql, params)
        return cur.rowcount, cur.lastrowid
    finally:
        cur.close()


def scalar(con, sql, params=()):
    cur = con.cursor()
    try:
        cur.execute(sql, params)
        row = cur.fetchone()
        return row[0] if row else None
    finally:
        cur.close()


def begin(con):
    run(con, "START TRANSACTION")


def accept(con, account, faction):
    begin(con)
    try:
        count, cid = run(con, """
            INSERT INTO crime_contracts
            (template_id,account_id,faction_id,mode_id,reward_amount,started_at,expires_at)
            SELECT t.template_id,m.account_id,m.faction_id,t.mode_id,
                   t.reward_amount,UNIX_TIMESTAMP(),
                   UNIX_TIMESTAMP()+t.duration_seconds
            FROM crime_contract_templates t
            JOIN faction_members m ON m.account_id=%s AND m.faction_id=%s
            WHERE t.template_id=1 AND t.mode_id=1 AND t.is_enabled=1
        """, (account, faction))
        if count != 1:
            raise AssertionError("not a valid faction member")
        run(con, """INSERT INTO crime_contract_events(contract_id,actor_account_id,event_type)
            VALUES(%s,%s,'ACCEPTED')""", (cid, account))
        run(con, "COMMIT")
        return cid
    except Exception:
        run(con, "ROLLBACK")
        raise


def pickup(con, cid, account, faction):
    begin(con)
    try:
        count, _ = run(con, """UPDATE crime_contracts
            SET state=2,picked_up_at=UNIX_TIMESTAMP()
            WHERE contract_id=%s AND account_id=%s AND faction_id=%s
                  AND state=1 AND expires_at>UNIX_TIMESTAMP()""",
            (cid, account, faction))
        if count != 1:
            raise AssertionError("pickup already done or expired")
        run(con, """INSERT INTO crime_contract_events(contract_id,actor_account_id,event_type)
            VALUES(%s,%s,'PICKED_UP')""", (cid, account))
        run(con, "COMMIT")
    except Exception:
        run(con, "ROLLBACK")
        raise


def settle(con, cid, account, faction, expected_version):
    begin(con)
    try:
        count, _ = run(con, """UPDATE accounts a
            JOIN crime_contracts c ON c.account_id=a.account_id
            SET a.balance=a.balance+c.reward_amount,
                a.txn_serial=a.txn_serial+1,
                a.row_version=a.row_version+1
            WHERE a.account_id=%s AND a.row_version=%s
              AND a.balance<=1000000000-c.reward_amount
              AND a.txn_serial<1000000000 AND c.contract_id=%s
              AND c.faction_id=%s AND c.state=2
              AND c.expires_at>UNIX_TIMESTAMP()
              AND NOT EXISTS(SELECT 1 FROM crime_contract_rewards r
                             WHERE r.contract_id=c.contract_id)""",
            (account, expected_version, cid, faction))
        if count != 1:
            raise AssertionError("CAS/idempotency/restriction failed")
        count, _ = run(con, """UPDATE crime_contracts
            SET state=3,finished_at=UNIX_TIMESTAMP()
            WHERE contract_id=%s AND account_id=%s AND faction_id=%s
              AND state=2 AND expires_at>UNIX_TIMESTAMP()""",
            (cid, account, faction))
        if count != 1:
            raise AssertionError("invalid stage")
        run(con, """INSERT INTO crime_contract_rewards
            (contract_id,account_id,amount,balance_after,row_version_after,txn_serial_after)
            SELECT contract_id,%s,reward_amount,
                   (SELECT balance FROM accounts WHERE account_id=%s),
                   (SELECT row_version FROM accounts WHERE account_id=%s),
                   (SELECT txn_serial FROM accounts WHERE account_id=%s)
            FROM crime_contracts WHERE contract_id=%s AND state=3""",
            (account, account, account, account, cid))
        run(con, """INSERT INTO crime_contract_events(contract_id,actor_account_id,event_type)
            VALUES(%s,%s,'COMPLETED')""", (cid, account))
        run(con, "COMMIT")
    except Exception:
        run(con, "ROLLBACK")
        raise


def main():
    con = mysql.connector.connect(**CFG)
    try:
        run(con, """INSERT INTO accounts(username,password_hash,background)
            VALUES('CrimeSoloA',%s,1),('CrimeSoloB',%s,1)""",
            ("$2b$12$" + "x" * 53, "$2b$12$" + "y" * 53))
        a = scalar(con, "SELECT account_id FROM accounts WHERE username='CrimeSoloA'")
        b = scalar(con, "SELECT account_id FROM accounts WHERE username='CrimeSoloB'")
        for account, faction in ((a, 6), (b, 7)):
            run(con, """INSERT INTO faction_members
                (account_id,faction_id,rank_id,joined_at,last_seen)
                VALUES(%s,%s,1,100,100)""", (account, faction))

        # Both organizations may perform solo missions with no opponents.
        one = accept(con, a, 6)
        two = accept(con, b, 7)
        assert one != two
        assert scalar(con, "SELECT COUNT(*) FROM crime_contracts WHERE state=1") == 2
        try:
            accept(con, a, 6)
            raise AssertionError("two active contracts accepted for one player")
        except mysql.connector.Error as exc:
            assert exc.errno == 1062, exc

        # Wrong organization can neither pick up nor get paid.
        try:
            pickup(con, one, a, 7)
            raise AssertionError("cross-org pickup accepted")
        except AssertionError as exc:
            assert "pickup already" in str(exc)
        pickup(con, one, a, 6)
        try:
            pickup(con, one, a, 6)
            raise AssertionError("repeated pickup accepted")
        except AssertionError as exc:
            assert "pickup already" in str(exc)

        old_balance = scalar(con, "SELECT balance FROM accounts WHERE account_id=%s", (a,))
        try:
            settle(con, one, a, 6, 99)
            raise AssertionError("stale wallet version credited")
        except AssertionError as exc:
            assert "CAS" in str(exc)
        assert scalar(con, "SELECT balance FROM accounts WHERE account_id=%s", (a,)) == old_balance
        settle(con, one, a, 6, 0)
        assert scalar(con, "SELECT balance FROM accounts WHERE account_id=%s", (a,)) == old_balance + 2000
        assert scalar(con, "SELECT txn_serial FROM accounts WHERE account_id=%s", (a,)) == 1
        assert scalar(con, "SELECT row_version FROM accounts WHERE account_id=%s", (a,)) == 1
        assert scalar(con, "SELECT COUNT(*) FROM crime_contract_rewards WHERE account_id=%s", (a,)) == 1
        try:
            settle(con, one, a, 6, 1)
            raise AssertionError("reward replay accepted")
        except AssertionError as exc:
            assert "CAS" in str(exc)
        assert scalar(con, "SELECT balance FROM accounts WHERE account_id=%s", (a,)) == old_balance + 2000

        # Expiry closes the unique active slot, with no monetary side effects.
        run(con, """UPDATE crime_contracts SET started_at=UNIX_TIMESTAMP()-120,
            expires_at=UNIX_TIMESTAMP()-1 WHERE contract_id=%s""", (two,))
        begin(con)
        run(con, """INSERT IGNORE INTO crime_contract_events
            (contract_id,actor_account_id,event_type)
            SELECT contract_id,account_id,'EXPIRED' FROM crime_contracts
            WHERE account_id=%s AND state IN(1,2) AND expires_at<=UNIX_TIMESTAMP()""", (b,))
        run(con, """UPDATE crime_contracts SET state=4,finished_at=UNIX_TIMESTAMP()
            WHERE account_id=%s AND state IN(1,2) AND expires_at<=UNIX_TIMESTAMP()""", (b,))
        run(con, "COMMIT")
        assert scalar(con, "SELECT state FROM crime_contracts WHERE contract_id=%s", (two,)) == 4
        assert scalar(con, "SELECT COUNT(*) FROM crime_contract_events WHERE contract_id=%s AND event_type='EXPIRED'", (two,)) == 1
        assert scalar(con, "SELECT balance FROM accounts WHERE account_id=%s", (b,)) == 0
        again = accept(con, b, 7)
        assert again != two
        assert scalar(con, "SELECT COUNT(*) FROM crime_contract_events WHERE contract_id=%s", (one,)) == 3
        assert scalar(con, "SELECT COUNT(*) FROM crime_contract_rewards") == 1
        print("CRIME-02.1 MariaDB UAT: PASS (unique active, stages, cross-org, CAS, replay, expiry, journal)")
    finally:
        con.close()


if __name__ == "__main__":
    main()
