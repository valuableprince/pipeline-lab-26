"""AUDIT-FIX-02: статические контракты и модель повторяемого погашения выплат."""
import unittest
from pathlib import Path

BASE = Path(__file__).resolve().parents[1]
MAIN = (BASE / "gamemodes/signal0317.pwn").read_text(encoding="utf-8")
ECON = (BASE / "gamemodes/includes/economy.inc").read_text(encoding="utf-8")
CLEAN = (BASE / "gamemodes/includes/cleaner_job.inc").read_text(encoding="utf-8")
CI = (BASE / ".github/workflows/build.yml").read_text(encoding="utf-8")
BUILD = (BASE / "tools/build_windows.py").read_text(encoding="utf-8")


class SettlementModel:
    """Модель протокола earned/paid. Не исполняет Pawn-код."""
    def __init__(self, balance=0, earned=0, paid=0):
        self.balance, self.earned, self.paid = balance, earned, paid

    def settle(self, disk_ok=True):
        if self.paid < 0 or self.paid > self.earned or not disk_ok:
            return False
        amount = self.earned - self.paid
        self.balance += amount
        self.paid = self.earned
        return True


class EconomyTests(unittest.TestCase):
    def test_centralized_wallet_contract(self):
        for s in ("stock bool:Economy_Credit(", "stock bool:Economy_Debit(",
                  "stock bool:Economy_Change(", "Economy_SyncHUD(playerid)",
                  "Auth_SaveAccount(playerid)", "gAccountTxnSerial[playerid]++;",
                  "ACCOUNT_MAX_MONEY - gAccountMoney[playerid]",
                  "amount < -gAccountMoney[playerid]"):
            self.assertIn(s, ECON)
        self.assertIn('#include "economy.inc"', MAIN)
        self.assertIn('"economy.inc"', CI)
        self.assertIn('"economy.inc"', BUILD)
        self.assertNotIn('Auth_AddMoney', MAIN + CLEAN)
        self.assertIn('ACCOUNT_FORMAT       "SIGNAL0317|5"', MAIN)

    def test_income_reconciliation(self):
        for s in ("new oldPaid = gAccountCleanerPaid[playerid];",
                  "gAccountCleanerPaid[playerid] = earned;",
                  "gAccountMoney[playerid] += amount;",
                  "if (!Auth_SaveAccount(playerid))",
                  "gAccountMoney[playerid] -= amount;",
                  "gAccountCleanerPaid[playerid] = oldPaid;",
                  "if (amount == 0) return true;",
                  "earned < gAccountCleanerPaid[playerid]"):
            self.assertIn(s, ECON)
        self.assertIn('gAccountCleanerPaid[playerid] = -1;', ECON)
        self.assertIn('Economy_CleanerInitialize(playerid, gCleanerEarned[playerid])', CLEAN)
        self.assertIn('Economy_CleanerSettle(playerid, gCleanerEarned[playerid])', CLEAN)
        self.assertIn('gAccountCleanerPaid[playerid] = paid;', MAIN)
        self.assertIn('gAccountTxnSerial[playerid] = serial;', MAIN)
        self.assertIn('new bool:v2 = !strcmp(header, "SIGNAL0317|2", false);', MAIN)
        self.assertIn('Cleaner_SettlePending(playerid)', MAIN)

    def test_replay_across_failures(self):
        w = SettlementModel(balance=500, earned=0, paid=0)
        w.earned = 75
        self.assertFalse(w.settle(disk_ok=False))
        self.assertEqual((w.balance, w.paid), (500, 0))
        self.assertTrue(w.settle())
        for _ in range(5):
            self.assertTrue(w.settle())
        self.assertEqual((w.balance, w.paid), (575, 75))
        w.earned += 100
        self.assertTrue(w.settle())
        self.assertEqual((w.balance, w.paid), (675, 175))
        self.assertFalse(SettlementModel(0, 4, 5).settle())

    def test_legacy_baseline_prevents_backpay(self):
        w = SettlementModel(balance=350, earned=300, paid=-1)
        self.assertFalse(w.settle())
        w.paid = w.earned  # подтверждённая миграция
        self.assertTrue(w.settle())
        self.assertEqual(w.balance, 350)
        w.earned += 25
        self.assertTrue(w.settle())
        self.assertEqual(w.balance, 375)


if __name__ == "__main__":
    unittest.main()
