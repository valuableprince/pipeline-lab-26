#!/usr/bin/env python3
"""Export editable UTF-8 PayDay phrases to a bounded CP1251 scriptfiles asset.

open.mp reads file strings as raw bytes, and the unmodified SA-MP chat uses CP1251.
The source in Git is UTF-8; only the generated runtime file must be CP1251.
"""
from __future__ import annotations

import argparse
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "data/payday/levelup_phrases.txt"
OUTPUT = ROOT / "build/scriptfiles/payday_levelup.txt"
MAX_COUNT = 64
MAX_BYTES = 120
CHAT_PREFIX_BYTES = 8  # "{9FAAB8}" before the selected phrase.
CHAT_MAX_BYTES = 144


def parse_phrases(source: str) -> list[str]:
    phrases: list[str] = []
    seen: set[str] = set()
    for number, raw in enumerate(source.splitlines(), start=1):
        line = raw.strip()
        if not line or line.startswith("#"):
            continue
        if any(ord(char) < 32 or ord(char) == 127 for char in line):
            raise ValueError(f"Line {number}: control characters are forbidden")
        if "{" in line or "}" in line:
            raise ValueError(f"Line {number}: inline color tags are not allowed")
        try:
            encoded = line.encode("cp1251", errors="strict")
        except UnicodeEncodeError as error:
            raise ValueError(f"Line {number}: unsupported CP1251 character") from error
        if len(encoded) > MAX_BYTES or len(encoded) + CHAT_PREFIX_BYTES > CHAT_MAX_BYTES:
            raise ValueError(f"Line {number}: phrase exceeds the chat limit")
        if len(encoded) < 10:
            raise ValueError(f"Line {number}: phrase is too short")
        if line.casefold() in seen:
            raise ValueError(f"Line {number}: duplicate phrase")
        seen.add(line.casefold())
        phrases.append(line)
    if not phrases:
        raise ValueError("Phrase library must contain at least one phrase")
    if len(phrases) > MAX_COUNT:
        raise ValueError(f"Maximum {MAX_COUNT} phrases are supported")
    return phrases


def export(source_path: Path = SOURCE, output_path: Path = OUTPUT) -> int:
    phrases = parse_phrases(source_path.read_text(encoding="utf-8-sig"))
    blob = ("\n".join(phrases) + "\n").encode("cp1251", errors="strict")
    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_bytes(blob)
    return len(phrases)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, default=SOURCE)
    parser.add_argument("--output", type=Path, default=OUTPUT)
    args = parser.parse_args()
    try:
        count = export(args.source, args.output)
    except (OSError, UnicodeError, ValueError) as error:
        parser.error(str(error))
    print(f"PayDay: exported {count} CP1251 phrases to {args.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
