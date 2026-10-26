# TARKENSO Ship Studio — Stage 1

## Scope

Stage 1 is a development-only prototype of a **single** movable cargo vessel assembled from GTA SA standard objects. The root object represents the hull; up to 48 parts can be attached using **standard** object attachments (not dynamic Streamer objects). The prototype is restricted to a confirmed founder on /aduty via the existing Studio_Require gate.

**Not production-ready.** It does not automatically scan GTA map collisions, keep passengers aligned during turns, or provide buoyancy/hydrodynamics. All route trials should be conducted in a closed test instance without passengers. Do not enable for public players yet.

## Installation

Already wired to the existing main gamemode; there is no database migration or extra plugin. Rebuild `compiled.amx` and deploy to your open.mp server with Streamer and existing plugins. Saved files are created relative to the server's `scriptfiles` root.

## First run

1. Ensure the administrator is verified as founder and has enabled `/aduty`.
2. Position yourself where the hull should spawn and run `/ship create 10230` **only if model 10230 is suitable on your installed GTA assets**. Any valid object model can be used as the hull. This is not a guaranteed recreation of the screenshot ship.
3. Add objects using **local offsets from the hull pivot**, e.g. `/ship add 10231 0 4 1 0 0 0`. Offset units are GTA world units; verify alignment in game.
4. Use `/ship move 10` (forward) and `/ship turn 15` (heading change). Negative values move/reorient the opposite way.
5. `/ship mark` stores the current hull position and heading. Add 2 or more points for a route.
6. `/ship run` starts a **one-shot** test through the points. `/ship stop` interrupts it; `/ship undo` removes the last recorded point.
7. `/ship save` writes `scriptfiles/shipstudio/cargo_01.dat`, with atomic-ish temp/backup rotation. On restart it is loaded automatically. `/ship load` loads when no ship is active. `/ship clear` removes the in-memory ship only.
8. `/ship export` writes `scriptfiles/shipstudio/cargo_01_route.inc` with a Pawn coordinate array to copy into the module after review.
9. `/ship status` displays object count, route count and edit lock; `/ship release` releases the edit lock. `/ship help` shows commands.

## Known limitations

- Source screenshot contains the **map author's object list**, but not the complete copyable coordinates in this conversation. A screenshot alone cannot recreate the exact hull, deck and containers. The prototype lets the author import an equivalent assembly by supplying model IDs and local offsets.
- Attachment physics/standing players must be tested on the target **open.mp client**. No claim of stable walking on the ship is made.
- One ship, one editor, no per-world isolation, global objects count against the server's object limit.
- `/ship turn` rotates immediately. The autopath uses standard MoveObject target rotations but may require additional smoothing for high-quality navigation.
- `/ship run` **teleports** to waypoint 1 before moving and is for a test environment only. It is not a continuously looping public vessel.
- NO dynamic collision detection; ship may pass through banks, bridges, or another moving vessel. Avoid obstacles visually and measure hull clearances independently.
- The default speed is 4 world units/second; route points are replayed in sequence with a 250 ms arrival check.
- Save file `cargo_01.dat` is runtime-generated and must be backed up when transferring hosting.

## Stage 2 required

Manual helm input while in a developer control mode; import from full CreateDynamicObject scripts, hull envelope preview and clearance checks against curated hazards/map geometry, smooth turning, collision survey and passenger synchronization tests, automated regression/performance benchmarks for 50+ parts, and eventual per-route persistence with multiple ships.
