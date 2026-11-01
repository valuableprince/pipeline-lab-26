"""DB-ERROR-01: a synchronous SQL failure must not poison async callbacks."""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]
DB = (ROOT / "gamemodes/includes/db_primary.inc").read_text("utf-8")
SET = (ROOT / "gamemodes/includes/bond_settlement.inc").read_text("utf-8")
COUP = (ROOT / "gamemodes/includes/bond_coupon.inc").read_text("utf-8")
DEBT = (ROOT / "gamemodes/includes/gov_debt.inc").read_text("utf-8")


class MysqlAsyncErrnoIsolation(unittest.TestCase):
    def test_sync_errors_redacted_with_number(self):
        self.assertIn("Sync SQL write failed, errno=%d (SQL redacted)", DB)
        self.assertNotIn('printf("%s", query)', DB)

    def test_async_callbacks_do_not_check_last_unthreaded_query(self):
        for source, handlers in (
            (SET, ("BondSettlement_OnPlayerDone", "BondSettlement_OnScan",
                   "BondSettlement_OnAutomated")),
            (COUP, ("BondCoupon_OnScan", "BondCoupon_OnAccrued",
                    "BondCoupon_OnNotices", "BondCoupon_OnNoticeMarked")),
            (DEBT, ("GovDebt_OnAdminAction", "GovDebt_OnScan",
                    "GovDebt_OnRecovered", "GovDebt_OnCrisis")),
        ):
            for handler in handlers:
                section = source.split(f"public {handler}(", 1)[1].split("\n}\n", 1)[0]
                self.assertNotIn("mysql_errno(gDBPrimary)", section, handler)

    def test_genuine_async_errors_have_retry_hooks(self):
        for source, helper in ((SET, "BondSettlement_OnAsyncError"),
                               (COUP, "BondCoupon_OnAsyncError"),
                               (DEBT, "GovDebt_OnAsyncError")):
            self.assertIn(f"stock {helper}(const callback[])", source)
            self.assertIn(f"{helper}(callback);", DB)
        self.assertIn("gBondCouponHadError = true;", COUP)
        self.assertIn("Async SQL error=%d callback=%s (query redacted)", DB)


if __name__ == "__main__":
    unittest.main()
