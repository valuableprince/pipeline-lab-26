#!/usr/bin/env python3
"""Fail closed on unexpected tracked paths, suspicious file types, and invalid payload header.

This only checks the current tree. It cannot undo data disclosed by prior pushes.
"""
from pathlib import Path
import subprocess
import sys

ALLOWED = {
    ".gitignore",
    ".github/workflows/validate.yml",
    "tools/verify_public_tree.py",
    "tools/pack.sh",
    "tools/check-local.sh",
    "tools/import-local.sh",
    "tools/test_public_tree.py",
}
PAYLOAD = "payload/release.age"
MAX_PAYLOAD = 25 * 1024 * 1024

def main() -> int:
    tracked = subprocess.check_output(["git", "ls-files", "-z"]).decode().split("\0")
    tracked = [p for p in tracked if p]
    unexpected = [p for p in tracked if p not in ALLOWED and p != PAYLOAD]
    if unexpected:
        print("Unexpected tracked paths:", *unexpected, sep="\n", file=sys.stderr)
        return 1
    for name in tracked:
        path = Path(name)
        if not path.is_file() or path.is_symlink():
            print(f"Missing, non-file or symlink: {name}", file=sys.stderr)
            return 1
    path = Path(PAYLOAD)
    if path.exists():
        if not path.is_file() or path.is_symlink():
            print("Payload must be a regular file", file=sys.stderr)
            return 1
        size = path.stat().st_size
        if not 100 <= size <= MAX_PAYLOAD:
            print("Payload size outside accepted range", file=sys.stderr)
            return 1
        with path.open("rb") as stream:
            header = stream.read(22)
        if header != b"age-encryption.org/v1\n":
            print("Invalid encrypted payload header", file=sys.stderr)
            return 1
    print("Public tree structure check passed")
    return 0

if __name__ == "__main__":
    sys.exit(main())
