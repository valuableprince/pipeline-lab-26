"""GOV-04 real MariaDB acceptance: crises, overdue debt and honest restructuring.
Runs ONLY after the GOV-00—03 UAT in an isolated CI database.
"""
import concurrent.futures
from datetime import datetime, timedelta, timezone
import os
import mysql.connector

CONFIG = dict(
    host=os.environ.get("GOV00_TEST_HOST","127.0.0.1"),
    port=int(os.environ.get("GOV00_TEST_PORT","3306")),
    user=os.environ.get("GOV00_TEST_USER","root"),
    password=os.environ["GOV00_TEST_PASSWORD"],
    database=os.environ.get("GOV00_TEST_DB","tarkenso_gov00"),autocommit=True,
)


def query(sql, args=()):
    with mysql.connector.connect(**CONFIG) as db, db.cursor() as cursor:
        cursor.execute(sql,args)
        return cursor.fetchall() if cursor.with_rows else cursor.rowcount


def call(proc, *args):
    with mysql.connector.connect(**CONFIG) as db, db.cursor() as c:
        c.execute("CALL "+proc+"("+",".join("%s" for _ in args)+")",args)
        result=c.fetchone()
        while c.nextset():
            pass
        return result


def rejected(proc,*args):
    try:
        call(proc,*args)
    except mysql.connector.Error:
        return
    raise AssertionError(f"{proc} unexpectedly accepted {args!r}")


def state():
    return tuple(map(int,query(
        "SELECT balance,bond_fund_reserved,bond_payout_reserved,minimum_reserve "
        "FROM treasury_accounts WHERE code='state'")[0]))


def available():
    b,f,p,m=state()
    return b-f-p-m


def outstanding(issue_id):
    return int(query(
        "SELECT COALESCE(SUM(principal_due+interest_due-principal_budget_awarded"
        "-interest_budget_awarded-insurance_awarded),0) "
        "FROM government_bond_claims WHERE issue_id=%s",(issue_id,))[0][0])


def main():
    assert query("SELECT version FROM schema_migrations WHERE version=22")==[(22,)]
    creator=int(query("SELECT account_id FROM accounts WHERE username='gov00uat'")[0][0])
    approver=int(query("SELECT account_id FROM accounts WHERE username='gov01uat'")[0][0])
    issue_id=int(query("SELECT issue_id FROM government_bond_issues "
                       "WHERE issue_code='GOV03-A'")[0][0])
    original=query("SELECT face_value,yield_basis_points,maturity_at,"
                   "insured_max_exposure FROM government_bond_issues "
                   "WHERE issue_id=%s",(issue_id,))[0]
    assert query("SELECT debt_status FROM government_bond_issues "
                 "WHERE issue_id=%s",(issue_id,))==[("DEFAULTED",)]
    assert outstanding(issue_id)==24400, outstanding(issue_id)
    assert available()==0, state()

    before_claims=query("SELECT claim_id,principal_due,interest_due,"
                        "principal_budget_awarded,interest_budget_awarded,"
                        "insurance_awarded,amount_collected FROM government_bond_claims "
                        "WHERE issue_id=%s ORDER BY claim_id",(issue_id,))
    assert len(before_claims)==2
    assert all(x[1:]==(20000,200,5000,0,3000,8000) or
               x[1:]==(20000,200,5000,0,3000,8000) for x in before_claims)

    # Status is computed from outstanding issues; never edits balances.
    balance_before=state()
    c=call("gov_bond_crisis_reconcile","bond:crisis:start")
    assert c[0]=="DEFAULT" and c[-1]==0,c
    assert state()==balance_before
    assert call("gov_bond_crisis_reconcile","bond:crisis:start")[-1]==1
    assert query("SELECT bond_crisis_state FROM treasury_accounts "
                 "WHERE code='state'")==[("DEFAULT",)]

    rejected("gov_bond_recovery_allocate","bond:recover:no-funds","GOV03-A",500)
    rejected("gov_bond_recovery_allocate","bond:recover:no-issue","GOV03-NO",500)
    rejected("gov_bond_recovery_allocate","bond:recover:bad","GOV03-A",0)

    # First overdue recovery uses a real credit; no cash is created by allocation.
    call("treasury_post","gov04:tax-receipt",3001,"gov04",None,"new-tax-revenue")
    assert available()==3001
    first=call("gov_bond_recovery_allocate","bond:recover:one","GOV03-A",3001)
    assert first[1:3]==(3001,0) and first[-1]==0,first
    assert available()==0
    assert outstanding(issue_id)==21399
    assert call("gov_bond_recovery_allocate","bond:recover:one","GOV03-A",3001)[-1]==1
    rejected("gov_bond_recovery_allocate","bond:recover:one","GOV03-A",3000)
    assert query("SELECT COUNT(*) FROM government_bond_recoveries "
                 "WHERE event_key='bond:recover:one'")==[(1,)]
    assert query("SELECT COUNT(*) FROM bond_payout_ledger "
                 "WHERE event_key='bond:recover:one'")==[(1,)]
    claim_rows=query("SELECT principal_budget_awarded FROM government_bond_claims "
                     "WHERE issue_id=%s ORDER BY claim_id",(issue_id,))
    assert sorted(c[0] for c in claim_rows)==[6500,6501],claim_rows

    # Two independent government officials must agree on a new TARGET date.
    target=(datetime.now(timezone.utc)+timedelta(days=7)).strftime("%Y-%m-%d %H:%M:%S")
    reason="Refinancing schedule after state fiscal crisis"
    p=call("gov_bond_restructure_propose","bond:plan:one",
           "GOV03-A",target,reason,creator)
    assert p[-1]==0,p
    plan_id=p[0]
    assert call("gov_bond_restructure_propose","bond:plan:one",
                "GOV03-A",target,reason,creator)[-1]==1
    rejected("gov_bond_restructure_propose","bond:plan:other",
             "GOV03-A",target,reason,creator)
    rejected("gov_bond_restructure_propose","bond:plan:one",
             "GOV03-A",target,"Conflicting purpose",creator)
    rejected("gov_bond_restructure_approve","bond:approve:plan", "GOV03-A",plan_id,creator)
    rejected("gov_bond_restructure_approve","bond:approve:plan", "GOV03-B",plan_id,approver)
    a=call("gov_bond_restructure_approve","bond:approve:plan",
           "GOV03-A",plan_id,approver)
    assert a[0]==plan_id and a[-1]==0,a
    assert call("gov_bond_restructure_approve","bond:approve:plan",
                "GOV03-A",plan_id,approver)[-1]==1
    rejected("gov_bond_restructure_approve","bond:approve:changed",
             "GOV03-A",plan_id,approver)
    assert query("SELECT debt_status,restructure_revision,debt_target_at "
                 "FROM government_bond_issues WHERE issue_id=%s",(issue_id,))[0][:2]==(
                     "RESTRUCTURED",1)
    assert query("SELECT face_value,yield_basis_points,maturity_at,"
                 "insured_max_exposure FROM government_bond_issues "
                 "WHERE issue_id=%s",(issue_id,))[0]==original
    assert outstanding(issue_id)==21399

    assert call("gov_bond_crisis_reconcile","bond:crisis:recovery")[0]=="RECOVERY"
    assert query("SELECT bond_crisis_state FROM treasury_accounts "
                 "WHERE code='state'")==[("RECOVERY",)]

    # Existing escrow remains collectible even after a new recovery round.
    owners=query("SELECT claim_id,account_id,amount_collected FROM government_bond_claims "
                 "WHERE issue_id=%s ORDER BY claim_id",(issue_id,))
    claim_id,account_id,collected=owners[0]
    assert collected==8000
    av=int(query("SELECT principal_budget_awarded+interest_budget_awarded+"
                 "insurance_awarded-amount_collected FROM government_bond_claims "
                 "WHERE claim_id=%s",(claim_id,))[0][0])
    assert av in (1500,1501)
    account_version=int(query("SELECT row_version FROM accounts WHERE account_id=%s",
                              (account_id,))[0][0])
    receipt=call("gov_bond_claim_collect","bond:recover:collect",
                 claim_id,account_id,av,account_version)
    assert receipt[-1]==0,receipt
    assert call("gov_bond_claim_collect","bond:recover:collect",
                claim_id,account_id,av,account_version)[-1]==1

    # Last round pays the WHOLE debt, principal before interest, no overpayment.
    call("treasury_post","gov04:additional-revenue",21399,
         "gov04",None,"additional-budget-income")
    assert available()==21399
    second=call("gov_bond_recovery_allocate","bond:recover:final","GOV03-A",21399)
    assert second[1:3]==(20999,400),second
    assert outstanding(issue_id)==0
    assert available()==0
    assert query("SELECT debt_status FROM government_bond_issues "
                 "WHERE issue_id=%s",(issue_id,))==[("SETTLED",)]
    assert query("SELECT SUM(principal_due+interest_due),"
                 "SUM(principal_budget_awarded+interest_budget_awarded+insurance_awarded) "
                 "FROM government_bond_claims WHERE issue_id=%s",
                 (issue_id,))==[(40400,40400)]
    rejected("gov_bond_recovery_allocate","bond:recover:after","GOV03-A",100)
    rejected("gov_bond_restructure_propose","bond:plan:paid",
             "GOV03-A",target,reason,creator)
    assert query("SELECT COUNT(*) FROM government_bond_restructure_audit "
                 "WHERE plan_id=%s",(plan_id,))==[(2,)]
    assert call("gov_bond_crisis_reconcile","bond:crisis:normal")[0]=="NORMAL"
    assert query("SELECT bond_crisis_state FROM treasury_accounts "
                 "WHERE code='state'")==[("NORMAL",)]
    assert query("SELECT COUNT(*) FROM bond_crisis_events")==[(3,)]

    # General funds invariant, including escrow that the government cannot spend.
    b,f,escrow,m=state()
    assert b>=f+escrow and escrow>0
    rejected("treasury_post","gov04:steal-reserve",-1,"gov04",None,"protected")
    assert query("SELECT SUM(principal_allocated+interest_allocated) "
                 "FROM government_bond_recoveries WHERE issue_id=%s",
                 (issue_id,))==[(24400,)]
    assert query("SELECT SUM(reserved_delta) FROM bond_payout_ledger "
                 "WHERE operation='RECOVERY' AND issue_id=%s",
                 (issue_id,))==[(24400,)]
    print("GOV-04 MariaDB UAT: PASS (3 crisis phases, two-person restructuring, "
          "full 24400 overdue recovery, exact pro-rata allocation, no forgiven claims)")


if __name__=="__main__":
    main()
