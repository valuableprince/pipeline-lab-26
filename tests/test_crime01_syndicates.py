"""CRIME-01: regression contracts for three persistent five-rank syndicates."""
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[1]
CORE = (ROOT / "gamemodes/includes/leader_system.inc").read_text(encoding="utf-8")
UI = (ROOT / "gamemodes/includes/leader_ui.inc").read_text(encoding="utf-8")
SQL = (ROOT / "db/migrations/031_crime_syndicates.sql").read_text(encoding="utf-8")
DOC = (ROOT / "docs/CRIME01_SYNDICATES.md").read_text(encoding="utf-8")


class CrimeSyndicateContracts(unittest.TestCase):
    def test_existing_organizations_preserved_and_three_added(self):
        self.assertIn("#define FACTION_COUNT (8)", CORE)
        self.assertIn("#define FACTION_FINANCE (5)", CORE)
        self.assertIn("#define FACTION_CRIME_FIRST (6)", CORE)
        self.assertIn("#define FACTION_CRIME_LAST (8)", CORE)
        for org, name in ((6, "Синдикат Гавань"),
                          (7, "Консорциум Вектор"),
                          (8, "Сеть Орион")):
            self.assertIn(f'case {org}: format(output, size, "{name}");', CORE)
        for old in ('Департамент шерифа', 'Медицинская служба',
                    'Радиостанция 03:17', 'Исследовательский отдел',
                    'Министерство финансов'):
            self.assertIn(old, CORE)
        self.assertIn('o < 3 || o == FACTION_FINANCE - 1 || Leader_IsCriminal(o + 1)', CORE)

    def test_rank_ceiling_and_leader_rights(self):
        for marker in (
            "stock bool:Leader_IsCriminal(org)",
            "if (org == FACTION_FINANCE) return 4;",
            "if (Leader_IsCriminal(org)) return 5;",
            "if (Leader_IsCriminal(org)) return 4;",
            "rank < 1 || rank > Leader_MaxRank(org)",
            "gFactionMemberRank[o][i] > Leader_MaxRank(org)",
            "gFactionMemberRank[o][idx] = Leader_LeaderRank(org);",
            "second >= Leader_LeaderRank(org)",
            "if (!Leader_RequireAdmin(playerid)) return 1;",
            "if (Leader_IsCriminal(org) && (second < 1 || second > 4))",
        ):
            self.assertIn(marker, CORE)
        for rank, title in enumerate(
            ("Новобранец", "Исполнитель", "Координатор", "Заместитель", "Лидер"), 1
        ):
            self.assertIn(f'case {rank}: format(output, size, "{title}");', CORE)
        self.assertNotIn("case 4: format(output, size, \"Синдикат", CORE)

    def test_file_and_sql_backends_keep_roster_integrity(self):
        # Invalid rank must be rejected on both loaded files and SQL rosters,
        # before any subsequent roster save can overwrite persisted state.
        self.assertIn('if (gFactionMemberRank[o][i] > Leader_MaxRank(org)) { ok = false; break; }', CORE)
        self.assertIn('gFactionMemberRank[o][i] > Leader_MaxRank(org)', CORE)
        self.assertIn('return Leader_ValidateRoster(org);', (
            ROOT / "gamemodes/includes/db_world.inc"
        ).read_text(encoding="utf-8"))
        for token in ('Leader_ValidateRoster(org)', 'Leader_Save(org)',
                      'Leader_AuditPlayer', 'Leader_RemoveMember(org, idx, true)'):
            self.assertIn(token, CORE)

    def test_migration_adds_only_new_enabled_ids(self):
        self.assertIn("START TRANSACTION;", SQL)
        self.assertIn("COMMIT;", SQL)
        self.assertRegex(SQL, r"(?s)INSERT INTO factions\s*\(faction_id, is_enabled, term_start, term_end\) VALUES")
        rows = re.findall(r"\((\d+),\s*(\d+),\s*(\d+),\s*(\d+)\)", SQL)
        self.assertEqual(rows, [("6", "1", "0", "0"),
                                ("7", "1", "0", "0"),
                                ("8", "1", "0", "0")])
        self.assertIn("INSERT INTO schema_migrations (version) VALUES (31);", SQL)
        self.assertNotIn("DELETE FROM", SQL)
        self.assertNotIn("UPDATE factions", SQL)

    def test_ui_and_solo_acceptance(self):
        self.assertIn('Leader_CrimeRankTitle(Leader_PlayerRank(playerid)', UI)
        self.assertIn('Leader_CrimeRankTitle(gFactionMemberRank[org - 1][i]', UI)
        self.assertIn('Синдикат: ID и ранг 1-4', UI)
        self.assertIn('/setleader 0 6', DOC)
        self.assertIn('/unleader', DOC)
        self.assertIn('031_crime_syndicates.sql', DOC)


if __name__ == "__main__":
    unittest.main()
