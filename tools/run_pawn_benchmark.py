#!/usr/bin/env python3
"""Run the same isolated open.mp AMX harness against base and PR Pawn sources.

Measured values are elapsed milliseconds on a shared GitHub runner, not CPU
cycles or production tickrate. Noise gating and minimum absolute delta keep
one jittery sample from being declared a regression. Always advisory.
"""
from __future__ import annotations

import argparse
import json
import os
from pathlib import Path
import re
import shutil
import socket
import statistics
import subprocess
import tempfile
import threading

from check_pawn_output import analyze_pawn_output

ROOT = Path(__file__).resolve().parents[1]
SHIP = "gamemodes/includes/ship_notice.inc"
HARNESS = ROOT / "tests/pawn/perf_ship_harness.pwn"
PATTERN = re.compile(r"PERF_SAMPLE:name=(\w+),pass=(\d+),ms=(\d+),checksum=(-?\d+)")
DONE = re.compile(r"PERF_DONE:cases=2,samples=7")
CASES = ("distance", "voyage")
PASSES = 7


def summarize(samples: list[int]) -> dict:
    if len(samples) != PASSES or any(ms < 0 for ms in samples):
        raise ValueError("benchmark samples are incomplete or invalid")
    median = statistics.median(samples)
    dispersion = statistics.median(abs(ms - median) for ms in samples)
    return {"samples_ms": samples, "median_ms": median,
            "mad_ms": dispersion,
            "noisy": median <= 0 or dispersion > max(3, median * 0.15)}


def compare(baseline: dict, current: dict) -> dict:
    if baseline["checksum"] != current["checksum"]:
        return {"status": "incomparable", "reason": "algorithm checksum changed; inspect results"}
    b, c = baseline["median_ms"], current["median_ms"]
    if b < 3 or baseline["noisy"] or current["noisy"]:
        return {"status": "noisy", "reason": "small or unstable time measurements"}
    change = 100.0 * (c - b) / b
    return {"status": "regression" if change >= 20.0 and (c - b) >= 3 else "ok",
            "change_percent": round(change, 2),
            "reason": "advisory threshold 20% plus 3 ms absolute"}


def parse_run(log: str) -> dict[str, dict]:
    if not DONE.search(log):
        raise ValueError("AMX test did not print PERF_DONE")
    by_name: dict[str, dict[int, tuple[int, int]]] = {name: {} for name in CASES}
    for name, seq, ms, checksum in PATTERN.findall(log):
        if name not in by_name:
            raise ValueError("unexpected benchmark case: " + name)
        index = int(seq)
        if index not in range(PASSES) or index in by_name[name]:
            raise ValueError(f"duplicate or invalid sample {name}/{index}")
        by_name[name][index] = (int(ms), int(checksum))
    result = {}
    for name, rows in by_name.items():
        if set(rows) != set(range(PASSES)):
            raise ValueError(f"missing AMX samples for {name}")
        checksums = {value[1] for value in rows.values()}
        if len(checksums) != 1:
            raise ValueError(f"unstable algorithm checksum for {name}")
        result[name] = {**summarize([rows[i][0] for i in range(PASSES)]),
                        "checksum": checksums.pop()}
    return result


def git_show(ref: str) -> bytes:
    # Git ref is an argv argument, never interpolated into a shell command.
    result = subprocess.run(["git", "show", f"{ref}:{SHIP}"], cwd=ROOT,
                            stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=20)
    if result.returncode or not result.stdout:
        raise RuntimeError(f"Cannot read {SHIP} from {ref}: " +
                           result.stderr.decode(errors="replace")[:250])
    return result.stdout


def allocate_udp_port() -> int:
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
        sock.bind(("127.0.0.1", 0))
        return sock.getsockname()[1]


def run_once(server: Path, source: bytes, label: str, output: Path) -> dict:
    with tempfile.TemporaryDirectory(prefix=f"tarkenso-perf-{label}-") as tmp:
        work = Path(tmp)
        stage = work / "stage"
        stage.mkdir()
        (stage / "ship_notice.inc").write_bytes(source.decode("utf-8").encode("cp1251"))
        pawn = stage / HARNESS.name
        pawn.write_bytes(HARNESS.read_text(encoding="utf-8").encode("cp1251"))
        (work / "gamemodes").mkdir()
        target = work / "gamemodes" / "perf_ship_harness.amx"
        env = os.environ.copy()
        env["LD_LIBRARY_PATH"] = os.pathsep.join((
            str(server / "qawno"), str(work), env.get("LD_LIBRARY_PATH", "")
        ))
        cmd = [str(server / "qawno/pawncc"), "-d3",
               f"-i{server / 'qawno/include'}", f"-i{stage}",
               f"-o{target}", str(pawn)]
        build = subprocess.run(cmd, cwd=work, stdout=subprocess.PIPE,
                               stderr=subprocess.STDOUT, env=env, timeout=45)
        compiler_log = build.stdout.decode("utf-8", errors="replace")
        (output / f"perf-{label}-compile.log").write_text(compiler_log, encoding="utf-8")
        if build.returncode or analyze_pawn_output(compiler_log) or not target.is_file() or target.stat().st_size == 0:
            raise RuntimeError(f"Performance AMX {label} compile failed:\n{compiler_log[:3500]}")

        shutil.copytree(server / "components", work / "components")
        (work / "scriptfiles").mkdir()
        config = {
            "announce": False, "enable_query": False,
            "name": "TARKENSO isolated AMX performance",
            "max_players": 4, "max_bots": 0,
            "artwork": {"enable": False},
            "network": {"bind": "127.0.0.1", "port": allocate_udp_port()},
            "pawn": {"legacy_plugins": [], "main_scripts": ["perf_ship_harness 1"],
                     "side_scripts": []},
            "rcon": {"enable": False, "password": "local-perf-only"},
        }
        (work / "config.json").write_text(json.dumps(config), encoding="utf-8")
        process = subprocess.Popen([str(server / "omp-server")], cwd=work,
                                   stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                   stdin=subprocess.DEVNULL, env=env)
        watchdog = threading.Timer(50, process.kill)
        watchdog.start()
        chunks, done = [], False
        try:
            assert process.stdout is not None
            for line in process.stdout:
                chunks.append(line)
                if b"PERF_DONE:" in line:
                    done = True
                    process.kill()
                    break
            chunks.append(process.communicate(timeout=5)[0])
        finally:
            watchdog.cancel()
            if process.poll() is None:
                process.kill()
                process.wait(timeout=5)
        log = b"".join(chunks).decode("utf-8", errors="replace")
        (output / f"perf-{label}-run.log").write_text(log, encoding="utf-8")
        if not done or process.returncode not in (0, -9) or "PERF_ERROR:" in log:
            raise RuntimeError(f"Performance AMX {label} failed:\n{log[-3500:]}")
        return parse_run(log)


def run(server: Path, baseline_ref: str, output: Path) -> dict:
    server = server.resolve()
    output.mkdir(parents=True, exist_ok=True)
    if not HARNESS.is_file() or not (server / "omp-server").is_file():
        raise RuntimeError("missing Pawn benchmark harness or open.mp SDK")
    baseline_source = git_show(baseline_ref)
    candidate_source = (ROOT / SHIP).read_bytes()
    # Running baseline and candidate on the SAME runner is much less noisy
    # than comparing separate GitHub VMs; timing remains approximate.
    base = run_once(server, baseline_source, "base", output)
    current = run_once(server, candidate_source, "head", output)
    cases = {name: {"baseline": base[name], "candidate": current[name],
                    "comparison": compare(base[name], current[name])} for name in CASES}
    result = {"schema_version": 1, "mode": "advisory",
              "baseline_ref": baseline_ref, "source": SHIP, "cases": cases}
    (output / "pawn_perf_bench.json").write_text(
        json.dumps(result, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    for name in CASES:
        item = cases[name]
        outcome = item["comparison"]
        print(f"Pawn AMX benchmark {name}: base={item['baseline']['median_ms']}ms "
              f"head={item['candidate']['median_ms']}ms; {outcome['status']}: {outcome['reason']}")
        if outcome["status"] == "regression":
            print("::warning title=Pawn AMX performance regression::"
                  f"{name}: {outcome['change_percent']}% slower than {baseline_ref} "
                  "(advisory, not a production tickrate claim)")
    return result


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--server", type=Path, required=True)
    parser.add_argument("--baseline-ref", default="origin/main")
    parser.add_argument("--output", type=Path, default=ROOT / "build")
    args = parser.parse_args(argv)
    try:
        run(args.server, args.baseline_ref, args.output.resolve())
    except (OSError, RuntimeError, ValueError, subprocess.TimeoutExpired, UnicodeError) as error:
        parser.error(str(error))
    return 0  # regressions are advisory until a stable baseline exists


if __name__ == "__main__":
    raise SystemExit(main())
