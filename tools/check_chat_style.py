#!/usr/bin/env python3
"""Check newly introduced user-facing Pawn chat strings across ALL game modules.

Only new/edited literal SendClientMessage callsites are blocking on PRs. No
mandatory message label or prefix. Existing legacy messages remain untouched;
dynamic text is reported as unverified, not falsely treated as validated.
"""
from __future__ import annotations

import argparse
import re
import subprocess
import sys
from pathlib import Path

# Default chat colours follow /ahelp. The crime reconnect notification is
# a scoped, separately documented UI with the same colours as the /crime
# dialogs. Keep that exception path-specific rather than globally weakening
# the chat style gate for unrelated modules.
CHAT_PALETTE = frozenset({"D9AA55", "FFFFFF", "B6E4B3", "9FAAB8"})
CRIME_RECONNECT_PALETTE = frozenset({"FFD15C", "FFFFFF", "C7C7C7", "E27E82"})
SCOPED_PALETTES = {
    "gamemodes/includes/crime_reconnect.inc": CRIME_RECONNECT_PALETTE,
}
CALL = re.compile(r"\b(?:SendClientMessage|SendClientMessageToAll)\s*\(")
LITERAL = re.compile(r'"((?:\\.|[^"\\])*)"')
COLOR_TOKEN = re.compile(r"\{([0-9A-Fa-f]{6})\}")
# Only recognizable transliterated Russian interface terms. Do not block arbitrary
# Latin words (commands, names, product labels, SQL tokens) or proper Russian text.
TRANSLIT_UI = re.compile(
    r"(?i)(?<![A-Za-z])(?:ispolzovanie|ispolzovaniye|sintaksis|"
    r"opisanie|opisaniye|oshibka|vvedite|nedostupno|neizvestnaya)(?![A-Za-z])"
)


def changed_line_numbers(diff: str) -> dict[str, set[int]]:
    current = None
    line = 0
    changed: dict[str, set[int]] = {}
    for entry in diff.splitlines():
        if entry.startswith("+++ b/"):
            current = entry[6:]
            continue
        if entry.startswith("@@"):
            match = re.search(r"\+(\d+)(?:,(\d+))?", entry)
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
        palette = SCOPED_PALETTES.get(path, CHAT_PALETTE)
        palette_name = "/crime reconnect" if path in SCOPED_PALETTES else "/ahelp"
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
            for color in COLOR_TOKEN.findall(msg):
                if color.upper() not in palette:
                    issues.append(
                        f"{path}:{first}: inline chat color {{{color}}} is outside {palette_name} palette"
                    )
            if TRANSLIT_UI.search(COLOR_TOKEN.sub("", msg)):
                issues.append(
                    f"{path}:{first}: possible Russian UI transliteration: {msg[:90]}"
                )
    return issues, checked, unknown


def main(argv=None):
    parser = argparse.ArgumentParser()
    parser.add_argument("--base", default="origin/main")
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[1])
    args = parser.parse_args(argv)
    result = subprocess.run(
        ["git", "diff", "--unified=0", args.base, "HEAD", "--", "gamemodes/"],
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
