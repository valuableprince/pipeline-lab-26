#!/usr/bin/env python3
"""Локальная первичная выдача аккаунта Veyran / Founder для TARKENSO.

Запускается ТОЛЬКО оператором на остановленном сервере. Пароль никогда
не передаётся в аргументах командной строки и не попадает в репозиторий.
"""

from __future__ import annotations

import argparse
import getpass
import os
from pathlib import Path


FOUNDER_NAME = "veyran"
ACCOUNT_FORMAT = "SIGNAL0317|4"
ROLE_FORMAT = "SIGNALADMIN|1"


def valid_password(value: str) -> bool:
    return 12 <= len(value) <= 64 and all(33 <= ord(char) <= 126 for char in value)


def exclusive_write(path: Path, text: str) -> None:
    """Создать файл без перезаписи и без доступных группе/остальным прав Unix."""
    fd = os.open(path, os.O_CREAT | os.O_EXCL | os.O_WRONLY, 0o600)
    with os.fdopen(fd, "w", encoding="ascii", newline="\n") as handle:
        handle.write(text)
        handle.flush()
        os.fsync(handle.fileno())


def bootstrap(scriptfiles: Path, password: str) -> tuple[Path, Path]:
    if not valid_password(password):
        raise ValueError("Пароль: 12–64 печатных ASCII-символа без пробелов.")
    try:
        import bcrypt
    except ImportError as exc:
        raise RuntimeError("Установите bcrypt: python -m pip install bcrypt") from exc

    accounts = scriptfiles / "signal_accounts"
    admins = scriptfiles / "signal_admins"
    account = accounts / (FOUNDER_NAME + ".acc")
    role = admins / (FOUNDER_NAME + ".adm")
    # Ник нельзя перехватить повторным созданием или подменить резервной копией.
    for item in (account, role, Path(str(account) + ".bak"), Path(str(role) + ".bak")):
        if item.exists() or item.is_symlink():
            raise FileExistsError(
                f"Обнаружен {item}. Ничего не перезаписано. "
                "Остановите сервер и проверьте существующий аккаунт вручную."
            )

    secret_hash = bcrypt.hashpw(password.encode("ascii"), bcrypt.gensalt(rounds=12)).decode("ascii")
    if len(secret_hash) != 60 or not secret_hash.startswith("$2"):
        raise RuntimeError("Не удалось подготовить совместимый bcrypt-хеш.")

    # Порядок строк соответствует Auth_WriteData (формат SIGNAL0317|4).
    account_content = "\n".join((
        ACCOUNT_FORMAT, secret_hash, "1", "0", "0", "100.00", "0.00",
        "-1", "0", "0", "0", "0", "0", "0", "0", ""
    ))
    role_content = "\n".join((ROLE_FORMAT, "6", "0", ""))
    accounts.mkdir(parents=True, exist_ok=True, mode=0o700)
    admins.mkdir(parents=True, exist_ok=True, mode=0o700)
    exclusive_write(account, account_content)
    try:
        exclusive_write(role, role_content)
    except BaseException:
        # Account exists and is protected from re-registration. Never overwrite
        # or silently recreate: operator must investigate the partial bootstrap.
        raise RuntimeError(
            f"Создан только {account}; файл роли создать не удалось. "
            f"Проверьте права на каталог и вручную создайте {role} "
            "с тремя строками SIGNALADMIN|1, 6, 0."
        )
    return account, role


def main() -> int:
    parser = argparse.ArgumentParser(description="Локальная инициализация Veyran (FOUNDER).")
    parser.add_argument("--scriptfiles", type=Path, default=Path("scriptfiles"),
                        help="Каталог scriptfiles остановленного open.mp сервера.")
    args = parser.parse_args()
    print("Остановите open.mp сервер перед созданием аккаунта Veyran.")
    first = getpass.getpass("Пароль Veyran (12–64 символа): ")
    second = getpass.getpass("Повторите пароль: ")
    if first != second:
        parser.error("Пароли не совпадают.")
    try:
        account, role = bootstrap(args.scriptfiles, first)
    except (ValueError, RuntimeError, FileExistsError, OSError) as exc:
        parser.error(str(exc))
    print(f"Аккаунт создан: {account}")
    print(f"Права основателя созданы: {role}")
    print("Запустите сервер, войдите как Veyran, используйте /aduty и /founder.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
