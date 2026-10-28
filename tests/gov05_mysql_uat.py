"""GOV-05 MariaDB: Ministry of Finance as standard faction 5, rank-aware bonds."""
from concurrent.futures import ThreadPoolExecutor
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
        while c.nextset(): pass
        return row


def rejects(proc,*args):
    try: call(proc,*args)
    except mysql.connector.Error: return
    raise AssertionError(f"{proc} accepted prohibited {args!r}")


def create(key,actor,face=500,qty=10):
    return call("gov_bond_issue_create_auto",key,actor,
        "Treasury State Bond","Real infrastructure project","GENERAL",
        face,qty,200,2,14)


def main():
    assert query("SELECT version FROM schema_migrations WHERE version=23")==[(23,)]
    assert query("SELECT faction_id,is_enabled FROM factions "
                 "WHERE faction_id=5")==[(5,1)]
    specialist=int(query("SELECT account_id FROM accounts "
                         "WHERE username='gov00uat'")[0][0])
    deputy=int(query("SELECT account_id FROM accounts "
                     "WHERE username='gov01uat'")[0][0])
    minister=int(query("SELECT account_id FROM accounts "
                       "WHERE username='gov03buyer1'")[0][0])
    trainee=int(query("SELECT account_id FROM accounts "
                      "WHERE username='gov03buyer2'")[0][0])
    before=query("SELECT balance,bond_fund_reserved,bond_fund_committed,"
                 "bond_payout_reserved FROM treasury_accounts WHERE code='state'")[0]
    # Direct SQL role registry no longer exists: the authoritative roster is
    # shared with /setleader, /finvite, /faccept and /frank.
    assert query("SHOW TABLES LIKE 'government_bond_officials'")==[]
    assert query("SHOW TABLES LIKE 'government_bond_role_audit'")==[]
    rejects("gov_bond_issue_create_auto","bond:minfin:unauth",specialist,
            "Treasury State Bond","Real infrastructure project","GENERAL",
            500,10,200,2,14)

    query("UPDATE factions SET term_start=UNIX_TIMESTAMP(),"
          "term_end=UNIX_TIMESTAMP()+2592000 WHERE faction_id=5")
    for actor,rank in ((specialist,2),(deputy,9),(minister,10),(trainee,1)):
        query("INSERT INTO faction_members(account_id,faction_id,rank_id,"
              "joined_at,last_seen) VALUES(%s,5,%s,UNIX_TIMESTAMP(),UNIX_TIMESTAMP())",
              (actor,rank))
    rejects("gov_bond_issue_create_auto","bond:minfin:trainee",trainee,
            "Treasury State Bond","Real infrastructure project","GENERAL",
            500,10,200,2,14)

    first=create("bond:minfin:first",specialist)
    assert first[1:]==("GOV-000001","DRAFT",0),first
    assert create("bond:minfin:first",specialist)[-1]==1
    rejects("gov_bond_issue_create_auto","bond:minfin:first",specialist,
            "Different State Bond","Real infrastructure project","GENERAL",
            500,10,200,2,14)
    assert query("SELECT sequence_id,last_number FROM "
                 "government_bond_code_sequences")==[(1,1)]
    # Specialist cannot approve own or anyone else's draft.
    rejects("gov_bond_official_approve","bond:minfin:approve:spec",
            first[1],specialist)
    rejects("gov_bond_official_approve","bond:minfin:approve:trainee",
            first[1],trainee)
    approved=call("gov_bond_official_approve","bond:minfin:approve:first",
                  first[1],deputy)
    assert approved[1]=="APPROVED" and approved[2]==1000,approved
    assert call("gov_bond_official_approve","bond:minfin:approve:first",
                first[1],deputy)[-1]==1
    rejects("gov_bond_official_open","bond:minfin:open:spec",
            first[1],specialist)
    assert call("gov_bond_official_open","bond:minfin:open:first",
                first[1],minister)[1]=="OFFERING"
    rejects("gov_bond_official_cancel","bond:minfin:cancel:spec",
            first[1],specialist)
    assert call("gov_bond_official_cancel","bond:minfin:cancel:first",
                first[1],minister)[1]=="CANCELLED"

    # Concurrency uses the same serial across all ministers and specialists.
    actors=(specialist,deputy,minister)
    with ThreadPoolExecutor(max_workers=4) as pool:
        created=list(pool.map(
            lambda i:create(f"bond:minfin:parallel:{i}",actors[i%3]),
            range(6)))
    codes=[r[1] for r in created]
    assert sorted(codes)==[f"GOV-{i:06d}" for i in range(2,8)],codes
    assert query("SELECT sequence_id,last_number FROM "
                 "government_bond_code_sequences")==[(1,7)]

    # Rank 3 deputy approves the OWN issue with ONE signature: no other player.
    deputy_issue=next(r for i,r in enumerate(created) if i%3==1)
    ok=call("gov_bond_official_approve","bond:minfin:deputy:self",
            deputy_issue[1],deputy)
    assert ok[1]=="APPROVED",ok
    assert query("SELECT approved_by_account_id,created_by_account_id "
                 "FROM government_bond_issues WHERE issue_id=%s",
                 (deputy_issue[0],))==[(deputy,deputy)]
    # Rank 4 minister may also issue and sign an own issue.
    minister_issue=next(r for i,r in enumerate(created) if i%3==2)
    ok=call("gov_bond_official_approve","bond:minfin:minister:self",
            minister_issue[1],minister)
    assert ok[1]=="APPROVED",ok
    assert query("SELECT approved_by_account_id,created_by_account_id "
                 "FROM government_bond_issues WHERE issue_id=%s",
                 (minister_issue[0],))==[(minister,minister)]

    # A specialist draft stays pending until leadership acts.
    spec_issue=next(r for i,r in enumerate(created) if i%3==0)
    assert query("SELECT status FROM government_bond_issues WHERE issue_id=%s",
                 (spec_issue[0],))==[("DRAFT",)]
    call("gov_bond_official_approve","bond:minfin:spec:approved",
         spec_issue[1],minister)
    # Selling one bond makes cancellation impossible, even with stale counters.
    call("gov_bond_official_open","bond:minfin:open:deputy",
         deputy_issue[1],deputy)
    query("INSERT INTO government_bond_holdings(issue_id,account_id,quantity) "
          "VALUES(%s,%s,1)",(deputy_issue[0],specialist))
    rejects("gov_bond_official_cancel","bond:minfin:cancel:sold",
            deputy_issue[1],minister)
    query("DELETE FROM government_bond_holdings WHERE issue_id=%s",
          (deputy_issue[0],))
    call("gov_bond_official_cancel","bond:minfin:cancel:unsold",
         deputy_issue[1],minister)

    ninth=create("bond:minfin:aftercancel",specialist)
    assert ninth[1]=="GOV-000008",ninth
    # Cancelled issue number was never reused.
    assert query("SELECT sequence_id,last_number FROM "
                 "government_bond_code_sequences")==[(1,8)]
    tenth=create("bond:minfin:minister:second",minister)
    assert tenth[1]=="GOV-000009",tenth
    assert call("gov_bond_official_approve","bond:minfin:minister:second:ok",
                tenth[1],minister)[1]=="APPROVED"
    mutated=False
    try:
        query("UPDATE government_bond_issues SET issue_code='GOV-000999' "
              "WHERE issue_id=%s",(ninth[0],))
    except mysql.connector.Error: mutated=True
    assert mutated

    # Promotions and revocations in the regular roster are authoritative NOW.
    query("UPDATE faction_members SET rank_id=1 WHERE account_id=%s "
          "AND faction_id=5",(deputy,))
    rejects("gov_bond_official_approve","bond:minfin:revoked",
            ninth[1],deputy)
    query("DELETE FROM faction_members WHERE account_id=%s "
          "AND faction_id=5",(specialist,))
    rejects("gov_bond_issue_create_auto","bond:minfin:removed",specialist,
            "Treasury State Bond","Real infrastructure project","GENERAL",
            500,10,200,2,14)
    assert query("SELECT COUNT(*) FROM government_bond_issues "
                 "WHERE issue_code REGEXP '^GOV-[0-9]{6}$'")==[(9,)]
    assert query("SELECT balance,bond_fund_reserved,bond_fund_committed,"
                 "bond_payout_reserved FROM treasury_accounts WHERE code='state'")[0]==before

    # 5% of real tax receipts funds insurance without minting cash.
    call("treasury_post","minfin:tax:real",1001,"tax",None,"income_tax")
    tax_id=int(query("SELECT ledger_id FROM treasury_ledger WHERE "
                     "event_key='minfin:tax:real'")[0][0])
    tax_key=f"fund:tax:{tax_id}"
    tax_before=query("SELECT balance,bond_fund_reserved,bond_fund_committed,"
                     "bond_payout_reserved FROM treasury_accounts WHERE code='state'")[0]
    assert call("gov_bond_tax_earmark",tax_key,"minfin:tax:real")[1]==50
    assert call("gov_bond_tax_earmark",tax_key,"minfin:tax:real")[-1]==1
    assert query("SELECT balance,bond_fund_reserved,bond_fund_committed,"
                 "bond_payout_reserved FROM treasury_accounts WHERE code='state'")[0]==(
                     tax_before[0],tax_before[1]+50,tax_before[2],tax_before[3])
    assert query("SELECT contribution,applied_rate_bps FROM "
                 "government_bond_tax_earmarks WHERE tax_ledger_id=%s",
                 (tax_id,))==[(50,500)]
    print("GOV-05 MariaDB UAT: PASS (standard Ministry of Finance ranks 1/2/9/10, "
          "two-signature specialist, single-signature deputy/minister, "
          "concurrency and continuous GOV codes, real 5pct tax)")
if __name__=="__main__":
    main()
