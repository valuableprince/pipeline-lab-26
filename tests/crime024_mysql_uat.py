"""CRIME-02.4 isolated MariaDB UAT, executed after earlier CRIME suites."""
from crime02_mysql_uat import CFG, run, scalar, begin
import mysql.connector


def attempt(con, account, faction, template, broker):
    begin(con)
    try:
        count, cid = run(con, """
          INSERT INTO crime_contracts
            (template_id,account_id,faction_id,mode_id,reward_amount,
             started_at,expires_at,contact_id,scenario_kind,scenario_event)
          SELECT t.template_id,m.account_id,m.faction_id,t.mode_id,
            t.reward_amount,UNIX_TIMESTAMP(),
            UNIX_TIMESTAMP()+t.duration_seconds,t.contact_id,t.scenario_kind,
            CASE WHEN t.scenario_kind=0 THEN 0 ELSE 1+FLOOR(RAND()*3) END
          FROM crime_contract_templates t
          JOIN faction_members m ON m.account_id=%s AND m.faction_id=%s
          JOIN crime_contact_progress p ON p.account_id=m.account_id
            AND p.contact_id=t.contact_id AND p.trust>=t.required_trust
              AND (t.scenario_kind=0 OR p.heat<70)
          WHERE t.template_id=%s AND t.contact_id=%s AND t.mode_id=1 AND t.is_enabled=1
            AND (t.is_repeatable=1 OR NOT EXISTS(
                SELECT 1 FROM crime_contracts h WHERE h.account_id=m.account_id
                  AND h.template_id=t.template_id AND h.state=3))
            AND NOT EXISTS(SELECT 1 FROM crime_contracts h
                WHERE h.account_id=m.account_id AND h.template_id=t.template_id
                  AND h.state IN(3,5)
                  AND h.finished_at>UNIX_TIMESTAMP()-(t.cooldown_seconds+p.heat*10))
        """, (account, faction, template, broker))
        if count == 1:
            run(con, """INSERT INTO crime_contract_events
                (contract_id,actor_account_id,event_type)
                VALUES(%s,%s,'ACCEPTED')""", (cid, account))
        run(con, "COMMIT")
        return cid if count == 1 else None
    except Exception:
        run(con, "ROLLBACK")
        raise


def close(con, cid, state):
    assert state in (3, 5)
    run(con, "UPDATE crime_contracts SET state=%s,finished_at=UNIX_TIMESTAMP() WHERE contract_id=%s", (state, cid))


def main():
    con = mysql.connector.connect(**CFG)
    try:
        account = scalar(con,
            "SELECT account_id FROM accounts WHERE username='CrimeSoloA'")
        assert account
        assert scalar(con, "SELECT COUNT(*) FROM schema_migrations WHERE version=35") == 1
        assert scalar(con, "SELECT COUNT(*) FROM crime_contract_templates WHERE cooldown_seconds=300") == 3
        # Prior scenario UAT deliberately sets template 4 cooldown to 0;
        # restore the real production default for this acceptance.
        run(con, "UPDATE crime_contract_templates SET cooldown_seconds=900 WHERE template_id=4")
        assert scalar(con, "SELECT COUNT(*) FROM crime_contract_templates WHERE cooldown_seconds=900") == 3

        # Player just finished this investigation in prior CRIME-02.3 test.
        assert attempt(con, account, 6, 4, 2) is None

        # Backdate only the disposable test fixture to simulate cooldown expiry.
        run(con, """UPDATE crime_contracts SET finished_at=UNIX_TIMESTAMP()-3000
            WHERE account_id=%s AND template_id=4 AND state=3""", (account,))
        first = attempt(con, account, 6, 4, 2)
        assert first
        event = scalar(con, "SELECT scenario_event FROM crime_contracts WHERE contract_id=%s", (first,))
        assert event in (1, 2, 3)
        assert scalar(con, "SELECT scenario_kind FROM crime_contracts WHERE contract_id=%s", (first,)) == 1
        # Reading the same contract from a fresh database connection must retain the event.
        other = mysql.connector.connect(**CFG)
        try:
            assert scalar(other, "SELECT scenario_event FROM crime_contracts WHERE contract_id=%s", (first,)) == event
        finally:
            other.close()
        # Active unique-account constraint still rejects a second operation.
        try:
            attempt(con, account, 6, 4, 2)
            raise AssertionError("second active contract was accepted")
        except mysql.connector.IntegrityError as exc:
            assert exc.errno == 1062, exc

        # Aborting should close the slot but briefly delay exact template replay.
        close(con, first, 5)
        assert attempt(con, account, 6, 4, 2) is None
        run(con, "UPDATE crime_contracts SET finished_at=UNIX_TIMESTAMP()-3000 WHERE contract_id=%s", (first,))
        second = attempt(con, account, 6, 4, 2)
        assert second and second != first
        assert scalar(con, "SELECT scenario_event FROM crime_contracts WHERE contract_id=%s", (second,)) in (1, 2, 3)
        close(con, second, 5)

        # One-time story gate rejects previously completed template even if the
        # cooldown expired. This option is ready for future unique narratives.
        run(con, "UPDATE crime_contract_templates SET is_repeatable=0 WHERE template_id=4")
        assert attempt(con, account, 6, 4, 2) is None
        run(con, "UPDATE crime_contract_templates SET is_repeatable=1 WHERE template_id=4")

        # Other template remains available; no global timer at low online.
        run(con, """UPDATE crime_contracts SET finished_at=UNIX_TIMESTAMP()-3000
            WHERE account_id=%s AND template_id=2 AND state=3""", (account,))
        third = attempt(con, account, 6, 2, 2)
        assert third
        assert scalar(con, "SELECT scenario_event FROM crime_contracts WHERE contract_id=%s", (third,)) == 0
        close(con, third, 5)

        # No rewards/trust are earned for aborts or denied offers.
        assert scalar(con, "SELECT COUNT(*) FROM crime_contract_rewards WHERE contract_id IN(%s,%s,%s)", (first,second,third)) == 0
        assert scalar(con, "SELECT COUNT(*) FROM crime_contact_trust_events WHERE contract_id IN(%s,%s,%s)", (first,second,third)) == 0
        print("CRIME-02.4 MariaDB UAT: PASS (cooldown, one-time, event persistence, retries, independent missions)")
    finally:
        con.close()


if __name__ == "__main__":
    main()
