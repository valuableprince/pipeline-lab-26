"""ADMIN-PUNISH-03: MariaDB state, safety, command routing and jail lifecycle."""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]
P = (ROOT / "gamemodes/includes/admin_punish.inc").read_text("utf-8")
ADMIN = (ROOT / "gamemodes/includes/admin_system.inc").read_text("utf-8")
GAME = (ROOT / "gamemodes/signal0317.pwn").read_text("utf-8")
ROUTER = (ROOT / "gamemodes/includes/commands.inc").read_text("utf-8")
HELP = (ROOT / "gamemodes/includes/admin_help.inc").read_text("utf-8")
DB = (ROOT / "gamemodes/includes/db_primary.inc").read_text("utf-8")
RACE = (ROOT / "gamemodes/includes/race_core.inc").read_text("utf-8")
SQL = (ROOT / "db/migrations/009_admin_punish.sql").read_text("utf-8")


class AdminPunishContract(unittest.TestCase):
    def test_schema_and_startup_fail_closed(self):
        for name in ("admin_jail_sentences", "admin_warnings", "admin_player_notes"):
            self.assertIn("CREATE TABLE " + name, SQL)
            self.assertIn(name, P)
        self.assertIn("REFERENCES accounts(account_id)", SQL)
        self.assertIn("INSERT INTO schema_migrations (version) VALUES (9)", SQL)
        self.assertIn("WHERE version=10 LIMIT 1", DB)
        self.assertIn("if (!DBPrimary_Init())", GAME)

    def test_commands_and_help(self):
        for command in ("ajail", "unjail", "jailinfo", "warns",
                        "unwarn", "anote", "anotes", "offlineban"):
            self.assertIn("cmd:" + command + "(playerid, params[])", ROUTER)
            self.assertIn('{"' + command + '", "/' + command, HELP)
            self.assertIn('AdminHelp_Usage(playerid, "' + command + '")', P)

    def test_jail_survives_relog_and_spawn(self):
        for token in ("jailed_until", "released_at IS NULL", "AdminPunish_Load(playerid)",
                      "AdminPunish_OnSpawn(playerid)", "AdminPunish_Reset(playerid)",
                      'SetTimer("AdminPunish_Tick", 5000, true)',
                      "KillTimer(gAdminJailTimer)"):
            self.assertIn(token, P + GAME)
        self.assertIn("AdminPunish_IsJailed(playerid)", ROUTER)
        self.assertIn("AdminPunish_IsJailed(playerid)", RACE)
        self.assertIn("GetPlayerVirtualWorld(playerid) != ADMIN_JAIL_WORLD", P)
        self.assertIn("GetPlayerInterior(playerid) != ADMIN_JAIL_INTERIOR", P)
        self.assertIn("if (!AdminPunish_Release(playerid, \"SYSTEM\"))", P)
        self.assertIn("SpawnPlayer(playerid)", P)
        self.assertIn("Story_TapeStop(playerid)", P)
        self.assertIn("Story_ClearMarker(playerid)", P)

    def test_hierarchy_and_audit(self):
        self.assertIn("Admin_ValidTarget(playerid, target)", P)
        self.assertIn("Admin_IsFounderName(name)", P)
        self.assertIn("rank >= gAdminLevel[playerid]", P)
        for name in ("AJAIL_REQUEST", "UNJAIL_REQUEST", "UNWARN_REQUEST",
                     "ADMIN_NOTE_REQUEST", "OFFLINE_BAN_REQUEST", "JAILINFO_VIEW",
                     "WARN_VIEW", "NOTES_VIEW"):
            self.assertIn('"' + name + '"', P)
        self.assertIn('"WARN"', ADMIN)
        self.assertIn("AdminPunish_WriteWarning(playerid, target, reason)", ADMIN)

    def test_offline_ban_uses_persisted_roles_and_existing_bans(self):
        self.assertIn("LEFT JOIN admin_roles r", P)
        self.assertIn("Admin_FindOnlineName(name)", P)
        self.assertIn("INSERT INTO bans", P)
        self.assertIn("LOWER('%e')", P)
        self.assertIn("Admin_Require(playerid, PERM_BAN_PERMANENT)", P)

    def test_sql_is_escaped_and_warnings_are_revocable(self):
        self.assertIn("mysql_format(gDBPrimary", P)
        self.assertIn("UPDATE admin_warnings SET revoked_at=NOW()", P)
        self.assertIn("warning_id=%d AND account_id=%d AND revoked_at IS NULL", P)
        self.assertIn("DBPrimary_Execute(q, affected)", P)
        self.assertIn("Auth_SaveAccount(playerid)", GAME)

    def test_utf8_sources_compile_as_cp1251(self):
        for value in (P, ADMIN, GAME, ROUTER, HELP, DB):
            self.assertEqual(value, value.encode("cp1251").decode("cp1251"))


if __name__ == "__main__":
    unittest.main()
