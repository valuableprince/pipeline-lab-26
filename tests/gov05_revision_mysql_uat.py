"""GOV-05.1: actual-opening deadlines and auditable returned bond applications.
Runs only on the disposable GOV00--06 MariaDB test schema after migration 025.
"""
from datetime import datetime, timezone, timedelta
import os
import mysql.connector

DB=dict(host=os.environ.get("GOV00_TEST_HOST","127.0.0.1"),
        port=int(os.environ.get("GOV00_TEST_PORT","3306")),
        user=os.environ.get("GOV00_TEST_USER","root"),
        password=os.environ["GOV00_TEST_PASSWORD"],
        database=os.environ.get("GOV00_TEST_DB","tarkenso_gov00"),autocommit=True)

def q(statement,values=()):
    with mysql.connector.connect(**DB) as db,db.cursor() as cursor:
        cursor.execute(statement,values)
        return cursor.fetchall() if cursor.with_rows else cursor.rowcount

def call(name,*args):
    with mysql.connector.connect(**DB) as db,db.cursor() as cursor:
        cursor.execute("CALL "+name+"("+",".join(["%s"]*len(args))+")",args)
        out=cursor.fetchone()
        while cursor.nextset(): pass
        return out

def deny(name,*args):
    try: call(name,*args)
    except mysql.connector.Error: return
    raise AssertionError(f"Unexpected SQL success: {name} {args!r}")

def main():
    assert q("SELECT version FROM schema_migrations WHERE version=25")==[(25,)]
    specialist=q("SELECT account_id FROM accounts WHERE username='gov00uat'")[0][0]
    deputy=q("SELECT account_id FROM accounts WHERE username='gov01uat'")[0][0]
    minister=q("SELECT account_id FROM accounts WHERE username='gov03buyer1'")[0][0]
    trainee=q("SELECT account_id FROM accounts WHERE username='gov03buyer2'")[0][0]
    # Previous UAT intentionally removed specialist and demoted deputy.
    q("INSERT INTO faction_members(account_id,faction_id,rank_id,joined_at,last_seen)"
      " VALUES(%s,5,2,UNIX_TIMESTAMP(),UNIX_TIMESTAMP())",(specialist,))
    q("UPDATE faction_members SET rank_id=3 WHERE faction_id=5 AND account_id=%s",(deputy,))
    before=q("SELECT balance,bond_fund_reserved,bond_fund_committed,"
             "bond_payout_reserved FROM treasury_accounts WHERE code='state'")[0]

    issue=call("gov_bond_issue_create_auto","bond:gov51:create",specialist,
               "City logistics program","Intermodal roads and bridges",
               "TARGETED",100,10,150,7,30)
    code=issue[1]
    assert code=="GOV-000010" and issue[2]=="DRAFT",issue
    reason="Уточните смету строительства и количество облигаций"
    deny("gov_bond_official_return","bond:gov51:return:low",code,trainee,reason)
    deny("gov_bond_official_return","bond:gov51:return:self",code,specialist,reason)
    returned=call("gov_bond_official_return","bond:gov51:return",code,deputy,reason)
    assert returned[1:4]==("RETURNED",specialist,reason),returned
    assert call("gov_bond_official_return","bond:gov51:return",code,deputy,reason)[-1]==1
    deny("gov_bond_official_return","bond:gov51:return:again",code,deputy,reason)
    deny("gov_bond_official_approve","bond:gov51:sign:returned",code,minister)
    deny("gov_bond_issue_resubmit","bond:gov51:resubmit:trainee",
         code,trainee,"New City Logistics","Bridges","TARGETED",100,5,150,7,30)
    deny("gov_bond_issue_resubmit","bond:gov51:resubmit:deputy",
         code,deputy,"New City Logistics","Bridges","TARGETED",100,5,150,7,30)
    assert q("SELECT reason,revision_number FROM government_bond_revision_reviews "
             "WHERE issue_id=%s",(issue[0],))==[(reason,1)]
    res=call("gov_bond_issue_resubmit","bond:gov51:resubmit",code,specialist,
             "New City Logistics","Bridge renewal","TARGETED",100,5,150,7,30)
    assert res[0:3]==(issue[0],code,"DRAFT"),res
    assert call("gov_bond_issue_resubmit","bond:gov51:resubmit",code,specialist,
                "New City Logistics","Bridge renewal","TARGETED",100,5,150,7,30)[-1]==1
    deny("gov_bond_issue_resubmit","bond:gov51:resubmit",code,specialist,
         "Changed Again","Bridge renewal","TARGETED",100,5,150,7,30)
    assert q("SELECT status,returned_reason,revision_count,yield_basis_points,"
             "max_quantity FROM government_bond_issues WHERE issue_id=%s",
             (issue[0],))==[("DRAFT",None,1,150,5)]
    assert q("SELECT action,from_status,to_status FROM government_bond_issue_audit "
             "WHERE issue_id=%s ORDER BY event_id",(issue[0],))==[
                 ("CREATE",None,"DRAFT"),("RETURN","DRAFT","RETURNED"),
                 ("RESUBMIT","RETURNED","DRAFT")]
    # Simulate a long minister review without advancing any real wall clock.
    # Durations remain 7 and 30 days; timestamps are deliberately stale.
    q("UPDATE government_bond_issues SET "
      "offer_start_at=UTC_TIMESTAMP()-INTERVAL 90 DAY,"
      "offer_end_at=UTC_TIMESTAMP()-INTERVAL 83 DAY,"
      "maturity_at=UTC_TIMESTAMP()-INTERVAL 53 DAY WHERE issue_id=%s",(issue[0],))
    assert call("gov_bond_official_approve",
                "bond:gov51:sign",code,minister)[1]=="APPROVED"
    now=datetime.now(timezone.utc).replace(tzinfo=None)
    assert call("gov_bond_official_open",
                "bond:gov51:open",code,deputy)[1]=="OFFERING"
    first=q("SELECT opened_at,offer_start_at,offer_end_at,maturity_at "
            "FROM government_bond_issues WHERE issue_id=%s",(issue[0],))[0]
    assert abs((first[0]-now).total_seconds())<120,first
    assert first[1]==first[0],first
    assert first[2]-first[1]==timedelta(days=7),first
    assert first[3]-first[2]==timedelta(days=30),first
    assert call("gov_bond_official_open",
                "bond:gov51:open",code,deputy)[-1]==1
    deny("gov_bond_official_return","bond:gov51:return:opened",code,minister,reason)
    deny("gov_bond_issue_resubmit","bond:gov51:resubmit:opened",code,specialist,
         "New City Logistics","Bridge renewal","TARGETED",100,5,150,7,30)
    assert q("SELECT COUNT(*) FROM government_bond_revision_reviews "
             "WHERE issue_id=%s",(issue[0],))==[(1,)]
    try:
        q("UPDATE government_bond_revision_reviews SET reason='Changed' "
          "WHERE issue_id=%s",(issue[0],))
        raise AssertionError("Revision audit was mutable")
    except mysql.connector.Error:
        pass
    after=q("SELECT balance,bond_fund_reserved,bond_fund_committed,"
            "bond_payout_reserved FROM treasury_accounts WHERE code='state'")[0]
    assert after[0]==before[0] and after[1]==before[1] and after[3]==before[3]
    assert after[2]>before[2]
    print("GOV-05.1 MariaDB UAT: PASS (return/resubmit permissions and replay, "
          "immutable reason, 90-day review, opened-at sale/maturity dates, "
          "treasury invariant)")

if __name__=="__main__":
    main()
