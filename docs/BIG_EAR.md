# The Big Ear — rotating installation (BIG-EAR-01)

## Scope
- The original vanilla radio telescope uses **object 16613** (`des_bigtelescope`) at **-346.67, 1595.08, 79.6641**.
- The model is a single rigid mesh, without separate animated support/reflection assembly. **This implementation rotates the whole mesh about Z**. It does NOT rotate only the dish surface, and does NOT support dish elevation. Doing that properly requires an asset split into at least a stationary pedestal and a moving reflector.
- No client plugin or custom models are required. The original map object (and any LOD sharing the same origin) is suppressed for clients in `Dish_OnConnect` using a narrow `RemoveBuildingForPlayer(-1, ..., 1.0)` call. The replacement is a server-created global object.
- The system is stateless across restarts (returns to automatic scanning at 0°), and has no dependency on SQL.

## Motion
- Automatic mode starts with the game mode. It sweeps between -140° and +140° at roughly 0.5° per second, changing targets in 10° segments.
- `MoveObject` needs *translational* motion for rotation interpolation. The controller alternates between a 0 and +0.01m Z offset to interpolate rotation, and chains movement using `OnObjectMoved` without continuous polling.
- Manual motion shares the same object and can interrupt automatic rotation. There is no move scheduling timer to leak.
- The map object has collision; moving the whole station can move/intersect characters or vehicles. **Do not allow public access to the rotating mesh until collision has been validated in the game.**

## Controls
All command traffic passes existing Pawn.CMD command admission and authentication gates. Status is available to authenticated players; changing mode/angle requires **admin level 5+** and **`/aduty`**.

| Command | Function |
|---|---|
| `/dish` | Status and quick help |
| `/dish status` | Angle, target and mode |
| `/dish auto` | Resume automatic sweeping |
| `/dish stop` | Stop immediately and enter manual mode |
| `/dish angle 45` | Turn to absolute angle (-140..140) |
| `/dish left 15` | Rotate 15° left (1..90) |
| `/dish right 15` | Rotate 15° right (1..90) |

## Acceptance checks
1. Enter as two clients, one with admin level 5+ /aduty. Confirm the original static mesh disappears and there is exactly one rotating installation for both players.
2. Observe at least two movement segments (automatic) and the reverse at ±140°. Check interpolation at long distance, including LOD: if a static silhouette survives, **fix the specific LOD model removal before merge**.
3. Run `/dish stop` mid-segment, `/dish left 15`, `/dish angle -45`, `/dish auto`; ensure continuity and no teleport or visual duplication.
4. Try commands from level 0, level 4, and level 5 without /aduty. All changes must be denied.
5. Stand near the support and check collision/surfing safety. The base currently rotates as part of the mesh; a fixed base is only possible with a split 3D asset.
6. Restart the game mode and reconnect, watching for missing objects, ghost LODs, multiple overlapping models and excessive streaming activity.

## Configuration
Constants in `gamemodes/includes/big_ear.inc`: `DISH_ROT_LIMIT`, `DISH_AUTO_STEP`, `DISH_MANUAL_SPEED` and `DISH_Z_TRAVEL`.

## Rollback
Revert the PR and rebuild `compiled.amx`; standard GTA map geometry is restored on reconnect.
