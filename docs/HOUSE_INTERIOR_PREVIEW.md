# House Interior Preview — developer-only

## Scope

This is a new static open.mp / Streamer map preview, not a player-owned home system.
Coordinates: (6000, 6000, 20), virtual world 10, interior 0. It deliberately
requires the existing founder identity and active `/aduty`, reusing
`Studio_Require(playerid)`. Ordinary players cannot enter.

## Usage

1. Build the gamemode normally and deploy `compiled.amx`.
2. Log in as the verified founder and run `/aduty`.
3. Use `/house status` to verify creation of 26 dynamic objects.
4. Use `/house enter` to teleport to the entrance area of the preview.
5. Stand within 2.5 m of either door and run `/house door` to rotate it.
6. Use `/house exit` to return to the captured position, interior, world and facing angle.

Entering requires being on foot; entering multiple times does not overwrite the
return coordinates. Disconnect/death/respawn reset the preview flag. No MySQL
migration and no runtime custom models are needed. `House_Shutdown` destroys
all tracked Streamer objects on gamemode exit.

## Plan

A single roughly 9.64 x 9.64 m compact floorplate centered at (6000, 6000).
The north and side walls are 19450 segments (~9.64 m each); the south wall
is composed of 19353 and 19426 segments leaving a **real 1.607 m opening**.
Interior partition walls separate the northeast bedroom, with a 1.607 m door
opening in its west partition. The west/south living area, northwest kitchen
and southeast foyer form an open-plan common zone.

The floor/ceiling use **19379 rotated around Y by 90 degrees** (wall model
bounding box: 9.63762 m width, 10.50146 m height, 0.17878 m thickness).
These are real geometrical dimensions, unlike the previously claimed 30x30 slab.

Sample furniture IDs include 1753 (couch), 1747 (TV), 1737 (dining table),
2147 (fridge), 2298 (bed), and 1502 (doors).

## Important constraints / acceptance checks

This is a **preview requiring in-game inspection**, not a certified polished
interior. Catalog dimensions and basic IDs were checked, but the exact
coordinate pivots, floor face orientation, furniture footprints, textures,
hinge side and collision behaviour **have not been verified in-game**.

- Walk the perimeter: inspect for gaps, protrusion and invisible collision.
- Walk through both doorways; test rotation states, especially when standing close.
- Verify player never falls through the rotated 19379 floor.
- Verify couch/TV/cabinets/chairs/bed are facing the desired directions and
  not clipping against geometry.
- Apply suitable wood/paint finishes **after** validating material indices and
  actual texture names with a GTA map editor. No guessed texture IDs are used.
- No smooth door motion claimed: `SetDynamicObjectRot` rotates immediately.
  A smoothly hinged door would need a verified hinge pivot or animation path.
- One test house and one shared world. Multiple houses, ownership, persistence,
  dynamic world allocation, entrance placement in exterior towns, and access
  control for ordinary players are future work.

## Repository integration

The module is `gamemodes/includes/house_preview.inc`; the existing
`signal0317.pwn` initializes it after Studio, shuts it down before Studio,
and resets per-player state on connection/disconnection/spawn. It registers
`/house` through the existing Pawn.CMD integration, without defining an
additional `OnGameModeInit`.
