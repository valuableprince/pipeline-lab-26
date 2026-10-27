"""PAYDAY-02: external phrase library, safe runtime trigger, CP1251 artifact."""
from pathlib import Path
from tempfile import TemporaryDirectory
import unittest

from tools.export_payday_phrases import (
    MAX_BYTES, MAX_COUNT, ROOT, SOURCE, export, parse_phrases,
)

PAYDAY = (ROOT / "gamemodes/includes/payday.inc").read_text(encoding="utf-8")
WORKFLOW = (ROOT / ".github/workflows/build.yml").read_text(encoding="utf-8")


class PayDayPhraseContracts(unittest.TestCase):
    def test_editorial_library_is_valid_utf8_and_bounded(self):
        entries = parse_phrases(SOURCE.read_text(encoding="utf-8"))
        self.assertGreaterEqual(len(entries), 30)
        self.assertLessEqual(len(entries), MAX_COUNT)
        self.assertIn(
            "Вы стали опытнее. По крайней мере, так утверждает наша база данных.",
            entries,
        )
        self.assertTrue(all(len(s.encode("cp1251")) <= MAX_BYTES for s in entries))

    def test_export_is_byte_exact_cp1251_without_bom(self):
        with TemporaryDirectory() as d:
            dest = Path(d) / "scriptfiles" / "payday_levelup.txt"
            expected = parse_phrases(SOURCE.read_text(encoding="utf-8"))
            self.assertEqual(export(SOURCE, dest), len(expected))
            data = dest.read_bytes()
            self.assertEqual(
                data, ("\n".join(expected) + "\n").encode("cp1251")
            )
            self.assertFalse(data.startswith(b"\xef\xbb\xbf"))

    def test_bad_external_inputs_fail_closed(self):
        for raw in (
            "",
            "текст {FF0000}цвет\n",
            "Всё получилось 😄\n",
            "Коротко.\n",
            "Одна и та же фраза записана сюда.\nОдна и та же фраза записана сюда.\n",
            "Очень длинная фраза " + "а" * (MAX_BYTES + 2),
            "Ваша удача\tкажется необъяснимой.\n",
            "Библиотека переполнена количеством новых фраз.\n" * (MAX_COUNT + 1),
        ):
            with self.subTest(raw=raw[:35]):
                with self.assertRaises(ValueError):
                    parse_phrases(raw)

    def test_runtime_loads_once_and_only_messages_on_levelup(self):
        self.assertIn('#define PAYDAY_LEVELUP_PHRASES_FILE "payday_levelup.txt"', PAYDAY)
        self.assertIn("fopen(PAYDAY_LEVELUP_PHRASES_FILE, io_read)", PAYDAY)
        self.assertIn("PAYDAY_LEVELUP_PHRASE_LIMIT (64)", PAYDAY)
        self.assertIn("PAYDAY_LEVELUP_PHRASE_SIZE (121)", PAYDAY)
        self.assertIn("fread(handle, line, sizeof(line))", PAYDAY)
        self.assertIn("fclose(handle);", PAYDAY)
        self.assertIn("gPayDayPhrases[random(gPayDayPhraseCount)]", PAYDAY)
        self.assertIn("if (gPayDayPhraseCount <= 0)", PAYDAY)
        self.assertIn("PayDay_LoadPhrases();", PAYDAY)
        self.assertEqual(PAYDAY.count("PayDay_LoadPhrases();"), 1)
        self.assertEqual(PAYDAY.count("PayDay_SendLevelUpPhrase(playerid);"), 1)
        notifications = PAYDAY[PAYDAY.index("stock PayDay_Notify("):PAYDAY.index("stock PayDay_Grant(")]
        self.assertIn("if (levelUp)", notifications)
        self.assertIn("PayDay_SendLevelUpPhrase(playerid);", notifications.split("if (levelUp)", 1)[1])
        self.assertLess(notifications.index("========================================"),
                        notifications.index("PayDay_SendLevelUpPhrase(playerid);"))
        self.assertNotIn("fopen(", notifications)
        self.assertLess(PAYDAY.index("if (!DBPrimary_Execute(query, affected) || affected != 1)"),
                        PAYDAY.index("PayDay_Notify(playerid, earnedExp, levelUp);"))

    def test_artifact_contains_phrase_file_and_tracks_editing(self):
        self.assertIn("data/payday/**", WORKFLOW)
        self.assertIn("tools/export_payday_phrases.py", WORKFLOW)
        self.assertIn("python3 tools/export_payday_phrases.py", WORKFLOW)
        self.assertIn("build/scriptfiles/payday_levelup.txt", WORKFLOW)
        self.assertIn("build/compiled.amx", WORKFLOW)


if __name__ == "__main__":
    unittest.main()
