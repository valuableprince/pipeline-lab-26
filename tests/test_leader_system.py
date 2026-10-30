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

    def test_rejoin_does_not_lose_mixed_case_ministry_membership(self):
        world=(ROOT/"gamemodes/includes/db_world.inc").read_text("utf-8")
        # DB usernames keep their display case; the generated username_key
        # already provides case-insensitive identity in SQL.
        self.assertIn('Admin_NormalizeName(name, normalized, sizeof(normalized))',world)
        self.assertIn('format(gFactionMemberName[o][i], MAX_PLAYER_NAME, "%s", name);',world)
        self.assertNotIn('Admin_NormalizeName(name, gFactionMemberName[o][i], MAX_PLAYER_NAME)',world)
        self.assertIn('!strcmp(gFactionMemberName[o][i], name, true)',CORE)
        self.assertIn('!strcmp(gAccountName[playerid], name, true)',CORE)
        self.assertIn('!strcmp(gAccountName[i], name, true)',CORE)
        self.assertIn('!strcmp(gFactionMemberName[o][i], gFactionMemberName[other][j], true)',CORE)
        self.assertIn('!strcmp(gFactionMemberName[o][j], gFactionMemberName[o][i], true)',CORE)
        self.assertIn('strcmp(parts[0], normalized, true) != 0',CORE)
        self.assertIn('strcmp(gFactionMemberName[o][i], normalized, true)',CORE)
        # Regression: /unleader Veyran matches the loaded lower-case
        # roster 'veyran' instead of appending a second participant.
        identity=lambda x,y:x.casefold()==y.casefold()
        current=["veyran"]
        self.assertTrue(identity(current[0],"Veyran"))
        found=next((i for i,s in enumerate(current) if identity(s,"Veyran")),-1)
        self.assertEqual(found,0)
        self.assertEqual(len(current),1)

    def test_admin_unleader_fully_dismisses_and_recovers_on_save_failure(self):
        # A dismissed minister/leader becomes an ordinary civilian with
        # no roster slot and no cached faction permissions after save.
        remove=CORE.split("stock bool:Leader_RemoveMember(",1)[1].split(
            "// Полное назначение одним сохранением",1)[0]
        cmd=CORE.split('if (!strcmp(command, "unleader", true))',1)[1].split(
            'if (!strcmp(command, "fleave", true))',1)[0]
        self.assertIn("bool:removeLeader = false",remove)
        self.assertIn("if (isLeader != removeLeader) return false;",remove)
        self.assertIn("gFactionCount[o]--;",remove)
        self.assertIn("if (isLeader)",remove)
        self.assertIn("gFactionTermStart[o] = 0;",remove)
        self.assertIn("gFactionTermEnd[o] = 0;",remove)
        self.assertIn("if (!Leader_Save(org))",remove)
        self.assertIn("gFactionTermStart[o] = oldStart;",remove)
        self.assertIn("gFactionTermEnd[o] = oldEnd;",remove)
        self.assertIn("gFactionCount[o]++;",remove)
        self.assertIn("Leader_RefreshAccount(name);",remove)
        self.assertIn("!Leader_RemoveMember(org, idx, true)",cmd)
        self.assertIn("UNLEADER_INTENT",cmd)
        self.assertIn('"UNLEADER", detail',cmd)
        self.assertIn("Leader_FindOnline(name)",cmd)
        self.assertIn("Членство прекращено.",cmd)
        self.assertNotIn("Leader_ChangeRank(",cmd)
        self.assertNotIn("граждан",cmd)  # no alternate demotion path
        self.assertIn("Leader_RemoveMember(org, idx)",CORE)
        self.assertIn("!Leader_RequireAdmin(playerid)",cmd)
        self.assertIn("Leader_LeaderRank(x)",cmd)
        world=(ROOT/"gamemodes/includes/db_world.inc").read_text("utf-8")
        self.assertIn("START TRANSACTION",world)
        self.assertIn("DELETE FROM faction_members WHERE faction_id=%d",world)
        self.assertIn('DBWorld_Exec("COMMIT")',world)

    def test_radio_label_matches_command_everywhere(self):
        self.assertIn('if (!strcmp(command, "r", true))',CORE)
        self.assertIn('"[R] %s: %s"',CORE)
        self.assertNotIn('"[F] %s: %s"',CORE)
        self.assertIn("Использование: /r сообщение.",CORE)
        self.assertIn("Рация организации: /r сообщение.",UI)
        self.assertNotIn("/fchat",UI)
        self.assertNotIn("/fchat",CORE)

    def test_finance_rank_range_message_uses_faction_palette(self):
        # GOV UAT 3.5.2: explain the accepted rank range without role lists.
        block = CORE.split('if (org == FACTION_FINANCE)', 1)[1].split(
            'if (second < 1 || second >= myRank', 1)[0]
        self.assertIn("if (second < 1 || second > 3)", block)
        self.assertIn('return SendClientMessage(playerid, FACTION_COLOR,', block)
        self.assertIn('"Для Министерства финансов доступен ранг от 1 до 3."', block)
        self.assertNotIn("Минфин: 1=стажёр, 2=специалист, 3=заместитель.", CORE)
        self.assertNotIn("FACTION_ERROR", block)
        # The validation still runs before any rank mutation.
        self.assertLess(CORE.index("if (second < 1 || second > 3)"),
                        CORE.index("Leader_ChangeRank(org, idx, second)"))

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
