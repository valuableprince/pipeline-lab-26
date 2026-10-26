"""Repository-wide chat message style gate tests."""
import unittest
from pathlib import Path
from tools.check_chat_style import changed_line_numbers, check

class ChatStyle(unittest.TestCase):
    def test_diff_parser(self):
        diff = ("diff --git a/gamemodes/includes/x.inc b/gamemodes/includes/x.inc\n"
                "+++ b/gamemodes/includes/x.inc\n"
                "@@ -1,0 +1,2 @@\n+SendClientMessage(playerid, -1,\n"
                '+ "[Tarkenso RPG] Готово.");\n')
        self.assertEqual(changed_line_numbers(diff)["gamemodes/includes/x.inc"], {1,2})

    def test_good_and_bad_calls(self):
        import tempfile
        with tempfile.TemporaryDirectory() as d:
            root = Path(d)
            src = root / "gamemodes/includes"
            src.mkdir(parents=True)
            file = src / "unit.inc"
            path = "gamemodes/includes/unit.inc"
            file.write_text(
                'SendClientMessage(playerid, ADMIN_OK, "[Tarkenso RPG] Готово.");\n'
                'SendClientMessage(playerid, ADMIN_ERROR, "Использование: /bad");\n'
                'SendClientMessage(playerid, -1, "[Tarkenso RPG] Справка: /ahelp");\n',
                encoding="utf-8")
            errors, checked, unknown = check({path: {1,2,3}}, root)
            self.assertEqual(checked, 3)
            self.assertEqual(unknown, 0)
            self.assertEqual(len(errors), 1)
            self.assertIn("raw usage", errors[0])
            errors, checked, unknown = check({path: {1,3}}, root)
            self.assertEqual(errors, [])

    def test_admin_help_has_uniform_multiline_brand(self):
        help_text = (Path(__file__).resolve().parents[1] / "gamemodes/includes/admin_help.inc").read_text(encoding="utf-8")
        for marker in ("Синтаксис:", "Описание:", "Пример:"):
            self.assertIn("[Tarkenso RPG] {FFFFFF}" + marker, help_text)

    def test_untouched_legacy_remains_nonblocking(self):
        import tempfile
        with tempfile.TemporaryDirectory() as d:
            root = Path(d)
            src = root / "gamemodes/includes"
            src.mkdir(parents=True)
            (src / "unit.inc").write_text(
                'SendClientMessage(playerid, -1, "Старый текст");\n', encoding="utf-8")
            errors, checked, unknown = check({}, root)
            self.assertEqual(errors, [])
            self.assertEqual(checked, 0)

if __name__ == "__main__":
    unittest.main()
