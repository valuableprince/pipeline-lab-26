"""RACE-PROGRESS-01: контракты надёжной статистики и вознаграждения."""
from pathlib import Path
from build_contracts import assert_staged
import unittest
import re

ROOT = Path(__file__).resolve().parents[1]
AUTH = (ROOT / "gamemodes/signal0317.pwn").read_text(encoding="utf-8")
PROGRESS = (ROOT / "gamemodes/includes/race_progress.inc").read_text(encoding="utf-8")
CORE = (ROOT / "gamemodes/includes/race_core.inc").read_text(encoding="utf-8")
ECONOMY = (ROOT / "gamemodes/includes/economy.inc").read_text(encoding="utf-8")


class RaceProgressTest(unittest.TestCase):
    def test_v4_migration_and_strict_format(self):
        self.assertIn('#define ACCOUNT_FORMAT       "SIGNAL0317|5"', AUTH)
        for version in ("SIGNAL0317|1", "SIGNAL0317|2", "SIGNAL0317|3"):
            self.assertIn(version, AUTH)
        self.assertIn("gAccountLegacy[playerid] = v1 || v2 || v3 || v4;", AUTH)
        for column in ("Starts", "Finishes", "Wins", "Best", "Earned", "LastPaid"):
            self.assertIn(f"gAccountRace{column}[MAX_PLAYERS]", AUTH)
            self.assertIn(f"gAccountRace{column}[playerid]", AUTH)
        self.assertIn("finishes > starts || wins > finishes", AUTH)
        self.assertIn("if (fread(handle, extra, sizeof(extra))) valid = false;", AUTH)
        self.assertIn('new path[96], temp[112], backup[112], line[320];', AUTH)
        self.assertIn("Admin_ReplaceFile(path, temp, backup)", AUTH)
        self.assertIn('RaceProgress_OnReady(playerid);', AUTH)

    def test_start_and_finish_exactly_once_checkpoint(self):
        self.assertIn("if (!RaceProgress_OnStart(playerid))", CORE)
        self.assertIn("Race_Remove(playerid);", CORE)
        self.assertIn("RaceProgress_OnFinish(playerid, gRacePlaces, gRaceStartingCount,", CORE)
        self.assertIn("gRaceStartingCount = gRaceCount;", CORE)
        self.assertIn("gRaceProgress[playerid]++;", CORE)
        self.assertIn("if (point < 0 || point >= RACE_STAGE_COUNT) return 0;", CORE)
        self.assertIn("gAccountRaceStarts[playerid]++", PROGRESS)
        self.assertIn("gAccountRaceStarts[playerid]--", PROGRESS)

    def test_atomic_reward_and_rollback(self):
        self.assertIn("RaceProgress_Reward(playerid, place, runners, seconds)", PROGRESS)
        for const in ("RACE_PAY_SOLO", "RACE_PAY_FIRST", "RACE_PAY_SECOND",
                      "RACE_PAY_FINISH", "RACE_PAY_COOLDOWN",
                      "RACE_MIN_REWARD_TIME"):
            self.assertIn("#define " + const, PROGRESS)
        self.assertIn("gAccountRaceLastPaid[playerid] = gettime();", PROGRESS)
        self.assertIn("gAccountTxnSerial[playerid]++;", PROGRESS)
        self.assertIn("gAccountMoney[playerid] += reward;", PROGRESS)
        self.assertIn("if (!Auth_SaveAccount(playerid))", PROGRESS)
        self.assertIn("gAccountMoney[playerid] = oldMoney;", PROGRESS)
        self.assertIn("gAccountRaceEarned[playerid] = oldEarned;", PROGRESS)
        self.assertIn("gAccountRaceLastPaid[playerid] = oldPaid;", PROGRESS)
        self.assertIn("gAccountTxnSerial[playerid] = oldSerial;", PROGRESS)
        self.assertIn('Economy_Log(playerid, reward, "race_finish");', PROGRESS)
        self.assertIn("Economy_SyncHUD(playerid)", PROGRESS)
        self.assertIn("Auth_SaveAccount(playerid)", ECONOMY)

    def test_offline_leaderboard_durable_and_fail_closed(self):
        self.assertIn('"signal_races/top10.dat"', PROGRESS)
        self.assertIn('RACE_TOP_HEADER', PROGRESS)
        self.assertIn("fopen(RACE_TOP_FILE, io_read)", PROGRESS)
        self.assertIn("fopen(RACE_TOP_TEMP, io_write)", PROGRESS)
        self.assertIn("Admin_ReplaceFile(RACE_TOP_FILE, RACE_TOP_TEMP, RACE_TOP_BACKUP)", PROGRESS)
        self.assertIn("gRaceTopWritable = RaceProgress_LoadTop();", PROGRESS)
        self.assertIn("if (!gRaceTopWritable) return false;", PROGRESS)
        self.assertIn("if (RaceProgress_SaveTop()) return true;", PROGRESS)
        self.assertIn("gRaceTopCount = oldCount;", PROGRESS)
        self.assertIn("gAccountRaceBest[playerid] > 0", PROGRESS)
        self.assertIn("RaceProgress_UpdateTop(playerid)", PROGRESS)

    def test_commands_and_encoding(self):
        for cmd in ('"stats"', '"top"'):
            self.assertIn(f"!strcmp(params, {cmd}, true)", CORE)
        self.assertIn('#include "race_progress.inc"\n#include "race_core.inc"', AUTH)
        assert_staged(self, "race_progress.inc")
        PROGRESS.encode("cp1251")
        AUTH.encode("cp1251")


if __name__ == "__main__":
    unittest.main()
