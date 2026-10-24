#!/usr/bin/env python3
"""Safely extract a tar stream into a newly created local directory."""
import os
from pathlib import Path, PurePosixPath
import shutil
import sys
import tarfile

MAX_FILES = 5000
MAX_BYTES = 250 * 1024 * 1024

def extract(stream, target):
    destination = Path(target).resolve(strict=True)
    total = 0
    count = 0
    with tarfile.open(fileobj=stream, mode="r|") as archive:
        for member in archive:
            name = member.name
            path = PurePosixPath(name)
            if (not name or name.startswith("/") or "\\" in name or
                    ".." in path.parts or any(":" in p for p in path.parts)):
                raise ValueError("Unsafe archive path")
            if not member.isdir() and not member.isfile():
                raise ValueError("Unsupported archive entry (links/devices not allowed)")
            count += 1
            total += member.size
            if count > MAX_FILES or total > MAX_BYTES:
                raise ValueError("Archive exceeds safety limits")
            parts = [p for p in path.parts if p not in (".", "")]
            output = destination.joinpath(*parts)
            if not output.resolve().is_relative_to(destination):
                raise ValueError("Archive path escapes destination")
            if member.isdir():
                output.mkdir(parents=True, exist_ok=True)
            else:
                output.parent.mkdir(parents=True, exist_ok=True)
                source = archive.extractfile(member)
                if source is None:
                    raise ValueError("Could not read archive file")
                with source, output.open("xb") as target_stream:
                    remaining = member.size
                    while remaining:
                        chunk = source.read(min(remaining, 1024 * 1024))
                        if not chunk:
                            raise ValueError("Truncated archive file")
                        target_stream.write(chunk)
                        remaining -= len(chunk)
    return count

if __name__ == "__main__":
    if len(sys.argv) != 2:
        sys.exit("Usage: safe_extract.py DESTINATION_DIRECTORY")
    try:
        extracted = extract(sys.stdin.buffer, sys.argv[1])
        print(f"Extracted {extracted} entries into the local workspace")
    except (OSError, ValueError, tarfile.TarError) as exc:
        sys.exit(f"Extraction refused: {exc}")
