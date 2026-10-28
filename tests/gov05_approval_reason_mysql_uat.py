"""GOV-05: read-only, accurate explanation of insurance-fund signing denial.

Run only on disposable MariaDB GOV00 UAT schema, after GOV-05.1 migration 025.
Verifies the EXACT SELECT shipped in Pawn with known financial arithmetic.
No UPDATE/INSERT, and no mutation of financial or bond ledgers.
"""
import os
import re
from pathlib import Path
import mysql.connector

db_name=os.environ.get("GOV00_TEST_DB","tarkenso_gov00")
assert db_name=="tarkenso_gov00","Use disposable UAT database only"
config=dict(
    host=os.environ.get("GOV00_TEST_HOST","127.0.0.1"),
    port=int(os.environ.get("GOV00_TEST_PORT","3306")),
    user=os.environ.get("GOV00_TEST_USER","root"),
    password=os.environ["GOV00_TEST_PASSWORD"],
    database=db_name,
    autocommit=True
)

pawn=(Path(__file__).resolve().parents[1]/
      "gamemodes/includes/bond_governance_ui.inc").read_text("utf-8")
block=pawn.split("stock bool:GovBond_UI_LoadApprovalReason(playerid)",1)[1].split(
    "stock GovBond_UI_Report(",1)[0]
sql_match=re.search(r'"(SELECT x\.issue_code.*?LIMIT 1\) x)"',block,re.S)
assert sql_match,"Expected read-only SQL diagnostic snapshot in Pawn"
sql=sql_match.group(1).replace("%d","%s").replace("'%e'","%s")
assert sql.startswith("SELECT") and ";" not in sql

with mysql.connector.connect(**config) as db, db.cursor(dictionary=True) as cur:
    cur.execute("SELECT version FROM schema_migrations WHERE version=25")
    assert cur.fetchone()["version"]==25
    cur.execute("SELECT i.issue_code,i.face_value,i.max_quantity,"
                "t.bond_fund_reserved AS reserved,"
                "t.bond_fund_committed AS committed,"
                "t.bond_fund_coverage_bps AS coverage,"
                "t.bond_fund_utilization_bps AS utilization "
                "FROM government_bond_issues i "
                "JOIN treasury_accounts t ON t.code='state' "
                "ORDER BY i.issue_id LIMIT 1")
    row=cur.fetchone()
    assert row and int(row["utilization"])>0
    # A nonmember must be diagnosed as lacking permission.
    account=999999999
    cur.execute(sql,(account,row["issue_code"]))
    diagnostic=cur.fetchone()
    assert diagnostic and diagnostic["issue_code"]==row["issue_code"]
    assert diagnostic["role_rank"]==0
    exposure=((int(row["face_value"])*int(row["coverage"])+9999)//10000)*int(row["max_quantity"])
    required=(int(row["committed"])+exposure)*10000
    minimum=(required+int(row["utilization"])-1)//int(row["utilization"])
    shortfall=max(0,minimum-int(row["reserved"]))
    assert int(diagnostic["exposure"])==exposure
    assert int(diagnostic["reserve_min"])==minimum
    assert int(diagnostic["reserve_short"])==shortfall
    assert int(diagnostic["reserved"])==int(row["reserved"])
    assert int(diagnostic["committed"])==int(row["committed"])
    # Existing guarantees must be accounted for: required minimum is
    # never just the coverage of the newly requested issue.
    assert minimum>=exposure
    print("GOV-05 approval denial UAT PASS: min reserve, existing commitments, "
          "shortfall and role from read-only SQL")
