"""CRIME-02.6: witness interviews are mandatory, durable and session-safe."""
from pathlib import Path
import unittest

ROOT=Path(__file__).resolve().parents[1]
SCHEMA=(ROOT/"db/migrations/037_crime_witness_interview.sql").read_text("utf-8")
CORE=(ROOT/"gamemodes/includes/crime_contracts.inc").read_text("utf-8")
WORLD=(ROOT/"gamemodes/includes/crime_world.inc").read_text("utf-8")
UI=(ROOT/"gamemodes/includes/crime_ui.inc").read_text("utf-8")
SCENE=(ROOT/"gamemodes/includes/crime_scenarios.inc").read_text("utf-8")
WORKFLOW=(ROOT/".github/workflows/build.yml").read_text("utf-8")


class WitnessContracts(unittest.TestCase):
    def test_migration_compatibility(self):
        for part in ("ADD COLUMN interview_question","ADD COLUMN interviewed_at",
                     "picked_up_at", "state IN (2,3)", "VALUES (37)"):
            self.assertIn(part,SCHEMA)
        self.assertIn("version=45 LIMIT 1",CORE)
        self.assertIn("037_crime_witness_interview.sql",WORKFLOW)
        self.assertIn("crime026_mysql_uat.py",WORKFLOW)

    def test_server_authoritative_interview(self):
        for part in ("stock bool:CrimeWorld_Interview(", "stock bool:CrimeWorld_InterviewReady(",
                     "gGovTransferBusy[playerid]", "faction != Leader_PlayerOrg(playerid)",
                     "interview_question=%d,interviewed_at=UNIX_TIMESTAMP()",
                     "interviewed_at=0 AND expires_at>UNIX_TIMESTAMP()",
                     "WITNESS_INTERVIEWED", "IsPlayerInRangeOfPoint(playerid, CRIME_WORLD_RADIUS",
                     'Crime_Exec("START TRANSACTION")', 'Crime_Exec("COMMIT")',
                     "Crime_Rollback()"):
            self.assertIn(part,WORLD)
        self.assertIn("deliveryStage && !CrimeWorld_InterviewReady(playerid, id)",CORE)
        self.assertIn("c.interviewed_at>0",CORE)
        self.assertIn("AND (scenario_kind=0 OR interviewed_at>0)",CORE)

    def test_trust_consistency_and_legacy(self):
        self.assertIn("interview_question=1 THEN 1 ELSE 0", CORE)
        self.assertIn("c.interview_question=1 THEN 1 ELSE 0", CORE)
        self.assertIn("WHEN scenario_resolution=2 THEN 2",CORE)
        self.assertIn("WHEN scenario_kind=0 THEN 5",CORE)
        self.assertIn("COALESCE(r.trust_delta,0) trust_delta", UI)
        self.assertIn("WITNESS_INTERVIEWED",UI)
        self.assertIn("EVIDENCE_FOUND",UI)
        self.assertIn("question == 0",SCENE)
        self.assertIn("CrimeScenario_Testimony(",SCENE)
        self.assertIn("CrimeScenario_InterviewChoices(",SCENE)

    def test_ui_dialog_binding_and_no_new_commands(self):
        for part in ("#define CRIME_DLG_INTERVIEW  (7962)",
                     "#define CRIME_DLG_TESTIMONY  (7963)",
                     "gCrimeUIWitnessContract[playerid]",
                     "case CRIME_DLG_INTERVIEW:", "case CRIME_DLG_TESTIMONY:",
                     "CrimeWorld_Interview(playerid, id, faction, listitem + 1)",
                     "CrimeWorld_InterviewReady(playerid, id)",
                     "CrimeScenario_Testimony(kind, eventId, question, choice",
                     "CrimeUI_Witness(playerid)", "gCrimeUIAuthToken[playerid] != gAuthToken[playerid]"):
            self.assertIn(part,UI+WORLD)
        self.assertNotIn("cmd:interview(",UI+WORLD)
        self.assertIn("if (phase == 2) CrimeUI_Witness(playerid);",WORLD)


if __name__=="__main__":
    unittest.main()
