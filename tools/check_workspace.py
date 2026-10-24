#!/usr/bin/env python3
"""Conservative local checks before producing an encrypted snapshot."""
from pathlib import Path
import sys

BLOCKED = {".git", ".env", ".ssh", ".aws", ".npmrc", ".pypirc", "id_rsa",
           "id_ed25519", "identity.txt", "secrets", "node_modules", "__pycache__"}
BLOCKED_SUFFIXES = {".pem", ".key", ".p12", ".pfx"}
LIMIT_BYTES = 250 * 1024 * 1024
LIMIT_FILES = 5000

def check(root):
    folder = Path(root)
    if not folder.is_dir() or folder.is_symlink():
        raise ValueError("Workspace missing or invalid")
    count, total = 0, 0
    for p in folder.rglob("*"):
        relative = p.relative_to(folder)
        if p.is_symlink():
            raise ValueError("Symlink not permitted in local workspace")
        if any(part.lower() in BLOCKED or part.lower().startswith(".env.")
               for part in relative.parts):
            raise ValueError("Potential private or temporary path in workspace")
        if p.suffix.lower() in BLOCKED_SUFFIXES:
            raise ValueError("Potential secret file in workspace")
        if p.is_file():
            count += 1
            total += p.stat().st_size
            if count > LIMIT_FILES or total > LIMIT_BYTES:
                raise ValueError("Workspace exceeds size limits")
    if count == 0:
        raise ValueError("Workspace has no files")
    return count, total

if __name__ == "__main__":
    try:
        if len(sys.argv) != 2:
            raise ValueError("Usage: check_workspace.py DIRECTORY")
        count, total = check(sys.argv[1])
        print(f"Workspace checked: {count} files, {total} bytes")
    except ValueError as exc:
        sys.exit(f"Workspace check failed: {exc}")
