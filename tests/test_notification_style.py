"""NOTIFY-01: common theme, semantic outcomes and system event migrations.

This is a source-level contract backed by the real Pawn/AMX compile job.
It intentionally leaves role-play speech (/me, /do), OOC (/b) and radio
(/r) as conversation, not system notices.
"""
from pathlib import Path
import re
import unittest

ROOT=Path(__file__).resolve().parents[1]
INCLUDES=ROOT/"gamemodes/includes"
CORE=(INCLUDES/"notification_style.inc").read_text(encoding="utf-8")
GAME=(ROOT/"gamemodes/signal0317.pwn").read_text(encoding="utf-8")


class NotificationStyleContract(unittest.TestCase):
    def test_module_is_loaded_before_all_gameplay_modules(self):
        self.assertIn('#include "notification_style.inc"',GAME)
        self.assertLess(GAME.index('#include "notification_style.inc"'),
                        GAME.index('#include "admin_system.inc"'))
        self.assertLess(GAME.index('#include "notification_style.inc"'),
                        GAME.index('#include "crime_reconnect.inc"'))

    def test_system_accents_are_unique_where_expected(self):
        expected={
            "ADMIN": ("Администрация","92B8FF"),
            "FACTION": ("Организация","9DD4B4"),
            "FINANCE": ("Госзаймы","D9AA55"),
            "CRIME": ("Криминальная система","FFD15C"),
            "WORK": ("Работа","A7D6B5"),
            "RACE": ("Гонки","E6BB79"),
            "SHIP": ("Судоходство","87D9E5"),
            "STORY": ("История","E6BB79"),
        }
        for system,(label,color) in expected.items():
            with self.subTest(system=system):
                marker=f"case TNOTIFY_{system}:"
                self.assertIn(marker,CORE)
                section=CORE.split(marker,1)[1].split("}",1)[0]
                self.assertIn(f'format(label,labelSize,"{label}");',section)
                self.assertIn(f'format(accent,accentSize,"{color}");',section)

    def test_semantics_and_144_char_limit(self):
        self.assertIn('#define TNOTIFY_MAX_CHAT (145)',CORE)
        for sev,color in [
            ("SUCCESS","B6E4B3"),
            ("WARNING","E27E82"),
            ("HINT","9FAAB8"),
        ]:
            self.assertIn(f'case TNOTIFY_{sev}: format(color,size,"{color}");',CORE)
        self.assertIn('default: format(color,size,"FFFFFF");',CORE)
        self.assertIn('"{%s}[%s] {%s}%s"',CORE)
        self.assertIn("format(line,size,",CORE)
        self.assertIn("TNotify_Format(system,severity,body,line,sizeof(line));",CORE)
        self.assertIn("TNotify_Semantic(severity,color,sizeof(color));",CORE)
        self.assertIn("if(!IsPlayerConnected(playerid) || IsPlayerNPC(playerid)) return 0;",CORE)
        self.assertIn("stock TNotify_All(system,severity,const body[])",CORE)
        self.assertIn("stock TNotify_Detail(playerid,severity,const body[])",CORE)
        self.assertNotIn("SetTimer(",CORE)
        self.assertNotIn("mysql_",CORE)

    def test_events_are_migrated_but_speech_is_not(self):
        systems={
            "payday.inc": ("TNOTIFY_GENERAL","TNotify_Send"),
            "bond_coupon.inc": ("TNOTIFY_FINANCE","TNotify_Send"),
            "ship_notice.inc": ("TNOTIFY_SHIP","TNotify_All"),
            "story_prologue.inc": ("TNOTIFY_STORY","TNotify_Send"),
            "race_core.inc": ("TNOTIFY_RACE","TNotify_Send"),
            "admin_system.inc": ("TNOTIFY_ADMIN","TNotify_Send"),
            "leader_system.inc": ("TNOTIFY_FACTION","TNotify_Send"),
            "cleaner_job.inc": ("TNOTIFY_WORK","TNotify_Send"),
            "crime_reconnect.inc": ("TNOTIFY_CRIME","TNotify_Send"),
        }
        for fname,(system,fn) in systems.items():
            with self.subTest(file=fname):
                data=(INCLUDES/fname).read_text(encoding="utf-8")
                self.assertIn(system,data)
                self.assertIn(fn,data)
        rp=(INCLUDES/"rp_chat.inc").read_text(encoding="utf-8")
        radio=(INCLUDES/"leader_system.inc").read_text(encoding="utf-8")
        self.assertIn("RP_COLOR_ACTION",rp)
        self.assertIn("RP_COLOR_DO",rp)
        self.assertIn("RP_COLOR_OOC",rp)
        self.assertIn("if (Leader_PlayerOrg(i) == org) SendClientMessage(i, FACTION_COLOR, message);",radio)

    def test_no_new_database_state_or_new_migration(self):
        for forbidden in ["CREATE TABLE","INSERT INTO","UPDATE accounts","START TRANSACTION"]:
            self.assertNotIn(forbidden,CORE)
        self.assertFalse((ROOT/"db/migrations/047_notification_style.sql").exists())


if __name__=="__main__":
    unittest.main()
