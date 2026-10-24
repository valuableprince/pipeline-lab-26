#!/usr/bin/env python3
"""Validate that the public tree contains only approved infrastructure and opaque payloads."""
from pathlib import Path
import subprocess
import sys

ALLOWED = {
    ".gitignore",
    ".github/workflows/validate.yml",
    "tools/verify_public_tree.py",
    "tools/pack.sh",
}
PAYLOAD = "payload/release.age"

def main() -> int:
    tracked = subprocess.check_output(
        ["git", "ls-files", "-z"], stderr=subprocess.DEVNULL
    ).decode("utf-8").split("\0")
    tracked = [path for path in tracked if path]
    unexpected = [path for path in tracked if path not in ALLOWED and path != PAYLOAD]
    if unexpected:
        print("Disallowed tracked paths:", *unexpected, sep="\n", file=sys.stderr)
        return 1
    path = Path(PAYLOAD)
    if path.exists():
        data = path.read_bytes()
        if not data.startswith(b"age-encryption.org/v1\n"):
            print("Invalid encrypted payload header", file=sys.stderr)
            return 1
        if len(data) < 100:
            print("Encrypted payload too small", file=sys.stderr)
            return 1
    print("Public tree validation passed")
    return 0

if __name__ == "__main__":
    sys.exit(main())
