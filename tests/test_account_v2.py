"""ACCOUNT-V2: данные игрока и старые санкции в едином .acc."""
import unittest
from pathlib import Path

BASE = Path(__file__).resolve().parents[1]
GAME = (BASE / "gamemodes/signal0317.pwn").read_text(encoding="utf-8")
ADMIN = (BASE / "gamemodes/includes/admin_system.inc").read_text(encoding="utf-8")
CLEAN = (BASE / "gamemodes/includes/cleaner_job.inc").read_text(encoding="utf-8")
DOC = (BASE / "docs/FILE_AUTH.md").read_text(encoding="utf-8")


class AccountV2Contract(unittest.TestCase):
    def test_version_and_legacy_support(self):
        self.assertIn('ACCOUNT_FORMAT       "SIGNAL0317|5"', GAME)
        self.assertIn('!strcmp(header, "SIGNAL0317|1", false)', GAME)
        self.assertIn('gAccountLegacy[playerid] = v1 || v2 || v3 || v4;', GAME)
        self.assertIn('gAccountLegacy[playerid] && !Auth_SaveAccount(playerid)', GAME)
        self.assertIn('fexist(backup) && !frename(backup, path)', GAME)
        self.assertIn('Admin_ReplaceFile(path, temp, backup)', GAME)
        self.assertIn('fread(handle, extra, sizeof(extra))', GAME)

    def test_fields_and_validation(self):
        for field in ("gAccountMoney[playerid]", "gAccountHealth[playerid]",
                      "gAccountArmour[playerid]", "gAdminMuteUntil[playerid]"):
            self.assertIn(field, GAME)
        self.assertIn("Auth_Unsigned(muteText, mutedUntil, 2147483647)", GAME)
        self.assertIn("Auth_Unsigned(moneyText, balance, ACCOUNT_MAX_MONEY)", GAME)
        self.assertIn("Auth_ParseVital(healthText, health)", GAME)
        self.assertIn("Auth_ParseVital(armourText, armour)", GAME)
        self.assertIn('"%s\\n%d\\n%d\\n%d\\n%.2f\\n%.2f\\n%d\\n%d\\n%d\\n%d\\n%d\\n%d\\n%d\\n%d\\n%d\\n"', GAME)

    def test_player_lifecycle(self):
        for token in ("ResetPlayerMoney(playerid);", "SetPlayerHealth(playerid, gAccountHealth[playerid]);",
                      "SetPlayerArmour(playerid, gAccountArmour[playerid]);", "Auth_CaptureVitals(playerid);",
                      "Auth_SaveAccount(playerid)", "public OnPlayerDeath(", "Auth_Autosave();",
                      'SetTimer("Auth_Autosave", ACCOUNT_AUTOSAVE_MS, true)'):
            self.assertIn(token, GAME)
        self.assertIn('ACCOUNT_AUTOSAVE_MS  (120000)', GAME)
        self.assertIn("gAccountDead[playerid] = true;", GAME)

    def test_mute_stays_in_account(self):
        self.assertIn('SIGNALMUTE|1', ADMIN)
        self.assertNotIn('fwrite(file, "SIGNALMUTE|1', ADMIN)
        self.assertIn("gAdminMuteUntil[playerid] = until;", ADMIN)
        self.assertIn("if (!Auth_SaveAccount(playerid))", ADMIN)
        self.assertIn("if (!fremove(path)) return false;", ADMIN)
        self.assertIn("Admin_WriteMute(target, until)", ADMIN)

    def test_cleaner_saves_salary_in_account(self):
        self.assertIn("Cleaner_SettlePending(playerid)", CLEAN)
        self.assertIn("Economy_CleanerSettle(playerid, gCleanerEarned[playerid])", CLEAN)
        self.assertNotIn("GivePlayerMoney(playerid, wage)", CLEAN)
        self.assertIn("Economy_SyncHUD(playerid)", (BASE / "gamemodes/includes/economy.inc").read_text(encoding="utf-8"))
        self.assertIn("SIGNAL0317|4", DOC)


if __name__ == "__main__":
    unittest.main()
