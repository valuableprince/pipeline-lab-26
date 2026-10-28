"""GOV-02 live MariaDB integration UAT. Only isolated, disposable CI database."""
import os
import concurrent.futures
from datetime import datetime, timedelta, timezone
import mysql.connector

CONFIG = dict(
    host=os.environ.get("GOV00_TEST_HOST","127.0.0.1"),
    port=int(os.environ.get("GOV00_TEST_PORT","3306")),
    user=os.environ.get("GOV00_TEST_USER","root"),
    password=os.environ["GOV00_TEST_PASSWORD"],
    database=os.environ.get("GOV00_TEST_DB","tarkenso_gov00"),
    autocommit=True,
)


def select(sql, params=()):
    with mysql.connector.connect(**CONFIG) as db:
        with db.cursor() as c:
            c.execute(sql,params)
            return c.fetchall() if c.with_rows else c.rowcount


def call(name, *params):
    placeholders=",".join(["%s"]*len(params))
    with mysql.connector.connect(**CONFIG) as db:
        with db.cursor() as c:
            c.execute(f"CALL {name}({placeholders})",params)
            row=c.fetchone()
            while c.nextset():
                pass
            return row


def rejects(name, *params):
    try:
        call(name,*params)
    except mysql.connector.Error:
        return
    raise AssertionError("Invalid request was accepted: "+name+" "+repr(params))


def draft(key, code, creator, face=10000, qty=10, yield_bps=100, kind="GENERAL"):
    return call("gov_bond_issue_create",key,code,"Test State Bond","Budget financing",
                kind,face,qty,yield_bps,START,END,MATURITY,creator)


CREATOR = None
APPROVER = None
START = None
END = None
MATURITY = None


def main():
    global CREATOR,APPROVER,START,END,MATURITY
    assert select("SELECT version FROM schema_migrations WHERE version=20")==[(20,)]
    CREATOR=int(select("SELECT account_id FROM accounts WHERE username='gov00uat'")[0][0])
    APPROVER=int(select("SELECT account_id FROM accounts WHERE username='gov01uat'")[0][0])
    t=datetime.now(timezone.utc).replace(microsecond=0,second=0)
    START=(t-timedelta(minutes=10)).strftime("%Y-%m-%d %H:%M:%S")
    END=(t+timedelta(days=2)).strftime("%Y-%m-%d %H:%M:%S")
    MATURITY=(t+timedelta(days=16)).strftime("%Y-%m-%d %H:%M:%S")
    before=select("SELECT balance,bond_fund_reserved,bond_fund_committed FROM treasury_accounts WHERE code='state'")[0]
    assert before[1:]==(100000,80000),before
    assert select("SELECT COUNT(*) FROM government_bond_holdings")==[(0,)]
    rejects("bond_fund_commit","fund:orphan","ORPHAN-01",1,None)

    # Use actual staging cash, no fabricated insurance balances.
    call("treasury_post","gov02:fund-topup",50000,"uat",None,"topup")
    call("bond_fund_capitalize","fund:gov02-capital",50000,None)
    select("UPDATE treasury_accounts SET bond_fund_enabled=1,"
           "bond_fund_config_version=bond_fund_config_version+1 WHERE code='state'")
    funds=select("SELECT balance,bond_fund_reserved,bond_fund_committed "
                 "FROM treasury_accounts WHERE code='state'")[0]
    assert funds==(150000,150000,80000), funds
    assert funds[0]-before[0]==50000  # only treasury_post added money

    first=draft("bond:create:A","GOV02-A",CREATOR)
    assert first[1:] == ("DRAFT",0),first
    assert draft("bond:create:A","GOV02-A",CREATOR)==(first[0],"DRAFT",1)
    rejects("gov_bond_issue_create","bond:create:A","GOV02-X","Test State Bond",
            "Budget financing","GENERAL",10000,10,100,START,END,MATURITY,CREATOR)
    rejects("gov_bond_issue_create","bond:create:duplicatecode","GOV02-A",
            "Test State Bond","Budget financing","GENERAL",10000,10,100,START,END,MATURITY,CREATOR)
    rejects("gov_bond_issue_create","bond:create:bad","GOV02-BAD",
            "Test State Bond","Budget financing","GENERAL",1000000000,2,100,
            START,END,MATURITY,CREATOR)
    rejects("gov_bond_issue_approve","bond:approve:A","GOV02-A",CREATOR)
    approved=call("gov_bond_issue_approve","bond:approve:A","GOV02-A",APPROVER)
    assert approved[1:]==("APPROVED",20000,100000,approved[4],0),approved
    assert call("gov_bond_issue_approve","bond:approve:A","GOV02-A",APPROVER)[-1]==1
    rejects("gov_bond_issue_approve","bond:approve:A:other","GOV02-A",APPROVER)
    rejects("gov_bond_issue_approve","bond:approve:A","GOV02-A",CREATOR)
    assert select("SELECT coverage_basis_points,premium_basis_points,grace_seconds,"
                  "insured_max_exposure FROM government_bond_issues "
                  "WHERE issue_code='GOV02-A'")==[(2000,200,86400,20000)]
    assert select("SELECT max_exposure,released_exposure FROM bond_fund_guarantees "
                  "WHERE issue_code='GOV02-A'")==[(20000,0)]
    try:
        select("UPDATE government_bond_issues SET face_value=10001 "
               "WHERE issue_code='GOV02-A'")
        raise AssertionError("Approved bond terms mutation bypassed trigger")
    except mysql.connector.Error:
        pass
    # Reject spending pledged cash while the issue is approved.
    rejects("treasury_post","gov02:reserve-breach",-1,"uat",None,"fund-protected")

    opened=call("gov_bond_issue_open","bond:open:A","GOV02-A",APPROVER)
    assert opened==(first[0],"OFFERING",0),opened
    assert call("gov_bond_issue_open","bond:open:A","GOV02-A",APPROVER)[-1]==1
    rejects("gov_bond_issue_open","bond:open:A:again","GOV02-A",APPROVER)

    # Stage GOV-03 will be sole writer of holdings; stage-level safety:
    # cancelling a sold issue is always rejected, even if administrator tries.
    select("INSERT INTO government_bond_holdings(issue_id,account_id,quantity) "
           "VALUES(%s,%s,1)",(first[0],CREATOR))
    select("UPDATE government_bond_issues SET sold_quantity=1 WHERE issue_id=%s",
           (first[0],))
    rejects("gov_bond_issue_cancel","bond:cancel:sold","GOV02-A",APPROVER)
    select("UPDATE government_bond_issues SET sold_quantity=0 WHERE issue_id=%s",
           (first[0],))
    rejects("gov_bond_issue_cancel","bond:cancel:holder","GOV02-A",APPROVER)
    select("DELETE FROM government_bond_holdings WHERE issue_id=%s",(first[0],))

    cancelled=call("gov_bond_issue_cancel","bond:cancel:A","GOV02-A",APPROVER)
    assert cancelled[1:]==("CANCELLED",20000,80000,cancelled[4],0),cancelled
    assert call("gov_bond_issue_cancel","bond:cancel:A","GOV02-A",APPROVER)[-1]==1
    rejects("gov_bond_issue_open","bond:open:cancelled","GOV02-A",APPROVER)
    assert select("SELECT released_exposure FROM bond_fund_guarantees "
                  "WHERE issue_code='GOV02-A'")==[(20000,)]
    assert select("SELECT operation,commitment_delta FROM bond_fund_ledger "
                  "WHERE event_key='bond:cancel:A'")==[("RELEASE",-20000)]
    assert select("SELECT COUNT(*) FROM government_bond_issue_audit "
                  "WHERE issue_id=%s",(first[0],))==[(4,)]

    # A draft can be cancelled without touching fund capacity.
    draft("bond:create:D","GOV02-DRAFT",CREATOR)
    c=call("gov_bond_issue_cancel","bond:cancel:D","GOV02-DRAFT",CREATOR)
    assert c[2]==0 and c[3]==80000
    rejects("gov_bond_issue_approve","bond:approve:D","GOV02-DRAFT",APPROVER)

    # Frozen terms: later treasury config changes affect only new approvals.
    select("UPDATE treasury_accounts SET bond_fund_coverage_bps=2500,"
           "bond_fund_premium_bps=250,bond_fund_config_version=bond_fund_config_version+1 "
           "WHERE code='state'")
    draft("bond:create:B","GOV02-B",CREATOR,face=101,qty=3)
    b=call("gov_bond_issue_approve","bond:approve:B","GOV02-B",APPROVER)
    assert b[2]==78, b  # ceil(101 * 25%) = 26, * 3 = 78
    assert select("SELECT coverage_basis_points,premium_basis_points,"
                  "insured_max_exposure FROM government_bond_issues "
                  "WHERE issue_code='GOV02-B'")==[(2500,250,78)]
    assert select("SELECT coverage_basis_points,premium_basis_points "
                  "FROM government_bond_issues WHERE issue_code='GOV02-A'")==[(2000,200)]
    rejects("gov_bond_issue_approve","bond:approve:B:2","GOV02-B",APPROVER)

    # Different event IDs under concurrent approvals can never commit twice.
    draft("bond:create:C","GOV02-C",CREATOR,face=10000,qty=10)
    with concurrent.futures.ThreadPoolExecutor(max_workers=2) as pool:
        results=list(pool.map(lambda key: attempt_approval(key),[
            "bond:approve:C:first","bond:approve:C:second"]))
    assert sorted(results)==["FAIL","OK"],results
    assert select("SELECT COUNT(*) FROM bond_fund_guarantees "
                  "WHERE issue_code='GOV02-C'")==[(1,)]
    assert select("SELECT COUNT(*) FROM government_bond_issue_audit "
                  "WHERE action='APPROVE' AND issue_id=(SELECT issue_id FROM "
                  "government_bond_issues WHERE issue_code='GOV02-C')")==[(1,)]
    committed=int(select("SELECT bond_fund_committed FROM treasury_accounts "
                         "WHERE code='state'")[0][0])
    assert committed==80000+78+25000,committed

    # Max-sized insurance request must fail without a partial commitment.
    draft("bond:create:BIG","GOV02-BIG",CREATOR,face=100000,qty=10)
    rejects("gov_bond_issue_approve","bond:approve:BIG","GOV02-BIG",APPROVER)
    assert select("SELECT status FROM government_bond_issues "
                  "WHERE issue_code='GOV02-BIG'")==[("DRAFT",)]
    assert select("SELECT COUNT(*) FROM bond_fund_guarantees "
                  "WHERE issue_code='GOV02-BIG'")==[(0,)]

    print("GOV-02 MariaDB UAT: PASS (atomic approval, snapshots, immutability, "
          "idempotent transitions, release, sold-issue protection, concurrency)")


def attempt_approval(key):
    try:
        call("gov_bond_issue_approve",key,"GOV02-C",APPROVER)
        return "OK"
    except mysql.connector.Error:
        return "FAIL"


if __name__=="__main__":
    main()
