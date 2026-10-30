"""CRIME-02.3 isolated MariaDB UAT after CRIME-02.1 and CRIME-02.2."""
from crime02_mysql_uat import CFG, run, scalar, begin
import mysql.connector


def accept(con, account, faction, template, contact):
    begin(con)
    try:
        changed, cid = run(con, """
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
            WHERE t.template_id=%s AND t.contact_id=%s
              AND t.mode_id=1 AND t.is_enabled=1
              AND (t.is_repeatable=1 OR NOT EXISTS(
                  SELECT 1 FROM crime_contracts h WHERE h.account_id=m.account_id
                  AND h.template_id=t.template_id AND h.state=3))
              AND NOT EXISTS(SELECT 1 FROM crime_contracts h
                  WHERE h.account_id=m.account_id AND h.template_id=t.template_id
                  AND h.state IN(3,5)
                  AND h.finished_at>UNIX_TIMESTAMP()-t.cooldown_seconds)
        """, (account, faction, template, contact))
        assert changed == 1
        run(con, """INSERT INTO crime_contract_events
            (contract_id,actor_account_id,event_type)
            VALUES(%s,%s,'ACCEPTED')""", (cid, account))
        run(con, "COMMIT")
        return cid
    except Exception:
        run(con, "ROLLBACK")
        raise


def first_stage(con, cid, account, faction, clue):
    begin(con)
    try:
        changed, _ = run(con, """UPDATE crime_contracts
            SET state=2,picked_up_at=UNIX_TIMESTAMP(),scenario_choice=%s
            WHERE contract_id=%s AND account_id=%s AND faction_id=%s
              AND state=1 AND expires_at>UNIX_TIMESTAMP()
              AND (scenario_kind=0 OR (evidence_found_at>0 AND %s IN(1,2)))""",
            (clue, cid, account, faction, clue))
        if changed != 1:
            run(con, "ROLLBACK")
            return False
        run(con, """INSERT INTO crime_contract_events
            (contract_id,actor_account_id,event_type)
            VALUES(%s,%s,'CLUE_FOUND')""", (cid, account))
        run(con, "COMMIT")
        return True
    except Exception:
        run(con, "ROLLBACK")
        raise


def interview(con, cid, account, faction, question):
    begin(con)
    try:
        changed, _ = run(con, """UPDATE crime_contracts
            SET interview_question=%s,interviewed_at=UNIX_TIMESTAMP()
            WHERE contract_id=%s AND account_id=%s AND faction_id=%s
              AND scenario_kind IN(1,2,3) AND state=2
              AND scenario_choice IN(1,2) AND interviewed_at=0
              AND expires_at>UNIX_TIMESTAMP()""",
            (question,cid,account,faction))
        if changed != 1:
            run(con, "ROLLBACK")
            return False
        run(con, """INSERT INTO crime_contract_events
            (contract_id,actor_account_id,event_type)
            VALUES(%s,%s,'WITNESS_INTERVIEWED')""",(cid,account))
        run(con, "COMMIT")
        return True
    except Exception:
        run(con, "ROLLBACK")
        raise


def resolve(con, cid, account, faction, decision):
    begin(con)
    try:
        version = scalar(con, "SELECT row_version FROM accounts WHERE account_id=%s", (account,))
        changed, _ = run(con, """
            UPDATE accounts a JOIN crime_contracts c ON c.account_id=a.account_id
            SET a.balance=a.balance+c.reward_amount,a.txn_serial=a.txn_serial+1,
                a.row_version=a.row_version+1
            WHERE a.account_id=%s AND a.row_version=%s
              AND a.balance<=1000000000-c.reward_amount
              AND a.txn_serial<1000000000 AND c.contract_id=%s
              AND c.faction_id=%s AND c.state=2 AND c.expires_at>UNIX_TIMESTAMP()
              AND (c.scenario_kind=0 OR
                (c.scenario_choice IN(1,2) AND c.interviewed_at>0 AND %s IN(1,2)))
              AND NOT EXISTS(SELECT 1 FROM crime_contract_rewards r
                             WHERE r.contract_id=c.contract_id)""",
            (account, version, cid, faction, decision))
        if changed != 1:
            run(con, "ROLLBACK")
            return False
        changed, _ = run(con, """UPDATE crime_contracts
            SET state=3,finished_at=UNIX_TIMESTAMP(),scenario_resolution=%s,
                risk_delta=CASE WHEN scenario_kind=0 THEN -8 WHEN %s=2 THEN 20
                  WHEN interview_question=2 THEN 8 WHEN scenario_choice=2 THEN 5 ELSE -12 END
            WHERE contract_id=%s AND account_id=%s AND faction_id=%s
              AND state=2 AND expires_at>UNIX_TIMESTAMP()""",
            (decision, decision, cid, account, faction))
        assert changed == 1
        changed, _ = run(con, """
            INSERT INTO crime_contract_rewards
            (contract_id,account_id,amount,balance_after,row_version_after,txn_serial_after)
            SELECT contract_id,%s,reward_amount,
              (SELECT balance FROM accounts WHERE account_id=%s),
              (SELECT row_version FROM accounts WHERE account_id=%s),
              (SELECT txn_serial FROM accounts WHERE account_id=%s)
            FROM crime_contracts WHERE contract_id=%s AND state=3""",
            (account, account, account, account, cid))
        assert changed == 1
        run(con, """INSERT INTO crime_contract_events
            (contract_id,actor_account_id,event_type)
            VALUES(%s,%s,'COMPLETED')""", (cid, account))
        changed, _ = run(con, """INSERT INTO crime_contact_trust_events
            (contract_id,account_id,contact_id,trust_delta)
            SELECT contract_id,account_id,contact_id,
                CASE WHEN scenario_kind=0 THEN 5
                     WHEN scenario_resolution=2 THEN 2
                     WHEN scenario_choice=1 THEN 5+(CASE WHEN interview_question=1 THEN 1 ELSE 0 END) ELSE 3 END
            FROM crime_contracts WHERE contract_id=%s
              AND account_id=%s AND state=3""", (cid, account))
        assert changed == 1
        changed, _ = run(con, """
            UPDATE crime_contact_progress p
            JOIN crime_contracts c ON c.account_id=p.account_id
                 AND c.contact_id=p.contact_id
            SET p.trust=LEAST(100,p.trust+(CASE
                WHEN c.scenario_kind=0 THEN 5
                WHEN c.scenario_resolution=2 THEN 2
                WHEN c.scenario_choice=1 THEN 5+(CASE WHEN c.interview_question=1 THEN 1 ELSE 0 END) ELSE 3 END)),
                p.completed_count=p.completed_count+1
            WHERE c.contract_id=%s AND c.account_id=%s AND c.state=3""",
            (cid, account))
        assert changed == 1
        # Risk ledger is created before the progress update. A transaction
        # rollback must undo both heat and the money sent to the account.
        changed, _ = run(con, """
            INSERT INTO crime_contact_heat_events
            (contract_id,account_id,contact_id,heat_delta)
            SELECT c.contract_id,c.account_id,c.contact_id,
              GREATEST(-CAST(p.heat AS SIGNED),
                LEAST(100-CAST(p.heat AS SIGNED),c.risk_delta))
            FROM crime_contracts c JOIN crime_contact_progress p
              ON p.account_id=c.account_id AND p.contact_id=c.contact_id
            WHERE c.contract_id=%s AND c.account_id=%s AND c.state=3
        """,(cid,account))
        assert changed == 1
        changed, _ = run(con, """
            UPDATE crime_contact_progress p JOIN crime_contact_heat_events h
              ON h.account_id=p.account_id AND h.contact_id=p.contact_id
            SET p.heat=GREATEST(0,LEAST(100,CAST(p.heat AS SIGNED)+h.heat_delta)),
                p.heat_event_count=p.heat_event_count+1
            WHERE h.contract_id=%s AND h.account_id=%s
        """,(cid,account))
        assert changed == 1
        changed,_=run(con, """
            INSERT INTO crime_syndicate_influence_events
             (contract_id,faction_id,contact_id,influence_delta,standing_delta)
            SELECT c.contract_id,c.faction_id,c.contact_id,
              (CASE WHEN c.scenario_kind=0 THEN 2 ELSE 4 END)+
               (CASE WHEN EXISTS(SELECT 1 FROM crime_syndicate_assets a
                 WHERE a.faction_id=c.faction_id AND a.contact_id=c.contact_id
                   AND a.asset_code=1) THEN 1 ELSE 0 END),
              (CASE WHEN c.scenario_kind=0 THEN 2 ELSE 4 END)+
               (CASE WHEN EXISTS(SELECT 1 FROM crime_syndicate_assets a
                 WHERE a.faction_id=c.faction_id AND a.contact_id=c.contact_id
                   AND a.asset_code=2) THEN 1 ELSE 0 END)
            FROM crime_contracts c WHERE c.contract_id=%s
              AND c.account_id=%s AND c.faction_id=%s AND c.state=3
        """,(cid,account,faction))
        assert changed==1
        changed,_=run(con, """
            UPDATE crime_syndicate_progress p JOIN crime_syndicate_influence_events e
              ON e.faction_id=p.faction_id
            SET p.influence=LEAST(1000000,p.influence+e.influence_delta),
                p.completed_count=p.completed_count+1
            WHERE e.contract_id=%s AND e.faction_id=%s
        """,(cid,faction))
        assert changed==1
        changed,_=run(con, """
            UPDATE crime_syndicate_contacts p JOIN crime_syndicate_influence_events e
              ON e.faction_id=p.faction_id AND e.contact_id=p.contact_id
            SET p.standing=LEAST(100,p.standing+e.standing_delta),
                p.completed_count=p.completed_count+1
            WHERE e.contract_id=%s AND e.faction_id=%s
        """,(cid,faction))
        assert changed==1
        run(con, "COMMIT")
        return True
    except Exception:
        run(con, "ROLLBACK")
        raise


def main():
    con = mysql.connector.connect(**CFG)
    try:
        account = scalar(con,
            "SELECT account_id FROM accounts WHERE username='CrimeSoloA'")
        assert account
        assert scalar(con, "SELECT COUNT(*) FROM crime_contract_templates") == 6
        assert scalar(con, "SELECT COUNT(*) FROM schema_migrations WHERE version=37") == 1
        # Keep the original branching outcome test focused on settlement;
        # retry policies have their own integration acceptance suite.
        run(con, "UPDATE crime_contract_templates SET cooldown_seconds=0 WHERE template_id=4")
        # Original templates remain couriers.
        assert scalar(con, "SELECT COUNT(*) FROM crime_contract_templates WHERE scenario_kind=0") == 3
        assert scalar(con, "SELECT COUNT(*) FROM crime_contract_templates WHERE scenario_kind BETWEEN 1 AND 3") == 3
        trust_start = scalar(con, """SELECT trust FROM crime_contact_progress
            WHERE account_id=%s AND contact_id=2""", (account,))
        assert trust_start == 5
        old_balance = scalar(con, "SELECT balance FROM accounts WHERE account_id=%s", (account,))
        earned = 0
        gained = 0
        for clue, decision, expected_trust in ((1, 1, 5), (2, 1, 3), (1, 2, 2)):
            cid = accept(con, account, 6, 4, 2)
            assert scalar(con, "SELECT scenario_kind FROM crime_contracts WHERE contract_id=%s", (cid,)) == 1
            assert not resolve(con, cid, account, 6, 1)  # Cannot bypass clue.
            assert not first_stage(con, cid, account, 6, clue)  # Evidence not touched.
            assert not first_stage(con, cid, account, 7, clue)  # Wrong faction.
            run(con, """UPDATE crime_contracts SET evidence_found_at=UNIX_TIMESTAMP()
                WHERE contract_id=%s AND account_id=%s AND state=1""", (cid, account))
            run(con, """INSERT INTO crime_contract_events
                (contract_id,actor_account_id,event_type)
                VALUES(%s,%s,'EVIDENCE_FOUND')""", (cid, account))
            assert first_stage(con, cid, account, 6, clue)
            assert not first_stage(con, cid, account, 6, clue)  # No replay.
            assert scalar(con, "SELECT scenario_choice FROM crime_contracts WHERE contract_id=%s", (cid,)) == clue
            assert not resolve(con, cid, account, 6, 0)  # Invalid resolution.
            assert not resolve(con, cid, account, 6, decision)  # Witness not interviewed.
            assert interview(con, cid, account, 6, 2)  # Pressure: legacy trust values.
            assert not interview(con, cid, account, 6, 1)  # Cannot change testimony.
            assert resolve(con, cid, account, 6, decision)
            assert not resolve(con, cid, account, 6, decision)
            assert scalar(con, "SELECT scenario_resolution FROM crime_contracts WHERE contract_id=%s", (cid,)) == decision
            assert scalar(con, "SELECT trust_delta FROM crime_contact_trust_events WHERE contract_id=%s", (cid,)) == expected_trust
            assert scalar(con, "SELECT COUNT(*) FROM crime_contract_events WHERE contract_id=%s", (cid,)) == 5
            earned += 2800
            gained += expected_trust
            assert scalar(con, "SELECT balance FROM accounts WHERE account_id=%s", (account,)) == old_balance+earned
            assert scalar(con, """SELECT trust FROM crime_contact_progress
                WHERE account_id=%s AND contact_id=2""", (account,)) == trust_start+gained
        # Isolated per-contact trust and unused template scenarios.
        assert scalar(con, "SELECT trust FROM crime_contact_progress WHERE account_id=%s AND contact_id=3", (account,)) is None
        print("CRIME-02.3 MariaDB UAT: PASS (clues, faction gating, +5/+3/+2 outcomes, replay and rollback)")
    finally:
        con.close()


if __name__ == "__main__":
    main()
