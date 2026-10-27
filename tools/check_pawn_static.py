#!/usr/bin/env python3
"""Conservative Pawn/open.mp static review; findings are advisory by default.

Not a runtime leak detector or full Pawn compiler. The scanner ignores comments
and string literals, balances function/call delimiters, and reports *possible*
lifecycle/control-flow mistakes for manual review. All results are reproducible.
"""
from __future__ import annotations

import argparse
from collections import Counter
from dataclasses import asdict, dataclass
import json
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[1]
FUNCTION = re.compile(r"\b(public|stock|static)\s+(?:\w+:)?([A-Za-z_]\w*)\s*\(")
CALL = re.compile(r"\b([A-Za-z_]\w*)\s*\(")
ASSIGNMENT = re.compile(
    r"(?:(?:new|static)\s+)?(?:(?:File|Cache):)?([A-Za-z_]\w*)\s*=\s*$"
)
DECL = re.compile(r"\b([A-Za-z_]\w*)\s*\[\s*(\d+)\s*\]")
RETURN = re.compile(r"\breturn\b")
PLAYER_MUTATOR = re.compile(
    r"\b(?:SetPlayer(?:Pos|Health|Armour|Score|Money|VirtualWorld|Interior|Skin)"
    r"|GivePlayerMoney|Kick|TogglePlayerControllable)\s*\(\s*playerid\b"
)
ASYNC_CALLBACK = re.compile(r"(?:_On(?:Account|Load|Query|Result|Save)|On\w*(?:Query|Result|Loaded|Complete))")
NUMBER = re.compile(r"\d+")


@dataclass(frozen=True, order=True)
class Finding:
    path: str
    line: int
    rule: str
    message: str


@dataclass(frozen=True)
class FunctionBody:
    name: str
    kind: str
    signature: str
    body: str
    clean: str
    start: int


def mask_opaque(source: str) -> str:
    """Replace string/char/comment bytes with spaces, preserve offsets/newlines."""
    out = list(source)
    i = 0
    while i < len(source):
        if source.startswith("//", i):
            end = source.find("\n", i)
            end = len(source) if end == -1 else end
        elif source.startswith("/*", i):
            close = source.find("*/", i + 2)
            end = len(source) if close == -1 else close + 2
        elif source[i] in ('"', "'"):
            delim = source[i]
            end = i + 1
            while end < len(source):
                if source[end] == "\\":
                    end += 2
                elif source[end] == delim:
                    end += 1
                    break
                else:
                    end += 1
            end = min(end, len(source))
        else:
            i += 1
            continue
        for j in range(i, end):
            if source[j] != "\n":
                out[j] = " "
        i = end
    return "".join(out)


def closing(clean: str, start: int, left: str, right: str) -> int:
    if start >= len(clean) or clean[start] != left:
        return -1
    depth = 0
    for i in range(start, len(clean)):
        if clean[i] == left:
            depth += 1
        elif clean[i] == right:
            depth -= 1
            if depth == 0:
                return i
    return -1


def split_args(source: str, clean: str) -> list[str]:
    pieces = []
    start = 0
    depth = 0
    for i, char in enumerate(clean):
        if char in "([{":
            depth += 1
        elif char in ")]}":
            depth -= 1
        elif char == "," and depth == 0:
            pieces.append(source[start:i].strip())
            start = i + 1
    pieces.append(source[start:].strip())
    return pieces


def functions(source: str, clean: str) -> list[FunctionBody]:
    results: list[FunctionBody] = []
    for match in FUNCTION.finditer(clean):
        paren = clean.find("(", match.start(), match.end())
        end = closing(clean, paren, "(", ")")
        if end < 0:
            continue
        brace = end + 1
        while brace < len(clean) and clean[brace].isspace():
            brace += 1
        if brace >= len(clean) or clean[brace] != "{":
            continue  # forward/native declaration, not a function body
        finish = closing(clean, brace, "{", "}")
        if finish == -1:
            continue
        results.append(FunctionBody(
            match[2], match[1], source[paren + 1:end],
            source[brace + 1:finish], clean[brace + 1:finish], brace + 1,
        ))
    return results


def calls(original: str, clean: str):
    for match in CALL.finditer(clean):
        opening = clean.find("(", match.start(), match.end())
        end = closing(clean, opening, "(", ")")
        if end < 0:
            continue
        yield match[1], match.start(), split_args(
            original[opening + 1:end], clean[opening + 1:end]
        )


def declared_buffers(body: str, clean: str) -> dict[str, int]:
    # Numeric, direct array declarations only. Symbolic sizes are left unknown.
    arrays: dict[str, int] = {}
    for match in DECL.finditer(clean):
        lead = clean[max(0, match.start() - 100):match.start()]
        if re.search(r"(?:\bnew|\bstatic|\bconst)\b[^;{}]*$", lead):
            arrays[match[1]] = int(match[2])
    return arrays


def is_guarded_failed_handle(fragment: str, name: str) -> bool:
    # Only suppress the common "if (!handle) return 0;" validation path.
    tail = fragment[-160:]
    return bool(re.search(
        r"\bif\s*\(\s*!\s*" + re.escape(name) +
        r"\s*\)\s*(?:\{\s*)?return\s+(?:0|false|INVALID_[A-Z_]+)\s*;",
        tail, re.I,
    ))


def analyze_sources(texts: dict[str, str]) -> list[Finding]:
    findings: set[Finding] = set()
    all_clean = {path: mask_opaque(src) for path, src in texts.items()}
    # Existing KillTimer may be in a different include; inspect the whole tree.
    kills = set()
    for clean in all_clean.values():
        for match in re.finditer(r"\bKillTimer\s*\(\s*([A-Za-z_]\w*)\s*\)", clean):
            kills.add(match[1])

    for path in sorted(texts):
        source, clean = texts[path], all_clean[path]
        for fn in functions(source, clean):
            def report(offset: int, rule: str, message: str):
                line = source.count("\n", 0, fn.start + offset) + 1
                findings.add(Finding(path, line, rule, message))

            arrays = declared_buffers(fn.body, fn.clean)
            for name, pos, args in calls(fn.body, fn.clean):
                prefix = fn.clean[max(0, pos - 110):pos]
                if name in ("fopen", "mysql_query"):
                    assigned = ASSIGNMENT.search(prefix)
                    if assigned:
                        var = assigned[1]
                        close_name = "fclose" if name == "fopen" else "cache_delete"
                        closure = re.search(
                            r"\b" + close_name + r"\s*\(\s*" + re.escape(var) + r"\s*\)",
                            fn.clean[pos:],
                        )
                        ownership = re.search(r"\breturn\s+" + re.escape(var) + r"\s*;", fn.clean[pos:])
                        if not closure and not ownership:
                            report(pos, "RESOURCE_UNRELEASED",
                                   f"{name} assigns {var}, but no {close_name}({var}) is visible in {fn.name}")
                        if closure:
                            section = fn.clean[pos:pos + closure.start()]
                            for ret in RETURN.finditer(section):
                                at = ret.start()
                                snippet = section[max(0, at - 120):]
                                if not is_guarded_failed_handle(snippet, var):
                                    report(pos + at, "RESOURCE_EARLY_RETURN",
                                           f"return before {close_name}({var}) may skip cleanup in {fn.name}")
                                    break
                    elif name == "fopen" and not re.search(r"\breturn\s*$", prefix):
                        report(pos, "RESOURCE_UNTRACKED",
                               "fopen result is not assigned; check handle ownership")

                if name in ("SetTimer", "SetTimerEx") and len(args) >= 3:
                    if args[2].strip() in ("true", "1"):
                        assigned = ASSIGNMENT.search(prefix)
                        if assigned and assigned[1] not in kills:
                            report(pos, "TIMER_UNRELEASED",
                                   f"repeating timer {assigned[1]} has no KillTimer({assigned[1]}) in scanned sources")
                        elif not assigned and not re.search(r"\breturn\s*$", prefix):
                            report(pos, "TIMER_UNTRACKED",
                                   "repeating timer handle is discarded; cancellation cannot be verified")

                if name in ("format", "strcat", "strmid", "fread"):
                    dest_arg = 1 if name == "fread" else 0
                    size_arg = {"format": 1, "strcat": 2, "strmid": 4, "fread": 2}[name]
                    if len(args) <= dest_arg:
                        continue
                    dest = args[dest_arg].strip()
                    base = re.match(r"([A-Za-z_]\w*)", dest)
                    capacity = arrays.get(base[1]) if base else None
                    if name == "format" and len(args) < 3:
                        report(pos, "BUFFER_FORMAT_ARGS", "format requires destination, maximum length and format string")
                    if len(args) > size_arg:
                        limit = args[size_arg].strip()
                        if capacity is not None and NUMBER.fullmatch(limit) and int(limit) > capacity:
                            report(pos, "BUFFER_LENGTH_OVERFLOW",
                                   f"{name} size {limit} exceeds declared {base[1]}[{capacity}]")
                        if name == "format" and limit.startswith("sizeof("):
                            target = re.fullmatch(r"sizeof\s*\(\s*([A-Za-z_]\w*)\s*(?:\[[^)]*\])?\s*\)", limit)
                            if target and base and target[1] != base[1]:
                                report(pos, "BUFFER_WRONG_SIZEOF",
                                       f"format destination {dest} uses sizeof({target[1]})")
                    elif name in ("strcat", "fread") and base and capacity is None:
                        # Array *parameters* may have unknown extents. Native sizeof
                        # defaults are safe for actual local arrays, so do not flag those.
                        if re.search(r"\b" + re.escape(base[1]) + r"\s*\[\s*\]", fn.signature):
                            report(pos, "BUFFER_REVIEW_BOUND",
                                   f"{name} on array parameter {base[1]} has no explicit maximum length")

            if fn.kind == "public":
                returns = list(RETURN.finditer(fn.clean))
                if not returns:
                    report(0, "CALLBACK_NO_RETURN",
                           f"public {fn.name} has no explicit return statement")
                elif re.search(r";\s*$", fn.clean):
                    # Stronger fall-through signal: the last statement is not a return.
                    tail = fn.clean[:].rstrip()
                    semicolon = tail.rfind(";")
                    statement_start = max(tail.rfind(";", 0, semicolon), tail.rfind("}", 0, semicolon))
                    final_stmt = tail[statement_start + 1:]
                    if not re.search(r"\breturn\b", final_stmt):
                        report(max(0, semicolon), "CALLBACK_FALLTHROUGH",
                               f"public {fn.name} may reach its end without return")

                if (ASYNC_CALLBACK.search(fn.name) and
                        re.search(r"\bplayerid\b", fn.signature) and
                        PLAYER_MUTATOR.search(fn.clean) and
                        not re.search(r"\bIsPlayerConnected\s*\(\s*playerid\s*\)", fn.clean)):
                    report(0, "PLAYER_ASYNC_STATE",
                           f"async-like callback {fn.name} changes player state without visible IsPlayerConnected(playerid)")

    return sorted(findings)


def source_tree(root: Path) -> dict[str, str]:
    gamemodes = root / "gamemodes"
    if not gamemodes.is_dir():
        raise FileNotFoundError(f"missing gamemodes source directory: {gamemodes}")
    paths = sorted((*gamemodes.rglob("*.pwn"), *gamemodes.rglob("*.inc")))
    if not paths:
        raise ValueError("no Pawn sources found")
    return {path.relative_to(root).as_posix(): path.read_text(encoding="utf-8")
            for path in paths}


def report_data(findings: list[Finding], scanned: int) -> dict:
    return {
        "schema_version": 1,
        "mode": "advisory",
        "files_scanned": scanned,
        "total_findings": len(findings),
        "by_rule": dict(sorted(Counter(f.rule for f in findings).items())),
        "findings": [asdict(f) for f in findings],
    }


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=ROOT)
    parser.add_argument("--output", type=Path, default=Path("build/pawn_static_report.json"))
    parser.add_argument("--strict", action="store_true", help="fail on any review finding; opt-in only")
    parser.add_argument("--max-annotations", type=int, default=60)
    args = parser.parse_args(argv)
    try:
        sources = source_tree(args.root)
        issues = analyze_sources(sources)
        result = report_data(issues, len(sources))
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(json.dumps(result, indent=2, ensure_ascii=False) + "\n",
                               encoding="utf-8")
    except (OSError, UnicodeError, ValueError) as error:
        print(f"Pawn static analyzer failed: {error}", file=sys.stderr)
        return 2

    for finding in issues[:max(0, args.max_annotations)]:
        detail = finding.message.replace("%", "%25").replace("\r", "%0D").replace("\n", "%0A")
        print(f"::warning file={finding.path},line={finding.line},title={finding.rule}::{detail}")
    print(f"Pawn static review: {len(sources)} files, {len(issues)} possible issues, "
          f"report: {args.output} (advisory; not proven leaks)")
    for rule, count in result["by_rule"].items():
        print(f"  {rule}: {count}")
    if len(issues) > args.max_annotations:
        print(f"  {len(issues) - args.max_annotations} further findings in JSON report")
    return 1 if args.strict and issues else 0


if __name__ == "__main__":
    raise SystemExit(main())
