"""Global dialog guard: real regressions, scanner coverage and failing CLI."""
from contextlib import redirect_stdout
from io import StringIO
from pathlib import Path
import tempfile
import unittest

from tools.check_dialog_ids import ROOT, audit, main


def show(expression):
    return f'ShowPlayerDialog(playerid, {expression}, DIALOG_STYLE_LIST, "title", "body", "OK", "Back");'


class DialogIdTests(unittest.TestCase):
    def clean(self, sources):
        report = audit(sources)
        self.assertEqual(report.errors, [])
        return report

    def bad(self, sources, *messages):
        report = audit(sources)
        self.assertTrue(report.errors)
        for message in messages:
            self.assertIn(message, "\n".join(report.errors))
        return report

    def test_current_project_has_no_collisions_or_unknown_ids(self):
        texts = {p.relative_to(ROOT).as_posix(): p.read_text("utf-8")
                 for p in (ROOT / "gamemodes").rglob("*") if p.suffix in {".pwn", ".inc"}}
        report = self.clean(texts)
        self.assertGreaterEqual(len(report.dialogs), 99)
        self.assertIn("TDS_Dialog", report.wrappers)
        self.assertIn("AuthDialog_Present", report.wrappers)

    def test_historical_ship_and_container_collision_fails(self):
        self.bad({"ship.inc": "#define SHIP_PANEL_MAIN (6340)\n" + show("SHIP_PANEL_MAIN"),
                  "cleaner.inc": "#define CLEANER_EDITOR_LIST (6340)\n" + show("CLEANER_EDITOR_LIST")},
                 "6340", "SHIP_PANEL_MAIN", "CLEANER_EDITOR_LIST", "ship.inc:1", "cleaner.inc:1")

    def test_historical_tds_and_ship_collision_fails_through_wrapper(self):
        self.bad({"ship.inc": "#define SHIP_MAIN (7100)\n" + show("SHIP_MAIN"),
                  "tds.inc": "#define TDS_MAIN (7100)\nstock TDS_Dialog(p, id) { " + show("id") + " }\nTDS_Dialog(p, TDS_MAIN);"},
                 "duplicate dialog ID 7100", "SHIP_MAIN", "TDS_MAIN")

    def test_two_windows_in_same_file_collide(self):
        self.bad({"a.inc": "#define ONE 99\n#define TWO 99\n" + show("ONE") + show("TWO")}, "duplicate dialog ID 99", "ONE", "TWO")

    def test_same_named_window_can_be_shown_from_multiple_modules(self):
        report = self.clean({"ids.inc": "#define WINDOW 123\n", "a.inc": show("WINDOW"), "b.inc": show("WINDOW")})
        self.assertEqual(set(report.dialogs), {123})

    def test_alias_and_unused_range_markers_are_not_separate_windows(self):
        report = self.clean({"a.inc": "#define FIRST_DIALOG 7200\n#define LAST_DIALOG 7247\n#define MAIN_DIALOG 7200\n#define ALIAS MAIN_DIALOG\n" + show("MAIN_DIALOG") + show("ALIAS")})
        self.assertEqual(set(report.dialogs), {7200})

    def test_unrelated_numeric_constants_do_not_claim_dialogs(self):
        self.clean({"a.inc": "#define OBJECT_MODEL 6100\n#define BUFFER_SIZE 6100\n#define WINDOW 6100\n" + show("WINDOW")})

    def test_literals_decimal_hex_and_repeated_calls(self):
        report = self.clean({"a.inc": show("6100") + show("0x17D4") + show("(6100)")})
        self.assertEqual(set(report.dialogs), {6100})
        self.bad({"a.inc": show("6100"), "b.inc": show("0x17D4")}, "6100")

    def test_arithmetic_and_const_declarations(self):
        report = self.clean({"a.inc": "#define BASE 6000\nconst WINDOW = BASE + (5 * 2);\n" + show("WINDOW")})
        self.assertEqual(set(report.dialogs), {6010})
        self.bad({"a.inc": "#define BASE 6000\n#define ONE (BASE+10)\n#define TWO 6010\n" + show("ONE") + show("TWO")}, "6010")

    def test_multiline_macro_expression(self):
        report = self.clean({"a.inc": "#define WINDOW \\\n (6000 + 10)\n" + show("WINDOW")})
        self.assertEqual(set(report.dialogs), {6010})

    def test_enum_explicit_and_incremented_ids(self):
        report = self.clean({"a.inc": "enum E_DIALOG { FIRST = 6000, SECOND, THIRD = SECOND+4 };\n" + show("FIRST") + show("SECOND") + show("THIRD")})
        self.assertEqual(set(report.dialogs), {6000, 6001, 6005})

    def test_ternary_checks_both_branches(self):
        self.bad({"a.inc": "#define A 6100\n#define B 6200\n" + show("flag ? A : B"),
                  "b.inc": "#define OTHER 6200\n" + show("OTHER")}, "6200")
        report = self.clean({"a.inc": show("flag ? (other ? 6100 : 6200) : 6300")})
        self.assertEqual(set(report.dialogs), {6100, 6200, 6300})

    def test_nested_wrappers_and_different_parameter_positions(self):
        report = self.clean({"a.inc": """
stock bool:Inner(p, id, const caption[]) {
    ShowPlayerDialog(p, id, DIALOG_STYLE_LIST, caption, "one,two", "OK", "");
}
Outer(id, p) { return Inner(p, id, "caption"); }
public Start(p) { return Outer(6100, p); }
"""})
        self.assertEqual(report.wrappers, ["Inner", "Outer"])
        self.assertEqual(set(report.dialogs), {6100})

    def test_function_alias_is_traced(self):
        report = self.clean({"a.inc": "#define ShowAlias ShowPlayerDialog\n" + show("6100").replace("ShowPlayerDialog", "ShowAlias")})
        self.assertEqual(set(report.dialogs), {6100})

    def test_dynamic_id_and_unknown_symbol_fail(self):
        for expr in ("ids[playerid]", "6100 + playerid", "UNKNOWN", "GetDialogId()"):
            with self.subTest(expression=expr):
                self.bad({"a.inc": show(expr)}, "a.inc:1")

    def test_cyclic_or_ambiguous_definitions_fail(self):
        self.bad({"a.inc": "#define A B\n#define B A\n" + show("A")}, "cyclic constant")
        self.bad({"a.inc": "#define WINDOW 6100\n#define WINDOW 6200\n" + show("WINDOW")}, "ambiguous definition")

    def test_recursive_unbound_wrapper_is_not_silently_accepted(self):
        self.bad({"a.inc": "stock Loop(p, id) { " + show("id") + " Loop(p, id); }"}, "unbound dialog wrapper")
        self.bad({"a.inc": "stock Unused(p, id) { " + show("id") + " }"}, "unbound dialog wrapper")

    def test_comments_strings_and_multiline_calls(self):
        report = self.clean({"a.inc": '''
// ShowPlayerDialog(p, 6100, 0);
/* #define WINDOW 6100
   ShowPlayerDialog(p, 6100, 0); */
new caption[] = "ShowPlayerDialog(p, 6100, 0); // not a call";
stock Window(p) {
    return ShowPlayerDialog(
        p,
        6200, /* ID */
        DIALOG_STYLE_MSGBOX, "https://example.org/", "a,b (c) {d}", "OK", "");
}
'''})
        self.assertEqual(set(report.dialogs), {6200})

    def test_api_boundaries_and_close_are_handled(self):
        report = self.clean({"a.inc": show("0") + show("32767") + show("-1") + show("INVALID_DIALOG_ID")})
        self.assertEqual(set(report.dialogs), {0, 32767})
        self.bad({"a.inc": show("32768")}, "outside open.mp range")

    def test_invalid_arithmetic_fails_without_execution(self):
        for expression in ("2 ** 99", "1 / 0", "1 << 99999", "2147483647 + 1", "__import__('os')"):
            with self.subTest(expression=expression):
                self.bad({"a.inc": show(expression)})

    def test_dialog_generating_macros_fail_explicitly(self):
        for source in ("#define ShowPlayerDialog NativeDialog\n",
                       "#define Popup(%0) ShowPlayerDialog(%0, 6100, 0)\n",
                       "#define Popup(%0) \\\n ShowPlayerDialog(%0, 6100, 0)\n"):
            with self.subTest(source=source):
                self.bad({"a.inc": source + show("6200")}, "unsupported dialog macro")

    def test_cli_blocks_collision_and_reports_both_files(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            folder = root / "gamemodes/includes"
            folder.mkdir(parents=True)
            (folder / "first.inc").write_text("#define FIRST 6100\n" + show("FIRST"))
            second = folder / "second.inc"
            second.write_text("#define SECOND 6200\n" + show("SECOND"))
            with redirect_stdout(StringIO()):
                self.assertEqual(main(["--root", directory]), 0)
            second.write_text("#define SECOND 6100\n" + show("SECOND"))
            stream = StringIO()
            with redirect_stdout(stream):
                self.assertEqual(main(["--root", directory]), 1)
            self.assertIn("first.inc:1", stream.getvalue())
            self.assertIn("second.inc:1", stream.getvalue())
            self.assertIn("6100", stream.getvalue())

    def test_cli_missing_or_empty_sources_cannot_pass(self):
        with tempfile.TemporaryDirectory() as directory, redirect_stdout(StringIO()):
            self.assertEqual(main(["--root", directory]), 2)
            (Path(directory) / "gamemodes").mkdir()
            self.assertEqual(main(["--root", directory]), 1)

    def test_ci_invokes_the_guard_and_watches_tool_changes(self):
        workflow = (ROOT / ".github/workflows/build.yml").read_text("utf-8")
        command = "python3 tools/check_dialog_ids.py"
        self.assertIn(command, workflow)
        self.assertLess(workflow.index(command), workflow.index("- name: Install runtime"))
        self.assertIn('"tools/check_dialog_ids.py"', workflow)


if __name__ == "__main__":
    unittest.main()
