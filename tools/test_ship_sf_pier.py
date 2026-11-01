#!/usr/bin/env python3
"""Execute the production Pawn scene, inspect geometry and run its native export.

Requires the official open.mp SDK; runs a private loopback server with no DB/GTA
client. Default: streamer doubles for deterministic failures. --streamer-plugin:
real v2.9.6 natives. Neither mode verifies rendering or connected-player streaming.
"""
import argparse
import importlib.util
import json
import os
from pathlib import Path
import re
import shlex
import shutil
import socket
import subprocess
import tempfile
import threading

from check_pawn_output import analyze_pawn_output

ROOT = Path(__file__).resolve().parents[1]


def run(server: Path, prefix: list[str], output: Path, streamer_plugin: Path | None = None):
    server = server.resolve()
    output.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="ship-sf-pier-") as temporary:
        work = Path(temporary)
        stage = work / "stage"
        stage.mkdir()
        for name in ("ship_sf_pier_map.inc", "streamer.inc"):
            (stage / name).write_bytes((ROOT / "gamemodes/includes" / name).read_text("utf-8").encode("cp1251"))
        source = stage / "ship_sf_pier_harness.pwn"
        harness = "ship_sf_pier_streamer_harness.pwn" if streamer_plugin else "ship_sf_pier_harness.pwn"
        source.write_bytes((ROOT / "tests/pawn" / harness).read_text("utf-8").encode("cp1251"))
        (work / "gamemodes").mkdir()
        target = work / "gamemodes/ship_sf_pier_harness.amx"
        env = os.environ.copy()
        env["LD_LIBRARY_PATH"] = os.pathsep.join((str(server / "qawno"), env.get("LD_LIBRARY_PATH", "")))

        def compile_pawn(path, name, expected_error=None):
            result = subprocess.run(prefix + [str(server / "qawno/pawncc"),
                f"-i{server}/qawno/include", f"-i{stage}", f"-o{target}", str(path)],
                cwd=work, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=60)
            log = result.stdout.decode(errors="replace")
            (output / f"ship-sf-pier-{name}-compile.log").write_text(log, encoding="utf-8")
            if expected_error:
                if result.returncode == 0 or expected_error not in log:
                    raise RuntimeError(f"Expected preview guard {expected_error}:\n{log}")
            elif result.returncode or analyze_pawn_output(log) or not target.is_file():
                raise RuntimeError(f"Pier compilation failed:\n{log}")

        # Compile every deployment mode; refuse an unset origin or unsurveyed live
        # deployment. Only the preview variant is executed below.
        content = source.read_bytes()
        for mode in (0, 2):
            variant = stage / f"mode_{mode}.pwn"
            variant.write_bytes((b"#define SF_PIER_SITE_VERIFIED (1)\n" if mode == 2 else b"") +
                                content.replace(b"#define SF_PIER_MAP_MODE (1)", f"#define SF_PIER_MAP_MODE ({mode})".encode()))
            compile_pawn(variant, f"mode-{mode}")
        for label, replacement, message in (
            ("unconfigured", content.replace(b"#define SF_PIER_SITE_CONFIGURED (1)", b"#define SF_PIER_SITE_CONFIGURED (0)"), "Configure SF_PIER_BASE"),
            ("unverified", content.replace(b"#define SF_PIER_MAP_MODE (1)", b"#define SF_PIER_MAP_MODE (2)"), "NEEDS_IN_GAME_SURVEY"),
            ("missing-origin", content.replace(b"#define SF_PIER_BASE_X (100.0)", b""), "Explicit SF_PIER_BASE"),
        ):
            variant = stage / f"{label}.pwn"
            variant.write_bytes(replacement)
            compile_pawn(variant, label, message)
        compile_pawn(source, "preview")
        shutil.copytree(server / "components", work / "components")
        (work / "scriptfiles").mkdir()
        if streamer_plugin:
            (work / "plugins").mkdir()
            shutil.copy2(streamer_plugin, work / "plugins/streamer.so")
        with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as listener:
            listener.bind(("127.0.0.1", 0))
            port = listener.getsockname()[1]
        (work / "config.json").write_text(json.dumps({
            "announce": False, "enable_query": False, "name": "Private SF pier logic tests",
            "max_players": 2, "max_bots": 0, "artwork": {"enable": False},
            "network": {"bind": "127.0.0.1", "port": port},
            "pawn": {"legacy_plugins": ["streamer"] if streamer_plugin else [],
                     "main_scripts": ["ship_sf_pier_harness 1"], "side_scripts": []},
            "rcon": {"enable": False, "password": "local-pier-test-only"},
        }), encoding="utf-8")
        def execute(name):
            process = subprocess.Popen(prefix + [str(server / "omp-server")], cwd=work, env=env,
                stdout=subprocess.PIPE, stderr=subprocess.STDOUT, stdin=subprocess.DEVNULL)
            watchdog = threading.Timer(60, process.kill)
            watchdog.start()
            chunks = []
            try:
                for line in process.stdout:
                    chunks.append(line)
                    if b"PIER TEST RESULT:" in line:
                        process.kill()
                        break
                chunks.append(process.communicate(timeout=5)[0])
            finally:
                watchdog.cancel()
                if process.poll() is None:
                    process.kill()
                    process.wait(timeout=5)
            log = b"".join(chunks).decode(errors="replace")
            (output / f"ship-sf-pier-{name}.log").write_text(log, encoding="utf-8")
            result = re.search(r"PIER TEST RESULT: checks=(\d+) failures=(\d+)", log)
            if process.returncode not in (0, -9) or not result or int(result[2]) or "PIER FAIL:" in log or int(result[1]) < 1000:
                raise RuntimeError(f"Pier {name} tests failed:\n{log}")
            return result

        result = execute("run")
        scene = work / "scriptfiles/ship_sf_pier_scene.csv"
        spec = importlib.util.spec_from_file_location("pier_acceptance", ROOT / "tests/test_ship_sf_pier.py")
        acceptance = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(acceptance)
        rows = acceptance.read_scene(scene)
        acceptance.validate_scene(rows)
        shutil.copy2(scene, output / "ship_sf_pier_scene.csv")
        export = work / "scriptfiles/ship_sf_pier/preview_objects.inc"
        # Compile the actual exported Pawn, with real streamer declarations.
        exported_source = stage / "exported.pwn"
        exported_source.write_bytes(b'#include <open.mp>\n#include "streamer.inc"\nmain() {}\npublic OnGameModeInit() {\n' +
            export.read_bytes() + b'\nreturn 1;\n}\n')
        compile_pawn(exported_source, "export")
        shutil.copy2(export, output / "ship_sf_pier_preview_objects.inc")
        editor = work / "scriptfiles/ship_sf_pier/map_editor.pwn"
        shutil.copy2(editor, output / "ship_sf_pier_map_editor.pwn")
        acceptance.validate_editor_export(editor.read_text("cp1251"), rows)
        compile_pawn(editor, "editor-disabled")
        editor_enabled = stage / "map_editor_enabled.pwn"
        editor_enabled.write_bytes(editor.read_bytes().replace(
            b"#define SF_PIER_EDITOR_PREVIEW_ENABLED (0)", b"#define SF_PIER_EDITOR_PREVIEW_ENABLED (1)"))
        compile_pawn(editor_enabled, "editor-enabled")
        editor_harness = stage / "ship_sf_pier_editor_harness.pwn"
        editor_harness.write_bytes((ROOT / "tests/pawn/ship_sf_pier_editor_harness.pwn").read_text("utf-8").encode("cp1251"))
        compile_pawn(editor_harness, "editor-runtime")
        editor_result = execute("editor-run")
        (output / "ship_sf_pier_preview_README.txt").write_text(
            "SHIP-MAP-01 / Tarkenso RolePlay Game\n\n"
            "NEEDS_IN_GAME_SURVEY. Файлы созданы с тестовой привязкой\n"
            "BASE=(100,200,30), ROT=90 градусов. Это не измеренные координаты в SF.\n\n"
            "ship_sf_pier_map_editor.pwn: буквальные CreateObject и SetObjectMaterial\n"
            "для импорта в Map Editor. Автосоздание на сервере отключено по умолчанию.\n"
            "При явном включении отдельный filterscript очищает только свои объекты.\n"
            "ship_sf_pier_preview_objects.inc: фрагмент CreateDynamicObject для теста.\n"
            "ship_sf_pier_scene.csv: локальные и мировые pivots объектов.\n\n"
            "Не загружать экспорт одновременно с основным модулем пирса.\n"
            "Для фактической привязки задать измеренные BASE_X/Y/Z/ROT, включить\n"
            "превью на тестовом сервере и вызвать ShipSFPierMap_Export(true).\n"
            "Порядок привязки и проверок: docs/SHIP_SF_PIER_MAP_01.md.\n",
            encoding="utf-8")
        mode = "real streamer" if streamer_plugin else "streamer doubles"
        print(f"SF pier ({mode}): {result[1]} Pawn assertions passed; 496-object geometry and both exports passed.")
        print(f"Map Editor export: {editor_result[1]} native-engine assertions passed. No GTA visual test.")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--server", type=Path, required=True)
    parser.add_argument("--prefix", default="", help="Optional compiler/server execution prefix")
    parser.add_argument("--output", type=Path, default=ROOT / "build")
    parser.add_argument("--streamer-plugin", type=Path, help="Run native integration against a pinned streamer.so")
    args = parser.parse_args()
    run(args.server, shlex.split(args.prefix), args.output.resolve(), args.streamer_plugin)
