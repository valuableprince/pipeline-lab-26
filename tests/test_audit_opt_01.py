"""AUDIT-OPT-01: структурная регрессия и модель плотного списка работников."""
from pathlib import Path
import re
import unittest

BASE = Path(__file__).resolve().parents[1]
LEADER = (BASE / "gamemodes/includes/leader_system.inc").read_text(encoding="utf-8")
CLEANER = (BASE / "gamemodes/includes/cleaner_job.inc").read_text(encoding="utf-8")
MAIN = (BASE / "gamemodes/signal0317.pwn").read_text(encoding="utf-8")


def function(source, name):
    match = re.search(r"(?:stock |public )(?:bool:)?"+re.escape(name)+r"\([^\n]*\)\n\{", source)
    if not match:
        raise AssertionError(f"No function {name}")
    depth = 1
    for i in range(match.end(), len(source)):
        depth += (source[i] == "{") - (source[i] == "}")
        if depth == 0:
            return source[match.end():i]
    raise AssertionError(f"Unclosed function {name}")


class WorkerIndex:
    def __init__(self):
        self.ids = []
        self.index = {}

    def add(self, player):
        if player not in self.index:
            self.index[player] = len(self.ids)
            self.ids.append(player)

    def remove(self, player):
        idx = self.index.pop(player, None)
        if idx is None:
            return
        moved = self.ids[-1]
        self.ids[idx] = moved
        self.ids.pop()
        if moved != player:
            self.index[moved] = idx


class OptContracts(unittest.TestCase):
    def test_worker_pool_structure(self):
        for needle in ("new gCleanerWorkers[MAX_PLAYERS];",
                       "new gCleanerWorkerIndex[MAX_PLAYERS];",
                       "new gCleanerWorkerCount;",
                       "new last = --gCleanerWorkerCount;",
                       "gCleanerWorkerIndex[moved] = index;",
                       "Cleaner_WorkerRemove(playerid);"):
            self.assertIn(needle, CLEANER)
        tick = function(CLEANER, "Cleaner_ZoneTick")
        self.assertIn("i < gCleanerWorkerCount", tick)
        self.assertIn("gCleanerWorkers[i]", tick)
        self.assertNotIn("i < MAX_PLAYERS", tick)
        self.assertIn("Cleaner_WorkerAdd(playerid)", function(CLEANER, "Cleaner_OnKeyStateChange"))
        self.assertIn("Cleaner_WorkerRemove(playerid)", function(CLEANER, "Cleaner_OnConnect"))
        self.assertIn("Cleaner_EndShift(playerid)", function(CLEANER, "Cleaner_OnSpawn"))
        self.assertIn("Cleaner_WorkerRemove(playerid)", function(CLEANER, "Cleaner_EndShift"))

    def test_worker_model_duplicate_and_swap_removal(self):
        registry = WorkerIndex()
        for player in [4, 9, 13, 18, 9]:
            registry.add(player)
        self.assertEqual(registry.ids, [4, 9, 13, 18])
        registry.remove(9)
        self.assertEqual(set(registry.ids), {4, 13, 18})
        self.assertEqual(registry.index[18], 1)
        registry.remove(18)
        registry.remove(99)
        registry.add(9)
        self.assertEqual(set(registry.ids), {4, 9, 13})
        self.assertEqual(sorted(registry.index.values()), [0, 1, 2])
        for player in list(registry.ids):
            registry.remove(player)
        self.assertEqual(registry.ids, [])
        self.assertEqual(registry.index, {})

    def test_leader_cache_and_commit_based_invalidation(self):
        for name in ("Leader_AddMember", "Leader_RemoveMember",
                     "Leader_AssignLeader", "Leader_ChangeRank"):
            body = function(LEADER, name)
            self.assertLess(body.index("if (!Leader_Save(org))"),
                            body.index("Leader_RefreshAccount("))
        self.assertIn("Leader_RefreshPlayer(playerid);", function(LEADER, "Leader_OnReady"))
        org = function(LEADER, "Leader_PlayerOrg")
        rank = function(LEADER, "Leader_PlayerRank")
        self.assertIn("gFactionCachedOrg[playerid]", org)
        self.assertIn("gFactionCachedRank[playerid]", rank)
        self.assertNotIn("Leader_Find(", org + rank)
        self.assertIn("Leader_OrgValid(org)", org)
        self.assertIn("gFactionCachedOrg[playerid] = 0;", function(LEADER, "Leader_OnConnect"))
        self.assertIn("gFactionCachedRank[playerid] = 0;", function(LEADER, "Leader_OnConnect"))

    def test_invites_do_not_erase_membership(self):
        clear = function(LEADER, "Leader_ClearPending")
        self.assertNotIn("gFactionCached", clear)
        disconnect = function(LEADER, "Leader_OnDisconnect")
        self.assertIn("Leader_ClearPending(i);", disconnect)
        self.assertNotIn("Leader_OnConnect(i);", disconnect)
        accept = function(LEADER, "Leader_Command")
        self.assertIn("Leader_ClearPending(playerid);", accept)
        self.assertNotIn("Leader_OnConnect(playerid);", accept)

    def test_sparse_pool_and_guards(self):
        for source, name in [(MAIN, "Auth_Autosave")]:
            body = function(source, name)
            self.assertIn("playerid < MAX_PLAYERS", body)
            self.assertNotIn("GetPlayerPoolSize()", body)
            self.assertIn("IsPlayerConnected(playerid)", body)
        self.assertIn("IsPlayerNPC(playerid)", function(CLEANER, "Cleaner_ZoneTick"))
        self.assertIn("i < gCleanerWorkerCount", function(CLEANER, "Cleaner_ZoneTick"))
        self.assertIn("GetPlayerState(playerid) != PLAYER_STATE_ONFOOT",
                      function(CLEANER, "Cleaner_OnKeyStateChange"))


if __name__ == "__main__":
    unittest.main()
