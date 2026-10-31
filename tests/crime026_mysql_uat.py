"""CRIME-02.6: transactional witness interviews, locked testimony, trust outcomes."""
from crime02_mysql_uat import CFG, run, scalar
from crime024_mysql_uat import attempt
from crime025_mysql_uat import collect
from crime023_mysql_uat import first_stage, interview, resolve
import mysql.connector


def main():
    db = mysql.connector.connect(**CFG)
    try:
        account = scalar(db,
            "SELECT account_id FROM accounts WHERE username='CrimeSoloA'")
        assert account
        assert scalar(db, "SELECT COUNT(*) FROM schema_migrations WHERE version=37") == 1

        # The previous UAT suite already settled one mission. Old accounts
        # retain exactly their prior wallet, payouts, and reputation.
        base = scalar(db,
            "SELECT trust FROM crime_contact_progress WHERE account_id=%s AND contact_id=2",
            (account,))
        balance = scalar(db, "SELECT balance FROM accounts WHERE account_id=%s",(account,))
        rewards = scalar(db,"SELECT COUNT(*) FROM crime_contract_rewards WHERE account_id=%s",(account,))

        for question, expected_gain in ((1, 6), (2, 5)):
            # Advance time only inside the isolated CI test fixture.
            run(db, """UPDATE crime_contracts
                SET finished_at=UNIX_TIMESTAMP()-2000
                WHERE account_id=%s AND template_id=4 AND state IN(3,5)""",(account,))
            cid = attempt(db, account, 6, 4, 2)
            assert cid
            assert not interview(db, cid, account, 6, question)  # No found clue/stage.
            assert collect(db, cid, account, 6)
            assert first_stage(db, cid, account, 6, 1)
            assert not resolve(db, cid, account, 6, 1)  # Witness mandatory.
            assert not interview(db, cid, account, 7, question)  # Wrong faction.
            assert interview(db, cid, account, 6, question)
            assert not interview(db, cid, account, 6, 1 if question == 2 else 2)
            assert scalar(db, "SELECT interview_question FROM crime_contracts WHERE contract_id=%s",
                          (cid,)) == question
            assert scalar(db, "SELECT interviewed_at FROM crime_contracts WHERE contract_id=%s",
                          (cid,)) > 0
            # A new SQL connection sees the same response after a relog.
            other = mysql.connector.connect(**CFG)
            try:
                assert scalar(other, "SELECT interview_question FROM crime_contracts WHERE contract_id=%s",
                              (cid,)) == question
            finally:
                other.close()
            assert scalar(db, """SELECT COUNT(*) FROM crime_contract_events
                WHERE contract_id=%s AND event_type='WITNESS_INTERVIEWED'""",(cid,)) == 1
            assert resolve(db,cid,account,6,1)
            assert not resolve(db,cid,account,6,1)
            base += expected_gain
            balance += 2800
            rewards += 1
            assert scalar(db, "SELECT trust_delta FROM crime_contact_trust_events WHERE contract_id=%s",
                          (cid,)) == expected_gain
            assert scalar(db, "SELECT trust FROM crime_contact_progress WHERE account_id=%s AND contact_id=2",
                          (account,)) == base
            assert scalar(db,"SELECT balance FROM accounts WHERE account_id=%s",(account,)) == balance
            assert scalar(db,"SELECT COUNT(*) FROM crime_contract_rewards WHERE account_id=%s",
                          (account,)) == rewards

        # Migration of a pre-2.6 accepted second-stage contract. This test
        # reproduces the backfill predicate, not a new character interaction.
        run(db, """UPDATE crime_contracts SET finished_at=UNIX_TIMESTAMP()-2000
            WHERE account_id=%s AND template_id=4 AND state=3""",(account,))
        legacy = attempt(db, account, 6, 4, 2)
        assert legacy
        assert collect(db, legacy, account, 6)
        assert first_stage(db, legacy, account, 6, 1)
        run(db, """UPDATE crime_contracts
            SET interviewed_at=GREATEST(1,picked_up_at)
            WHERE contract_id=%s AND scenario_kind BETWEEN 1 AND 3
              AND state IN(2,3) AND interviewed_at=0""",(legacy,))
        assert resolve(db,legacy,account,6,1)
        assert scalar(db,"SELECT interview_question FROM crime_contracts WHERE contract_id=%s",
                      (legacy,)) == 0
        assert scalar(db,"SELECT trust_delta FROM crime_contact_trust_events WHERE contract_id=%s",
                      (legacy,)) == 5
        print("CRIME-02.6 MariaDB UAT: PASS (mandatory interview, replay, relog, +6/+5 trust, legacy)")
    finally:
        db.close()


if __name__ == "__main__":
    main()
