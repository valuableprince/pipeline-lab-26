#!/usr/bin/env python3
"""Paired, isolated AMX scaling tests; real production functions with mocked I/O.

This does NOT make 500 TCP/UDP-connected clients or invoke MySQL. The workload
uses 100/250/500 synthetic player records inside the actual Pawn interpreter.
The organizations workload counts operations on four rosters (max 64 each).
"""
from __future__ import annotations

import argparse
import json
import os
from pathlib import Path
import re
import shutil
import socket
import subprocess
import tempfile
import threading

try:
    from tools.check_pawn_static import closing, mask_opaque
    from tools.check_pawn_output import analyze_pawn_output
    from tools.run_pawn_benchmark import compare, summarize
except ModuleNotFoundError:
    from check_pawn_static import closing, mask_opaque
    from check_pawn_output import analyze_pawn_output
    from run_pawn_benchmark import compare, summarize

ROOT = Path(__file__).resolve().parents[1]
HARNESS = ROOT / "tests/pawn/perf_scaling_harness.pwn"
SOURCES = (
    "gamemodes/includes/payday.inc",
    "gamemodes/includes/leader_system.inc",
    "gamemodes/includes/studio_system.inc",
)
SAMPLE = re.compile(
    r"PERF_SCALE:name=(payday|organizations|zones)_(100|250|500),"
    r"pass=(\d+),ms=(\d+),checksum=(-?\d+)"
)
DONE = "PERF_SCALE_DONE:cases=9,samples=7"
CASE_NAMES = tuple(f"{kind}_{n}" for kind in ("payday", "organizations", "zones")
                   for n in (100, 250, 500))
FUNCTION_MAP = {
    "leader_system.inc": ("Leader_ValidateRoster", "perf_leader_validate.inc"),
    "studio_system.inc": ("StudioZone_Poll", "perf_studio_poll.inc"),
}


def extract_function(source: str, name: str) -> str:
    clean = mask_opaque(source)
    pattern = re.compile(
        r"\b(?:stock\s+(?:bool:)?|public\s+)" + re.escape(name) +
        r"\s*\([^)]*\)\s*\{"
    )
    match = pattern.search(clean)
    if match is None:
        raise ValueError(f"production function {name} missing")
    opening = clean.rfind("{", match.start(), match.end())
    end = closing(clean, opening, "{", "}")
    if end < 0:
        raise ValueError(f"production function {name} is unbalanced")
    return source[match.start():end+1] + "\n"


def get_sources(ref: str | None) -> dict[str, bytes]:
    result: dict[str, bytes] = {}
    for path in SOURCES:
        if ref is None:
            raw = (ROOT / path).read_bytes()
        else:
            res = subprocess.run(
                ["git", "show", f"{ref}:{path}"],
                cwd=ROOT, stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=20
            )
            if res.returncode or not res.stdout:
                raise ValueError(f"cannot load {path} at {ref}: " +
                                 res.stderr.decode(errors="replace")[:180])
            raw = res.stdout
        raw.decode("utf-8")  # reject invalid editorial encoding
        result[path] = raw
    return result


def stage_sources(stage: Path, snapshot: dict[str, bytes]) -> None:
    for path, raw in snapshot.items():
        source = raw.decode("utf-8")
        name = Path(path).name
        if name == "payday.inc":
            target, transformed = "payday.inc", source
        else:
            function, target = FUNCTION_MAP[name]
            transformed = extract_function(source, function)
            if name == "leader_system.inc" and "Leader_MaxRank(" in transformed:
                # The benchmark extracts the actual production validator.
                # New rank policies are helpers: include exactly their source
                # implementations and constants rather than approximating
                # the validator inside the harness.
                helper_names = ("Leader_IsCriminal", "Leader_MaxRank",
                                "Leader_LeaderRank", "Leader_DeputyRank")
                helpers = [extract_function(source, helper)
                           for helper in helper_names]
                constants = []
                for macro in ("FACTION_FINANCE", "FACTION_CRIME_FIRST",
                              "FACTION_CRIME_LAST"):
                    pattern = r"(?m)^#define\s+" + macro + r"\s+\(\d+\)$"
                    definition = re.search(pattern, source)
                    if definition is None:
                        raise ValueError(f"missing faction constant {macro}")
                    constants.append(definition.group(0))
                transformed = "\n".join(constants + helpers) + "\n" + transformed
        (stage / target).write_bytes(transformed.encode("cp1251", errors="strict"))


def parse_samples(log: str) -> dict[str, dict]:
    if DONE not in log or "PERF_SCALE_ERROR:" in log:
        raise ValueError("scaling AMX did not complete correctly")
    data: dict[str, dict[int, tuple[int, int]]] = {name: {} for name in CASE_NAMES}
    for family, count, index, duration, checksum in SAMPLE.findall(log):
        name = f"{family}_{count}"
        seq = int(index)
        if seq >= 7 or seq in data[name]:
            raise ValueError(f"duplicate/out-of-range sample {name}/{seq}")
        data[name][seq] = (int(duration), int(checksum))
    output: dict[str, dict] = {}
    for name, rows in data.items():
        if set(rows) != set(range(7)):
            raise ValueError(f"missing scaling samples: {name}")
        checksums = {c for _, c in rows.values()}
        if len(checksums) != 1 or next(iter(checksums)) <= 0:
            raise ValueError(f"invalid checksum consistency: {name}")
        output[name] = {**summarize([rows[i][0] for i in range(7)]),
                        "checksum": checksums.pop()}
    return output


def run_amx(server: Path, snapshot: dict[str, bytes], label: str, output: Path) -> dict:
    with tempfile.TemporaryDirectory(prefix=f"tarkenso-scaling-{label}-") as d:
        work = Path(d)
        stage = work / "stage"
        stage.mkdir()
        stage_sources(stage, snapshot)
        (stage / HARNESS.name).write_bytes(
            HARNESS.read_text(encoding="utf-8").encode("cp1251"))
        (work / "gamemodes").mkdir()
        target = work / "gamemodes/perf_scaling_harness.amx"
        env = os.environ.copy()
        env["LD_LIBRARY_PATH"] = os.pathsep.join((
            str(server / "qawno"), str(work), env.get("LD_LIBRARY_PATH", "")
        ))
        cmd = [str(server / "qawno/pawncc"), "-d3",
               f"-i{server / 'qawno/include'}", f"-i{stage}",
               f"-o{target}", str(stage / HARNESS.name)]
        result = subprocess.run(cmd, cwd=work, env=env,
                                stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                timeout=60)
        log = result.stdout.decode(errors="replace")
        (output / f"scaling-{label}-compile.log").write_text(log, encoding="utf-8")
        if result.returncode or analyze_pawn_output(log) or not target.is_file() or not target.stat().st_size:
            raise RuntimeError(f"scaling AMX compile failed ({label}):\n{log[:6500]}")
        shutil.copytree(server / "components", work / "components")
        (work / "scriptfiles").mkdir()
        with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
            sock.bind(("127.0.0.1", 0))
            port = sock.getsockname()[1]
        config = {
            "announce": False, "enable_query": False, "max_players": 4,
            "max_bots": 0, "name": "TARKENSO offline scaling benchmark",
            "artwork": {"enable": False},
            "network": {"bind": "127.0.0.1", "port": port},
            "pawn": {"legacy_plugins": [],
                     "main_scripts": ["perf_scaling_harness 1"],
                     "side_scripts": []},
            "rcon": {"enable": False, "password": "isolated-perf-test"},
        }
        (work / "config.json").write_text(json.dumps(config), encoding="utf-8")
        process = subprocess.Popen([str(server / "omp-server")], cwd=work,
                                   env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                   stdin=subprocess.DEVNULL)
        watchdog = threading.Timer(90, process.kill)
        watchdog.start()
        chunks, done = [], False
        try:
            assert process.stdout is not None
            for line in process.stdout:
                chunks.append(line)
                if DONE.encode() in line or b"PERF_SCALE_ERROR:" in line:
                    done = DONE.encode() in line
                    process.kill()
                    break
            chunks.append(process.communicate(timeout=5)[0])
        finally:
            watchdog.cancel()
            if process.poll() is None:
                process.kill()
                process.wait(timeout=5)
        log = b"".join(chunks).decode(errors="replace")
        (output / f"scaling-{label}-run.log").write_text(log, encoding="utf-8")
        if not done or process.returncode not in (0, -9):
            raise RuntimeError(f"scaling AMX run failed ({label}):\n{log[-6500:]}")
        return parse_samples(log)


def run(server: Path, baseline_ref: str, output: Path) -> dict:
    output.mkdir(parents=True, exist_ok=True)
    baseline = get_sources(baseline_ref)
    candidate = get_sources(None)
    a = run_amx(server.resolve(), baseline, "base", output)
    b = run_amx(server.resolve(), candidate, "head", output)
    by_family = {
        "payday": (SOURCES[0],),
        "organizations": (SOURCES[1],),
        "zones": (SOURCES[2],),
    }
    cases = {}
    for name in CASE_NAMES:
        family = name.rsplit("_", 1)[0]
        unchanged = all(baseline[p] == candidate[p] for p in by_family[family])
        comparison = compare(a[name], b[name], unchanged)
        cases[name] = {"baseline": a[name], "candidate": b[name],
                       "comparison": comparison}
        print(f"Pawn scaling {name}: base={a[name]['median_ms']}ms; "
              f"head={b[name]['median_ms']}ms; {comparison['status']}")
        if comparison["status"] == "regression":
            print(f"::warning title=Pawn scaling regression::{name} "
                  f"slower by {comparison['change_percent']}% versus {baseline_ref}")
    result = {"schema_version": 1, "mode": "advisory",
              "synthetic_clients": True, "database": "mock-no-network",
              "baseline_ref": baseline_ref, "cases": cases}
    (output / "pawn_scaling_bench.json").write_text(
        json.dumps(result, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    return result


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--server", type=Path, required=True)
    parser.add_argument("--baseline-ref", default="origin/main")
    parser.add_argument("--output", type=Path, default=ROOT / "build")
    args = parser.parse_args(argv)
    try:
        run(args.server, args.baseline_ref, args.output)
    except (OSError, ValueError, RuntimeError, UnicodeError, subprocess.TimeoutExpired) as e:
        parser.error(str(e))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
