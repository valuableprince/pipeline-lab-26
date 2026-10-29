"""CRIME-02.2 MariaDB UAT: introduction, trust gates, atomic completion.

Run after tests/crime02_mysql_uat.py in the same disposable database.
"""
from crime02_mysql_uat import CFG, run, scalar, begin
import mysql.connector


def offer(con, account, faction, template_id, broker_id):
    return run(con, """
        INSERT INTO crime_contracts
        (template_id,account_id,faction_id,mode_id,reward_amount,
         started_at,expires_at,contact_id)
        SELECT t.template_id,m.account_id,m.faction_id,t.mode_id,
               t.reward_amount,UNIX_TIMESTAMP(),
               UNIX_TIMESTAMP()+t.duration_seconds,t.contact_id
        FROM crime_contract_templates t
        JOIN faction_members m ON m.account_id=%s AND m.faction_id=%s
        JOIN crime_contact_progress p ON p.account_id=m.account_id
            AND p.contact_id=t.contact_id AND p.trust>=t.required_trust
        WHERE t.template_id=%s AND t.contact_id=%s
          AND t.mode_id=1 AND t.is_enabled=1
    """, (account, faction, template_id, broker_id))


def main():
    con = mysql.connector.connect(**CFG)
    try:
        a = scalar(con, "SELECT account_id FROM accounts WHERE username='CrimeSoloA'")
        assert a
        assert scalar(con, "SELECT COUNT(*) FROM crime_contacts") == 3
        assert scalar(con, "SELECT COUNT(*) FROM crime_contract_templates") == 6
        assert scalar(con, "SELECT COUNT(*) FROM schema_migrations WHERE version=33") == 1

        # No in-world introduction means no access to the broker's offers.
        count, _ = offer(con, a, 6, 2, 2)
        assert count == 0
        run(con, """INSERT INTO crime_contact_progress
            (account_id,contact_id,trust,completed_count,first_met_at,last_met_at)
            VALUES(%s,2,0,0,UNIX_TIMESTAMP(),UNIX_TIMESTAMP())""", (a,))
        run(con, "UPDATE crime_contract_templates SET required_trust=10 WHERE template_id=2")
        count, _ = offer(con, a, 6, 2, 2)
        assert count == 0
        run(con, "UPDATE crime_contract_templates SET required_trust=0 WHERE template_id=2")

        # Broker mismatch must not allow accepting another broker's contract.
        count, _ = offer(con, a, 6, 2, 3)
        assert count == 0
        count, _ = offer(con, a, 6, 2, 2)
        assert count == 1
        cid = scalar(con, "SELECT MAX(contract_id) FROM crime_contracts WHERE account_id=%s", (a,))
        assert scalar(con, "SELECT contact_id FROM crime_contracts WHERE contract_id=%s", (cid,)) == 2

        # One live contract per account is still enforced by a generated unique index.
        try:
            offer(con, a, 6, 2, 2)
            raise AssertionError("duplicate active contract accepted")
        except mysql.connector.IntegrityError as exc:
            assert exc.errno == 1062, exc

        run(con, "UPDATE crime_contracts SET state=2 WHERE contract_id=%s", (cid,))
        old_balance = scalar(con, "SELECT balance FROM accounts WHERE account_id=%s", (a,))
        old_version = scalar(con, "SELECT row_version FROM accounts WHERE account_id=%s", (a,))
        old_serial = scalar(con, "SELECT txn_serial FROM accounts WHERE account_id=%s", (a,))
        begin(con)
        try:
            changed, _ = run(con, """
                UPDATE accounts a JOIN crime_contracts c ON c.account_id=a.account_id
                SET a.balance=a.balance+c.reward_amount,
                    a.txn_serial=a.txn_serial+1,a.row_version=a.row_version+1
                WHERE a.account_id=%s AND a.row_version=%s
                  AND a.balance<=1000000000-c.reward_amount
                  AND a.txn_serial<1000000000 AND c.contract_id=%s
                  AND c.faction_id=6 AND c.state=2
                  AND c.expires_at>UNIX_TIMESTAMP()
                  AND NOT EXISTS(SELECT 1 FROM crime_contract_rewards r
                    WHERE r.contract_id=c.contract_id)
            """, (a, old_version, cid))
            assert changed == 1
            changed, _ = run(con, """UPDATE crime_contracts SET state=3,
                finished_at=UNIX_TIMESTAMP()
                WHERE contract_id=%s AND account_id=%s AND faction_id=6
                AND state=2 AND expires_at>UNIX_TIMESTAMP()""", (cid, a))
            assert changed == 1
            changed, _ = run(con, """INSERT INTO crime_contract_rewards
                (contract_id,account_id,amount,balance_after,row_version_after,txn_serial_after)
                SELECT contract_id,%s,reward_amount,%s+reward_amount,%s+1,%s+1
                FROM crime_contracts
                WHERE contract_id=%s AND state=3 AND account_id=%s""",
                (a, old_balance, old_version, old_serial, cid, a))
            assert changed == 1
            run(con, """INSERT INTO crime_contract_events
                (contract_id,actor_account_id,event_type)
                VALUES(%s,%s,'COMPLETED')""", (cid, a))
            changed, _ = run(con, """INSERT INTO crime_contact_trust_events
                (contract_id,account_id,contact_id,trust_delta)
                SELECT contract_id,account_id,contact_id,5
                FROM crime_contracts
                WHERE contract_id=%s AND account_id=%s AND state=3""", (cid, a))
            assert changed == 1
            changed, _ = run(con, """UPDATE crime_contact_progress p
                JOIN crime_contracts c ON c.account_id=p.account_id
                    AND c.contact_id=p.contact_id
                SET p.trust=LEAST(100,p.trust+5),
                    p.completed_count=p.completed_count+1
                WHERE c.contract_id=%s AND c.account_id=%s AND c.state=3""", (cid, a))
            assert changed == 1
            run(con, "COMMIT")
        except Exception:
            run(con, "ROLLBACK")
            raise

        assert scalar(con, "SELECT balance FROM accounts WHERE account_id=%s", (a,)) == old_balance+2200
        assert scalar(con, "SELECT trust FROM crime_contact_progress WHERE account_id=%s AND contact_id=2", (a,)) == 5
        assert scalar(con, "SELECT completed_count FROM crime_contact_progress WHERE account_id=%s AND contact_id=2", (a,)) == 1
        assert scalar(con, "SELECT COUNT(*) FROM crime_contact_trust_events WHERE contract_id=%s", (cid,)) == 1
        assert scalar(con, "SELECT trust FROM crime_contact_progress WHERE account_id=%s AND contact_id=1", (a,)) is None
        # A replayed payout cannot increase either money or trust.
        changed, _ = run(con, """UPDATE crime_contracts SET state=3
            WHERE contract_id=%s AND state=2""", (cid,))
        assert changed == 0
        try:
            run(con, """INSERT INTO crime_contact_trust_events
                (contract_id,account_id,contact_id,trust_delta)
                VALUES(%s,%s,2,5)""", (cid, a))
            raise AssertionError("trust replay was accepted")
        except mysql.connector.IntegrityError as exc:
            assert exc.errno == 1062, exc
        print("CRIME-02.2 MariaDB UAT: PASS (3 NPCs, first contact, trust gate, atomic credit/trust, replay)")
    finally:
        con.close()


if __name__ == "__main__":
    main()
