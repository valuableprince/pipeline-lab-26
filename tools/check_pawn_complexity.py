#!/usr/bin/env python3
"""Heuristic hot-path/loop review for Pawn/open.mp; advisory, not a time profiler.

Uses the existing Pawn scanner's opaque-text masking and function extraction.
Loops with literal strings in comments are ignored; macros, preprocessor
alternatives and dynamic callbacks are not fully understood.
"""
from __future__ import annotations

import argparse
from collections import Counter, defaultdict, deque
from dataclasses import asdict, dataclass
import json
from pathlib import Path
import re
import sys

from check_pawn_static import calls, closing, functions, mask_opaque, source_tree

ROOT = Path(__file__).resolve().parents[1]
LOOPS = re.compile(r"\b(?:for|foreach|while)\s*\(")
HEAVY = re.compile(
    r"\b(?:mysql_query|mysql_tquery|DBPrimary_Execute|fopen|fwrite|fread|"
    r"Create(?:Dynamic)?Object|Destroy(?:Dynamic)?Object|"
    r"TextDrawCreate|CreatePlayerTextDraw)\s*\("
)
MUTATOR = re.compile(r"\b(?:SendClientMessage|SetPlayerPos|SetPlayerVirtualWorld)\s*\(")
MACRO = re.compile(r"(?m)^\s*#define\s+([A-Z][A-Z0-9_]*)\s+\(?(\d+)\)?\s*(?://.*)?$")
TIMER = re.compile(
    r'\bSetTimer(?:Ex)?\s*\(\s*"([A-Za-z_]\w*)"\s*,\s*'
    r'([A-Za-z_]\w*|\d+)\s*,\s*(true|false|1|0)\s*[,)]', re.I
)
PLAYER_BOUND = re.compile(r"\b(?:MAX_PLAYERS|GetMaxPlayers|Player|playerid)\b", re.I)
MASS_BOUND = re.compile(r"\b(?:BUSINESS(?:ES)?|MAX_BUSINESSES|MAX_OBJECTS|MAX_VEHICLES|FACTION|MAX_PLAYERS|g\w*Count)\b", re.I)


@dataclass(frozen=True, order=True)
class Warning:
    path: str
    line: int
    rule: str
    function: str
    context: str


@dataclass(frozen=True)
class Loop:
    start: int
    end: int
    header: str


def loops_of(code: str) -> list[Loop]:
    """Recover compound loop extents, not regex-counting braces in strings."""
    result: list[Loop] = []
    for match in LOOPS.finditer(code):
        op = code.find("(", match.start(), match.end())
        end = closing(code, op, "(", ")")
        if end < 0:
            continue
        begin = end + 1
        while begin < len(code) and code[begin].isspace():
            begin += 1
        if begin < len(code) and code[begin] == "{":
            stop = closing(code, begin, "{", "}")
        else:
            stop = code.find(";", begin)
        if stop >= 0:
            result.append(Loop(match.start(), stop + 1, code[op+1:end]))
    return result


def timer_roots(texts: dict[str, str]) -> dict[str, int]:
    consts = {}
    for src in texts.values():
        for name, value in MACRO.findall(src):
            consts.setdefault(name, int(value))
    roots: dict[str, int] = {}
    for src in texts.values():
        # Match against code stripped of comments while keeping string literals.
        # All timer calls must have a literal function name for this check.
        sanitized = re.sub(r"//[^\n]*|/\*.*?\*/", " ", src, flags=re.S)
        for name, period, repeat in TIMER.findall(sanitized):
            if repeat.lower() not in ("true", "1"):
                continue
            ms = int(period) if period.isdigit() else consts.get(period)
            if ms is not None and 0 < ms <= 1000:
                roots[name] = min(roots.get(name, ms), ms)
    return roots


def scan(texts: dict[str, str]) -> list[Warning]:
    fns = {}
    callers: dict[str, set[str]] = defaultdict(set)
    timers = timer_roots(texts)
    for path, source in sorted(texts.items()):
        clean = mask_opaque(source)
        for fn in functions(source, clean):
            # Duplicate names may be preprocessor alternatives: first visible
            # declaration is used for reachability, warnings stay advisory.
            fns.setdefault(fn.name, (path, source, fn))
    for fn_name, (_, _, fn) in fns.items():
        for call, _, _ in calls(fn.body, fn.clean):
            if call in fns and call != fn_name:
                callers[fn_name].add(call)

    hot: dict[str, str] = {}
    for root in fns:
        if root == "OnPlayerUpdate":
            hot[root] = "per-player update callback"
        elif root == "OnPlayerConnect":
            hot[root] = "connection callback"
        elif root == "OnGameModeInit":
            hot[root] = "startup callback"
        elif root in timers:
            hot[root] = f"repeating timer {timers[root]}ms"

    # Traverse 2 calls deep to detect work delegated into a helper; avoid
    # treating the entire gamemode as hot through unrelated deep call graphs.
    queue = deque((name, 0, label) for name, label in hot.items())
    visited = set()
    contexts: dict[str, set[str]] = defaultdict(set)
    while queue:
        name, depth, label = queue.popleft()
        if (name, depth, label) in visited or name not in fns:
            continue
        visited.add((name, depth, label))
        contexts[name].add(label)
        if depth < 2:
            for child in sorted(callers.get(name, ())):
                queue.append((child, depth+1, label))

    issues: set[Warning] = set()
    for name, (path, source, fn) in fns.items():
        flow = contexts.get(name)
        if not flow:
            continue
        ls = loops_of(fn.clean)
        # Report loops containing explicit high-cost native/resource calls.
        for item in ls:
            loop_code = fn.clean[item.start:item.end]
            for expensive in HEAVY.finditer(loop_code):
                # A single warning per hot loop and native call category.
                line = source.count("\n", 0, fn.start + item.start + expensive.start()) + 1
                issues.add(Warning(path, line, "HOT_LOOP_IO",
                                   name, f"{expensive.group(0).split('(')[0]} in loop; reached from {', '.join(sorted(flow))}"))
        for outer in ls:
            for inner in ls:
                if inner.start <= outer.start or inner.end > outer.end:
                    continue
                # Only directly nested in one another, rather than arbitrary
                # unrelated loops separated in the same function.
                if not (PLAYER_BOUND.search(outer.header+inner.header) and
                        MASS_BOUND.search(outer.header+inner.header)):
                    continue
                line = source.count("\n", 0, fn.start + inner.start) + 1
                issues.add(Warning(path, line, "NESTED_MASS_SCAN", name,
                                   f"nested player/entity-like loops reached from {', '.join(sorted(flow))}"))
        if name in timers and timers[name] <= 250 and len(ls) >= 2:
            line = source.count("\n", 0, fn.start) + 1
            issues.add(Warning(path, line, "FAST_TIMER_LOOPS", name,
                               f"{timers[name]}ms timer contains {len(ls)} loops; verify work per tick"))
    return sorted(issues)


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=ROOT)
    parser.add_argument("--output", type=Path, default=Path("build/pawn_complexity.json"))
    parser.add_argument("--strict", action="store_true")
    args = parser.parse_args(argv)
    try:
        sources = source_tree(args.root)
        issues = scan(sources)
        report = {"schema_version": 1, "mode": "advisory",
                  "files_scanned": len(sources), "total_findings": len(issues),
                  "by_rule": dict(sorted(Counter(x.rule for x in issues).items())),
                  "findings": [asdict(x) for x in issues]}
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    except (OSError, ValueError, UnicodeError) as e:
        print("Pawn complexity scanner failed:", e, file=sys.stderr)
        return 2
    for item in issues[:50]:
        msg = item.context.replace("%", "%25").replace("\n", " ")
        print(f"::warning file={item.path},line={item.line},title={item.rule}::{item.function}: {msg}")
    print(f"Pawn Complexity Guard: {len(sources)} files, {len(issues)} potential issues (advisory)")
    return 1 if args.strict and issues else 0


if __name__ == "__main__":
    raise SystemExit(main())
