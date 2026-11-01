# SHIP-OCEAN-PROBE: controlled GTA map-edge experiment

**Purpose:** Test the already positioned Los Santos cargo ship travelling straight east beyond the conventional GTA SA X=3000 boundary. This feature does **not** change the game map, collision geometry, water streaming or any production sea routes. Some static map elements are not visible beyond GTA's conventional square boundary. Client behavior must be established by observation before choosing the final route.

**Context:** The hull reached approximately X=2991.96 Y=-2268.13 Z=7 A=0.89 and refused another `/ship move 100` because `Ship_ValidCoord` limited X to 3000.

## Commands (development server only)

Use founder privileges with `/aduty`, **exit the editor**, and move your player away from the ship before the test. Back up `scriptfiles/shipstudio/cargo_01.dat` and `ports_01.dat`.

```text
/ship status
/ship oceanprobe on
/ship move 50
```

Observe until the movement finishes; then:

```text
/ship status
```

Inspect **ship visibility, all attached parts, water, camera and position** past the 3000 boundary. If satisfactory, optionally repeat `/ship move 50`; tests are bounded at X=3600. The probe is intentionally disabled by default on each server start.

To return **without a teleport**:

```text
/ship move -50
/ship status
```

Repeat until X is at or below 3000. After each move, wait for it to finish before the next command. Then:

```text
/ship oceanprobe off
```

If the ship disappears, starts desynchronizing, loses parts or behaves oddly, stop the test. As a last resort, with **no players near/on the hull**:

```text
/ship stop
/ship set 2991.96 -2268.13 7.0 0.89
/ship oceanprobe off
```

The saved LS berth is still at X=2682; it is **not** altered by the probe.

## Safeguards

- Only the cargo ship model 9585 can use X>3000, only while the founder explicitly enables `/ship oceanprobe on`, and only in manual `/ship move` / `/ship set`.
- X beyond 3000 is capped at 3600. Standard Y/Z and default X limits otherwise stay unchanged.
- `/ship save`, `/ship mark`, and port `berth/exit` **cannot** record out-of-map positions; this prevents a restart loading an invalid hull or an unsound automated route.
- No automatic `depart` or `run` feature is granted by the probe; port and maritime routes are unchanged.
- Movement toward out-of-map space refuses to start if another player is near/on the hull.
- The ocean probe cannot be switched off until the ship is back inside X≤3000.
- It is a test feature, **not proof** that going beyond X=3000 is stable or safe in GTA/open.mp.

## Decision after the test

- If rendering/sync fails beyond 3000: retain the vanilla boundary and redesign the LS departure with a turn within the ocean-side corridor, or relocate/rotate the berth.
- If the entire hull remains stable: analyze the shortest safe out-and-back segment and review ship passenger handling before considering a production change that persists coordinates or allows automatic routes outside vanilla map boundaries.
