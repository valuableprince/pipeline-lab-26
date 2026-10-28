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
    expected="GOV-000001"
    assert first[1:]==(expected,"DRAFT",0),first
    assert create("bond:gov5:create:first",creator)[-1]==1
    rejects("gov_bond_issue_create_auto","bond:gov5:create:first",creator,
            "Different State Bond","Real infrastructure project","GENERAL",
            500,10,200,2,14)
    assert query("SELECT issue_code FROM government_bond_issues "
                 "WHERE issue_id=%s",(first[0],))==[(expected,)]
    assert query("SELECT sequence_id,last_number FROM government_bond_code_sequences")==[(1,1)]
    assert query("SELECT COUNT(*) FROM government_bond_issue_audit "
                 "WHERE event_key='bond:gov5:create:first'")==[(1,)]

    # Global serial allocation uses ONE InnoDB row for all years and issuers.
    with ThreadPoolExecutor(max_workers=4) as pool:
        created=list(pool.map(lambda i:create(f"bond:gov5:create:parallel:{i}",creator),
                              range(6)))
    codes=[r[1] for r in created]
    assert len(codes)==len(set(codes))==6,codes
    assert sorted(codes)==[f"GOV-{i:06d}" for i in range(2,8)],codes
    assert query("SELECT sequence_id,last_number FROM government_bond_code_sequences")==[(1,7)]

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
                 "WHERE issue_id=%s",(auto[0],))==[("GOV-000008",)]
    # No exposure is released when an approved issue has already sold bonds.
    query("INSERT INTO government_bond_holdings(issue_id,account_id,quantity) "
          "VALUES(%s,%s,1)",(auto[0],creator))
    rejects("gov_bond_official_cancel","bond:gov5:cancel:sold",auto[1],controller)
    query("DELETE FROM government_bond_holdings WHERE issue_id=%s",(auto[0],))
    call("gov_bond_official_cancel","bond:gov5:cancel:unsold",auto[1],controller)
    # Former codes are NOT reused after cancellation.
    ninth=create("bond:gov5:create:after-cancel",creator)
    assert ninth[1]=="GOV-000009",ninth
    assert query("SELECT sequence_id,last_number FROM government_bond_code_sequences")==[(1,9)]
    rejected_update=False
    try:
        query("UPDATE government_bond_issues SET issue_code='GOV-000999' "
              "WHERE issue_id=%s",(ninth[0],))
    except mysql.connector.Error:
        rejected_update=True
    assert rejected_update, "public bond issue code must be immutable"
    assert query("SELECT issue_code FROM government_bond_issues "
                 "WHERE issue_id=%s",(ninth[0],))==[("GOV-000009",)]

    # Revoked officials cannot issue even with an old menu/client request.
    call("gov_bond_role_set","bond:gov5:revoke",creator,creator,"TREASURER",0)
    rejects("gov_bond_issue_create_auto","bond:gov5:create:revoked",creator,
            "Treasury State Bond","Real infrastructure project","GENERAL",
            500,10,200,2,14)
    assert query("SELECT enabled FROM government_bond_officials "
                 "WHERE account_id=%s",(creator,))==[(0,)]
    assert query("SELECT COUNT(*) FROM government_bond_role_audit")==[(4,)]
    assert query("SELECT COUNT(*) FROM government_bond_issues "
                 "WHERE issue_code REGEXP '^GOV-[0-9]{6}$'")==[(9,)]
    after=query("SELECT balance,bond_fund_reserved,bond_fund_committed,"
                "bond_payout_reserved FROM treasury_accounts WHERE code='state'")[0]
    assert after==before,(before,after)
    # GOV-01's configured 5% is now actually earmarked from tax ledger credits.
    # This DOES NOT mint cash; the treasury balance stays equal to real receipts.
    rejects("gov_bond_tax_earmark","fund:tax:1","bond:gov5:create:first")
    call("treasury_post","gov05:actual:tax",1001,"tax",None,"income_tax")
    tax_id=int(query("SELECT ledger_id FROM treasury_ledger "
                     "WHERE event_key='gov05:actual:tax'")[0][0])
    tax_event=f"fund:tax:{tax_id}"
    tax_before=query("SELECT balance,bond_fund_reserved,bond_fund_committed,"
                     "bond_payout_reserved FROM treasury_accounts WHERE code='state'")[0]
    earmarked=call("gov_bond_tax_earmark",tax_event,"gov05:actual:tax")
    assert earmarked[1]==50 and earmarked[-1]==0,earmarked
    assert call("gov_bond_tax_earmark",tax_event,"gov05:actual:tax")[-1]==1
    rejects("gov_bond_tax_earmark",f"fund:tax:{tax_id+1}","gov05:actual:tax")
    tax_after=query("SELECT balance,bond_fund_reserved,bond_fund_committed,"
                    "bond_payout_reserved FROM treasury_accounts WHERE code='state'")[0]
    assert tax_after==(tax_before[0],tax_before[1]+50,
                       tax_before[2],tax_before[3]),(tax_before,tax_after)
    assert query("SELECT contribution,applied_rate_bps FROM government_bond_tax_earmarks "
                 "WHERE tax_ledger_id=%s",(tax_id,))==[(50,500)]
    assert query("SELECT reserve_delta FROM bond_fund_ledger "
                 "WHERE event_key=%s",(tax_event,))==[(50,)]

    print("GOV-05 MariaDB UAT: PASS (globally sequential codes, parallel creation, "
          "role control, insurance and real 5% tax earmarks)")


if __name__=="__main__":
    main()
