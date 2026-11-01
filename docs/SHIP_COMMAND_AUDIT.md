# TARKENSO — Ship command audit (after PR #133)

Audit scope: every `/ship` verb, `/ship port` port operation, `/ship edit` action, 28 TextDraw actions, `/shipcam`, `/shipsurf`, and Ship Panel dialog dispatch.

Automated checks: `tests/test_ship_command_audit.py` plus existing ship-related contract tests and Pawn compilation. **No live GTA/open.mp click or persistence test was performed in CI.** Collision avoidance and movement with passengers remain explicitly unverified in the existing ship prototype.

## Administrator actions

| Command | Purpose / expected outcome |
|---|---|
| `/ship` | Open control dialog (moderator on duty) |
| `/ship help` | Show available commands |
| `/ship status` | Show position, angle, object/route counts and edit-lock owner |
| `/ship cargo` | Create standard cargo assembly if none exists |
| `/ship create 9585` | Create hull with supplied model at admin's location |
| `/ship add 9586 0 0 10 0 0 0` | Add attached model with local offsets and angles |
| `/ship move 10` | Start manual movement along ship heading (limit ±100 m; nonzero) |
| `/ship turn 15` | Change yaw (limit ±180°) |
| `/ship set 2682.0 -2273.0 7.0 0.9` | Place ship at exact XYZ and yaw if hull area is clear |
| `/ship mark` | Add current transform to **sea route** (not a berth) |
| `/ship undo` | Remove last sea route point |
| `/ship preset lssf` | Load unverified LS→SF route draft |
| `/ship preset sfls` | Load reverse route draft |
| `/ship run` | Start legacy sea route **only when already within 12 units of waypoint 1**; never teleport the ship; otherwise explain refusal and suggest `/ship oceanroute start` for new LS departure |
| `/ship stop` | Stop manual/port/sea movement |
| `/ship save` | Write `scriptfiles/shipstudio/cargo_01.dat` |
| `/ship load` | Load file into empty ship slot |
| `/ship export` | Export sea route to `cargo_01_route.inc` if at least 2 points |
| `/ship clear` | Remove in-memory ship, **not** saved file; disallowed with people near hull |
| `/ship release` | Release developer edit lock (owner only) |

## Port commands

| Command | Purpose / expected outcome |
|---|---|
| `/ship port` | Show LS/SF berth/exit saved status |
| `/ship port ls berth` | Save **current hull transform** to LS berth |
| `/ship port ls exit` | Save current hull transform to LS exit |
| `/ship port sf berth` | Save current hull transform to SF berth |
| `/ship port sf exit` | Save current hull transform to SF exit |
| `/ship port ls depart` | Launch LS departure if required geometry is recorded and validated |
| `/ship port sf depart` | Launch SF departure under equivalent checks |

Berths are persisted separately to `scriptfiles/shipstudio/ports_01.dat`. The berth/exit recording commands are allowed while the same administrator owns the active editor. Departure is **not** allowed while editor is open.

## Editor and diagnostics

- `/ship edit` or `/ship edit start`: open overhead editor. Mouse buttons: north/south/east/west, rotation, Z, step, zoom; save/cancel; LS/SF berth/exit; four route selections; add/undo/preview/stop/save/load route.
- `/ship edit save`: save position and exit editor **only when editor is open**.
- `/ship edit cancel`: cancel editor and attempt to restore original hull position.
- `/shipcam`: enter/exit ship observation camera (moderator on duty).
- `/shipsurf watch [id]`, `/shipsurf status [id]`, `/shipsurf off [id]`: read-only native surfing diagnostics (founder on duty).

## Manual UAT required after installing an AMX built from this PR

1. As founder on `/aduty`, run `/ship status` and verify XYZ **and angle**.
2. With the editor **closed and nobody at the hull**, run `/ship set 2682.0 -2273.0 7.0 0.9`; confirm status output matches.
3. Run `/ship port ls berth`, then `/ship port`: expect `Los Santos: berth=SET`.
4. Run `/ship save`, restart the server; confirm `/ship status` retains XYZ/yaw and port retains `berth=SET`.
5. Open `/ship edit`, verify each of **SAVE SHIP**, **LS BERTH**, **ADD POINT**, **PREVIEW** invokes its own action (no neighboring-button misfires).
6. While editor is open, `/ship port ls berth` should be accepted; `/ship port ls depart` must be rejected.
7. While editor is open, `/ship edit save` should save and close. Running it **after** closing should explain that editor is closed.
8. Check bad syntax for `set`, `add`, `move`, `turn`, `preset`, `port`, `shipsurf`: expect usage messages, not silent changes.
9. With a tester **near the hull** (closed test server), `/ship clear`, `/ship set`, `/ship run` should refuse to teleport or remove the hull.
10. Verify `/ship stop` cancels port movement, and `/ship move`, `/ship turn`, `/ship add`, `/ship mark`, `/ship undo`, `/ship preset` cannot modify an active port/sea voyage.
11. Check LS and SF `berth`/`exit` storage separately. **Do not start public departure** until the full-size hull has been checked for shore, crane and bridge collisions.

## Legacy `/ship run` vs LS ocean departure

As of the no-snap fix, `/ship run` is **only** a test of the old sea waypoint list, not a command to depart from the saved LS berth. It checks distance from the ship's present coordinates to the first waypoint. More than 12 units away -> rejects without modifying the ship or the active route. If within 12 units and the hull is empty, proceeds toward waypoint 2 from the **current ship position**, without resetting yaw or teleporting.

For the new opt-in LS ocean-edge path, use `/ship oceanprobe on` followed by `/ship oceanroute start`, after installing the corresponding AMX. The offshore route has a required pause and remains an unvalidated in-game test.
