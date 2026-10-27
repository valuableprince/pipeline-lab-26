"""Repository-wide chat message style gate tests."""
import unittest
from pathlib import Path
from tools.check_chat_style import changed_line_numbers, check

class ChatStyle(unittest.TestCase):
    def test_diff_parser(self):
        diff = ("diff --git a/gamemodes/includes/x.inc b/gamemodes/includes/x.inc\n"
                "+++ b/gamemodes/includes/x.inc\n"
                "@@ -1,0 +1,2 @@\n+SendClientMessage(playerid, -1,\n"
                '+ "{D9AA55}Готово.");\n')
        self.assertEqual(changed_line_numbers(diff)["gamemodes/includes/x.inc"], {1,2})

    def test_palette_no_mandatory_prefix_and_translit(self):
        import tempfile
        with tempfile.TemporaryDirectory() as d:
            root = Path(d)
            src = root / "gamemodes/includes"
            src.mkdir(parents=True)
            file = src / "unit.inc"
            path = "gamemodes/includes/unit.inc"
            file.write_text(
                'SendClientMessage(playerid, -1, "{D9AA55}[PAYDAY] {FFFFFF}Новый уровень.");\n'
                'SendClientMessage(playerid, ADMIN_OK, "{B6E4B3}Готово.");\n'
                'SendClientMessage(playerid, ADMIN_ERROR, "Использование: /command");\n'
                'SendClientMessage(playerid, -1, "{FF0000}Ошибка.");\n'
                'SendClientMessage(playerid, -1, "{9FAAB8}Ispolzovanie: /command");\n'
                'SendClientMessage(playerid, -1, "{9faab8}Подсказка: /ahelp");\n',
                encoding="utf-8")
            errors, checked, unknown = check({path: {1, 2, 3, 4, 5, 6}}, root)
            self.assertEqual(checked, 6)
            self.assertEqual(unknown, 0)
            self.assertEqual(len(errors), 2)
            self.assertTrue(any("outside /ahelp palette" in error for error in errors))
            self.assertTrue(any("transliteration" in error for error in errors))
            errors, checked, unknown = check({path: {1, 2, 3, 6}}, root)
            self.assertEqual(errors, [])
            self.assertEqual(checked, 4)

    def test_dynamic_chat_remains_unverified_not_rejected(self):
        import tempfile
        with tempfile.TemporaryDirectory() as d:
            root = Path(d)
            src = root / "gamemodes/includes"
            src.mkdir(parents=True)
            (src / "unit.inc").write_text(
                "SendClientMessage(playerid, -1, message);\n", encoding="utf-8")
            errors, checked, unknown = check({"gamemodes/includes/unit.inc": {1}}, root)
            self.assertEqual(errors, [])
            self.assertEqual((checked, unknown), (0, 1))

    def test_admin_help_palette_reference(self):
        help_text = (Path(__file__).resolve().parents[1] / "gamemodes/includes/admin_help.inc").read_text(encoding="utf-8")
        for color in ("{D9AA55}", "{FFFFFF}", "{B6E4B3}", "{9FAAB8}"):
            self.assertIn(color, help_text)
        for marker in ("Синтаксис:", "Описание:", "Пример:"):
            self.assertIn(marker, help_text)

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
