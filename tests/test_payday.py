"""PAYDAY-01 contracts for hourly EXP, SQL persistence, and retired NPC runtime."""
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[1]
GAME = (ROOT / "gamemodes/signal0317.pwn").read_text(encoding="utf-8")
PAYDAY = (ROOT / "gamemodes/includes/payday.inc").read_text(encoding="utf-8")
DB = (ROOT / "gamemodes/includes/db_primary.inc").read_text(encoding="utf-8")
SQL = (ROOT / "db/migrations/007_payday.sql").read_text(encoding="utf-8")
COMMANDS = (ROOT / "gamemodes/includes/commands.inc").read_text(encoding="utf-8")
STUDIO = (ROOT / "gamemodes/includes/studio_system.inc").read_text(encoding="utf-8")
WORLD = (ROOT / "gamemodes/includes/db_world.inc").read_text(encoding="utf-8")


class PayDayContract(unittest.TestCase):
    def test_timing_and_no_afk_or_salary(self):
        self.assertIn('SetTimer("PayDay_Tick", 1000, true)', PAYDAY)
        self.assertIn("gettime(hour, minute, second)", PAYDAY)
        self.assertIn("minute != 0", PAYDAY)
        self.assertIn("currentHourKey == gPayDayLastHour", PAYDAY)
        self.assertIn("PAYDAY_EXP_PER_HOUR (1)", PAYDAY)
        self.assertNotIn("GivePlayerMoney(", PAYDAY)
        self.assertNotIn("gCleanerDuty", PAYDAY)

    def test_persistence_and_score(self):
        for token in ("ADD COLUMN character_level", "ADD COLUMN character_exp",
                      "ADD COLUMN last_payday_hour", "VALUES (7)"):
            self.assertIn(token, SQL)
        self.assertIn("WHERE version=10 LIMIT 1", DB)
        for column in ("character_level", "character_exp", "last_payday_hour"):
            self.assertIn(column, DB)
            self.assertIn(column, PAYDAY)
        self.assertIn("row_version=row_version+1", PAYDAY)
        self.assertIn("last_payday_hour<%d", PAYDAY)
        self.assertIn("SetPlayerScore(playerid, level)", PAYDAY)
        self.assertIn("SetPlayerScore(playerid, gCharacterLevel[playerid])", PAYDAY)
        self.assertIn("PayDay_OnReady(playerid);", GAME)
        self.assertIn("PayDay_Init();", GAME)
        self.assertIn("PayDay_Shutdown();", GAME)
        self.assertIn("cmd:mn(playerid, params[])", COMMANDS)
        self.assertNotIn("cmd:level(playerid, params[])", COMMANDS)

    def test_chat_receipt_and_native_right_game_text(self):
        self.assertIn('========== TARKENSO | PAYDAY ==========', PAYDAY)
        self.assertIn('Опыт за час:', PAYDAY)
        self.assertNotIn("[Tarkenso RPG]", PAYDAY)
        self.assertIn("#define PAYDAY_COLOR (0xFFFFFFFF)", PAYDAY)
        for color in re.findall(r"\\{([0-9A-Fa-f]{6})\\}", PAYDAY):
            self.assertIn(color.upper(), {"D9AA55", "FFFFFF", "B6E4B3", "9FAAB8"})
        self.assertIn('Прогресс:', PAYDAY)
        self.assertIn('Осталось:', PAYDAY)
        self.assertIn('Поздравляем!', PAYDAY)
        self.assertIn('new earnedExp = level < PAYDAY_LEVEL_LIMIT ? PAYDAY_EXP_PER_HOUR : 0;', PAYDAY)
        self.assertIn('GameTextForPlayer(playerid, "~w~TARKENSO~n~~y~PAYDAY", 5000, 1);', PAYDAY)
        self.assertEqual(PAYDAY.count("PayDay_Notify(playerid, earnedExp, levelUp);"), 1)
        # A rejected, stale or failed DB update must never display the receipt.
        self.assertLess(PAYDAY.index("if (!DBPrimary_Execute(query, affected) || affected != 1)"),
                        PAYDAY.index("PayDay_Notify(playerid, earnedExp, levelUp);"))
        self.assertLess(PAYDAY.index("gDBPrimaryVersion[playerid]++;"),
                        PAYDAY.index("PayDay_Notify(playerid, earnedExp, levelUp);"))
        self.assertNotIn("GivePlayerMoney(", PAYDAY)

    def test_npc_runtime_removed_without_db_drops(self):
        for name in ("npc_core.inc", "npc_editor.inc", "npc_route_editor.inc"):
            self.assertFalse((ROOT / "gamemodes/includes" / name).exists())
            self.assertNotIn('#include "' + name + '"', GAME)
        for symbol in ("NpcCore_Init()", "NPCRoute_Init()", "NpcEditor_OnDialog",
                       "NPC_Create(", "OnNPCFinishMove("):
            self.assertNotIn(symbol, GAME)
        self.assertNotIn("NPCRoute_", WORLD)
        self.assertNotIn("NpcEditor_", STUDIO)
        self.assertNotIn("NPC", STUDIO[STUDIO.index("stock Studio_Open(playerid)"):])
        self.assertNotIn("DROP TABLE", SQL)


if __name__ == "__main__":
    unittest.main()
