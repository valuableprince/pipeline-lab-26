"""AUDIT-FIX-01: регрессии блокировок, сохраняемого mute и резервных копий."""
import re
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
AUTH = (ROOT / "gamemodes/signal0317.pwn").read_text(encoding="utf-8")
ADMIN = (ROOT / "gamemodes/includes/admin_system.inc").read_text(encoding="utf-8")


def function_body(source, name):
    pattern = r"stock (?:bool:)?"+re.escape(name)+r"\([^\n]*\)\n\{"
    match = re.search(pattern, source)
    assert match, f"Нет функции {name}"
    start = match.end()
    level = 1
    for pos in range(start, len(source)):
        if source[pos] == "{":
            level += 1
        elif source[pos] == "}":
            level -= 1
            if level == 0:
                return source[start:pos]
    raise AssertionError("Незакрытая функция")


class AuditFix01(unittest.TestCase):
    def test_lock_slots_released_after_expiration(self):
        find = function_body(AUTH, "Auth_FindLockSlot")
        self.assertIn("AUTH_FAILURE_WINDOW", AUTH)
        self.assertIn("gLockTouched[AUTH_LOCK_SLOTS]", AUTH)
        self.assertIn("gLockUntil[i] > now || gLockTouched[i] > now", find)
        self.assertIn("gLockName[i][0] = EOS;", find)
        self.assertIn("if (!create || freeSlot < 0) return -1;", find)
        fail = function_body(AUTH, "Auth_AuthFailure")
        self.assertIn("gLockTouched[slot] = gettime() + AUTH_FAILURE_WINDOW;", fail)
        self.assertIn("gLockTouched[slot] = gLockUntil[slot];", fail)

    def test_mute_is_persistent_and_fail_closed(self):
        load = function_body(ADMIN, "Admin_LoadMute")
        write = function_body(ADMIN, "Admin_WriteMute")
        ready = function_body(ADMIN, "Admin_OnReady")
        self.assertIn("SIGNALMUTE|1", load)
        self.assertIn("SIGNALMUTE|1", load)
        self.assertIn("Auth_SaveAccount(playerid)", write)
        self.assertIn("gAdminMuteUntil[playerid] = until;", write)
        self.assertIn('"%s/%s.mute"', ADMIN)  # legacy read only
        self.assertIn("Admin_LoadMute(playerid)", ready)
        self.assertIn("Kick(playerid)", ready)
        self.assertIn("if (!Admin_WriteMute(target, until))", ADMIN)
        self.assertIn("gAdminMuteUntil[playerid] > gettime()", AUTH)
        self.assertIn("gAdminMuteUntil[playerid] > gettime()",
                      (ROOT / "gamemodes/includes/leader_system.inc").read_text(encoding="utf-8"))

    def test_roles_survive_failed_rename(self):
        read = function_body(ADMIN, "Admin_ReadRole")
        save = function_body(ADMIN, "Admin_WriteRole")
        replace = function_body(ADMIN, "Admin_ReplaceFile")
        self.assertIn("fexist(backup) && !frename(backup, path)", read)
        self.assertIn("Admin_ReplaceFile(path, tmp, backup)", save)
        self.assertNotIn("fremove(path)", save)
        self.assertLess(replace.index("frename(path, backup)"),
                        replace.index("frename(temp, path)"))
        self.assertIn("frename(backup, path)", replace)
        self.assertIn("if (hadOriginal && fexist(backup)) fremove(backup);", replace)


if __name__ == "__main__":
    unittest.main()
