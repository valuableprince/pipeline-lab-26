"""Contract and behavioural tests for the advisory TARKENSO Pawn scanner."""
from contextlib import redirect_stdout
from io import StringIO
import json
from pathlib import Path
from tempfile import TemporaryDirectory
import unittest

from tools.check_pawn_static import (
    analyze_sources, calls, closing, functions, main, mask_opaque,
    report_data, source_tree, split_args,
)


def rules(text: str) -> set[str]:
    return {issue.rule for issue in analyze_sources({"gamemodes/demo.inc": text})}


class PawnStaticAnalyzerTests(unittest.TestCase):
    def test_mask_comments_and_strings_not_mistaken_for_code(self):
        sample = r'''
// fclose(fake); public OnBad() { }
stock Sample()
{
    new text[60];
    format(text, sizeof(text), "fopen() { } // \"quoted\"");
    /* SetTimer("Nope", 1000, true); */
    return 1;
}
'''
        self.assertNotIn("RESOURCE_UNRELEASED", rules(sample))
        self.assertNotIn("TIMER_UNTRACKED", rules(sample))
        clean = mask_opaque(sample)
        self.assertEqual(clean.count("\n"), sample.count("\n"))
        self.assertEqual(len(clean), len(sample))
        self.assertEqual(len(functions(sample, clean)), 1)

    def test_nested_call_split_and_parenthesis(self):
        original = 'a, sizeof(b[0]), some(2, 3), "comma, not separator"'
        clean = mask_opaque(original)
        self.assertEqual(split_args(original, clean), [
            "a", "sizeof(b[0])", "some(2, 3)", '"comma, not separator"'
        ])
        self.assertEqual(closing(clean, clean.index("("), "(", ")"),
                         clean.index(")", clean.index("(")))
        self.assertEqual(
            [(name, args[0]) for name, _, args in calls(
                "format(a, sizeof(a), \"%d\", x);",
                mask_opaque("format(a, sizeof(a), \"%d\", x);")
            ) if name == "format"],
            [("format", "a")],
        )

    def test_file_resource_unreleased(self):
        source = '''
stock BadFile()
{
    new File:handle = fopen("save.dat", io_read);
    if (!handle) return 0;
    return 1;
}
'''
        self.assertIn("RESOURCE_UNRELEASED", rules(source))

    def test_file_resource_return_before_close(self):
        source = '''
stock BadPath()
{
    new File:handle = fopen("save.dat", io_read);
    if (!handle) return 0;
    if (broken) return 0;
    fclose(handle);
    return 1;
}
'''
        self.assertIn("RESOURCE_EARLY_RETURN", rules(source))

    def test_file_resource_valid_guard_and_cleanup(self):
        source = '''
stock GoodFile()
{
    new File:handle = fopen("save.dat", io_read);
    if (!handle) return 0;
    new text[64];
    fread(handle, text, sizeof(text));
    fclose(handle);
    return 1;
}
'''
        self.assertFalse(any(r.startswith("RESOURCE_") for r in rules(source)))

    def test_mysql_cache_release(self):
        bad = '''
stock BadCache()
{
    new Cache:result = mysql_query(db, "SELECT 1");
    return 1;
}
'''
        good = '''
stock GoodCache()
{
    new Cache:result = mysql_query(db, "SELECT 1");
    cache_delete(result);
    return 1;
}
'''
        self.assertIn("RESOURCE_UNRELEASED", rules(bad))
        self.assertNotIn("RESOURCE_UNRELEASED", rules(good))

    def test_repeating_timer_with_and_without_kill(self):
        bad = '''
new gTaskTimer;
stock StartTask()
{
    gTaskTimer = SetTimer("Tick", 1000, true);
    return 1;
}
'''
        good = dict(
            **{"gamemodes/task.inc": bad},
            **{"gamemodes/cleanup.inc": '''
stock StopTask()
{
    KillTimer(gTaskTimer);
    return 1;
}
'''}
        )
        self.assertIn("TIMER_UNRELEASED", rules(bad))
        self.assertNotIn("TIMER_UNRELEASED",
                         {f.rule for f in analyze_sources(good)})
        self.assertIn("TIMER_UNTRACKED", rules('''
stock Start()
{
    SetTimer("Tick", 1000, true);
    return 1;
}
'''))
        self.assertNotIn("TIMER_UNTRACKED", rules('''
stock Start()
{
    SetTimer("Once", 1000, false);
    return 1;
}
'''))

    def test_buffer_length_and_wrong_sizeof(self):
        sample = '''
stock BadFormat()
{
    new result[20], other[150];
    format(result, sizeof(other), "%d", 1);
    format(result, 100, "%d", 1);
    strcat(result, other, 50);
    return 1;
}
'''
        reported = rules(sample)
        self.assertIn("BUFFER_WRONG_SIZEOF", reported)
        self.assertIn("BUFFER_LENGTH_OVERFLOW", reported)

    def test_valid_buffer_and_unknown_extents(self):
        sample = '''
stock GoodFormat()
{
    new result[64];
    format(result, sizeof(result), "%d", 1);
    strcat(result, "-");
    return 1;
}
'''
        self.assertFalse(any(r.startswith("BUFFER_") for r in rules(sample)))
        self.assertIn("BUFFER_REVIEW_BOUND", rules('''
stock Append(buffer[])
{
    strcat(buffer, "something");
    return 1;
}
'''))

    def test_callbacks_with_missing_return_and_partial_fallthrough(self):
        source = '''
public OnAbsent()
{
    printf("x");
}
public OnPartial()
{
    if (ready) return 1;
    printf("x");
}
public OnGood()
{
    if (ready) return 1;
    return 0;
}
'''
        found = analyze_sources({"gamemodes/demo.pwn": source})
        self.assertEqual(
            {(f.rule, f.message.split(" ")[1]) for f in found
             if f.rule.startswith("CALLBACK_")},
            {("CALLBACK_NO_RETURN", "OnAbsent"),
             ("CALLBACK_FALLTHROUGH", "OnPartial")},
        )

    def test_async_player_mutation_is_reviewed(self):
        code = '''
public DB_OnQuery(playerid, result)
{
    SetPlayerPos(playerid, 2.0, 3.0, 4.0);
    return 1;
}
'''
        self.assertIn("PLAYER_ASYNC_STATE", rules(code))
        self.assertNotIn("PLAYER_ASYNC_STATE", rules(code.replace(
            "SetPlayerPos", "if (!IsPlayerConnected(playerid)) return 0;\n    SetPlayerPos"
        )))

    def test_advisory_outputs_json_and_strict_is_opt_in(self):
        with TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "gamemodes").mkdir()
            (root / "gamemodes" / "sample.inc").write_text(
                'public OnBad()\n{\n    printf("hello");\n}\n', encoding="utf-8"
            )
            report = root / "report.json"
            self.assertEqual(len(source_tree(root)), 1)
            with redirect_stdout(StringIO()):
                self.assertEqual(main([
                    "--root", str(root), "--output", str(report)
                ]), 0)
            obj = json.loads(report.read_text(encoding="utf-8"))
            self.assertGreater(obj["total_findings"], 0)
            self.assertIn("CALLBACK_NO_RETURN", obj["by_rule"])
            with redirect_stdout(StringIO()):
                self.assertEqual(main([
                    "--root", str(root), "--output", str(report), "--strict"
                ]), 1)
            self.assertEqual(report_data([], 3)["files_scanned"], 3)

    def test_empty_tree_cannot_silently_pass(self):
        with TemporaryDirectory() as directory:
            with redirect_stdout(StringIO()):
                self.assertEqual(main([
                    "--root", directory, "--output", str(Path(directory) / "x.json")
                ]), 2)


if __name__ == "__main__":
    unittest.main()
