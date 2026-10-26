#!/usr/bin/env python3
"""Run production editor logic with mocked UI/players in a local open.mp process.

Requires an unpacked official open.mp SDK. No client, DB or game plugins needed.
The server binds only to loopback and never announces to a master server.
"""
import argparse
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


def run(server: Path, prefix: list[str], output: Path, harness="tdstudio_harness.pwn", mysql_config=None, mysql_plugin=None, restart=False) -> None:
    server = server.resolve()
    output.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="tdstudio-test-") as temporary:
        work = Path(temporary)
        stage = work / "stage"
        stage.mkdir()
        for module in (ROOT / "gamemodes/includes").glob("tdstudio*.inc"):
            (stage / module.name).write_bytes(module.read_text(encoding="utf-8").encode("cp1251"))
        for helper in (ROOT / "tests/pawn").glob("*.inc"):
            (stage / helper.name).write_bytes(helper.read_text(encoding="utf-8").encode("cp1251"))
        source = stage / "tdstudio_harness.pwn"
        source.write_bytes(("#define TDS_TEST_RESTART\n" if restart else "").encode() + (ROOT / "tests/pawn" / harness).read_text(encoding="utf-8").encode("cp1251"))
        (work / "gamemodes").mkdir()
        target = work / "gamemodes/tdstudio_harness.amx"
        child_env = os.environ.copy()
        child_env["LD_LIBRARY_PATH"] = os.pathsep.join((str(server / "qawno"), str(work), child_env.get("LD_LIBRARY_PATH", "")))
        compilation = subprocess.run(prefix + [str(server / "qawno/pawncc"), "-d3",
            f"-i{server / 'qawno/include'}", f"-i{stage}", f"-o{target}", str(source)],
            cwd=work, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=45, env=child_env)
        log = compilation.stdout.decode("utf-8", errors="replace")
        (output / "tdstudio-test-compile.log").write_text(log, encoding="utf-8")
        if compilation.returncode or analyze_pawn_output(log) or not target.exists() or not target.stat().st_size:
            raise RuntimeError(f"Harness compilation failed:\n{log}")
        shutil.copytree(server / "components", work / "components")
        (work / "scriptfiles").mkdir()
        if mysql_config and mysql_plugin:
            shutil.copy2(mysql_config,work / "mysql.ini")
            (work / "plugins").mkdir()
            shutil.copy2(mysql_plugin / "plugins/mysql.so",work / "plugins/mysql.so")
            shutil.copy2(mysql_plugin / "log-core.so",work / "log-core.so")
        with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as listener:
            listener.bind(("127.0.0.1", 0))
            port = listener.getsockname()[1]
        config = {
            "announce": False, "enable_query": False, "name": "TD Studio isolated logic tests",
            "max_players": 16, "max_bots": 0, "artwork": {"enable": False},
            "network": {"bind": "127.0.0.1", "port": port},
            "pawn": {"legacy_plugins": ["mysql"] if mysql_config else [], "main_scripts": ["tdstudio_harness 1"], "side_scripts": []},
            "rcon": {"enable": False, "password": "local-test-no-network-rcon"},
        }
        (work / "config.json").write_text(json.dumps(config), encoding="utf-8")
        # Stop the fixture after the completed assertion report. TDS_Shutdown is
        # exercised inside Pawn before this marker; engine/process teardown is
        # outside this test's mocked-player scope.
        process = subprocess.Popen(prefix + [str(server / "omp-server")], cwd=work,
            stdout=subprocess.PIPE, stderr=subprocess.STDOUT, stdin=subprocess.DEVNULL, env=child_env)
        watchdog = threading.Timer(45, process.kill)
        watchdog.start()
        chunks = []
        stopped_after_report = False
        try:
            for line in process.stdout:
                chunks.append(line)
                if b"TDS TEST RESULT:" in line:
                    stopped_after_report = True
                    process.kill()
                    break
            chunks.append(process.communicate(timeout=5)[0])
        finally:
            watchdog.cancel()
            if process.poll() is None:
                process.kill()
                process.wait(timeout=5)
        log = b"".join(chunks).decode("utf-8", errors="replace")
        (output / "tdstudio-test-run.log").write_text(log, encoding="utf-8")
        marker = re.search(r"TDS TEST RESULT: checks=(\d+) failures=(\d+)", log)
        if not stopped_after_report or process.returncode not in (0, -9) or not marker or int(marker[1]) < 50 or int(marker[2]) or "TDS FAIL:" in log:
            raise RuntimeError(f"Editor logic tests failed:\n{log}")
        if harness != "tdstudio_harness.pwn":
            from test_tdstudio_stage2 import compile_exports
            compile_exports(work / "scriptfiles/tdstudio_exports", output, server, prefix)
        print(f"TD Studio: {marker[1]} Pawn assertions passed (mocked players/UI; no GTA visual test).")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--server", type=Path, required=True, help="SDK directory containing omp-server and qawno")
    parser.add_argument("--prefix", default="", help="Optional command prefix, e.g. a 32-bit emulator")
    parser.add_argument("--output", type=Path, default=ROOT / "build")
    args = parser.parse_args()
    run(args.server, shlex.split(args.prefix), args.output.resolve())


if __name__ == "__main__":
    main()
