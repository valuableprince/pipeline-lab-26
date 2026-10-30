"""CRIME-02.1 structural regression for solo contracts and wallet safety."""
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[1]
SOURCE = (ROOT / "gamemodes/includes/crime_contracts.inc").read_text("utf-8")
SCHEMA = (ROOT / "db/migrations/032_crime_contract_core.sql").read_text("utf-8")
GAME = (ROOT / "gamemodes/signal0317.pwn").read_text("utf-8")
COMMANDS = (ROOT / "gamemodes/includes/commands.inc").read_text("utf-8")
DOC = (ROOT / "docs/CRIME02_SOLO_CONTRACT_CORE.md").read_text("utf-8")


class Crime02ContractTests(unittest.TestCase):
    def test_integrated_and_fail_closed_without_sql(self):
        self.assertIn('#include "crime_contracts.inc"', GAME)
        self.assertIn("Crime_Init();", GAME)
        self.assertIn("cmd:crime(playerid, params[])", COMMANDS)
        self.assertIn("return CrimeUI_Main(playerid);", COMMANDS)
        self.assertIn("#if defined TARKENSO_DB_PRIMARY", SOURCE)
        self.assertIn('version=44 LIMIT 1', SOURCE)
        self.assertIn('gDBPrimaryLoaded[playerid]', SOURCE)
        self.assertIn("Leader_IsCriminal(org)", SOURCE)
        self.assertIn("m.faction_id=%d", SOURCE)
        self.assertNotIn("SetPlayerCheckpoint(", SOURCE)
        self.assertNotIn("OnPlayerUpdate(", SOURCE)

    def test_state_transitions_are_authoritative_and_durable(self):
        for token in (
            "UNIQUE KEY uq_crime_one_active_contract (active_account_id)",
            "GENERATED ALWAYS AS",
            "CREATE TABLE crime_contract_templates",
            "CREATE TABLE crime_contracts",
            "CREATE TABLE crime_contract_events",
            "CREATE TABLE crime_contract_rewards",
            "INSERT INTO schema_migrations (version) VALUES (32)",
            "REFERENCES factions(faction_id)",
            "REFERENCES accounts(account_id)",
        ):
            self.assertIn(token, SCHEMA)
        for transition in (
            "SET state=2,picked_up_at=UNIX_TIMESTAMP()",
            "SET state=3,finished_at=UNIX_TIMESTAMP()",
            "SET state=4,finished_at=UNIX_TIMESTAMP()",
            "SET state=5,finished_at=UNIX_TIMESTAMP()",
            "AND state=1 AND expires_at>UNIX_TIMESTAMP()",
            "AND c.state=2 AND c.expires_at>UNIX_TIMESTAMP()",
        ):
            self.assertIn(transition, SOURCE)
        self.assertIn('UNIQUE KEY uq_crime_contract_event', SCHEMA)
        self.assertIn("UNIX_TIMESTAMP()+t.duration_seconds", SOURCE)
        self.assertIn("Crime_Expire(playerid)", SOURCE)

    def test_atomic_and_idempotent_wallet_settlement(self):
        core = SOURCE.split("stock Crime_Settle(", 1)[1].split("stock Crime_Abort(", 1)[0]
        self.assertIn("Auth_SaveAccount(playerid)", core)
        self.assertIn("gGovTransferBusy[playerid] = true;", core)
        self.assertIn('Crime_Exec("START TRANSACTION")', core)
        self.assertIn('Crime_Exec("COMMIT")', core)
        self.assertIn("Crime_Rollback()", core)
        self.assertIn("a.row_version=a.row_version+1", core)
        self.assertIn("a.txn_serial=a.txn_serial+1", core)
        self.assertIn("a.balance=a.balance+c.reward_amount", core)
        self.assertIn("a.row_version=%d", core)
        self.assertIn("NOT EXISTS(SELECT 1 FROM crime_contract_rewards", core)
        self.assertIn("INSERT INTO crime_contract_rewards", core)
        self.assertIn("'COMPLETED'", core)
        self.assertIn("Crime_ReadWallet(playerid", core)
        self.assertIn("gDBPrimaryVersion[playerid] = version;", core)
        self.assertIn("gAccountTxnSerial[playerid] = serial;", core)
        self.assertIn("Economy_SyncHUD(playerid)", core)
        self.assertNotIn("Economy_Credit(", core)
        self.assertLess(core.index("INSERT INTO crime_contract_rewards"),
                        core.index('Crime_Exec("COMMIT")'))

    def test_solo_operations_and_ui_are_integrated(self):
        ui = (ROOT / "gamemodes/includes/crime_ui.inc").read_text("utf-8")
        for name in ("Crime_Start", "Crime_Advance", "Crime_Settle",
                     "Crime_Abort", "Crime_Expire", "Crime_LoadActive"):
            self.assertRegex(SOURCE, r"stock (?:bool:)?"+name+r"\(")
        self.assertNotIn("stock Crime_Command(", SOURCE)
        self.assertIn("IsPlayerInRangeOfPoint", SOURCE)
        self.assertIn("faction != Leader_PlayerOrg(playerid)", SOURCE)
        self.assertIn("FROM crime_contract_events e JOIN crime_contracts c", ui)
        self.assertIn("c.account_id=%d", ui)
        self.assertIn("CrimeUI_OnDialog(playerid, dialogid, response, listitem)", GAME)
        self.assertIn("CrimeUI_Reset(playerid)", GAME)
        self.assertIn("CRIME-02.2", DOC)
        self.assertIn("031", DOC)
        self.assertIn("032_crime_contract_core.sql", DOC)

    def test_crime_ranks_do_not_affect_ministry(self):
        core = (ROOT / "gamemodes/includes/leader_system.inc").read_text("utf-8")
        self.assertIn("#define FACTION_FINANCE (5)", core)
        self.assertIn("#define FACTION_CRIME_FIRST (6)", core)
        self.assertIn("#define FACTION_CRIME_LAST (8)", core)
        self.assertIn("if (org == FACTION_FINANCE) return 4;", core)
        self.assertIn("if (Leader_IsCriminal(org)) return 5;", core)


if __name__ == "__main__":
    unittest.main()
