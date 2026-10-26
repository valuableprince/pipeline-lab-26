"""Проверки структуры проекта и обязательного русского текста."""

import pathlib
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
SCRIPT = ROOT / "gamemodes" / "signal0317.pwn"
COMMANDS = ROOT / "gamemodes" / "includes" / "commands.inc"


class ScaffoldTest(unittest.TestCase):
    def test_pawn_entrypoint_exists(self):
        self.assertTrue(SCRIPT.is_file())
        text = SCRIPT.read_text(encoding="utf-8")
        for required in (
            "#include <open.mp>",
            "public OnGameModeInit()",
            "public OnPlayerConnect(playerid)",
            "public OnPlayerSpawn(playerid)",
            '#include "commands.inc"',
            '#include "Pawn.CMD.inc"',
            '#include "sscanf2.inc"',
            '#include "story_prologue.inc"',
        ):
            with self.subTest(required=required):
                self.assertIn(required, text)
        auth_dialogs = (ROOT / "gamemodes" / "includes" / "auth_dialogs.inc").read_text(encoding="utf-8")
        self.assertIn("Создание личного дела", auth_dialogs)
        story = (ROOT / "gamemodes/includes/story_prologue.inc").read_text(encoding="utf-8")
        self.assertIn("ПРОЛОГ ЗАВЕРШЁН", story)
        commands = COMMANDS.read_text(encoding="utf-8")
        for command in ("help", "story", "position"):
            self.assertIn("cmd:" + command + "(", commands)
        self.assertNotIn("OnPlayerCommandText", text)

    def test_commands_are_latin_only(self):
        text = SCRIPT.read_text(encoding="utf-8")
        for forbidden in ('"/помощь"', '"/сюжет"', '"/позиция"'):
            self.assertNotIn(forbidden, text)

    def test_local_config_is_ignored(self):
        ignore = (ROOT / ".gitignore").read_text(encoding="utf-8")
        self.assertIn("config.json", ignore)
        self.assertIn("*.amx", ignore)

    def test_english_user_facing_placeholders_removed(self):
        text = SCRIPT.read_text(encoding="utf-8")
        for old in ("Welcome to Fort Carson", "PROLOGUE: Room 17",
                    "Quest engine: not implemented", "Location: %.2f"):
            self.assertNotIn(old, text)


if __name__ == "__main__":
    unittest.main()
