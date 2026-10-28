"""Regression for /bondgov registry collation error 1267 and global kick.

The same UI is compiled with cp1251 text for Pawn while the database stores
account usernames as ascii_bin. SQL must not coerce a Cyrillic string against
an ASCII username column. Fallbacks belong in Pawn presentation only.
"""
from pathlib import Path
import re
import unittest

ROOT=Path(__file__).resolve().parents[1]
UI=(ROOT/"gamemodes/includes/bond_governance_ui.inc").read_text("utf-8")
DB=(ROOT/"gamemodes/includes/db_primary.inc").read_text("utf-8")
WORKFLOW=(ROOT/".github/workflows/build.yml").read_text("utf-8")

class FinanceRegistryCollation(unittest.TestCase):
    def test_registry_sql_has_no_non_ascii_username_fallback(self):
        sql=[s for s in re.findall(r'"(SELECT [^"\n]+)"',UI)
             if "government_bond_issues i" in s and " AS applicant" in s]
        self.assertEqual(len(sql),2)
        for q in sql:
            with self.subTest(query=q[:65]):
                self.assertIn("COALESCE(a.username,'') AS applicant",q)
                self.assertNotIn("Неизвестен",q)
                self.assertNotIn("Не назначен",q)
                self.assertTrue(all(ord(ch)<128 for ch in q))
        detail=next(q for q in sql if " AS minister" in q)
        self.assertIn("COALESCE(rej.username,'') AS rejector",detail)
        self.assertIn("), '') AS minister",detail)
        self.assertIn('if(!applicant[0])format(applicant,sizeof(applicant),"Неизвестен");',UI)
        self.assertIn('if(!minister[0])format(minister,sizeof(minister),"Не назначен");',UI)
        self.assertIn('if(!rejector[0])format(rejector,sizeof(rejector),"Неизвестен");',UI)

    def test_semantic_sql_error_does_not_disconnect_every_player(self):
        handler=DB.split("public OnQueryError(",1)[1].split("#else",1)[0]
        self.assertIn('printf("[DB-PRIMARY] Async SQL error=%d callback=%s (query redacted)."',handler)
        self.assertIn("case 1042,2002,2003,2006,2013,2055:",handler)
        fatal=handler.split("case 1042,2002,2003,2006,2013,2055:",1)[1].split("default:",1)[0]
        self.assertIn("gDBPrimaryReady = false;",fatal)
        self.assertIn("Kick(i);",fatal)
        nonfatal=handler.split("default:",1)[1]
        self.assertNotIn("Kick(i);",nonfatal)
        self.assertNotIn("gDBPrimaryReady = false;",nonfatal)
        self.assertIn("SQL operation rejected; players remain connected.",nonfatal)
        self.assertIn('GovBond_UI_OnError(errorid, callback, query)',handler)

    def test_finance_reader_restores_recoverable_ui_state(self):
        reader=UI.split("public bool:GovBond_UI_OnError(",1)[1].split("stock GovBond_UI_OnDialog(",1)[0]
        self.assertIn('!strcmp(callback,"GovBond_UI_OnRead",true)',reader)
        self.assertIn("gGovUILoading[i]=false;",reader)
        self.assertIn("gGovUIReadSerial[i]++;",reader)
        self.assertIn("Ошибка чтения реестра Минфина",reader)
        self.assertIn("errorid==2006",reader)
        self.assertIn("return true;",reader)

    def test_mariadb_executes_exact_runtime_queries(self):
        self.assertIn("tests/gov05_registry_collation_mysql_uat.py",WORKFLOW)

if __name__=="__main__":unittest.main()
