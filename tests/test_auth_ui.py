"""AUTH-UI-01: проверки оформленных стандартных диалогов и защиты аккаунтов."""

from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]
CORE = (ROOT / "gamemodes/signal0317.pwn").read_text(encoding="utf-8")
DIALOGS = (ROOT / "gamemodes/includes/auth_dialogs.inc").read_text(encoding="utf-8")
CAMERA = (ROOT / "gamemodes/includes/auth_camera.inc").read_text(encoding="utf-8")
COMMANDS = (ROOT / "gamemodes/includes/commands.inc").read_text(encoding="utf-8")
BUILD = (ROOT / "tools/build_windows.py").read_text(encoding="utf-8")
WORKFLOW = (ROOT / ".github/workflows/pawn-build.yml").read_text(encoding="utf-8")


class AuthUiContractTest(unittest.TestCase):
    def test_dialogs_are_primary_ui_without_textdraws(self):
        for phrase in (
            "ShowPlayerDialog(", "DIALOG_STYLE_PASSWORD", "DIALOG_STYLE_LIST",
            "DIALOG_STYLE_MSGBOX", "AuthDialog_Intro(", "AuthDialog_Register(",
            "AuthDialog_Confirm(", "AuthDialog_Background(", "AuthDialog_Login(",
            "AuthDialog_Success(", "С возвращением!", "АККАУНТ СОЗДАН",
            "Tarkenso RPG", "{3B82F6}", "{34D399}", "{EF4444}",
        ):
            with self.subTest(phrase=phrase):
                self.assertIn(phrase, DIALOGS)
        self.assertIn('#include "auth_dialogs.inc"', CORE)
        self.assertNotIn("CreatePlayerTextDraw(", CORE + DIALOGS)
        self.assertNotIn("OnPlayerClickPlayerTextDraw(", CORE + DIALOGS)
        self.assertFalse((ROOT / "gamemodes/includes/auth_ui.inc").exists())

    def test_registration_and_login_security(self):
        for phrase in (
            "gAuthDialogOpen[playerid] = true", "DIALOG_STYLE_PASSWORD",
            "return AuthDialog_Present(", "Пароль будет защищён bcrypt",
        ):
            self.assertIn(phrase, DIALOGS)
        for phrase in (
            "if (!gAuthDialogOpen[playerid]) return 1;",
            "case DIALOG_INTRO:", "case DIALOG_REGISTER:", "case DIALOG_CONFIRM:",
            "case DIALOG_BACKGROUND:", "case DIALOG_LOGIN:", "case DIALOG_SUCCESS:",
            "Auth_ValidPassword(inputtext)", "gPasswordPending[playerid][0] = EOS",
            "gAuthState[playerid] = AUTH_HASHING;",
            "gAuthState[playerid] = AUTH_VERIFYING;",
            "Auth_EnterWorld(playerid)", "Auth_AuthFailure(playerid)",
        ):
            self.assertIn(phrase, CORE)
        self.assertNotIn("printf(inputtext", CORE + DIALOGS)
        self.assertNotIn('fwrite(handle, gPasswordPending', CORE)

    def test_background_choice_is_exactly_three_rows(self):
        start = DIALOGS.index("stock AuthDialog_Background(")
        end = DIALOGS.index("stock AuthDialog_Region(", start)
        rows = DIALOGS[start:end]
        self.assertEqual(rows.count('\\n'), 2)
        self.assertIn("Я не помню ночь аварии", rows)
        self.assertIn("В радиоэфире кто-то назвал моё имя", rows)
        self.assertIn("Подпись в письме была не её", rows)
        self.assertIn("gBackground[playerid] = listitem + 1;", CORE)
        self.assertIn("if (listitem < 0 || listitem > 2)", CORE)

    def test_account_storage_kept_as_is(self):
        for phrase in (
            "SIGNAL0317|1", "SIGNAL0317|2", "AUTH_COST           (12)", "frename(temp, path)",
            "Auth_ReadAccount(playerid)", "Auth_WriteAccount(playerid, hash)",
            "bcrypt_verify(", "bcrypt_hash(", "Auth_IsLocked(playerid)",
            "gAuthToken[playerid] != token", "AUTH_LOCK_SECONDS   (300)",
        ):
            self.assertIn(phrase, CORE)

    def test_session_and_npc_guards(self):
        for phrase in (
            "OnPlayerRequestSpawn(playerid)", "OnPlayerDisconnect(playerid, reason)",
            "gAuthState[playerid] != AUTH_READY", "AuthCamera_Stop(playerid)",
            "TogglePlayerSpectating(playerid, false)",
        ):
            self.assertIn(phrase, CORE)
        for phrase in (
            "TogglePlayerSpectating(playerid, true)", "InterpolateCameraPos(",
            "InterpolateCameraLookAt(", "KillTimer(",
            "gAuthToken[playerid] != token",
        ):
            self.assertIn(phrase, CAMERA)

    def test_cp1251_build_includes_dialog_module(self):
        for source in (CORE, DIALOGS, CAMERA):
            source.encode("cp1251", errors="strict")
        for module in ("auth_camera.inc",
                       "auth_dialogs.inc", "studio_system.inc", "studio_ui.inc"):
            self.assertIn(f'"{module}"', BUILD)
            self.assertIn(f'"{module}"', WORKFLOW)


if __name__ == "__main__":
    unittest.main()
