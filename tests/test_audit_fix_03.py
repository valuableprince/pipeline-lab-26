"""AUDIT-FIX-03 — структурные регрессии транзакций лидерства и модель отката.

Эти тесты не подменяют отказоустойчивые игровые UAT с реальной файловой системой.
"""

from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[1]
SRC = (ROOT / "gamemodes/includes/leader_system.inc").read_text(encoding="utf-8")
DOC = (ROOT / "docs/LEADER_SYSTEM.md").read_text(encoding="utf-8")


def body(name: str) -> str:
    match = re.search(r"stock (?:bool:)?"+re.escape(name)+r"\([^\n]*\)\n\{", SRC)
    if not match:
        raise AssertionError("Нет функции: " + name)
    depth = 1
    for index in range(match.end(), len(SRC)):
        depth += (SRC[index] == "{") - (SRC[index] == "}")
        if depth == 0:
            return SRC[match.end():index]
    raise AssertionError("Незакрытая функция: " + name)


class AppointmentModel:
    """Модель одной транзакции (не выполнение кода Pawn)."""

    def __init__(self, members=None, term=(0, 0)):
        self.members = list(members or [])
        self.term = term

    def assign(self, name, save_ok):
        old_members, old_term = list(self.members), self.term
        self.members = [(n, 10 if n == name else rank) for n, rank in self.members]
        if all(n != name for n, _ in self.members):
            self.members.append((name, 10))
        self.term = (1000, 1000 + 2592000)
        if not save_ok:
            self.members = old_members
            self.term = old_term
        return save_ok


class LeaderFix03(unittest.TestCase):
    def test_single_save_for_complete_appointment(self):
        assign = body("Leader_AssignLeader")
        self.assertIn("gFactionMemberRank[o][idx] = Leader_LeaderRank(org);", assign)
        self.assertIn("return org == FACTION_FINANCE ? 4 : 10;", SRC)
        self.assertIn("gFactionTermStart[o] = now;", assign)
        self.assertIn("gFactionTermEnd[o] = now + FACTION_TERM_SECONDS;", assign)
        self.assertEqual(assign.count("Leader_Save(org)"), 1)
        self.assertNotIn("Leader_AddMember(", assign)
        self.assertIn("if (!Leader_AssignLeader(org, gAccountName[target]))", SRC)
        command = body("Leader_Command")
        setleader = command[command.index('if (!strcmp(command, "setleader", true)'):command.index('if (!strcmp(command, "unleader", true)')]
        self.assertNotIn("Leader_AddMember(org, gAccountName[target], 8)", setleader)
        self.assertNotIn("Leader_ChangeRank(org, idx, 10)", setleader)

    def test_rollback_restores_new_and_existing_members(self):
        assign = body("Leader_AssignLeader")
        for statement in (
            "gFactionTermStart[o] = oldStart;",
            "gFactionTermEnd[o] = oldEnd;",
            "gFactionCount[o] = previousCount;",
            "gFactionMemberName[o][idx][0] = EOS;",
            "gFactionMemberRank[o][idx] = oldRank;",
            "gFactionMemberJoined[o][idx] = 0;",
            "gFactionMemberSeen[o][idx] = 0;",
        ):
            self.assertIn(statement, assign)
        for initial in ([], [("sheriff", 7)]):
            with self.subTest(initial=initial):
                model = AppointmentModel(initial, term=(0, 0))
                self.assertFalse(model.assign("sheriff", save_ok=False))
                self.assertEqual(model.members, initial)
                self.assertEqual(model.term, (0, 0))
                self.assertTrue(model.assign("sheriff", save_ok=True))
                self.assertEqual(model.members.count(("sheriff", 10)), 1)
                self.assertEqual(len(model.members), 1)

    def test_replace_never_deletes_live_roster(self):
        save = body("Leader_Save")
        self.assertIn("Leader_ValidateRoster(org)", save)
        self.assertIn("frename(path, backup)", save)
        self.assertIn("frename(temp, path)", save)
        self.assertIn("frename(backup, path)", save)
        self.assertNotIn("fremove(path)", save)
        self.assertLess(save.index("frename(path, backup)"), save.index("frename(temp, path)"))
        self.assertIn("gFactionAvailable[o] = false;", save)
        self.assertIn("Leader_CopyFile(backup, path)", body("Leader_Load"))

    def test_invariants_and_sensitive_admin_actions(self):
        validate = body("Leader_ValidateRoster")
        self.assertIn("leaders > 1 || deputies > FACTION_DEPUTY_MAX", validate)
        self.assertIn("gFactionTermEnd[o] > gFactionTermStart[o]", validate)
        self.assertIn("gFactionMemberRank[o][i] < 1 || gFactionMemberRank[o][i] > 10", validate)
        rank = body("Leader_ChangeRank")
        self.assertIn("rank < 1 || rank >= Leader_LeaderRank(org)", rank)
        for action in ("SET_LEADER", "RENEW_LEADER", "UNLEADER"):
            self.assertIn('"' + action + '_INTENT"', SRC)
            self.assertIn('"' + action + '"', SRC)
        self.assertIn("AUDIT-FIX-03", DOC)


if __name__ == "__main__":
    unittest.main()
