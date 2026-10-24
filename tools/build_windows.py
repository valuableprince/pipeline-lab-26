#!/usr/bin/env python3
"""Сборка Pawn-режима с русскими строками для локального open.mp сервера."""

import argparse
from pathlib import Path
import subprocess
import sys
import tempfile

if __package__:
    from .check_pawn_output import analyze_pawn_output
else:
    from check_pawn_output import analyze_pawn_output


ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "gamemodes" / "signal0317.pwn"


def encode_source(source_text: str) -> bytes:
    """Преобразование русского исходника UTF-8 в кодовую страницу клиента."""
    return source_text.encode("cp1251", errors="strict")


def compiler_command(compiler: Path, include_dir: Path, target: Path, source: Path):
    """Аргументы компилятора без запуска командной оболочки."""
    return [
        str(compiler),
        "-;+",
        "-(+",
        "-\\",
        "-Z-",
        "-d0",
        "-O2",
        "-t4",
        f"-i{include_dir}",
        f"-i{source.parent}",
        f"-o{target}",
        str(source),
    ]


def build(server: Path, output: Path) -> int:
    compiler = server / "qawno" / "pawncc.exe"
    include_dir = server / "qawno" / "include"
    if not compiler.is_file():
        print(f"Ошибка: компилятор не найден: {compiler}", file=sys.stderr)
        return 2
    if not (include_dir / "open.mp.inc").is_file():
        print(f"Ошибка: не найден open.mp.inc: {include_dir}", file=sys.stderr)
        return 2

    try:
        source_bytes = encode_source(SOURCE.read_text(encoding="utf-8-sig"))
    except (UnicodeError, OSError) as exc:
        print(f"Ошибка чтения/кодировки исходника: {exc}", file=sys.stderr)
        return 2

    output.mkdir(parents=True, exist_ok=True)
    target = output / "compiled"
    amx = target.with_suffix(".amx")
    if amx.exists():
        amx.unlink()

    with tempfile.TemporaryDirectory(prefix="compile_") as tmp:
        temporary_source = Path(tmp) / "signal0317.pwn"
        temporary_source.write_bytes(source_bytes)
        try:
            for module in sorted((ROOT / "gamemodes" / "includes").glob("*.inc")):
                (Path(tmp) / module.name).write_bytes(
                    encode_source(module.read_text(encoding="utf-8"))
                )
        except (UnicodeError, OSError) as exc:
            print(f"Ошибка подготовки модулей игрового режима: {exc}", file=sys.stderr)
            return 2
        command = compiler_command(compiler, include_dir, target, temporary_source)
        print("Компиляция игрового режима «СИГНАЛ: 03:17»...")
        try:
            result = subprocess.run(
                command,
                cwd=compiler.parent,
                check=False,
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
                text=True,
                errors="replace",
            )
        except OSError as exc:
            print(f"Ошибка запуска компилятора: {exc}", file=sys.stderr)
            return 2

        output_text = result.stdout or ""
        if output_text:
            print(output_text, end="" if output_text.endswith("\n") else "\n")
        issues = analyze_pawn_output(output_text)
        if result.returncode != 0 or issues or not amx.is_file() or amx.stat().st_size == 0:
            if amx.is_file():
                amx.unlink()
            for issue in issues:
                print(f"Pawn QA: {issue}", file=sys.stderr)
            print("Ошибка: компиляция с ошибками/предупреждениями или AMX отсутствует.", file=sys.stderr)
            return result.returncode or 1

    print(f"Сборка готова: {amx}")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description="Сборка русскоязычного игрового режима open.mp")
    parser.add_argument("--server", type=Path, default=Path("D:/Server"),
                        help="Путь к установленному серверу open.mp")
    parser.add_argument("--output", type=Path, default=ROOT / "build",
                        help="Папка для результата компиляции")
    args = parser.parse_args()
    return build(args.server.resolve(), args.output.resolve())


if __name__ == "__main__":
    raise SystemExit(main())
