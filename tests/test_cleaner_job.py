"""Контрактные тесты системы уборщика TARKENSO: NPC, личный UI и выплаты."""
from pathlib import Path
from build_contracts import assert_staged
import unittest

ROOT = Path(__file__).resolve().parents[1]
MAIN = (ROOT / "gamemodes/signal0317.pwn").read_text(encoding="utf-8")
COMMANDS = (ROOT / "gamemodes/includes/commands.inc").read_text(encoding="utf-8")
JOB = (ROOT / "gamemodes/includes/cleaner_job.inc").read_text(encoding="utf-8")


class CleanerIntegrationTest(unittest.TestCase):
    def test_wired_into_game_mode(self):
        self.assertIn('#include "cleaner_job.inc"', MAIN)
        for hook in ("Cleaner_Init();", "Cleaner_Shutdown();", "Cleaner_OnConnect(playerid);",
                     "Cleaner_OnReady(playerid);", "Cleaner_OnDisconnect(playerid);",
                     "Cleaner_OnSpawn(playerid);",
                     "Cleaner_OnKeyStateChange(playerid, newkeys, oldkeys);"):
            self.assertIn(hook, MAIN)

    def test_commands_are_registered(self):
        for command in ("trashdrop", "cleanstats"):
            self.assertIn(f"cmd:{command}(playerid, params[])", COMMANDS)
        self.assertNotIn("cmd:cleaner(", COMMANDS)
        self.assertNotIn("cmd:trash(", COMMANDS)
        self.assertIn("gAuthState[playerid] != AUTH_READY", COMMANDS)

    def test_shared_litter_and_safety(self):
        self.assertIn("CLEANER_SPOT_COUNT      (87)", JOB)
        self.assertIn("CLEANER_VISIBLE_LIMIT   (24)", JOB)
        self.assertIn("gCleanerTrash[nearest] = INVALID_OBJECT_ID;", JOB)
        self.assertIn("gCleanerBag[playerid] >= Cleaner_Capacity(playerid)", JOB)
        self.assertIn("Cleaner_NearDepot(playerid)", JOB)
        self.assertIn("GetPlayerVirtualWorld(playerid) == 0", JOB)
        self.assertIn("!IsPlayerInAnyVehicle(playerid)", JOB)

    def test_zone_collection_and_latching(self):
        self.assertIn("CLEANER_ZONE_TICK_MS    (350)", JOB)
        self.assertIn("CLEANER_ZONE_ENTER_SQ   (4.0)", JOB)
        self.assertIn("CLEANER_ZONE_RELEASE_SQ (12.25)", JOB)
        self.assertIn("stock Cleaner_ZoneCollect(playerid)", JOB)
        self.assertIn("public Cleaner_ZoneTick()", JOB)
        self.assertIn("gCleanerZoneHold[playerid] = nearest + 1;", JOB)
        self.assertIn("if (hx * hx + hy * hy + hz * hz <= CLEANER_ZONE_RELEASE_SQ)", JOB)
        self.assertIn("gCleanerZoneHold[playerid] = 0;", JOB)
        self.assertIn("if (gCleanerTrash[i] == INVALID_OBJECT_ID) continue;", JOB)
        self.assertIn("GetPlayerState(playerid) != PLAYER_STATE_ONFOOT", JOB)
        self.assertIn("!Cleaner_InWorld(playerid)", JOB)
        self.assertIn("gettime() < gCleanerNextCollect[playerid]", JOB)
        self.assertIn('SetTimer("Cleaner_ZoneTick", CLEANER_ZONE_TICK_MS, true)', JOB)
        self.assertIn("KillTimer(gCleanerZoneTimer);", JOB)
        self.assertIn("Cleaner_RemoveTrashLabels(nearest);", JOB)
        self.assertNotIn("Cleaner_Collect(playerid)", JOB)
        self.assertNotIn("cmd:trash(playerid", COMMANDS)
        body = JOB[JOB.index("stock Cleaner_ZoneCollect(playerid)"):JOB.index("public Cleaner_ZoneTick()")]
        self.assertLess(body.index("gCleanerZoneHold[playerid] = nearest + 1;"), body.index("gCleanerBag[playerid]++;"))
        self.assertLess(body.index("gCleanerTrash[nearest] = INVALID_OBJECT_ID;"), body.index("gCleanerBag[playerid]++;"))

    def test_persistence_before_wage(self):
        self.assertIn("if (!Cleaner_Save(playerid))", JOB)
        deposit = JOB[JOB.index("stock Cleaner_Deposit(playerid)"):]
        self.assertLess(deposit.index("if (!Cleaner_Save(playerid))"), deposit.index("if (!Cleaner_SettlePending(playerid))"))
        self.assertIn("ORVELARCLEANER|1", JOB)
        self.assertIn("gCleanerDataError[playerid] = true", JOB)


    def test_npc_alt_shift_lifecycle(self):
        self.assertIn("CLEANER_DEPOT_X         (-219.9823)", JOB)
        self.assertIn("CLEANER_DEPOT_Y         (1070.8785)", JOB)
        self.assertIn("CLEANER_DEPOT_Z         (19.7422)", JOB)
        self.assertIn("CLEANER_DEPOT_A         (87.6387)", JOB)
        self.assertIn("CLEANER_DEPOT_A);", JOB)
        self.assertIn("public OnPlayerKeyStateChange(playerid, KEY:newkeys, KEY:oldkeys)", MAIN)
        self.assertIn("(newkeys & KEY_WALK)", JOB)
        self.assertIn("(oldkeys & KEY_WALK)", JOB)
        self.assertIn("GetTickCount() - gCleanerLastInteract[playerid] < 750", JOB)
        self.assertIn("gCleanerDataError[playerid]", JOB)
        self.assertIn("Cleaner_NearDepot(playerid)", JOB)
        self.assertIn("gCleanerDuty[playerid] = true;", JOB)
        self.assertIn("Cleaner_StartNightContract(playerid);", JOB)
        self.assertIn("Cleaner_Deposit(playerid);", JOB)
        self.assertIn("if (gCleanerBag[playerid] > 0)", JOB)
        self.assertIn("CleanerDump_IsReady()", JOB)
        self.assertIn("Cleaner_FinishNightContract(playerid)", JOB)
        self.assertIn("Cleaner_EndShift(playerid)", JOB)
        self.assertIn("gCleanerDuty[playerid] = false;", JOB)
        self.assertNotIn("Cleaner_OnEnterCheckpoint", MAIN + JOB)
        self.assertNotIn("Cleaner_SyncCheckpoint", MAIN + JOB)
        self.assertNotIn("SetPlayerCheckpoint(", JOB)
        self.assertNotIn("Cleaner_CheckpointTick", JOB)
        self.assertIn("Левый ALT — начать / завершить смену", JOB)

    def test_private_worker_only_labels(self):
        self.assertIn("PlayerText3D:gCleanerTrashLabel[MAX_PLAYERS][CLEANER_SPOT_COUNT]", JOB)
        self.assertIn("CreatePlayer3DTextLabel(playerid,", JOB)
        self.assertIn("DeletePlayer3DTextLabel(playerid,", JOB)
        self.assertIn("Cleaner_SyncTrashLabels(playerid);", JOB)
        self.assertIn("Cleaner_HideTrashLabels(playerid);", JOB)
        self.assertIn("Cleaner_RemoveTrashLabels(nearest);", JOB)
        self.assertIn("gCleanerDuty[playerid] &&", JOB)
        self.assertIn("GetPlayerVirtualWorld(playerid) == 0", JOB)
        self.assertIn("GetPlayerInterior(playerid) == 0", JOB)
        self.assertNotIn("Create3DTextLabel(\n        \"{A7D6B5}МУСОР", JOB)
        self.assertNotIn("gCleanerTrashLabel[CLEANER_SPOT_COUNT]", JOB)

    def test_slow_refill_and_collection_animation(self):
        self.assertIn("CLEANER_REFILL_MS       (120000)", JOB)
        self.assertIn("if (count < target) Cleaner_SpawnOne();", JOB)
        self.assertIn("for (new i = 0; i < 16; i++) Cleaner_SpawnOne();", JOB)
        self.assertIn("CLEANER_ANIM_MS         (3700)", JOB)
        self.assertIn("gettime() + CLEANER_COLLECT_DELAY", JOB)
        self.assertIn('ApplyAnimation(playerid, "BOMBER", "BOM_Plant_Loop"', JOB)
        self.assertEqual(JOB.count('ApplyAnimation(playerid, "BOMBER", "BOM_Plant_Loop"'), 2)
        self.assertIn('true, false, false, false, CLEANER_ANIM_MS, SYNC_ALL)', JOB)
        self.assertIn("ClearAnimations(playerid);", JOB)

    def test_carried_bag_and_progress_hud(self):
        for needle in (
            "CLEANER_BAG_MODEL       (1265)",
            "SetPlayerAttachedObject(playerid, slot, CLEANER_BAG_MODEL, 6",
            "IsPlayerAttachedObjectSlotUsed(playerid, slot)",
            "RemovePlayerAttachedObject(playerid, slot)",
            "Cleaner_GiveBagProp(playerid)",
            "Cleaner_RemoveBagProp(playerid)",
            "PlayerText:gCleanerBagHud[MAX_PLAYERS]",
            'CreatePlayerTextDraw(playerid,',
            '"BAG: 0/6"',
            '"BAG: %d/%d"',
            "TEXT_DRAW_ALIGN_RIGHT",
            "PlayerTextDrawSetString(",
            "PlayerTextDrawDestroy(",
            "Cleaner_BagHudShow(playerid);",
            "Cleaner_BagHudUpdate(playerid);",
            "Cleaner_BagHudHide(playerid);",
            "Cleaner_OnDisconnect(playerid)",
            "Cleaner_OnSpawn(playerid)",
        ):
            self.assertIn(needle, JOB)
        self.assertNotIn("gCleanerBagHudSh", JOB)
        self.assertNotIn('"LD_SPAC:white"', JOB)
        self.assertNotIn('"Мусор собран: %d/%d."', JOB)

    def test_new_spread_litter(self):
        self.assertIn("CLEANER_SPACING_SQ      (196.0)", JOB)
        self.assertIn("stock bool:Cleaner_SpotClear(point)", JOB)
        self.assertIn("dx * dx + dy * dy < CLEANER_SPACING_SQ", JOB)
        self.assertIn("if (!Cleaner_SpotClear(point)", JOB)
        self.assertIn("if (!Cleaner_SpotClear(i)) continue;", JOB)
        self.assertIn("new target = 16 +", JOB)
        self.assertIn("CLEANER_VISIBLE_LIMIT   (24)", JOB)
        import re, math
        section = JOB.split("new const Float:gCleanerSpots",1)[1].split("new const gCleanerSpotDistrict",1)[0]
        pts = re.findall(r"\{(-?\d+(?:\.\d+)?), (-?\d+(?:\.\d+)?), (-?\d+(?:\.\d+)?)\}", section)
        self.assertEqual(len(pts), 87)
        coords = [(float(x), float(y)) for x,y,z in pts]
        # 67 новых точек должны быть распределены по разным улицам и
        # не быть сгруппированы по два рядом; старые 20 сохранены.
        self.assertGreater(max(x for x,y in coords)-min(x for x,y in coords), 300)
        self.assertGreater(max(y for x,y in coords)-min(y for x,y in coords), 200)
        for i,p in enumerate(coords[20:],20):
            for q in coords[i+1:]:
                self.assertGreaterEqual(math.hypot(p[0]-q[0],p[1]-q[1]), 30)

    def test_job_cleaner_02_ecology(self):
        for token in ("CLEANER_DISTRICTS       (2)", "CLEANER_POLLUTION_MAX   (100)",
                      "CLEANER_NIGHT_GOAL      (3)", "CLEANER_NIGHT_BONUS     (100)",
                      "CLEANER_NIGHT_COOLDOWN  (86400)",
                      "stock bool:Cleaner_CreateLitter(", "stock Cleaner_AdjustDirt(",
                      "Cleaner_ActiveTrash() >= CLEANER_VISIBLE_LIMIT",
                      "Cleaner_SpawnOne()", "Cleaner_VisibleTarget()",
                      "Cleaner_EnvironmentTick()", "Cleaner_LoadEnvironment()",
                      "Cleaner_SaveEnvironment()"):
            self.assertIn(token, JOB)
        self.assertIn("Cleaner_AdjustDirt(gCleanerSpotDistrict[nearest], -7);", JOB)
        self.assertIn("CLEANER_DIRECTORY \"/districts.env\"", JOB)
        self.assertIn("Cleaner_ActiveTrash() >= CLEANER_VISIBLE_LIMIT", JOB)

    def test_night_contract_persistence_and_guards(self):
        self.assertIn("stock Cleaner_StartNightContract(playerid)", JOB)
        self.assertIn("if (Cleaner_Rank(playerid) < 2 || !Cleaner_NightTime(playerid))", JOB)
        self.assertIn("gCleanerNightProgress[playerid]++;", JOB)
        self.assertIn("ORVELARCLEANER|1", JOB)
        self.assertIn("ORVELARCLEANER|2", JOB)
        self.assertIn("gCleanerNightLastPaid[playerid] = gettime();", JOB)
        self.assertIn("if (!Cleaner_Save(playerid))", JOB)
        self.assertIn("Cleaner_SettlePending(playerid)", JOB)
        self.assertIn("if (!Cleaner_FinishNightContract(playerid)) return 1;", JOB)
        self.assertIn("earned != collected * CLEANER_PAY_PER_UNIT + completed * CLEANER_NIGHT_BONUS", JOB)
        night = JOB.split("stock bool:Cleaner_FinishNightContract(playerid)", 1)[1].split("stock Cleaner_OnConnect(", 1)[0]
        for token in ("gCleanerNightCompleted[playerid]++;", "gCleanerEarned[playerid] += CLEANER_NIGHT_BONUS;",
                      "gCleanerNightCompleted[playerid]--;", "gCleanerEarned[playerid] -= CLEANER_NIGHT_BONUS;"):
            self.assertIn(token, night)

    def test_builds_stage_include(self):
        assert_staged(self, "cleaner_job.inc")


if __name__ == "__main__":
    unittest.main()
