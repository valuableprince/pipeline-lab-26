#!/usr/bin/env python3
"""Check newly introduced user-facing Pawn chat strings across ALL game modules.

Only new/edited SendClientMessage literal callsites are blocking on PRs. Existing
legacy messages remain outside the gate until they are edited. Dynamic text is
reported as unverified, never falsely treated as validated.
"""
from __future__ import annotations

import argparse
import re
import subprocess
import sys
from pathlib import Path

PREFIX = ("[TARKENSO]", "{D9AA55}[TARKENSO]", "{D9AA55}[Tarkenso]")
ALLOWED_FUNCTIONS = ("SendClientMessage", "SendClientMessageToAll")
CALL = re.compile(r"\\b(?:SendClientMessage|SendClientMessageToAll)\\s*\\(")
LITERAL = re.compile(r'"((?:\\\\.|[^"\\\\])*)"')
TRIGGER = re.compile(r"(?i)(использование\\s*:|синтаксис\\s*:|usage\\s*:)")
ANSI = re.compile(r"\\{[0-9A-Fa-f]{6}\\}")


def changed_line_numbers(diff: str) -> dict[str, set[int]]:
    current = None
    line = 0
    changed: dict[str, set[int]] = {}
    for entry in diff.splitlines():
        if entry.startswith("+++ b/"):
            current = entry[6:]
            continue
        if entry.startswith("@@"):
            match = re.search(r"\\+(\\d+)(?:,(\\d+))?", entry)
            if match:
                line = int(match.group(1))
            continue
        if current is None or not current.startswith("gamemodes/"):
            continue
        if not current.endswith((".inc", ".pwn")):
            continue
        if entry.startswith("+") and not entry.startswith("+++"):
            changed.setdefault(current, set()).add(line)
            line += 1
        elif entry.startswith(" "):
            line += 1
    return changed


def calls(text: str):
    # This is a bounded lexical analyzer, not a Pawn compiler. Ignore comments
    # before collecting names, and keep string literals opaque when balancing.
    for match in CALL.finditer(text):
        start = match.start()
        pos = match.end()
        nesting = 1
        quote = False
        escaped = False
        while pos < len(text) and nesting:
            c = text[pos]
            if quote:
                if escaped:
                    escaped = False
                elif c == "\\\\":
                    escaped = True
                elif c == '"':
                    quote = False
            elif c == '"':
                quote = True
            elif c == "(":
                nesting += 1
            elif c == ")":
                nesting -= 1
            pos += 1
        if nesting == 0:
            first = text.count("\n", 0, start) + 1
            last = text.count("\n", 0, pos) + 1
            yield first, last, text[match.end():pos - 1]


def check(changed: dict[str, set[int]], root: Path):
    issues = []
    checked = 0
    unknown = 0
    for path, touched in sorted(changed.items()):
        full = root / path
        if not full.is_file():
            continue
        for first, last, args in calls(full.read_text(encoding="utf-8")):
            if not any(i in touched for i in range(first, last + 1)):
                continue
            literals = LITERAL.findall(args)
            if not literals:
                unknown += 1
                continue
            # The last string literal is the message; object/string data arguments
            # before it might also include dynamic content, handled conservatively.
            msg = literals[-1]
            checked += 1
            normalized = ANSI.sub("", msg).lstrip()
            if TRIGGER.search(msg) and not normalized.startswith("[TARKENSO]"):
                issues.append(f"{path}:{first}: use unified help, not raw usage: {msg[:90]}")
            elif not normalized.startswith("[TARKENSO]"):
                issues.append(f"{path}:{first}: chat string lacks [TARKENSO] prefix: {msg[:90]}")
    return issues, checked, unknown


def main(argv=None):
    parser = argparse.ArgumentParser()
    parser.add_argument("--base", default="origin/main")
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[1])
    args = parser.parse_args(argv)
    result = subprocess.run(
        ["git", "diff", "--unified=0", f"{args.base}...HEAD", "--", "gamemodes/"],
        cwd=args.root, text=True, capture_output=True, check=False,
    )
    if result.returncode:
        print(f"Chat style audit could not read base {args.base}: {result.stderr}", file=sys.stderr)
        return 2
    issues, checked, unknown = check(changed_line_numbers(result.stdout), args.root)
    for issue in issues:
        print(f"CHAT-STYLE ERROR: {issue}")
    print(f"Chat style: inspected {checked} changed literal chat call(s), "
          f"unverified dynamic call(s) {unknown}, errors {len(issues)}.")
    return 1 if issues else 0


if __name__ == "__main__":
    raise SystemExit(main())
