"""Контрактные проверки реализации файловой регистрации без запуска Pawn и GTA."""

from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]
SCRIPT = (ROOT / "gamemodes" / "signal0317.pwn").read_text(encoding="utf-8")
SCRIPT += (ROOT / "gamemodes" / "includes" / "auth_dialogs.inc").read_text(encoding="utf-8")
COMMANDS = (ROOT / "gamemodes" / "includes" / "commands.inc").read_text(encoding="utf-8")
DOC = (ROOT / "docs" / "FILE_AUTH.md").read_text(encoding="utf-8")


class AuthScaffoldTest(unittest.TestCase):
    def test_registration_is_story_driven(self):
        for text in (
            "ПИСЬМО БЕЗ ОТПРАВИТЕЛЯ",
            "Создание личного дела",
            "Контрольный отпечаток",
            "|  Память",
            "Я не помню ночь аварии",
            "В радиоэфире кто-то назвал моё имя",
            "Подпись в письме была не её",
        ):
            with self.subTest(text=text):
                self.assertIn(text, SCRIPT)

    def test_bcrypt_and_no_plain_password_persistence(self):
        for token in (
            "DIALOG_STYLE_PASSWORD",
            "AUTH_REGISTER_PASSWORD",
            "AUTH_REGISTER_CONFIRM",
            "AUTH_VERIFYING",
            "OnPlayerRequestSpawn(playerid)",
            "gAuthState[playerid] != AUTH_READY",
            "bcrypt_hash(",
            "bcrypt_verify(",
            "bcrypt_get_hash(",
            "gAuthToken[playerid] != token",
            "AUTH_LOCK_SECONDS",
        ):
            with self.subTest(token=token):
                self.assertIn(token, SCRIPT)
        self.assertNotIn("SHA256_PassHash", SCRIPT)
        self.assertNotIn("fwrite(handle, gPasswordPending", SCRIPT)

    def test_file_integrity_and_validation_contracts(self):
        for text in (
            "signal_accounts",
            "fcreatedir(ACCOUNT_DIRECTORY)",
            "fexist(path)",
            "io_read",
            "io_write",
            "frename(temp, path)",
            "Auth_SetAccountName",
            "Auth_ValidPassword",
            "c == '_'",
            "SIGNAL0317|1",
            "strlen(hash) != 60",
        ):
            with self.subTest(text=text):
                self.assertIn(text, SCRIPT)

    def test_commands_remain_in_english(self):
        for command in ("/help", "/story", "/position"):
            self.assertIn('cmd:' + command.lstrip('/') + '(', COMMANDS)
        for wrong in ('"/помощь"', '"/сюжет"', '"/позиция"'):
            self.assertNotIn(wrong, SCRIPT)

    def test_verification_plan_exists(self):
        self.assertIn("Ручная приёмка", DOC)
        self.assertIn("samp-bcrypt", DOC)
        self.assertIn("Windows x86", DOC)
        self.assertIn("сбрасываются при перезапуске", DOC)


if __name__ == "__main__":
    unittest.main()
