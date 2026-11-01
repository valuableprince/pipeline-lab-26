"""Independent geometric acceptance checks for the production Pawn scene export.

The CSV comes from the real include executed by tools/test_ship_sf_pier.py.
No Python reimplementation of the scene builder is used. No GTA/world claims.
"""
from collections import Counter
import csv
import math
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[1]


def read_scene(path):
    with Path(path).open(newline="") as file:
        return [{k: (int(v) if k in ("model", "material") else float(v))
                 for k, v in row.items()} for row in csv.DictReader(file)]


def model_bounds():
    source = (ROOT / "gamemodes/includes/ship_sf_pier_map.inc").read_text("utf-8")
    rows = re.findall(r"\{(\d{4}),\s*([^{}\n]+)\}", source)
    return {int(model): tuple(map(float, data.split(","))) for model, data in rows}


def box(row, bounds):
    """Independently rotate all eight AABB corners; return a world-local AABB."""
    lo, hi = bounds[:3], bounds[3:]
    rx, rz = math.radians(row["rx"]), math.radians(row["rz"])
    corners = []
    for x in (lo[0], hi[0]):
        for y in (lo[1], hi[1]):
            for z in (lo[2], hi[2]):
                yy = y * math.cos(rx) - z * math.sin(rx)
                zz = y * math.sin(rx) + z * math.cos(rx)
                corners.append((x * math.cos(rz) - yy * math.sin(rz) + row["x"],
                                x * math.sin(rz) + yy * math.cos(rz) + row["y"],
                                zz + row["z"]))
    return tuple(min(p[i] for p in corners) for i in range(3)) + tuple(
        max(p[i] for p in corners) for i in range(3))


def validate_scene(rows):
    """Raise on gaps, unsupported boxes, blocked lanes or hull intersections."""
    assert len(rows) == 496
    assert Counter(row["model"] for row in rows) == {
        3095: 339, 3620: 1, 3570: 20, 3571: 20, 3572: 20,
        3626: 2, 1278: 15, 1214: 24, 1327: 24, 3578: 30, 3076: 1}
    transforms = [tuple(row[k] for k in ("model", "x", "y", "z", "rx", "rz")) for row in rows]
    assert len(set(transforms)) == len(rows), "duplicate object transforms"
    bounds = model_bounds()
    boxed = [(row, box(row, bounds[row["model"]])) for row in rows]
    floors = [b for r, b in boxed if r["model"] == 3095 and r["rx"] == 0]
    assert len(floors) == 248
    actual = {(round(b[0] / 9), round(b[1] / 9)) for b in floors}
    required = {(x, y) for x in range(32) for y in range(7)} | {
        (x, y) for x in range(3) for y in range(-8, 0)}
    assert actual == required, "missing/extraneous foundation cell"
    for b in floors:
        assert abs(b[5]) < 0.00003 and abs(b[3] - b[0] - 9) < 0.00003
        assert abs(b[4] - b[1] - 9) < 0.00003
    # Whole straight quay is 288u, with 29u longitudinal margin per end for 230u.
    assert max(b[3] for b in floors) >= 280
    assert all(r["material"] == 1 for r in rows if r["model"] == 3095)
    marking = next(row for row in rows if row["model"] == 3076)
    assert marking["material"] == 0 and marking["rz"] == 90
    assert abs(box(marking, bounds[3076])[2] - 0.02) < 0.00003
    crane = next(row for row in rows if row["model"] == 3620)
    assert crane["rz"] == 180, "crane boom must face +Y, feet must stay inland"
    # Independently measured visual DFF vertices at Z<-12: these include ALL
    # crane feet; unlike the whole model AABB their actual support is inland.
    feet = box(crane, (-3.422424, 8.128845, -13.095438,
                       3.432312, 16.238708, -12.072056))
    assert 0 <= feet[0] < feet[3] <= 288 and 0 <= feet[1] < feet[4] <= 63
    assert abs(feet[2]) < 0.00003
    cargo = [(r, b) for r, b in boxed if r["model"] in (3570, 3571, 3572)]
    for row, b in cargo:
        if abs(b[2]) < 0.00003:
            continue
        supports = [p for _, p in cargo if abs(p[5] - b[2]) < 0.00003 and
                    abs(p[0] - b[0]) < 0.00003 and abs(p[1] - b[1]) < 0.00003]
        assert len(supports) == 1, f"unsupported upper container {row}"
    # 3076 is decorative paint above the intact slabs, with no collision.
    props = [(r, b) for r, b in boxed if r["model"] not in (3095, 3076)]

    def overlaps(a, b):
        return all(min(a[i + 3], b[i + 3]) - max(a[i], b[i]) > 0.00003 for i in range(3))

    for index, (row, b) in enumerate(props):
        for other, c in props[index + 1:]:
            assert not overlaps(b, c), f"intersecting props {row} / {other}"
    # Continuous truck path, 16u main aisle, 48x48 turning area, 3u pedestrian
    # strip inside the 9u quay apron (floodlight islands sit farther seaward).
    for zone in ((5, -72, 0.01, 22, 24, 6), (13.5, 24, 0.01, 266, 40, 6),
                 (218, 5, 0.01, 266, 53, 6), (5, 52, 0.01, 266, 55, 6)):
        for row, b in props:
            assert not overlaps(zone, b), f"blocked reserved space {zone}: {row}"
    # 9585 exact collision offsets around a trial root at (144, 88.711, 7-BASE_Z).
    # Expand by 2u; use continuous swept boxes, not isolated centre samples.
    # Z spans the assembled 9584 upperworks too (+47.468737 relative to root).
    # Terrain, original IPLs, saved custom ship parts still need GTA UAT.
    hull = (26.359375, 69, -15, 260.359375, 109.148437, 60)
    for dx_min, dy_min, dx_max, dy_max in ((0, 0, 0, 20), (0, 20, 300, 20)):
        swept = (hull[0] + dx_min, hull[1] + dy_min, hull[2],
                 hull[3] + dx_max, hull[4] + dy_max, hull[5])
        for row, b in boxed:
            assert not overlaps(swept, b), f"hull envelope intersects scene {row}"
    for row in rows:
        assert abs(row["world_x"] - (100 - row["y"])) < 0.0001
        assert abs(row["world_y"] - (200 + row["x"])) < 0.0001
        assert abs(row["world_z"] - (30 + row["z"])) < 0.0001
        assert abs(row["world_rz"] - (90 + row["rz"])) < 0.0001


def validate_editor_export(source, rows):
    """Require literal, importable native calls matching the actual world scene."""
    assert "#define SF_PIER_EDITOR_PREVIEW_ENABLED (0)" in source
    assert "public OnFilterScriptExit()" in source and "SFPierEditor_Clear();" in source
    assert "CreateDynamicObject(" not in source
    calls = re.findall(r"sfPierObject = CreateObject\(([^)]+)\);", source)
    assert len(calls) == len(rows), "Map Editor object count differs from scene"
    for call, row in zip(calls, rows):
        args = [float(value.strip()) for value in call.split(",")]
        expected = (row["model"], row["world_x"], row["world_y"], row["world_z"],
                    row["rx"], 0, row["world_rz"], 350)
        assert len(args) == len(expected) and all(abs(a - b) < 0.00003 for a, b in zip(args, expected)), (
            "Map Editor literal transform differs from scene", call, row)
    concrete = sum(row["material"] == 1 for row in rows)
    for index in (0, 1):
        assert source.count(f'SetObjectMaterial(sfPierObject, {index}, 9514, "711_sfw", "ws_carpark2");') == concrete
    assert source.count("if (!SFPierEditor_Keep(sfPierObject)) return 0;") == len(rows)


class ShipSFPierIntegration(unittest.TestCase):
    def test_loaded_only_by_mode_lifecycle_and_staged(self):
        from build_contracts import assert_staged
        assert_staged(self, "ship_sf_pier_map.inc")
        game = (ROOT / "gamemodes/signal0317.pwn").read_text("utf-8")
        self.assertEqual(game.count('#include "ship_sf_pier_map.inc"'), 1)
        self.assertEqual(game.count("ShipSFPierMap_Init();"), 1)
        self.assertEqual(game.count("ShipSFPierMap_Shutdown();"), 1)
        player_callbacks = game.split("public OnPlayerConnect(", 1)[1]
        self.assertNotIn("ShipSFPierMap_", player_callbacks)

    def test_verified_model_registry(self):
        bounds = model_bounds()
        self.assertEqual(len(bounds), 11)
        for model, b in bounds.items():
            self.assertTrue(all(b[i + 3] > b[i] for i in range(3)), model)
        self.assertAlmostEqual(bounds[3095][5], .555365)
        for model in (3570, 3571, 3572):
            self.assertAlmostEqual(bounds[model][5] - bounds[model][2], 2.697324)

    def test_editor_export_validator_rejects_unsafe_or_misaligned_export(self):
        row = {"model": 3095, "world_x": 101.25, "world_y": 202.5, "world_z": 30.75,
               "rx": 90, "world_rz": 180, "material": 1}
        source = '''#define SF_PIER_EDITOR_PREVIEW_ENABLED (0)
sfPierObject = CreateObject(3095, 101.25, 202.5, 30.75, 90, 0, 180, 350);
if (!SFPierEditor_Keep(sfPierObject)) return 0;
SetObjectMaterial(sfPierObject, 0, 9514, "711_sfw", "ws_carpark2");
SetObjectMaterial(sfPierObject, 1, 9514, "711_sfw", "ws_carpark2");
public OnFilterScriptExit() { SFPierEditor_Clear(); }
'''
        validate_editor_export(source, [row])
        for bad in (source.replace("(0)", "(1)", 1), source.replace("101.25", "101.5"),
                    source.replace("90, 0, 180", "0, 90, 180"),
                    source.replace("SFPierEditor_Clear();", ""),
                    source.replace("CreateObject(", "CreateDynamicObject(")):
            with self.subTest(source=bad), self.assertRaises(AssertionError):
                validate_editor_export(bad, [row])


if __name__ == "__main__":
    unittest.main()
