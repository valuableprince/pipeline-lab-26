"""Автоматические проверки русского языка; не заменяют тест в SA-MP."""

import pathlib
import unittest

from tools.build_windows import encode_source, compiler_command

ROOT = pathlib.Path(__file__).resolve().parents[1]


class LocalizationTest(unittest.TestCase):
    def test_source_roundtrip_cp1251(self):
        source = (ROOT / "gamemodes" / "signal0317.pwn").read_text(encoding="utf-8")
        source += (ROOT / "gamemodes" / "includes" / "auth_dialogs.inc").read_text(encoding="utf-8")
        self.assertIn("СИГНАЛ: 03:17", source)
        encoded = encode_source(source)
        self.assertEqual(encoded.decode("cp1251"), source)
        self.assertIn("Создание личного дела", encoded.decode("cp1251"))

    def test_unsupported_unicode_rejected(self):
        with self.assertRaises(UnicodeEncodeError):
            encode_source("Нельзя использовать смайлик 😀")

    def test_compiler_uses_explicit_source_and_include(self):
        cmd = compiler_command(
            pathlib.Path("C:/omp/qawno/pawncc.exe"),
            pathlib.Path("C:/omp/qawno/include"),
            pathlib.Path("C:/out/signal0317"),
            pathlib.Path("C:/tmp/signal0317.pwn"),
        )
        self.assertIn("-iC:/omp/qawno/include", cmd)
        self.assertIn("-oC:/out/signal0317", cmd)
        self.assertEqual(cmd[-1], "C:/tmp/signal0317.pwn")


if __name__ == "__main__":
    unittest.main()
