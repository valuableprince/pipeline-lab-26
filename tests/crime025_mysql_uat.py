"""CRIME-02.5 MariaDB UAT: durable physical clue, one-time event, no bypass."""
from crime02_mysql_uat import CFG, run, scalar, begin
from crime024_mysql_uat import attempt
from crime023_mysql_uat import first_stage, resolve
import mysql.connector


def collect(con, cid, account, faction):
    begin(con)
    try:
        changed, _ = run(con, """
          UPDATE crime_contracts SET evidence_found_at=UNIX_TIMESTAMP()
          WHERE contract_id=%s AND account_id=%s AND faction_id=%s
            AND scenario_kind IN(1,2,3) AND state=1
            AND evidence_found_at=0 AND expires_at>UNIX_TIMESTAMP()
        """, (cid, account, faction))
        if changed != 1:
            run(con, "ROLLBACK")
            return False
        changed, _ = run(con, """
          INSERT INTO crime_contract_events(contract_id,actor_account_id,event_type)
          VALUES(%s,%s,'EVIDENCE_FOUND')
        """, (cid, account))
        if changed != 1:
            raise AssertionError("proof event must be unique")
        run(con, "COMMIT")
        return True
    except Exception:
        run(con, "ROLLBACK")
        raise


def main():
    con = mysql.connector.connect(**CFG)
    try:
        a = scalar(con,
            "SELECT account_id FROM accounts WHERE username='CrimeSoloA'")
        assert a is not None
        assert scalar(con, "SELECT COUNT(*) FROM schema_migrations WHERE version=36") == 1

        # Restore availability from previous CRIME-02.3/02.4 tests (disposable only).
        run(con, """UPDATE crime_contracts SET finished_at=UNIX_TIMESTAMP()-2000
            WHERE account_id=%s AND template_id=4 AND state IN(3,5)""", (a,))
        cid = attempt(con, a, 6, 4, 2)
        assert cid
        assert scalar(con,
            "SELECT evidence_found_at FROM crime_contracts WHERE contract_id=%s", (cid,)) == 0

        # A dialogue or stale client callback cannot advance without world proof.
        assert not first_stage(con, cid, a, 6, 1)
        assert not collect(con, cid, a, 7)  # Wrong syndicate.
        assert collect(con, cid, a, 6)
        assert not collect(con, cid, a, 6)  # Duplicate interaction.
        evidence_ts = scalar(con,
            "SELECT evidence_found_at FROM crime_contracts WHERE contract_id=%s", (cid,))
        assert evidence_ts > 0
        second = mysql.connector.connect(**CFG)
        try:
            assert scalar(second,
                "SELECT evidence_found_at FROM crime_contracts WHERE contract_id=%s", (cid,)) == evidence_ts
        finally:
            second.close()

        assert scalar(con, """SELECT COUNT(*) FROM crime_contract_events
            WHERE contract_id=%s AND event_type='EVIDENCE_FOUND'""", (cid,)) == 1
        assert first_stage(con, cid, a, 6, 1)
        assert not collect(con, cid, a, 6)  # No evidence events after stage change.
        assert not first_stage(con, cid, a, 6, 1)
        assert resolve(con, cid, a, 6, 1)
        assert not resolve(con, cid, a, 6, 1)
        assert scalar(con, """SELECT COUNT(*) FROM crime_contract_rewards
            WHERE contract_id=%s""", (cid,)) == 1
        assert scalar(con, """SELECT COUNT(*) FROM crime_contact_trust_events
            WHERE contract_id=%s""", (cid,)) == 1

        # No side effects for an expired/aborted investigation.
        run(con, """UPDATE crime_contracts SET finished_at=UNIX_TIMESTAMP()-2000
            WHERE account_id=%s AND template_id=4 AND state=3""", (a,))
        stale = attempt(con, a, 6, 4, 2)
        assert stale
        run(con, """UPDATE crime_contracts SET state=4,started_at=UNIX_TIMESTAMP()-3600,
            expires_at=UNIX_TIMESTAMP()-1,finished_at=UNIX_TIMESTAMP()
            WHERE contract_id=%s""", (stale,))
        assert not collect(con, stale, a, 6)
        assert scalar(con, """SELECT COUNT(*) FROM crime_contract_events
            WHERE contract_id=%s AND event_type='EVIDENCE_FOUND'""", (stale,)) == 0

        print("CRIME-02.5 MariaDB UAT: PASS (evidence, ownership, replay, expiry, persistence, reward)")
    finally:
        con.close()


if __name__ == "__main__":
    main()
