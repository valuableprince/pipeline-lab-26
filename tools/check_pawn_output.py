#!/usr/bin/env python3
"""AUDIT-QA-01: строгая проверка диагностики официального компилятора Pawn.

Предупреждение должно ломать сборку даже при выходном коде 0.
Используется и в Linux CI, и в локальном Windows-сборщике.
"""
import argparse
from pathlib import Path
import re
import sys

DIAGNOSTIC = re.compile(r"\b(?:warning|error|fatal error)\s*\d+\s*:", re.I)
SUMMARY = re.compile(r"\b[1-9]\d*\s+(?:warnings?|errors?)\b", re.I)


def analyze_pawn_output(output: str) -> list[str]:
    """Возвращает список проблем, не выдавая успешный статус по пустому логу."""
    if not output.strip():
        return ["пустой вывод компилятора"]
    problems = []
    if "pawn compiler" not in output.casefold():
        problems.append("в журнале отсутствует идентификатор Pawn compiler")
    for line in output.splitlines():
        if DIAGNOSTIC.search(line) or SUMMARY.search(line):
            problems.append(line.strip())
    return problems


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="Требовать Pawn-сборку без предупреждений")
    parser.add_argument("log", type=Path, help="текстовый лог stdout/stderr компилятора")
    args = parser.parse_args(argv)
    try:
        output = args.log.read_text(encoding="utf-8", errors="replace")
    except OSError as exc:
        print(f"Pawn QA: журнал компиляции недоступен: {exc}", file=sys.stderr)
        return 1
    issues = analyze_pawn_output(output)
    if issues:
        for issue in issues:
            print(f"Pawn QA: {issue}", file=sys.stderr)
        return 1
    print("Pawn QA: компилятор запущен, ошибок и предупреждений нет.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
