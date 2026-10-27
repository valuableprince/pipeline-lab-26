"""CONSOLE-RU-01: lossless CP1251 -> UTF-8 for Russian Pawn console text.

Code contracts and a complete 256-character mapping check: the game AMX is
CP1251, whereas log viewers and UTF-8 files expect Unicode.
"""
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[1]
INC = (ROOT / "gamemodes/includes/console_ru.inc").read_text(encoding="utf-8")
GAME = (ROOT / "gamemodes/signal0317.pwn").read_text(encoding="utf-8")
PROF = (ROOT / "gamemodes/includes/auth_profiler.inc").read_text(encoding="utf-8")

class ConsoleRuTests(unittest.TestCase):
    def test_cp1251_table_covers_entire_extended_page(self):
        match = re.search(r"gConsoleRU_CP1251\[64\] = \{([^}]+)\}", INC, re.S)
        self.assertIsNotNone(match)
        numbers = [int(x, 16) for x in re.findall(r"0x[0-9A-Fa-f]+", match.group(1))]
        self.assertEqual(len(numbers), 64)
        for byte in range(0x80, 0xC0):
            try:
                expected = ord(bytes([byte]).decode("cp1251"))
            except UnicodeDecodeError:
                expected = ord("?")
            self.assertEqual(numbers[byte - 0x80], expected,
                             f"incorrect CP1251 byte {byte:#x}")
        # Russian alphabet incl. uppercase/lowercase takes CP1251 bytes C0..FF.
        for byte in range(0xC0, 0x100):
            self.assertEqual(0x0410 + byte - 0xC0,
                             ord(bytes([byte]).decode("cp1251")))

    def test_utf8_output_boundary(self):
        for marker in [
            "stock ConsoleRU_ToUTF8(", "destination[length] = EOS;",
            "if (length + needed >= capacity) break;",
            "source[i] & 0xFF", "stock ConsoleRU_Log(",
            'print(text);', 'fopen(CONSOLE_RU_FILE, io_append)',
            "new text[480];", "ConsoleRU_Init();"
        ]:
            self.assertIn(marker, INC + GAME)
        self.assertIn('"[TARKENSO] Проверка кодировки: Привет, мир! Ёж, №1, —."', INC)
        self.assertIn("fwrite(out, text)", INC)
        self.assertNotIn("SendClientMessage", INC)
        self.assertNotIn("mysql_", INC)

    def test_auth_profiler_uses_utf8_russian_log(self):
        self.assertIn("ConsoleRU_Log(message);", PROF)
        self.assertIn("Медленный этап", PROF)
        self.assertIn("время ожидания, не CPU", PROF)
        self.assertLess(GAME.index('#include "console_ru.inc"'),
                        GAME.index('#include "auth_profiler.inc"'))
        self.assertIn('AUTHPROF_WARN_MS', PROF)

    def test_cp1251_staging_preserved(self):
        wf = (ROOT / ".github/workflows/build.yml").read_text(encoding="utf-8")
        self.assertIn('.encode("cp1251"', wf)
        self.assertIn("Создание личного дела",
                      (ROOT / "gamemodes/includes/auth_dialogs.inc").read_text(encoding="utf-8"))

if __name__ == "__main__":
    unittest.main()
