"""Encoding boundary: Pawn client strings are CP1251, host log console is UTF-8.

Use portable ASCII for print/printf format strings so that server log output
remains readable regardless of NickNinth panel locale. Keep Russian player UI.
"""
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "gamemodes" / "signal0317.pwn"
INCLUDES = ROOT / "gamemodes" / "includes"

# Direct Pawn console format strings, including calls split across multiple lines.
LOG_FORMAT = re.compile(r'\b(?:print|printf)\s*\(\s*"((?:[^"\\]|\\.)*)"', re.MULTILINE)


class ServerConsoleEncodingTest(unittest.TestCase):
    def test_all_console_log_format_strings_are_ascii(self):
        paths = (SOURCE, *INCLUDES.glob("*.inc"))
        checked = 0
        for path in paths:
            source = path.read_text(encoding="utf-8")
            for match in LOG_FORMAT.finditer(source):
                checked += 1
                with self.subTest(file=path.name, line=source.count("\n", 0, match.start()) + 1):
                    self.assertTrue(
                        match.group(1).isascii(),
                        "Console messages must use ASCII; player messages can remain Russian."
                    )
        self.assertGreater(checked, 40, "Expected to validate console logging across the game mode")

    def test_startup_is_readable_without_cp1251_console_configuration(self):
        main = SOURCE.read_text(encoding="utf-8")
        db = (INCLUDES / "db_primary.inc").read_text(encoding="utf-8")
        cleaner = (INCLUDES / "cleaner_job.inc").read_text(encoding="utf-8")
        studio = (INCLUDES / "studio_system.inc").read_text(encoding="utf-8")
        for marker in (
            "[Tarkenso RPG / SIGNAL 03:17] Starting game mode",
            "[DB-PRIMARY] MySQL unavailable",
        ):
            self.assertIn(marker, main)
        self.assertIn("MySQL schema v10 detected", db)
        self.assertIn("[Cleaner] Street cleaner job: Fort Carson", cleaner)
        self.assertIn("[Studio] Core and zone editor initialized", studio)

    def test_russian_in_game_strings_are_kept_in_cp1251_source(self):
        dialogs = (INCLUDES / "auth_dialogs.inc").read_text(encoding="utf-8")
        self.assertIn("Создание личного дела", dialogs)
        self.assertIn("Создание личного дела", dialogs.encode("cp1251").decode("cp1251"))


if __name__ == "__main__":
    unittest.main()
