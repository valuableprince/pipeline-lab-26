#!/usr/bin/env python3
"""Blocking SQL injection guard for TARKENSO Pawn sources and SQL migrations.

This is a conservative lexical check, not a substitute for runtime SQL tests or
dataflow analysis. It examines every .pwn/.inc (including disabled #if branches).
The default policy: mysql_format with quoted %e for text and typed numeric
formatters; never format SQL with Pawn's unescaped format("%s", ...).
"""
from __future__ import annotations

import argparse
from dataclasses import asdict, dataclass
import json
from pathlib import Path
import re

if __package__:
    from .check_pawn_static import calls, mask_opaque
else:
    from check_pawn_static import calls, mask_opaque

ROOT = Path(__file__).resolve().parents[1]
STRING = re.compile(r'"(?:\\.|[^"\\])*"')
SQL_WORD = re.compile(
    r"\b(?:SELECT\s+|INSERT\s+INTO\s+|UPDATE\s+\w+\s+SET\s+|"
    r"DELETE\s+FROM\s+|REPLACE\s+INTO\s+|CALL\s+\w+\s*\(|"
    r"WHERE\s+\w+\s*(?:=|LIKE\b)|TRUNCATE\s+TABLE\s+|"
    r"ALTER\s+TABLE\s+|CREATE\s+TABLE\s+|DROP\s+TABLE\s+|"
    r"START\s+TRANSACTION\b)", re.I
)
SPECIFIER = re.compile(r"%(?:[-+ #0]*\d*(?:\.\d+)?)?([A-Za-z%])")
DECLARATION = re.compile(r"\b(?:native|forward|stock|public|static)\s+(?:\w+:)?\s*$")
DIRECT_INPUT = re.compile(
    r"\b(?:inputtext|params|message|password|username|user_input|"
    r"player_text|player_name|GetPlayerName)\b", re.I
)
SQL_COMMENT_OR_STRING = re.compile(
    r"/\*.*?\*/|--[^\n]*|#[^\n]*|'(?:''|\\.|[^'\\])*'|"
    r'"(?:""|\\.|[^"\\])*"', re.S
)
SQL_DYNAMIC = re.compile(r"\b(?:PREPARE\s+\w+\s+FROM|EXECUTE\s+IMMEDIATE)\b", re.I)
SINKS = {"mysql_query", "mysql_tquery", "mysql_pquery"}
NUMERIC = {"d", "i", "f", "F", "b"}


@dataclass(frozen=True, order=True)
class Finding:
    path: str
    line: int
    rule: str
    message: str


def static_string(expr: str) -> str | None:
    """Concatenated Pawn double-quoted literals; reject runtime expressions."""
    matches = list(STRING.finditer(expr))
    if not matches or STRING.sub("", expr).strip():
        return None
    return "".join(match.group()[1:-1] for match in matches)


def is_declaration(clean: str, offset: int) -> bool:
    beginning = clean.rfind("\n", 0, offset) + 1
    return bool(DECLARATION.search(clean[beginning:offset]))


def audit_pawn(path: str, source: str) -> tuple[list[Finding], int]:
    clean = mask_opaque(source)
    seen = [
        (name, offset, args) for name, offset, args in calls(source, clean)
        if not is_declaration(clean, offset)
    ]
    query_buffers = {
        args[1].strip()
        for name, _, args in seen
        if name in SINKS and len(args) > 1
        and re.fullmatch(r"[A-Za-z_]\w*", args[1].strip())
    }
    errors: list[Finding] = []
    count = 0

    def add(offset: int, rule: str, message: str) -> None:
        errors.append(Finding(path, source.count("\n", 0, offset) + 1, rule, message))

    for name, offset, args in seen:
        if name == "mysql_format":
            count += 1
            if len(args) < 4:
                add(offset, "SQL001", "mysql_format missing compile-time format string")
                continue
            template = static_string(args[3])
            if template is None:
                add(offset, "SQL001", "mysql_format must use a static SQL template")
                continue
            fields = list(SPECIFIER.finditer(template))
            for field in fields:
                kind = field[1]
                if kind == "%":
                    continue
                if kind == "s" or kind not in NUMERIC | {"e"}:
                    add(offset, "SQL002", f"unsafe/unsupported %{kind} in mysql_format; use quoted %e for text")
                elif kind == "e" and not (
                    field.start() > 0 and template[field.start() - 1] == "'"
                    and field.end() < len(template) and template[field.end()] == "'"
                ):
                    add(offset, "SQL003", "text escape %e must be enclosed in SQL single quotes")
            expected = sum(field[1] != "%" for field in fields)
            if len(args) - 4 != expected:
                add(offset, "SQL004", f"mysql_format has {len(args) - 4} values for {expected} placeholders")

        elif name == "format" and len(args) >= 3:
            template = static_string(args[2])
            if template is not None and (
                SQL_WORD.search(template)
                or (args[0].strip() in query_buffers and SPECIFIER.search(template))
            ):
                add(offset, "SQL005", "Pawn format() cannot construct SQL; use mysql_format(..., '%e', ...)")

        elif name in SINKS:
            count += 1
            if len(args) < 2:
                add(offset, "SQL006", "SQL execution missing query argument")
                continue
            expression = args[1].strip()
            literal = static_string(expression)
            if literal is not None:
                if any(m[1] in {"s", "e"} for m in SPECIFIER.finditer(literal)):
                    add(offset, "SQL006", "unexpanded string placeholder in SQL execution")
            elif DIRECT_INPUT.search(expression) or not re.fullmatch(r"[A-Za-z_]\w*", expression):
                add(offset, "SQL006", "nonconstant SQL execution argument needs manual review")

        elif name in {"strcat", "strins"} and len(args) >= 2:
            if args[0].strip() in query_buffers and static_string(args[1]) is None:
                add(offset, "SQL007", "runtime concatenation into an executed SQL query")

    return sorted(set(errors)), count


def audit_sql(path: str, source: str) -> list[Finding]:
    clean = SQL_COMMENT_OR_STRING.sub(
        lambda m: re.sub(r"[^\n]", " ", m.group()), source
    )
    return [
        Finding(path, clean.count("\n", 0, m.start()) + 1, "SQL008",
                "dynamic SQL in migration requires a reviewed static query")
        for m in SQL_DYNAMIC.finditer(clean)
    ]


def audit(texts: dict[str, str], migrations: dict[str, str] | None = None) -> tuple[list[Finding], int]:
    findings = []
    calls_checked = 0
    for path, source in sorted(texts.items()):
        errors, checked = audit_pawn(path, source)
        findings.extend(errors)
        calls_checked += checked
    for path, source in sorted((migrations or {}).items()):
        findings.extend(audit_sql(path, source))
    return sorted(set(findings)), calls_checked


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=ROOT)
    parser.add_argument("--output", type=Path, default=Path("build/sql_security_report.json"))
    args = parser.parse_args(argv)
    game_dir = args.root / "gamemodes"
    db_dir = args.root / "db"
    if not game_dir.is_dir() or not db_dir.is_dir():
        print("SQL-SECURITY ERROR: required gamemodes/ and db/ directories missing")
        return 2
    try:
        game = {
            p.relative_to(args.root).as_posix(): p.read_text(encoding="utf-8")
            for p in sorted(game_dir.rglob("*")) if p.suffix in {".pwn", ".inc"}
        }
        migrations = {
            p.relative_to(args.root).as_posix(): p.read_text(encoding="utf-8")
            for p in sorted(db_dir.rglob("*.sql"))
        }
    except (OSError, UnicodeError) as exc:
        print(f"SQL-SECURITY ERROR: failed to read source: {exc}")
        return 2
    findings, checked = audit(game, migrations)
    if checked < 10:
        findings.append(Finding("gamemodes", 1, "SQL000", "SQL query coverage unexpectedly low"))
    if not migrations:
        findings.append(Finding("db", 1, "SQL000", "no SQL migrations to inspect"))
    payload = {
        "status": "PASS" if not findings else "FAIL",
        "pawn_files": len(game),
        "sql_files": len(migrations),
        "sql_calls_checked": checked,
        "findings": [asdict(f) for f in sorted(findings)],
    }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(payload, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    for finding in sorted(findings):
        print(f"SQL-SECURITY ERROR: {finding.path}:{finding.line}: {finding.rule}: {finding.message}")
    print(f"SQL Security Gate: {payload['status']}; {len(game)} Pawn files, "
          f"{len(migrations)} SQL files, {checked} SQL calls, {len(findings)} findings.")
    return 1 if findings else 0


if __name__ == "__main__":
    raise SystemExit(main())
