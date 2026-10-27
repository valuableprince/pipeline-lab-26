# Ship Studio: top-down visual positioning (stage 1)

**Base:** PR #68 port setup. The visual editor is in feature branch `feat/ship-visual-editor-stage1` and is not automatically shipped to `main`.

## Usage

From an authorized founder account with `/aduty`:
1. Stop the ship with `/ship stop`. Leave the deck and immediate hull envelope (including dockside spaces inside the safety rectangle). Ask any passengers to disembark.
2. Run `/ship edit`. A fixed top-down camera and mouse-selectable TextDraw buttons appear; player movement is disabled while the editor is active.
3. Click NORTH / SOUTH / WEST / EAST to move 0.1, 1, 10 units at a time. TURN + / TURN - rotate; Z UP / Z DOWN adjust hull height. STEP +/- changes precision. ZOOM +/- changes the camera height.
4. **SAVE SHIP** persists the vessel's current root transform and route to `scriptfiles/shipstudio/cargo_01.dat`. **CANCEL** restores the previous transform, provided no one has approached the hull.
5. Use **LS BERTH**, **LS EXIT**, **SF BERTH**, **SF EXIT** to independently save the current transform to `scriptfiles/shipstudio/ports_01.dat` (from the port-system branch). Port buttons do not close the editor. Remember to save the vessel afterward to persist its parked position as well.
6. ESC cancels editing. For backup: `/ship edit save` or `/ship edit cancel`.

## Controls and constraints
- All buttons are clicked with the native SA-MP TextDraw cursor. GTA/open.mp does not reliably expose arbitrary desktop keys (Q/E/PageUp/PageDown) as distinct gameplay inputs; mouse controls avoid that limitation. Keyboard arrow support is **not** promised in this stage.
- No forced player teleportation or custom client dependency.
- Only one edit session per server. Founder, duty, on-foot, no jetpack/fly, no race, no punishment, world/interior 0.
- Deny entry and terminate an active session if any connected player is surfing the ship root/part or within a **conservative rotated hull envelope** (145 units longitudinal, 65 lateral, -13 to +75 height). This deliberately may reject an editor standing too close to the quay: move onto dry land outside the hull rectangle first.
- If someone arrives mid-edit, editor closes **without moving the hull back** (avoid moving it under their feet). Move/cancel again only when the area is clear.
- Existing marine routes and root attachments unchanged; no collision avoidance is inferred from the overhead camera.
- If you're planning to record a berth, confirm the whole 230-unit hull fits by the port and inspect from multiple angles before accepting.

## Roadmap
Client test of panel positions, selection on different resolutions, camera clipping, object/part alignment, save/load, and native surfing safety. No live in-game test can be inferred from CI compilation alone.
