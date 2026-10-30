"""CRIME-02.5: physical scene security, evidence persistence, actor lifecycle."""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]
SQL = (ROOT/"db/migrations/036_crime_world_evidence.sql").read_text("utf-8")
CORE = (ROOT/"gamemodes/includes/crime_contracts.inc").read_text("utf-8")
WORLD = (ROOT/"gamemodes/includes/crime_world.inc").read_text("utf-8")
UI = (ROOT/"gamemodes/includes/crime_ui.inc").read_text("utf-8")
GAME = (ROOT/"gamemodes/signal0317.pwn").read_text("utf-8")


class CrimePhysicalScenes(unittest.TestCase):
    def test_durable_evidence_schema_and_upgrade(self):
        for token in ("ADD COLUMN evidence_found_at",
                      "JOIN crime_contract_events", "e.event_type='CLUE_FOUND'",
                      "WHERE c.scenario_kind BETWEEN 1 AND 3",
                      "VALUES (36)"):
            self.assertIn(token, SQL)
        self.assertIn("version=36 LIMIT 1", CORE)
        self.assertIn("evidence_found_at>0 AND %d IN(1,2)", CORE)
        self.assertIn("CrimeWorld_EvidenceReady(playerid, id)", CORE)
        self.assertIn("CrimeWorld_Init()", CORE)

    def test_world_objects_and_game_hooks(self):
        for token in ("CreatePlayerObject(", "DestroyPlayerObject(",
                      "CreateActor(", "DestroyActor(", "SetActorInvulnerable(",
                      "gCrimePropContract[playerid]", "CrimeWorld_Reset(playerid)",
                      "CrimeWorld_Shutdown()", "CrimeWorld_ShowClue(",
                      "KEY_YES", "GetPlayerVirtualWorld(playerid)",
                      "GetPlayerInterior(playerid)", "IsPlayerInAnyVehicle(playerid)"):
            self.assertIn(token, WORLD)
        self.assertIn('#include "crime_world.inc"', GAME)
        self.assertIn("CrimeWorld_OnKeys(playerid, newkeys, oldkeys)", GAME)
        self.assertIn("CrimeWorld_Shutdown();", GAME)
        self.assertEqual(GAME.count("CrimeWorld_Reset(playerid);"), 2)
        self.assertNotIn("OnPlayerUpdate(", WORLD)
        self.assertNotIn("SetPlayerCheckpoint(", WORLD)

    def test_single_transaction_and_no_clue_replay(self):
        for token in ("CrimeWorld_EvidenceReady(playerid, id)",
                      "UPDATE crime_contracts SET evidence_found_at=UNIX_TIMESTAMP()",
                      "AND evidence_found_at=0 AND expires_at>UNIX_TIMESTAMP()",
                      "INSERT INTO crime_contract_events",
                      "'EVIDENCE_FOUND'", "Crime_Rollback()",
                      'Crime_Exec("COMMIT")', "scenario_kind IN(1,2,3)",
                      "faction_id=%d", "gDBPrimaryAccountId[playerid]"):
            self.assertIn(token, WORLD)
        block = WORLD.split("stock bool:CrimeWorld_Collect(", 1)[1].split("stock CrimeWorld_OnKeys(", 1)[0]
        self.assertLess(block.index('Crime_Exec("START TRANSACTION")'),
                        block.index("UPDATE crime_contracts SET evidence_found_at="))
        self.assertLess(block.index("'EVIDENCE_FOUND'"), block.index('Crime_Exec("COMMIT")'))

    def test_menu_cannot_bypass_physical_interaction(self):
        self.assertIn("CrimeWorld_ShowClue(playerid, id, kind, phase, x, y, z)", UI)
        self.assertIn("CrimeWorld_EvidenceReady(playerid, id)", UI)
        self.assertIn("CrimeUI_Scene(playerid);", WORLD)
        self.assertIn("if (kind != 0) return 1;", UI)
        self.assertIn("CrimeWorld_Reset(playerid)", UI)
        self.assertIn("IsPlayerInRangeOfPoint(playerid, CRIME_WORLD_RADIUS", UI)
        self.assertIn("if (faction != Leader_PlayerOrg(playerid)) return 0;", WORLD)
        self.assertIn("IsPlayerInRangeOfPoint(playerid, CRIME_WORLD_RADIUS", WORLD)
        self.assertNotIn("cmd:crimeevidence(", GAME)


if __name__ == "__main__":
    unittest.main()
