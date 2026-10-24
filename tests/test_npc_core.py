"""NPC-CORE-01 structural regression checks (server-side SQL and editor contracts)."""
from pathlib import Path
import re
import unittest

R = Path(__file__).resolve().parents[1]
G = (R/"gamemodes/signal0317.pwn").read_text("utf-8")
C = (R/"gamemodes/includes/npc_core.inc").read_text("utf-8")
E = (R/"gamemodes/includes/npc_editor.inc").read_text("utf-8")
S = (R/"gamemodes/includes/studio_system.inc").read_text("utf-8")
RTE = (R/"gamemodes/includes/npc_route_editor.inc").read_text("utf-8")
CM = (R/"gamemodes/includes/commands.inc").read_text("utf-8")
SQL = (R/"db/migrations/005_npc_core.sql").read_text("utf-8")
CI = (R/".github/workflows/pawn-build.yml").read_text("utf-8")
WIN = (R/"tools/build_windows.py").read_text("utf-8")

class NpcCoreContracts(unittest.TestCase):
    def test_migration_additive_and_gender_catalogue(self):
        for table in ("npc_skins", "npc_names", "npcs"):
            self.assertIn("CREATE TABLE " + table, SQL)
        for token in ("AUTO_INCREMENT PRIMARY KEY", "gender ENUM('male','female')",
                      "INSERT INTO schema_migrations (version) VALUES (5)",
                      "FOREIGN KEY (route_slot) REFERENCES npc_routes",
                      "row_version BIGINT UNSIGNED", "UNIQUE KEY uq_npc_display_name"):
            self.assertIn(token, SQL)
        self.assertNotRegex(SQL, r"(?i)\bDROP\s+TABLE\b")
        self.assertNotRegex(SQL, r"(?i)\bDELETE\s+FROM\s+npc_routes\b")

    def test_runtime_has_no_hardcoded_population(self):
        self.assertIn("FROM npcs WHERE is_enabled=1 ORDER BY npc_id", C)
        self.assertIn("version=5 LIMIT 1", C)
        self.assertIn("stock NpcCore_OnFinishMove(npcid)", C)
        self.assertIn("NPC_CORE_MAX_WALKERS", C)
        self.assertIn("NPC_Create(nickname)", C)
        self.assertIn("public NpcCore_StreamTick()", C)
        self.assertIn('SetTimer("NpcCore_StreamTick", 2000, true)', C)
        self.assertIn("IsPlayerNPC(playerid)", C)
        self.assertIn("NpcCore_StartWalker(i)", C)
        self.assertIn("CreateActor(gNpcCoreSkin[index]", C)
        self.assertIn("NpcCore_ClearWorld();", C)
        self.assertNotIn("CityNPC_Init()", C)
        self.assertNotIn("CityNPC_Shutdown()", C)
        self.assertIn("NpcCore_OnFinishMove(npcid)", G)
        self.assertIn("NpcCore_Init();", G)
        self.assertIn("NpcCore_Shutdown();", G)
        self.assertIn("gNpcCoreReverse[index]", C)

    def test_studio_guards_and_npc_editor_actions(self):
        self.assertIn("return Admin_IsFounder(playerid) && gAdminDuty[playerid]", S)
        self.assertIn("if (!Admin_IsFounder(playerid) || !gAdminDuty[playerid])", C)
        self.assertIn("return NpcEditor_Command(playerid, tail);", S)
        self.assertIn("return NPCRoute_Command(playerid, tail);", S)
        self.assertIn('cmd:nset(playerid, params[])', CM)
        self.assertIn("NpcEditor_Capture(playerid)", CM)
        for token in ("NPC_ED_SKIN","NPC_ED_NAME","NPC_ED_MODE",
                      "NPC_ED_ROUTE","NPC_ED_CONFIRM","NPC_ED_DELETE",
                      "NpcEditor_AutoName","NpcEditor_NameOK",
                      "NpcEditor_Save","NpcEditor_Toggle","NpcEditor_Delete",
                      "NpcEditor_Cancel","NpcEditor_OnDisconnect"):
            self.assertIn(token, E)
        self.assertIn("ORDER BY RAND() LIMIT 1", E)
        self.assertIn("WHERE LOWER(p.display_name)=LOWER(n.display_name)", E)
        self.assertIn("cache_insert_id()", E)
        self.assertIn("WHERE npc_id=%d AND row_version=%d", E)
        self.assertIn("gNpcEdPreview = CreatePlayerObject(playerid", E)
        self.assertIn("NPCRoute_Load(gNpcEdRoute)", E)
        self.assertIn("NpcEditor_OnDialog(playerid, dialogid, response, listitem, inputtext)", G)

    def test_legacy_route_editor_is_untouched(self):
        for cmd in ("record", "crossing", "stop", "undo", "show", "test", "save", "apply"):
            self.assertIn(f'!strcmp(action, "{cmd}", true)', RTE)
        self.assertIn("DBWorld_RouteLoad(slot)", RTE)
        self.assertIn("DBWorld_RouteSave()", RTE)
        self.assertIn("NPCROUTE_COUNT", RTE)

    def test_crossings_never_block_movement_on_traffic_signals(self):
        # Street-crossing annotations and existing route records remain intact,
        # but neither production NPCs nor route-preview NPCs wait at lights.
        for unwanted in (
            "gNPCRouteSignalGreen", "gNPCRoutePreviewWaiting",
            "gNPCRoutePreviewCrossing", "NPCRoute_SyncSignal",
            "NPCRoute_PreviewTick", "gNPCRoutePreviewTimer",
        ):
            self.assertNotIn(unwanted, RTE)
        self.assertIn("stock bool:NPCRoute_PreviewNext()", RTE)
        self.assertIn("NPC_Move(gNPCRoutePreview", RTE)
        self.assertIn("NPCRoute_OnNPCFinishMove(npcid)", RTE)
        self.assertIn('!strcmp(action, "crossing", true)', RTE)
        self.assertIn("gNPCRouteDraftCross", RTE)
        self.assertIn("NPC_Move(gNpcCoreBot[index]", C)

    def test_both_compilers_stage_new_modules(self):
        for p in ("npc_core.inc", "npc_editor.inc"):
            self.assertIn('"'+p+'"', CI)
            self.assertIn('"'+p+'"', WIN)
            self.assertIn('#include "'+p+'"', G)

if __name__=="__main__":
    unittest.main()
