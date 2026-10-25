#!/usr/bin/env python3
"""Install the pinned open.mp-compatible rustext server plugin (Python 3)."""
from __future__ import annotations

import argparse
import hashlib
import io
import json
import os
from pathlib import Path
import shutil
import sys
import tempfile
from urllib.request import urlopen
from zipfile import ZipFile

VERSION = "v2.0.11"
URL = "https://github.com/ziggi/rustext/releases/download/v2.0.11/rustext-2.0.11-nomemhack.zip"
SHA256 = "b51e5d55492ff5e249665270333342131070cd0d171f43ad9893a8d563b2638b"
MAX_BYTES = 5 * 1024 * 1024


def updated_config(data):
    if not isinstance(data, dict):
        raise ValueError("config.json must contain an object")
    data = dict(data)
    pawn = data.get("pawn", {})
    if not isinstance(pawn, dict):
        raise ValueError("pawn must be an object")
    pawn = dict(pawn)
    plugins = pawn.get("legacy_plugins", [])
    if isinstance(plugins, str):
        plugins = plugins.split()
    if not isinstance(plugins, list) or any(not isinstance(p, str) for p in plugins):
        raise ValueError("pawn.legacy_plugins must be a string or string list")
    plugins = list(plugins)
    if not any(Path(p).stem.lower() == "rustext" for p in plugins):
        plugins.append("rustext")
    pawn["legacy_plugins"] = plugins
    data["pawn"] = pawn
    return data


def extract_library(archive, platform):
    suffix = ".so" if platform == "linux" else ".dll"
    with ZipFile(io.BytesIO(archive)) as zf:
        files = [item for item in zf.infolist()
                 if not item.is_dir()
                 and Path(item.filename.replace("\\", "/")).name.lower() == "rustext" + suffix]
        if len(files) != 1 or not (0 < files[0].file_size <= MAX_BYTES):
            raise ValueError("Expected exactly one valid rustext" + suffix)
        return zf.read(files[0])


def download():
    with urlopen(URL, timeout=30) as response:
        blob = response.read(MAX_BYTES + 1)
    if len(blob) > MAX_BYTES or hashlib.sha256(blob).hexdigest() != SHA256:
        raise ValueError("rustext release SHA256 verification failed")
    return blob


def atomic_write(path, data):
    path.parent.mkdir(parents=True, exist_ok=True)
    fd, temp = tempfile.mkstemp(prefix=".rustext-", dir=path.parent)
    try:
        with os.fdopen(fd, "wb") as out:
            out.write(data)
            out.flush()
            os.fsync(out.fileno())
        os.replace(temp, path)
    finally:
        if os.path.exists(temp):
            os.unlink(temp)


def install(server, platform):
    config_path = server / "config.json"
    if not config_path.is_file():
        raise ValueError("Missing open.mp config.json at " + str(config_path))
    updated = updated_config(json.loads(config_path.read_text(encoding="utf-8-sig")))
    library_bytes = extract_library(download(), platform)
    filename = "rustext.so" if platform == "linux" else "rustext.dll"
    library_path = server / "plugins" / filename
    if library_path.is_file():
        shutil.copy2(library_path, library_path.with_suffix(library_path.suffix + ".before-rustext.bak"))
    shutil.copy2(config_path, config_path.with_name("config.json.before-rustext.bak"))
    atomic_write(library_path, library_bytes)
    atomic_write(config_path, (json.dumps(updated, ensure_ascii=False, indent=2) + "\n").encode("utf-8"))
    return library_path


def main():
    parser = argparse.ArgumentParser(description="Install SHA256-pinned rustext v2.0.11 nomemhack")
    parser.add_argument("--server", type=Path, required=True, help="Path to open.mp server root")
    parser.add_argument("--platform", choices=("windows", "linux"),
                        default="windows" if sys.platform == "win32" else "linux")
    args = parser.parse_args()
    try:
        path = install(args.server.expanduser().resolve(), args.platform)
    except (ValueError, OSError, EOFError) as error:
        print("rustext install failed: " + str(error), file=sys.stderr)
        return 1
    print("Installed:", path)
    print("Enabled rustext in config.json / pawn.legacy_plugins")
    print("Restart open.mp. Check the console log for rustext 2.0.11.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
