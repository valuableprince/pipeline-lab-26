"""DB-MIGRATION-01: строгий, по умолчанию только читающий перенос файлов TARKENSO.

Запуск: python -m tools.migrate_legacy --scriptfiles /path/to/scriptfiles
БД меняется только с --apply --confirm EMPTY_DATABASE.
"""
from __future__ import annotations

import argparse
from dataclasses import dataclass, field
from decimal import Decimal, InvalidOperation
import hashlib
import json
import os
from pathlib import Path
import re
import sys
from typing import Any

NAME_RE = re.compile(r"[a-z0-9_]{3,20}\Z")
HASH_RE = re.compile(r"\$2[aby]\$[0-9]{2}\$[./A-Za-z0-9]{53}\Z")
MAX_INT = 2_147_483_647
LIMIT = 1_000_000_000
ACCOUNT_LAYOUT = {1: 3, 2: 7, 3: 9, 4: 15, 5: 16}
FIELDS = ("username", "password_hash", "background", "mute_until", "balance",
          "health", "armour", "cleaner_paid", "txn_serial",
          "race_starts", "race_finishes", "race_wins", "race_best",
          "race_earned", "race_last_paid", "start_region", "source_version")


class MigrationError(ValueError):
    """Нарушение целостности снимка: импорт запрещён."""


def username(value: str) -> str:
    if not NAME_RE.fullmatch(value):
        raise MigrationError(f"Недопустимый нормализованный ник: {value!r}")
    return value


def integer(value: str, maximum: int = MAX_INT, minimum: int = 0) -> int:
    if not re.fullmatch(r"[0-9]+", value):
        if value == "-1" and minimum == -1:
            return -1
        raise MigrationError("Некорректная числовая строка")
    # Не допускаем скрытого переполнения Pawn strval.
    if len(value) > 10 or (len(value) == 10 and
                            (int(value) > MAX_INT)):
        raise MigrationError("Переполнение целого")
    number = int(value)
    if number < minimum or number > maximum:
        raise MigrationError("Числовое поле вне допустимого диапазона")
    return number


def vital(value: str, minimum: int) -> Decimal:
    if not re.fullmatch(r"[0-9]+(?:\.[0-9]+)?", value):
        raise MigrationError("Неправильный формат здоровья/брони")
    try:
        number = Decimal(value)
    except InvalidOperation as exc:
        raise MigrationError("Неправильный формат Decimal") from exc
    if not (Decimal(minimum) <= number <= 100) or number.as_tuple().exponent < -2:
        raise MigrationError("Здоровье/броня должны помещаться в DECIMAL(5,2)")
    return number


@dataclass
class Snapshot:
    accounts: dict[str, dict[str, Any]] = field(default_factory=dict)
    cleaner: dict[str, tuple[int, int, int, int, int]] = field(default_factory=dict)
    admins: dict[str, tuple[int, int]] = field(default_factory=dict)
    bans: dict[str, tuple[int, str]] = field(default_factory=dict)
    factions: dict[int, tuple[int, int, list[tuple[str, int, int, int]]]] = field(default_factory=dict)
    districts: tuple[int, int] = (20, 20)
    top: list[tuple[str, int]] = field(default_factory=list)
    source_sha256: str = ""
    snapshot_files: int = 0

    def report(self) -> dict[str, Any]:
        return {
            "source_sha256": self.source_sha256,
            "snapshot_files": self.snapshot_files,
            "accounts": len(self.accounts),
            "cleaner_profiles": len(self.cleaner),
            "admin_roles": len(self.admins),
            "bans": len(self.bans),
            "factions": len(self.factions),
            "faction_members": sum(len(v[2]) for v in self.factions.values()),
            "districts": 2,
            "race_top": len(self.top),
        }


class SnapshotReader:
    def __init__(self, root: Path):
        self.root = root
        self.digest = hashlib.sha256()
        self.files = 0

    def lines(self, name: str, *, fallback: bool = True) -> list[str] | None:
        path = self.root / name
        if not path.is_file() and fallback and path.with_name(path.name + ".bak").is_file():
            path = path.with_name(path.name + ".bak")
        if not path.is_file():
            return None
        data = path.read_bytes()
        if not data or b"\x00" in data:
            raise MigrationError(f"Пустой или бинарный источник: {name}")
        try:
            decoded = data.decode("utf-8")
        except UnicodeDecodeError:
            decoded = data.decode("cp1251")
        # Запрет повреждённых или дополнительных строк (включая пустые).
        lines = decoded.splitlines()
        if not lines or any(not x for x in lines):
            raise MigrationError(f"Неполный файл: {name}")
        relative = path.relative_to(self.root).as_posix()
        self.digest.update(relative.encode("utf-8") + b"\x00" + len(data).to_bytes(8, "big") + data)
        self.files += 1
        return lines

    def scan(self, directory: str, extension: str) -> list[str]:
        parent = self.root / directory
        if not parent.exists():
            return []
        if not parent.is_dir():
            raise MigrationError(f"Не каталог: {directory}")
        found: set[str] = set()
        for f in sorted(parent.iterdir()):
            if not f.is_file():
                raise MigrationError(f"Неожиданный вложенный путь: {f.name}")
            if f.name.endswith(".tmp"):
                raise MigrationError(f"Незавершённая запись .tmp: {directory}/{f.name}")
            if f.name.endswith(extension):
                found.add(f.name[:-len(extension)])
            elif f.name.endswith(extension + ".bak"):
                name = f.name[:-len(extension + ".bak")]
                if not (parent / (name + extension)).exists():
                    found.add(name)
        return sorted(found)


def parse_account(name: str, lines: list[str]) -> dict[str, Any]:
    if not re.fullmatch(r"SIGNAL0317\|[1-5]", lines[0]):
        raise MigrationError("Неизвестный формат аккаунта")
    version = int(lines[0][-1])
    if len(lines) != ACCOUNT_LAYOUT[version] or not HASH_RE.fullmatch(lines[1]):
        raise MigrationError("Повреждённое личное дело или bcrypt-хеш")
    fields: dict[str, Any] = {
        "username": username(name), "password_hash": lines[1],
        "background": integer(lines[2], 3, 1), "mute_until": 0,
        "balance": 0, "health": Decimal("100.00"), "armour": Decimal("0.00"),
        "cleaner_paid": -1, "txn_serial": 0,
        "race_starts": 0, "race_finishes": 0, "race_wins": 0,
        "race_best": 0, "race_earned": 0, "race_last_paid": 0,
        "start_region": 1, "source_version": version,
    }
    if version >= 2:
        fields.update(mute_until=integer(lines[3]), balance=integer(lines[4], LIMIT),
                      health=vital(lines[5], 1), armour=vital(lines[6], 0))
    if version >= 3:
        fields.update(cleaner_paid=integer(lines[7], 1_000_000, -1),
                      txn_serial=integer(lines[8], LIMIT))
    if version >= 4:
        starts, finishes, wins, best, earned, last = (
            integer(lines[9], LIMIT), integer(lines[10], LIMIT),
            integer(lines[11], LIMIT), integer(lines[12], 300),
            integer(lines[13], LIMIT), integer(lines[14]))
        if (finishes > starts or wins > finishes or
            (finishes == 0) != (best == 0) or
            (earned == 0) != (last == 0)):
            raise MigrationError("Непоследовательная гоночная статистика")
        fields.update(race_starts=starts, race_finishes=finishes, race_wins=wins,
                      race_best=best, race_earned=earned, race_last_paid=last)
    if version >= 5:
        fields["start_region"] = integer(lines[15], 3, 1)
    return fields


def parse_cleaner(lines: list[str]) -> tuple[int, int, int, int, int]:
    if lines[0] not in ("ORVELARCLEANER|1", "ORVELARCLEANER|2"):
        raise MigrationError("Неизвестный формат уборщика")
    v2 = lines[0].endswith("|2")
    if len(lines) != (6 if v2 else 4):
        raise MigrationError("Неполная статистика уборщика")
    xp, collected, earned = (integer(i, 1_000_000) for i in lines[1:4])
    completed = integer(lines[4], 1_000_000) if v2 else 0
    last = integer(lines[5]) if v2 else 0
    if xp != collected or earned != collected * 25 + completed * 100:
        raise MigrationError("Расходятся заработок и статистика уборщика")
    if (completed == 0) != (last == 0):
        raise MigrationError("Недействительная дата ночной премии")
    return xp, collected, earned, completed, last


def parse_faction(lines: list[str]) -> tuple[int, int, list[tuple[str, int, int, int]]]:
    if len(lines) < 4 or lines[0] != "SIGNALFACTION|1":
        raise MigrationError("Неизвестный формат организации")
    start, end, count = integer(lines[1]), integer(lines[2]), integer(lines[3], 64)
    if count != len(lines) - 4:
        raise MigrationError("Несовпадение размера состава")
    members = []
    leaders = deputies = 0
    names: set[str] = set()
    for entry in lines[4:]:
        parts = entry.split("|")
        if len(parts) != 4:
            raise MigrationError("Некорректная строка сотрудника")
        name = username(parts[0])
        rank, joined, seen = (integer(parts[1], 10, 1),
                              integer(parts[2]), integer(parts[3]))
        if name in names:
            raise MigrationError("Дублирующий сотрудник")
        names.add(name)
        leaders += rank == 10
        deputies += rank == 9
        members.append((name, rank, joined, seen))
    if (leaders > 1 or deputies > 2 or
        (leaders == 0 and (start or end)) or
        (leaders == 1 and not (start > 0 and end > start))):
        raise MigrationError("Нарушены ограничения состава")
    return start, end, members


def load_snapshot(root: Path) -> Snapshot:
    root = root.resolve()
    if not root.is_dir():
        raise MigrationError(f"Нет каталога scriptfiles: {root}")
    r, shot = SnapshotReader(root), Snapshot()
    for name in r.scan("signal_accounts", ".acc"):
        shot.accounts[username(name)] = parse_account(
            name, r.lines(f"signal_accounts/{name}.acc") or [])
    if not shot.accounts:
        raise MigrationError("Не найдены аккаунты; проверьте путь к scriptfiles")
    for name in r.scan("signal_jobs", ".clean"):
        if username(name) not in shot.accounts:
            raise MigrationError(f"Сиротская статистика уборщика: {name}")
        shot.cleaner[name] = parse_cleaner(r.lines(f"signal_jobs/{name}.clean") or [])
    for name, account in shot.accounts.items():
        if account["cleaner_paid"] == -1:
            account["cleaner_paid"] = shot.cleaner.get(name, (0, 0, 0, 0, 0))[2]
        if name in shot.cleaner and account["cleaner_paid"] > shot.cleaner[name][2]:
            raise MigrationError(f"Выплаты уборщика превосходят заработок: {name}")
    for name in r.scan("signal_admins", ".adm"):
        if username(name) not in shot.accounts:
            raise MigrationError(f"Сиротская админроль: {name}")
        lines = r.lines(f"signal_admins/{name}.adm") or []
        if len(lines) != 3 or lines[0] != "SIGNALADMIN|1":
            raise MigrationError(f"Некорректная админроль: {name}")
        if lines[1] not in "0123456" or len(lines[1]) != 1 or lines[2] not in ("0", "1"):
            raise MigrationError(f"Некорректный уровень полномочий: {name}")
        shot.admins[name] = int(lines[1]), int(lines[2])
    for name in r.scan("signal_admins", ".mute"):
        if username(name) not in shot.accounts:
            raise MigrationError(f"Сиротская блокировка чата: {name}")
        lines = r.lines(f"signal_admins/{name}.mute") or []
        if len(lines) != 2 or lines[0] != "SIGNALMUTE|1":
            raise MigrationError(f"Некорректный Mute: {name}")
        shot.accounts[name]["mute_until"] = max(
            shot.accounts[name]["mute_until"], integer(lines[1], MAX_INT, 1))
    for name in r.scan("signal_bans", ".ban"):
        username(name)  # допускаются незарегистрированные имена
        lines = r.lines(f"signal_bans/{name}.ban") or []
        if len(lines) != 3 or lines[0] != "SIGNALBAN|1" or not 0 < len(lines[2]) <= 96:
            raise MigrationError(f"Некорректная блокировка: {name}")
        shot.bans[name] = integer(lines[1]), lines[2]
    for name in r.scan("signal_factions", ".dat"):
        if not re.fullmatch(r"org_[1-4]", name):
            raise MigrationError(f"Неизвестный файл фракции: {name}")
        shot.factions[int(name[-1])] = parse_faction(
            r.lines(f"signal_factions/{name}.dat") or [])
    # В игре 1–3 доступны, 4 зарезервирована. В БД создаём все четыре ID.
    for faction_id in range(1, 5):
        shot.factions.setdefault(faction_id, (0, 0, []))
    seen_members: set[str] = set()
    for _, (_, _, members) in shot.factions.items():
        for name, _, _, _ in members:
            if name not in shot.accounts:
                raise MigrationError(f"Сиротское членство организации: {name}")
            if name in seen_members:
                raise MigrationError(f"Несколько организаций у {name}")
            seen_members.add(name)
    env = r.lines("signal_jobs/districts.env")
    if env is not None:
        if len(env) != 3 or env[0] != "ORVELARENV|1":
            raise MigrationError("Некорректное состояние кварталов")
        shot.districts = integer(env[1], 100), integer(env[2], 100)
    leaders = r.lines("signal_races/top10.dat")
    if leaders is not None:
        if leaders[0] != "TARKENSO_RACE_TOP|1" or len(leaders) > 11:
            raise MigrationError("Некорректная таблица гонок")
        used: set[str] = set()
        previous = 0
        for line in leaders[1:]:
            if line.count("|") != 1:
                raise MigrationError("Некорректная строка топ-10")
            name, value = line.split("|")
            name = username(name)
            seconds = integer(value, 300, 1)
            if seconds < previous or name in used:
                raise MigrationError("Несортированные или повторные рекорды")
            shot.top.append((name, seconds))
            used.add(name)
            previous = seconds
    # Любой оставшийся .tmp означает, что снимок сделан не в безопасной точке.
    for folder in ("signal_accounts", "signal_jobs", "signal_admins",
                   "signal_bans", "signal_factions", "signal_races"):
        path = root / folder
        if path.is_dir() and any(p.name.endswith(".tmp") for p in path.iterdir()):
            raise MigrationError(f"Обнаружена незавершённая запись в {folder}")
    shot.source_sha256 = r.digest.hexdigest()
    shot.snapshot_files = r.files
    return shot


def apply_snapshot(shot: Snapshot, root: Path) -> None:
    # Credentials never go to command line, log or repository.
    try:
        import mysql.connector  # type: ignore[import-not-found]
    except ImportError as exc:
        raise MigrationError("Установите mysql-connector-python") from exc
    host = os.environ.get("TARKENSO_DB_HOST", "")
    database = os.environ.get("TARKENSO_DB_NAME", "")
    user = os.environ.get("TARKENSO_DB_USER", "")
    password = os.environ.get("TARKENSO_DB_PASSWORD", "")
    if not (host and database and user and password):
        raise MigrationError("Не заданы TARKENSO_DB_HOST/NAME/USER/PASSWORD")
    ca = os.environ.get("TARKENSO_DB_SSL_CA")
    if host not in ("localhost", "127.0.0.1", "::1") and not ca:
        raise MigrationError("Удалённая БД: используйте TLS c проверкой CA или SSH tunnel")
    settings: dict[str, Any] = dict(host=host, database=database, user=user,
                                     password=password, port=int(os.environ.get("TARKENSO_DB_PORT", "3306")),
                                     connection_timeout=10, autocommit=False)
    if ca:
        settings.update(ssl_ca=ca, ssl_verify_cert=True, ssl_verify_identity=True)
    connection = mysql.connector.connect(**settings)
    tables = ("accounts", "cleaner_profiles", "admin_roles", "bans", "factions",
              "faction_members", "cleaner_districts", "race_top", "legacy_imports")
    try:
        cursor = connection.cursor()
        connection.start_transaction(isolation_level="SERIALIZABLE")
        cursor.execute("SELECT version FROM schema_migrations ORDER BY version")
        if [x[0] for x in cursor.fetchall()] != [1, 2]:
            raise MigrationError("Неверная версия схемы; нужен пустой DB v2")
        for table in tables:
            cursor.execute(f"SELECT COUNT(*) FROM {table}")
            if cursor.fetchone()[0] != 0:
                raise MigrationError(f"Таблица {table} не пуста: импорт запрещён")
        # Повторное чтение источника перед записью.
        if load_snapshot(root).source_sha256 != shot.source_sha256:
            raise MigrationError("Источники изменились после проверки. Импорт отменён")
        fields_sql = ", ".join(FIELDS)
        placeholders = ", ".join(["%s"] * len(FIELDS))
        for account in shot.accounts.values():
            cursor.execute(f"INSERT INTO accounts ({fields_sql}) VALUES ({placeholders})",
                           tuple(account[k] for k in FIELDS))
        cursor.execute("SELECT username, account_id FROM accounts")
        ids = dict(cursor.fetchall())
        for name, row in shot.cleaner.items():
            cursor.execute("INSERT INTO cleaner_profiles (account_id,xp,collected,earned,night_completed,night_last_paid) VALUES (%s,%s,%s,%s,%s,%s)",
                           (ids[name], *row))
        for name, row in shot.admins.items():
            cursor.execute("INSERT INTO admin_roles (account_id,admin_level,event_role) VALUES (%s,%s,%s)",
                           (ids[name], *row))
        for name, row in shot.bans.items():
            cursor.execute("INSERT INTO bans (username,banned_until,reason) VALUES (%s,%s,%s)",
                           (name, *row))
        for faction, (start, end, members) in shot.factions.items():
            cursor.execute("INSERT INTO factions (faction_id,is_enabled,term_start,term_end) VALUES (%s,%s,%s,%s)",
                           (faction, faction <= 3, start, end))
            for name, rank, joined, seen in members:
                cursor.execute("INSERT INTO faction_members (account_id,faction_id,rank_id,joined_at,last_seen) VALUES (%s,%s,%s,%s,%s)",
                               (ids[name], faction, rank, joined, seen))
        for i, dirt in enumerate(shot.districts):
            cursor.execute("INSERT INTO cleaner_districts (district_id,pollution) VALUES (%s,%s)",
                           (i, dirt))
        for position, (name, seconds) in enumerate(shot.top, 1):
            cursor.execute("INSERT INTO race_top (position_id,username,best_seconds) VALUES (%s,%s,%s)",
                           (position, name, seconds))
        cursor.execute("INSERT INTO legacy_imports (source_sha256,snapshot_files,accounts_count) VALUES (%s,%s,%s)",
                       (shot.source_sha256, shot.snapshot_files, len(shot.accounts)))
        # Последняя защита от изменения живых данных.
        if load_snapshot(root).source_sha256 != shot.source_sha256:
            raise MigrationError("Снимок изменился во время транзакции")
        connection.commit()
    except Exception:
        connection.rollback()
        raise
    finally:
        connection.close()


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="TARKENSO: проверка/импорт файлов в MySQL")
    parser.add_argument("--scriptfiles", required=True, type=Path)
    parser.add_argument("--apply", action="store_true")
    parser.add_argument("--confirm", default="")
    args = parser.parse_args(argv)
    if args.apply and args.confirm != "EMPTY_DATABASE":
        parser.error("Для записи требуется --apply --confirm EMPTY_DATABASE")
    if not args.apply and args.confirm:
        parser.error("--confirm допустим только вместе с --apply")
    try:
        shot = load_snapshot(args.scriptfiles)
        print(json.dumps(shot.report(), ensure_ascii=False, sort_keys=True, indent=2))
        if args.apply:
            apply_snapshot(shot, args.scriptfiles)
            print("IMPORT COMMITTED: целостный снимок импортирован.")
        else:
            print("DRY RUN: БД и исходные файлы не изменены.")
    except (MigrationError, OSError, ValueError) as exc:
        print(f"DB-MIGRATION-01: {exc}", file=sys.stderr)
        return 1
    except Exception as exc:
        # No SQL parameters, passwords or raw account fields in public logs.
        print(f"DB-MIGRATION-01: импорт отменён ({type(exc).__name__}); транзакция откатана.", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
