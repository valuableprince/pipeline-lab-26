"""GOV-05 /bondgov SQL regression: ASCII account names with Cyrillic UI fallbacks.

Connects to the disposable MariaDB CI database after migration 025. Queries
are extracted from the Pawn source to prevent testing a different query.
No persistent writes, production data or passwords in diagnostics.
"""
import os
import re
from pathlib import Path
import mysql.connector

ROOT=Path(__file__).resolve().parents[1]
SRC=(ROOT/"gamemodes/includes/bond_governance_ui.inc").read_text("utf-8")
DB=os.environ.get("GOV00_TEST_DB","tarkenso_gov00")
assert DB=="tarkenso_gov00","Refuse to run on non-disposable database"

def extract(prefix):
    r=re.search(r'"(SELECT '+re.escape(prefix)+r'[^"\n]+)"',SRC)
    assert r,"Missing runtime query beginning: "+prefix
    query=r.group(1)
    assert "Неизвестен" not in query and "Не назначен" not in query
    return query

def main():
    listing=extract("i.issue_id,i.issue_code,i.status")
    details=extract("i.issue_id,i.issue_code,i.title")
    assert listing.count("%d")==1
    assert details.count("%d")==1
    assert "COALESCE(a.username,'') AS applicant" in listing
    assert "COALESCE(a.username,'') AS applicant" in details
    assert "COALESCE(rej.username,'') AS rejector" in details
    assert "), '') AS minister" in details
    db=mysql.connector.connect(
        host=os.environ.get("GOV00_TEST_HOST","127.0.0.1"),
        port=int(os.environ.get("GOV00_TEST_PORT","3306")),
        user=os.environ.get("GOV00_TEST_USER","root"),
        password=os.environ["GOV00_TEST_PASSWORD"],
        database=DB,autocommit=True)
    try:
        with db.cursor(dictionary=True) as cur:
            cur.execute("SELECT issue_id FROM government_bond_issues ORDER BY issue_id LIMIT 1")
            issue=cur.fetchone()
            assert issue,"GOV-05 UAT must create at least one bond issue"
            issue_id=issue["issue_id"]
            for collation in ("utf8mb4_general_ci","utf8mb4_unicode_ci"):
                cur.execute("SET NAMES utf8mb4 COLLATE "+collation)
                cur.execute(listing % 0)
                page=cur.fetchall()
                assert page and any(x["issue_id"]==issue_id for x in page),page
                assert all(isinstance(row["applicant"],str) for row in page)
                cur.execute(details % issue_id)
                row=cur.fetchone()
                assert row and row["issue_id"]==issue_id,row
                assert isinstance(row["applicant"],str)
                assert isinstance(row["minister"],str)
                assert isinstance(row["rejector"],str)
            print("GOV-05 registry collation PASS: exact Pawn list/detail SQL "
                  "work under two connection collations; no Cyrillic literal "
                  "is mixed with ascii_bin account usernames")
    finally:db.close()

if __name__=="__main__":main()
