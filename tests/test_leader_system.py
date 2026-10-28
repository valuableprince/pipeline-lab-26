"""LEADER-01: контрактные проверки лидерства, команд и изоляции прав."""

from pathlib import Path
from build_contracts import assert_staged
import re
import unittest

ROOT = Path(__file__).resolve().parents[1]
CORE = (ROOT / "gamemodes/includes/leader_system.inc").read_text(encoding="utf-8")
UI = (ROOT / "gamemodes/includes/leader_ui.inc").read_text(encoding="utf-8")
GAME = (ROOT / "gamemodes/signal0317.pwn").read_text(encoding="utf-8")
COMMANDS = (ROOT / "gamemodes/includes/commands.inc").read_text(encoding="utf-8")
WORKFLOW = (ROOT / ".github/workflows/build.yml").read_text(encoding="utf-8")
DOC = (ROOT / "docs/LEADER_SYSTEM.md").read_text(encoding="utf-8")


class LeaderCoreContract(unittest.TestCase):
    def test_three_original_one_reserved_and_finance_ministry(self):
        for value in ('FACTION_COUNT (5)', 'FACTION_DEPUTY_MAX (2)',
                      'FACTION_ROSTER_MAX (64)', 'FACTION_TERM_DAYS (30)',
                      'FACTION_TERM_SECONDS (2592000)', 'gFactionAvailable[o] = o < 3 || o == FACTION_FINANCE - 1',
                      'rank < 1 || rank > 10', 'Leader_LeaderRank(org)'):
            self.assertIn(value, CORE)

    def test_ministry_is_fifth_org_with_four_visible_ranks(self):
        for v in ("#define FACTION_FINANCE (5)",
                  'case FACTION_FINANCE: format(output, size, "Министерство финансов");',
                  "stock Leader_PublicRank(org, internalRank)",
                  "return org == FACTION_FINANCE ? 4 : 10;",
                  "return org == FACTION_FINANCE ? 3 : 9;",
                  "return internalRank;",
                  "stock bool:Leader_FinanceAuthorized(playerid, minPublicRank)",
                  "(org == FACTION_FINANCE && rank > 4)) return false;",
                  "gFactionMemberRank[o][idx] = Leader_LeaderRank(org);",
                  "org == FACTION_FINANCE && rank > 4",
                  "Leader_ActiveTerm(FACTION_FINANCE)"):
            self.assertIn(v,CORE)
        self.assertIn("Leader_PublicRank(org,Leader_PlayerRank(playerid))",UI)

    def test_independent_lifecycle_and_auth(self):
        for token in ('#include "leader_system.inc"', '#include "leader_ui.inc"',
                      'Leader_Init();', 'Leader_OnConnect(playerid);',
                      'Leader_OnReady(playerid);', 'Leader_OnDisconnect(playerid);',
                      'Leader_UI_OnDialog(playerid, dialogid, response, listitem, inputtext)'):
            self.assertIn(token, GAME)
        self.assertIn("gAuthState[playerid] != AUTH_READY", CORE)
        self.assertIn("Admin_Allowed(playerid, PERM_MANAGE_STAFF)", CORE)
        self.assertIn("gAdminDuty[playerid]", CORE)
        self.assertNotIn("gFactionMemberRank", (ROOT / "gamemodes/includes/admin_system.inc").read_text(encoding="utf-8"))

    def test_disk_integrity_and_fail_closed(self):
        for value in ('SIGNALFACTION|1', 'fcreatedir(FACTION_DIRECTORY)',
                      'frename(path, backup)', 'Leader_CopyFile(backup, path)',
                      'frename(temp, path)', 'Leader_AuditPlayer', 'if (!Leader_Load(o + 1))',
                      'gFactionAvailable[o] = false', 'Leader_ParseNumber',
                      '2147483647', 'gFactionCount[o] > FACTION_ROSTER_MAX', 'Leader_ValidateRoster(org)'):
            self.assertIn(value, CORE)

    def test_restrictions_and_transactions(self):
        for value in ('Leader_ActiveTerm(org)', 'Leader_PlayerOrg(target)',
                      'existingRank >= myRank', 'second >= myRank', 'deputies >= FACTION_DEPUTY_MAX',
                      'Leader_RemoveMember(org, idx)', 'gFactionPendingToken[playerid] != gAuthToken[playerid]',
                      'gFactionPendingExpiry[playerid] < gettime()', 'Leader_Save(org)',
                      'gFactionInviteCooldown[playerid] > gettime()'):
            self.assertIn(value, CORE)

    def test_solo_qa_admin_may_assign_or_renew_self(self):
        # Leader_Command shares this online-target validation between
        # /setleader and /renewleader. Self is now an authorized target.
        guard = CORE.split('if (!strcmp(command, "setleader", true) || !strcmp(command, "renewleader", true))',1)[1].split('if (!strcmp(command, "unleader", true))',1)[0]
        self.assertIn("if (!Leader_RequireAdmin(playerid)) return 1;",guard)
        self.assertIn("!Admin_Ready(first)",guard)
        self.assertNotIn("first == playerid",guard)
        self.assertIn('if (!Leader_AssignLeader(org, gAccountName[target]))',guard)
        self.assertIn('Leader_AuditPlayer(playerid, "SET_LEADER_INTENT", detail)',guard)
        self.assertIn('Leader_AuditPlayer(playerid, "SET_LEADER", detail)',guard)
        self.assertIn('Leader_AuditPlayer(playerid, "RENEW_LEADER_INTENT", detail)',guard)
        self.assertIn('Leader_AuditPlayer(playerid, "RENEW_LEADER", detail)',guard)
        self.assertIn('gFactionTermEnd[o] = gettime() + FACTION_TERM_SECONDS;',guard)
        self.assertIn('Leader_Save(org)',guard)
        self.assertIn('return gAdminLevel[playerid] >= 5;', (ROOT / "gamemodes/includes/admin_system.inc").read_text("utf-8"))
        self.assertIn('gAdminDuty[playerid]',CORE)

    def test_chat_and_activity(self):
        self.assertIn('gAdminMuteUntil[playerid] > gettime()', CORE)
        self.assertIn('gFactionMemberSeen[o][index] = gettime()', CORE)
        self.assertIn('days >= 14', CORE)
        self.assertIn('days >= 7', CORE)

    def test_pawncmd_commands_and_native_dialogs(self):
        cmds = ['leaders', 'fmenu', 'leader', 'fstats', 'r', 'finvite',
                'faccept', 'funinvite', 'frank', 'fleave', 'setleader', 'unleader',
                'renewleader']
        for cmd in cmds:
            self.assertRegex(COMMANDS, rf'(?m)^cmd:{cmd}\(playerid, params\[\]\)')
            self.assertIn(f'Leader_Command(playerid, "{cmd}", params)', COMMANDS)
        self.assertNotIn("cmd:fchat(", COMMANDS)
        self.assertNotIn('"fchat"', CORE)
        self.assertIn('"Использование: /r сообщение."', CORE)
        self.assertIn("DIALOG_STYLE_INPUT", UI)
        self.assertIn("DIALOG_STYLE_LIST", UI)
        self.assertNotIn("PlayerTextDraw", UI)
        self.assertNotIn("OnPlayerCommandText", GAME)

    def test_build_and_documentation(self):
        for module in ('leader_system.inc', 'leader_ui.inc'):
            assert_staged(self, module)
        self.assertIn("branches: [main]", WORKFLOW)
        for text in ('Ручная приёмка', '/setleader', '/faccept', '30 суток', 'signal_factions',
                     'audit.log', 'PostgreSQL', 'организации'):
            self.assertIn(text, DOC)


if __name__ == "__main__":
    unittest.main()
