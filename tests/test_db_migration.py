"""DB-MIGRATION-01: тесты строгого импорта без БД и без настоящих секретов."""
from pathlib import Path
import tempfile
import unittest
from contextlib import redirect_stderr, redirect_stdout
from io import StringIO
from decimal import Decimal

from tools.migrate_legacy import MigrationError, load_snapshot, parse_account, main


HASH = "$2b$12$" + "A" * 53


def acc(version=4, *, balance=125, paid=75):
    data = [f"SIGNAL0317|{version}", HASH, "2"]
    if version >= 2:
        data.extend(["0", str(balance), "90.50", "25.00"])
    if version >= 3:
        data.extend([str(paid), "3"])
    if version >= 4:
        data.extend(["5", "2", "1", "65", "150", "1234567890"])
    if version >= 5:
        data.append("3")
    return "\n".join(data) + "\n"


def write(root, name, content):
    target = root / name
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_text(content, encoding="utf-8")
    return target


class AccountFormats(unittest.TestCase):
    def test_all_account_versions(self):
        for ver in (1, 2, 3, 4, 5):
            with self.subTest(ver=ver):
                a = parse_account("player_17", acc(ver).splitlines())
                self.assertEqual(a["source_version"], ver)
                self.assertEqual(a["password_hash"], HASH)
                self.assertEqual(a["background"], 2)
                self.assertEqual(a["balance"], 0 if ver == 1 else 125)
                self.assertEqual(a["health"], Decimal("100") if ver == 1 else Decimal("90.50"))
                self.assertEqual(a["cleaner_paid"], -1 if ver <= 2 else 75)
                self.assertEqual(a["race_finishes"], 2 if ver >= 4 else 0)
                self.assertEqual(a["start_region"], 3 if ver == 5 else 1)

    def test_strict_bad_account(self):
        for text in (acc(4).replace(HASH, "bad"),
                     acc(4) + "999\n",
                     acc(4).replace("90.50", "101.50"),
                     acc(4).replace("90.50", "1.001"),
                     acc(4).replace("\n5\n2\n1\n", "\n1\n2\n1\n"),
                     acc(5).replace("\n3\n", "\n4\n"),
                     "SIGNAL0317|5\n" + HASH + "\n2\n"):
            with self.subTest(case=text[:20]):
                with self.assertRaises(MigrationError):
                    parse_account("player_17", text.splitlines())


class SnapshotContracts(unittest.TestCase):
    def setUp(self):
        self.t = tempfile.TemporaryDirectory()
        self.addCleanup(self.t.cleanup)
        self.root = Path(self.t.name)
        write(self.root, "signal_accounts/player_17.acc", acc())
        write(self.root, "signal_jobs/player_17.clean",
              "ORVELARCLEANER|2\n3\n3\n75\n0\n0\n")
        write(self.root, "signal_admins/player_17.adm",
              "SIGNALADMIN|1\n5\n1\n")
        write(self.root, "signal_bans/offline_user.ban",
              "SIGNALBAN|1\n0\nmanual action\n")
        write(self.root, "signal_factions/org_1.dat",
              "SIGNALFACTION|1\n100\n2592100\n1\nplayer_17|10|100|101\n")
        write(self.root, "signal_jobs/districts.env",
              "ORVELARENV|1\n33\n44\n")
        write(self.root, "signal_races/top10.dat",
              "TARKENSO_RACE_TOP|1\nplayer_17|65\n")

    def test_full_snapshot_and_reproducible_digest(self):
        a, b = load_snapshot(self.root), load_snapshot(self.root)
        self.assertEqual(a.source_sha256, b.source_sha256)
        self.assertEqual(a.accounts["player_17"]["balance"], 125)
        self.assertEqual(a.cleaner["player_17"], (3, 3, 75, 0, 0))
        self.assertEqual(a.admins["player_17"], (5, 1))
        self.assertEqual(a.bans["offline_user"], (0, "manual action"))
        self.assertEqual(a.factions[1][2], [("player_17", 10, 100, 101)])
        self.assertEqual(len(a.factions), 4)
        self.assertEqual(a.factions[4], (0, 0, []))
        self.assertEqual(a.districts, (33, 44))
        self.assertEqual(a.top, [("player_17", 65)])
        self.assertEqual(a.snapshot_files, 7)
        self.assertNotIn(HASH, str(a.report()))

    def test_legacy_paid_migration_not_repaid(self):
        write(self.root, "signal_accounts/player_17.acc", acc(2))
        shot = load_snapshot(self.root)
        self.assertEqual(shot.accounts["player_17"]["cleaner_paid"], 75)

    def test_legacy_mute_merge(self):
        write(self.root, "signal_admins/player_17.mute",
              "SIGNALMUTE|1\n1792785600\n")
        shot = load_snapshot(self.root)
        self.assertEqual(shot.accounts["player_17"]["mute_until"], 1792785600)

    def test_backup_only_without_modifying_files(self):
        f = self.root / "signal_accounts/player_17.acc"
        f.rename(f.with_name(f.name + ".bak"))
        shot = load_snapshot(self.root)
        self.assertEqual(len(shot.accounts), 1)
        self.assertFalse(f.exists())

    def test_disallow_unfinished_tmp(self):
        write(self.root, "signal_accounts/player_17.acc.tmp", "partial")
        with self.assertRaisesRegex(MigrationError, ".tmp"):
            load_snapshot(self.root)

    def test_detect_duplicate_faction_membership(self):
        write(self.root, "signal_factions/org_2.dat",
              "SIGNALFACTION|1\n0\n0\n1\nplayer_17|3|120|125\n")
        with self.assertRaises(MigrationError):
            load_snapshot(self.root)

    def test_detect_orphan_role_and_invalid_wage(self):
        write(self.root, "signal_admins/other.acc.adm", "SIGNALADMIN|1\n6\n0\n")
        with self.assertRaises(MigrationError):
            load_snapshot(self.root)
        (self.root / "signal_admins/other.acc.adm").unlink()
        write(self.root, "signal_jobs/player_17.clean",
              "ORVELARCLEANER|2\n3\n3\n74\n0\n0\n")
        with self.assertRaises(MigrationError):
            load_snapshot(self.root)

    def test_dry_run_does_not_change_files(self):
        before = load_snapshot(self.root).source_sha256
        with redirect_stdout(StringIO()) as output:
            code = main(["--scriptfiles", str(self.root)])
        self.assertEqual(code, 0)
        self.assertIn("DRY RUN", output.getvalue())
        self.assertNotIn(HASH, output.getvalue())
        self.assertEqual(load_snapshot(self.root).source_sha256, before)

    def test_apply_requires_explicit_confirmation(self):
        with redirect_stderr(StringIO()):
            with self.assertRaises(SystemExit):
                main(["--scriptfiles", str(self.root), "--apply"])


if __name__ == "__main__":
    unittest.main()
