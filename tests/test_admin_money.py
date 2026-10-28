"""ADMIN-MONEY-01 command safety contracts for admin-only money grants."""

from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[1]
COMMANDS = (ROOT / "gamemodes/includes/commands.inc").read_text("utf-8")
ADMIN_MONEY = (ROOT / "gamemodes/includes/admin_money.inc").read_text("utf-8")
ADMIN_HELP = (ROOT / "gamemodes/includes/admin_help.inc").read_text("utf-8")
ADMIN = (ROOT / "gamemodes/includes/admin_system.inc").read_text("utf-8")
ECONOMY = (ROOT / "gamemodes/includes/economy.inc").read_text("utf-8")
MAIN = (ROOT / "gamemodes/signal0317.pwn").read_text("utf-8")


class AdminMoneyContracts(unittest.TestCase):
    def test_command_is_registered_and_help_is_level_five(self):
        self.assertRegex(
            COMMANDS,
            r"cmd:givemoney\(playerid, params\[\]\)\s*\{\s*return AdminMoney_Give\(playerid,params\);",
        )
        self.assertIn('#include "admin_money.inc"', MAIN)
        self.assertEqual(MAIN.count('#include "admin_money.inc"'), 1)
        self.assertLess(MAIN.index('#include "admin_help.inc"'),
                        MAIN.index('#include "admin_money.inc"'))
        self.assertLess(MAIN.index('#include "admin_money.inc"'),
                        MAIN.index('#include "commands.inc"'))
        self.assertIn('{"givemoney", "/givemoney ID сумма"', ADMIN_HELP)
        self.assertRegex(
            ADMIN_HELP,
            r'\{"givemoney",\s*"/givemoney ID сумма".*?,\s*3,\s*5\}',
        )

    def test_permission_duty_online_and_strict_amount(self):
        self.assertIn("Admin_Require(playerid,PERM_MANAGE_STAFF)", ADMIN_MONEY)
        self.assertIn("case PERM_BAN_PERMANENT, PERM_MANAGE_STAFF: return gAdminLevel[playerid] >= 5;", ADMIN)
        self.assertIn("if (!gAdminDuty[playerid])", ADMIN)
        self.assertIn("!Admin_UnsignedNumber(targetText)", ADMIN_MONEY)
        self.assertIn("!Admin_UnsignedNumber(amountText)", ADMIN_MONEY)
        self.assertIn("Admin_Next(params,at,extra,sizeof(extra))", ADMIN_MONEY)
        self.assertIn("!Admin_Ready(target)", ADMIN_MONEY)
        self.assertIn("IsPlayerNPC(target)", ADMIN_MONEY)
        self.assertIn("amount<1 || amount>ADMIN_MONEY_MAX_GRANT", ADMIN_MONEY)
        self.assertIn("#define ADMIN_MONEY_MAX_GRANT (1000000)", ADMIN_MONEY)
        self.assertIn("ACCOUNT_MAX_MONEY-gAccountMoney[target]", ADMIN_MONEY)
        self.assertIn("gAccountTxnSerial[target]>=ACCOUNT_MAX_MONEY", ADMIN_MONEY)

    def test_self_grant_explicitly_allowed(self):
        self.assertNotIn("target==playerid", ADMIN_MONEY)
        self.assertNotIn("target!=playerid", ADMIN_MONEY)
        self.assertNotIn("Выдавать деньги самому себе запрещено", ADMIN_MONEY)

    def test_save_before_hud_and_double_audit(self):
        self.assertIn("gGovTransferBusy[target]", ADMIN_MONEY)
        self.assertIn("gDBPrimaryLoaded[target]", ADMIN_MONEY)
        self.assertIn('Admin_LogPlayer(playerid,"GIVEMONEY_REQUEST",detail)', ADMIN_MONEY)
        self.assertIn('Admin_LogPlayer(playerid,"GIVEMONEY",detail)', ADMIN_MONEY)
        self.assertIn('Admin_LogPlayer(playerid,"GIVEMONEY_FAILED",detail)', ADMIN_MONEY)
        self.assertIn('Economy_Credit(target,amount,"admin_givemoney")', ADMIN_MONEY)
        self.assertIn("if (!Auth_SaveAccount(playerid))", ECONOMY)
        self.assertLess(ECONOMY.index("if (!Auth_SaveAccount(playerid))"),
                        ECONOMY.index("Economy_SyncHUD(playerid);", ECONOMY.index("stock bool:Economy_Change")))
        self.assertNotIn("GivePlayerMoney(", ADMIN_MONEY)
        self.assertNotIn("gAccountMoney[target]+=", ADMIN_MONEY)
        self.assertNotIn("UPDATE accounts", ADMIN_MONEY)
        self.assertIn("SendClientMessage(target,ADMIN_OK,message)", ADMIN_MONEY)


if __name__ == "__main__":
    unittest.main()
