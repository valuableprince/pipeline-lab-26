#!/usr/bin/env python3
"""Summarize append-only Stage-1 open.mp NPC load probe telemetry."""

import argparse
import csv
import json
from pathlib import Path
import sys

COLUMNS = (
    "run_id", "elapsed_sec", "active_npcs", "target_npcs",
    "tickrate", "min_tickrate", "worst_timer_interval_ms",
    "guard_heartbeat_ms", "guard_delayed_count", "state",
)
END_STATES = {
    "complete", "manual", "shutdown", "flag_removed",
    "owner_disconnect", "owner_unavailable", "npc_create_failed",
    "npc_spawn_failed", "low_tickrate",
}


def load_samples(path: Path) -> list[dict]:
    """Read CSV without a header, rejecting malformed/negative counters."""
    rows = []
    with path.open("r", encoding="utf-8-sig", newline="") as stream:
        for lineno, raw in enumerate(csv.reader(stream), 1):
            if not raw:
                continue
            if lineno == 1 and raw == list(COLUMNS):
                continue  # New per-run CSV files contain a header.
            if len(raw) != len(COLUMNS):
                raise ValueError(f"line {lineno}: expected {len(COLUMNS)} columns, got {len(raw)}")
            try:
                values = [int(value) for value in raw[:-1]]
            except ValueError as exc:
                raise ValueError(f"line {lineno}: invalid numeric field") from exc
            if any(value < 0 for value in values):
                raise ValueError(f"line {lineno}: negative measurement")
            state = raw[-1]
            if not state or not state.replace("_", "").isalnum():
                raise ValueError(f"line {lineno}: invalid state")
            row = dict(zip(COLUMNS, [*values, state]))
            if row["active_npcs"] > row["target_npcs"]:
                raise ValueError(f"line {lineno}: active NPCs exceed target")
            rows.append(row)
    if not rows:
        raise ValueError("no measurements in CSV")
    return rows


def summarize(samples: list[dict]) -> list[dict]:
    """Keep separate test runs separate even when samples share a CSV."""
    groups = {}
    for row in samples:
        groups.setdefault(row["run_id"], []).append(row)
    reports = []
    for run_id, group in sorted(groups.items()):
        last = group[-1]
        target = group[0]["target_npcs"]
        peak = max(r["active_npcs"] for r in group)
        reports.append({
            "run_id": run_id,
            "samples": len(group),
            "duration_sec": max(r["elapsed_sec"] for r in group),
            "target_npcs": target,
            "peak_npcs": peak,
            "min_tickrate": min(r["tickrate"] for r in group),
            "worst_timer_interval_ms": max(r["worst_timer_interval_ms"] for r in group),
            "max_guard_heartbeat_ms": max(r["guard_heartbeat_ms"] for r in group),
            "end_state": last["state"] if last["state"] in END_STATES else "incomplete",
            "target_reached": peak >= target,
        })
    return reports


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("csv_file", type=Path, help="scriptfiles/load_test_01_DD.MM.YYYY.csv (legacy loadtest.csv also supported)")
    parser.add_argument("--json", action="store_true", help="machine-readable JSON")
    args = parser.parse_args(argv)
    try:
        reports = summarize(load_samples(args.csv_file))
    except (OSError, ValueError) as exc:
        print(f"loadtest_report: {exc}", file=sys.stderr)
        return 2

    if args.json:
        print(json.dumps(reports, ensure_ascii=False, indent=2))
    else:
        for r in reports:
            print(
                f"Run {r['run_id']}: {r['peak_npcs']}/{r['target_npcs']} NPC; "
                f"{r['duration_sec']}s; min tickrate {r['min_tickrate']}; "
                f"worst timer {r['worst_timer_interval_ms']}ms; "
                f"state={r['end_state']}; target_reached={r['target_reached']}"
            )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
