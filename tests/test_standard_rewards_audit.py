"""Audit contract: no native GTA cash income reaches authoritative accounts."""
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[1]
GAME = (ROOT / "gamemodes/signal0317.pwn").read_text("utf-8")
INCLUDES = ROOT / "gamemodes/includes"
ECON = (INCLUDES / "economy.inc").read_text("utf-8")
ADMIN = (INCLUDES / "admin_tools.inc").read_text("utf-8")
PAYDAY = (INCLUDES / "payday.inc").read_text("utf-8")
RACE = (INCLUDES / "race_progress.inc").read_text("utf-8")
CLEANER = (INCLUDES / "cleaner_job.inc").read_text("utf-8")
VENDORED = {"Pawn.CMD.inc", "sscanf2.inc", "streamer.inc"}


class StandardRewardsAudit(unittest.TestCase):
    def test_native_stunt_rewards_stay_disabled(self):
        self.assertIn("EnableStuntBonusForAll(false);", GAME)
        self.assertIn("EnableStuntBonusForPlayer(playerid, false);", GAME)
        for path in (ROOT / "gamemodes").rglob("*.inc"):
            if path.name in VENDORED:
                continue
            source = path.read_text("utf-8")
            self.assertNotRegex(source, r"EnableStuntBonusFor(?:All|Player)\s*\([^;]*\btrue\s*\)")

    def test_client_wallet_cannot_credit_authoritative_balance(self):
        self.assertIn("GetPlayerMoney(playerid) != gAccountMoney[playerid]", ECON)
        self.assertIn("Economy_SyncHUD(playerid);", ECON)
        self.assertIn("ECONOMY_RECONCILE_INTERVAL_MS (5000)", ECON)
        self.assertIn('SetTimer("Economy_ReconcileTick",', ECON)
        self.assertIn("Economy_Shutdown();", GAME)
        self.assertIn("KillTimer(gEconomyReconcileTimer);", ECON)
        tick = ECON.split("public Economy_ReconcileTick()", 1)[1].split("\n}\n", 1)[0]
        self.assertNotIn("Auth_SaveAccount(", tick)
        self.assertNotIn("gAccountMoney[playerid] =", tick)
        self.assertIn("gAuthState[playerid] != AUTH_READY", tick)
        self.assertIn("GetPlayerState(playerid) == PLAYER_STATE_SPECTATING", tick)

    def test_admin_check_displays_server_balance(self):
        self.assertIn("gAccountMoney[target],", ADMIN)
        self.assertNotIn("GetPlayerMoney(target)", ADMIN)

    def test_no_hidden_money_pickups_or_direct_awards(self):
        for path in [ROOT / "gamemodes/signal0317.pwn", *INCLUDES.glob("*.inc")]:
            if path.name in VENDORED:
                continue
            source = path.read_text("utf-8")
            for forbidden in ("AddStaticPickup(", "CreatePickup(", "CreatePlayerPickup("):
                self.assertNotIn(forbidden, source, path.as_posix())
            if "GivePlayerMoney(" in source or "ResetPlayerMoney(" in source:
                self.assertIn(path.name, {"signal0317.pwn", "economy.inc"})

    def test_intentional_income_is_preserved(self):
        self.assertIn("Economy_CleanerSettle(playerid, gCleanerEarned[playerid])", CLEANER)
        self.assertIn("RaceProgress_Reward(playerid, place, runners, seconds)", RACE)
        self.assertIn("Economy_SyncHUD(playerid);", RACE)
        self.assertIn("PAYDAY_EXP_PER_HOUR (1)", PAYDAY)
        self.assertNotIn("GivePlayerMoney(", PAYDAY)
        self.assertNotIn("Economy_Credit(", PAYDAY)


if __name__ == "__main__":
    unittest.main()
