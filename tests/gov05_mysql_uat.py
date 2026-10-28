"""GOV-05 real MariaDB UAT: official roles, atomic auto issue codes and authorization."""
from concurrent.futures import ThreadPoolExecutor
from datetime import datetime, timezone
import os
import mysql.connector

CONFIG=dict(host=os.environ.get("GOV00_TEST_HOST","127.0.0.1"),
 port=int(os.environ.get("GOV00_TEST_PORT","3306")),
 user=os.environ.get("GOV00_TEST_USER","root"),
 password=os.environ["GOV00_TEST_PASSWORD"],
 database=os.environ.get("GOV00_TEST_DB","tarkenso_gov00"),autocommit=True)


def query(sql,args=()):
    with mysql.connector.connect(**CONFIG) as db,db.cursor() as c:
        c.execute(sql,args)
        return c.fetchall() if c.with_rows else c.rowcount


def call(proc,*args):
    with mysql.connector.connect(**CONFIG) as db,db.cursor() as c:
        c.execute("CALL "+proc+"("+",".join("%s" for _ in args)+")",args)
        row=c.fetchone()
        while c.nextset():
            pass
        return row


def rejects(proc,*args):
    try: call(proc,*args)
    except mysql.connector.Error: return
    raise AssertionError(f"accepted invalid {proc} {args!r}")


def create(key,creator,face=500,qty=10):
    return call("gov_bond_issue_create_auto",key,creator,
        "Treasury State Bond","Real infrastructure project","GENERAL",
        face,qty,200,2,14)


def main():
    assert query("SELECT version FROM schema_migrations WHERE version=23")==[(23,)]
    creator=int(query("SELECT account_id FROM accounts WHERE username='gov00uat'")[0][0])
    controller=int(query("SELECT account_id FROM accounts WHERE username='gov01uat'")[0][0])
    outsider=int(query("SELECT account_id FROM accounts WHERE username='gov03buyer1'")[0][0])
    before=query("SELECT balance,bond_fund_reserved,bond_fund_committed,"
                 "bond_payout_reserved FROM treasury_accounts WHERE code='state'")[0]
    year=int(query("SELECT YEAR(UTC_TIMESTAMP())")[0][0])
    rejects("gov_bond_issue_create_auto","bond:gov5:unauth",creator,
            "Treasury State Bond","Real infrastructure project","GENERAL",
            500,10,200,2,14)
    grant=call("gov_bond_role_set","bond:gov5:role:creator",creator,creator,
               "TREASURER",1)
    assert grant[0:3]==(creator,"TREASURER",1) and grant[-1]==0,grant
    assert call("gov_bond_role_set","bond:gov5:role:creator",
                creator,creator,"TREASURER",1)[-1]==1
    rejects("gov_bond_role_set","bond:gov5:role:creator",creator,creator,"DIRECTOR",1)
    call("gov_bond_role_set","bond:gov5:role:controller",creator,controller,"CONTROLLER",1)
    assert query("SELECT role_code,enabled FROM government_bond_officials "
                 "WHERE account_id=%s",(controller,))==[("CONTROLLER",1)]

    first=create("bond:gov5:create:first",creator)
    expected=f"GOV-{year}-0001"
    assert first[1:]==(expected,"DRAFT",0),first
    assert create("bond:gov5:create:first",creator)[-1]==1
    rejects("gov_bond_issue_create_auto","bond:gov5:create:first",creator,
            "Different State Bond","Real infrastructure project","GENERAL",
            500,10,200,2,14)
    assert query("SELECT issue_code FROM government_bond_issues "
                 "WHERE issue_id=%s",(first[0],))==[(expected,)]
    assert query("SELECT last_number FROM government_bond_code_sequences "
                 "WHERE issue_year=%s",(year,))==[(1,)]
    assert query("SELECT COUNT(*) FROM government_bond_issue_audit "
                 "WHERE event_key='bond:gov5:create:first'")==[(1,)]

    # Year-serial allocation is protected by InnoDB sequence row: no collisions.
    with ThreadPoolExecutor(max_workers=4) as pool:
        created=list(pool.map(lambda i:create(f"bond:gov5:create:parallel:{i}",creator),
                              range(6)))
    codes=[r[1] for r in created]
    assert len(codes)==len(set(codes))==6,codes
    assert sorted(codes)==[f"GOV-{year}-{i:04d}" for i in range(2,8)],codes
    assert query("SELECT last_number FROM government_bond_code_sequences "
                 "WHERE issue_year=%s",(year,))==[(7,)]

    # Only independently assigned officials can approve; creator != approver.
    rejects("gov_bond_official_approve","bond:gov5:approve:denied",expected,creator)
    rejects("gov_bond_official_approve","bond:gov5:approve:outsider",expected,outsider)
    approved=call("gov_bond_official_approve","bond:gov5:approve:first",expected,controller)
    assert approved[1]=="APPROVED" and approved[2]==1000,approved
    assert call("gov_bond_official_approve",
                "bond:gov5:approve:first",expected,controller)[-1]==1
    rejects("gov_bond_official_approve","bond:gov5:approve:repeat",expected,controller)
    assert query("SELECT COUNT(*) FROM bond_fund_guarantees "
                 "WHERE issue_code=%s",(expected,))==[(1,)]
    rejects("gov_bond_official_open","bond:gov5:open:controller",expected,controller)
    opened=call("gov_bond_official_open","bond:gov5:open:first",expected,creator)
    assert opened[1:] == ("OFFERING",0),opened
    rejects("gov_bond_official_cancel","bond:gov5:cancel:creator",expected,creator)
    cancel=call("gov_bond_official_cancel","bond:gov5:cancel:first",expected,controller)
    assert cancel[1]=="CANCELLED" and cancel[2]==1000,cancel
    assert call("gov_bond_official_cancel","bond:gov5:cancel:first",expected,controller)[-1]==1

    # Even a director cannot self-approve and has no right to cancel sold issue.
    call("gov_bond_role_set","bond:gov5:role:director",creator,outsider,"DIRECTOR",1)
    auto=create("bond:gov5:create:director",outsider,face=100,qty=2)
    rejects("gov_bond_official_approve","bond:gov5:approve:director",auto[1],outsider)
    call("gov_bond_official_approve","bond:gov5:approve:director:other",auto[1],controller)
    call("gov_bond_official_open","bond:gov5:open:director",auto[1],outsider)
    assert query("SELECT issue_code FROM government_bond_issues "
                 "WHERE issue_id=%s",(auto[0],))==[(f"GOV-{year}-0008",)]
    # No exposure is released when an approved issue has already sold bonds.
    query("INSERT INTO government_bond_holdings(issue_id,account_id,quantity) "
          "VALUES(%s,%s,1)",(auto[0],creator))
    rejects("gov_bond_official_cancel","bond:gov5:cancel:sold",auto[1],controller)
    query("DELETE FROM government_bond_holdings WHERE issue_id=%s",(auto[0],))
    call("gov_bond_official_cancel","bond:gov5:cancel:unsold",auto[1],controller)
    # Former codes are NOT reused after cancellation.
    ninth=create("bond:gov5:create:after-cancel",creator)
    assert ninth[1]==f"GOV-{year}-0009",ninth

    # Revoked officials cannot issue even with an old menu/client request.
    call("gov_bond_role_set","bond:gov5:revoke",creator,creator,"TREASURER",0)
    rejects("gov_bond_issue_create_auto","bond:gov5:create:revoked",creator,
            "Treasury State Bond","Real infrastructure project","GENERAL",
            500,10,200,2,14)
    assert query("SELECT enabled FROM government_bond_officials "
                 "WHERE account_id=%s",(creator,))==[(0,)]
    assert query("SELECT COUNT(*) FROM government_bond_role_audit")==[(4,)]
    assert query("SELECT COUNT(*) FROM government_bond_issues "
                 "WHERE issue_code LIKE %s",(f"GOV-{year}-%",))==[(9,)]
    after=query("SELECT balance,bond_fund_reserved,bond_fund_committed,"
                "bond_payout_reserved FROM treasury_accounts WHERE code='state'")[0]
    assert after==before,(before,after)
    print("GOV-05 MariaDB UAT: PASS (atomic yearly codes, parallel creation, "
          "roles, separated signatures, insurance commitment, cancellation, revocation)")


if __name__=="__main__":
    main()
