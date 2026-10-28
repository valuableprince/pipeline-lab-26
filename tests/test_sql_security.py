"""SQL Security Gate: regressions for unsafe query construction and bypasses."""
from pathlib import Path
import tempfile
import unittest

from tools.check_sql_security import audit_pawn, audit_sql, main, static_string


def rules(source: str) -> set[str]:
    findings, _ = audit_pawn("gamemodes/fixture.inc", source)
    return {f.rule for f in findings}


class SQLSecurityGateTests(unittest.TestCase):
    def test_safe_mysql_format_escaped_text_and_numbers(self):
        source = (
            'mysql_format(db, q, sizeof(q), '
            '"SELECT account_id FROM accounts WHERE username=\\'%e\\' AND id=%d", name, 7);'
        )
        self.assertEqual(rules(source), set())

    def test_safe_hex_literal_still_uses_escaping(self):
        self.assertEqual(rules(
            'mysql_format(db, q, sizeof(q), "CALL save_hex(X\\'%e\\',X\\'%e\\')", name, description);'
        ), set())

    def test_unescaped_pawn_format_for_sql_is_blocked(self):
        self.assertIn("SQL005", rules(
            'format(query, sizeof(query), "SELECT * FROM accounts WHERE username=\\'%s\\'", inputtext);'
            'mysql_tquery(db, query);'
        ))

    def test_indirect_format_sql_is_blocked(self):
        self.assertIn("SQL005", rules(
            'format(q, sizeof(q), "%s", inputtext); mysql_query(db, q);'
        ))

    def test_percent_s_in_mysql_format_is_blocked(self):
        self.assertIn("SQL002", rules(
            'mysql_format(db, q, sizeof(q), "INSERT INTO logs(message) VALUES(\\'%s\\')", note);'
        ))

    def test_mysql_format_unquoted_escaped_string_is_blocked(self):
        self.assertIn("SQL003", rules(
            'mysql_format(db, q, sizeof(q), "SELECT * FROM users WHERE name=%e", name);'
        ))

    def test_mysql_format_dynamic_template_is_blocked(self):
        self.assertIn("SQL001", rules(
            "mysql_format(db, q, sizeof(q), untrusted_template, 42);"
        ))

    def test_mysql_format_placeholder_mismatch_is_blocked(self):
        self.assertIn("SQL004", rules(
            'mysql_format(db, q, sizeof(q), "SELECT * FROM users WHERE name=\\'%e\\' AND id=%d", name);'
        ))

    def test_direct_user_input_to_query_is_blocked(self):
        self.assertIn("SQL006", rules("mysql_tquery(db, inputtext);"))

    def test_query_buffer_runtime_concat_is_blocked(self):
        self.assertIn("SQL007", rules(
            'mysql_tquery(db, query); strcat(query, inputtext, sizeof(query));'
        ))

    def test_safe_constant_query_and_literal_concat(self):
        self.assertEqual(rules(
            'mysql_query(db, "SELECT 1"); '
            'mysql_tquery(db, query); strcat(query, " LIMIT 1", sizeof(query));'
        ), set())

    def test_comments_and_ordinary_chat_are_ignored(self):
        self.assertEqual(rules(
            '// format(q, sizeof(q), "SELECT * FROM users WHERE u=\\'%s\\'", inputtext);\n'
            '/* mysql_format(db,q,100,"SELECT \\'%s\\'", user); */\n'
            'format(message, sizeof(message), "Player: %s", name);\n'
            'native mysql_format(MySQL:handle, output[], max_len, const fmt[], ...);\n'
        ), set())

    def test_dynamic_sql_in_migration_is_blocked(self):
        self.assertEqual({x.rule for x in audit_sql(
            "db/migrations/017.sql", "SET @q = CONCAT('SELECT ', col); PREPARE s FROM @q;"
        )}, {"SQL008"})

    def test_sql_comments_and_literals_not_migration_dynamic(self):
        self.assertEqual(audit_sql(
            "db/migrations/017.sql",
            "-- PREPARE q FROM @input;\nSELECT 'PREPARE q FROM @input';"
        ), [])

    def test_static_string_detects_only_literal_fragments(self):
        self.assertEqual(static_string('"SELECT " "1"'), "SELECT 1")
        self.assertIsNone(static_string('prefix + "SELECT 1"'))

    def test_missing_directories_fail_closed(self):
        with tempfile.TemporaryDirectory() as tmp:
            self.assertEqual(main(["--root", tmp, "--output",
                                   str(Path(tmp) / "report.json")]), 2)

    def test_ci_job_is_required_for_build(self):
        workflow = (Path(__file__).resolve().parents[1] /
                    ".github/workflows/build.yml").read_text(encoding="utf-8")
        self.assertIn("name: SQL Security Gate", workflow)
        self.assertIn("needs: sql_security", workflow)
        self.assertIn("python3 tools/check_sql_security.py", workflow)


if __name__ == "__main__":
    unittest.main()
