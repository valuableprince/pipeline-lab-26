# SHIP-MANEUVER-01 — визуальная настройка манёвров в порту

This feature is based on the unmerged Ship Port PR #68 and the overhead editor PR #70. Its PR includes those base changes until their PRs are merged. It does **not** provide certified collision clearance or recurring schedule.

## Отход и заход для каждого порта

From the founder account on /aduty, stop the ship, ensure all passengers have left, and start `/ship edit`. The top-down panel now offers:
- **LS OUT / LS IN / SF OUT / SF IN** — choose one of four independent harbor routes.
- **ADD POINT** — record the present root position and heading as the next intermediate waypoint.
- **UNDO** — remove the most recent draft waypoint (does not affect saved routes).
- **SAVE ROUTE** — save all four draft routes in `scriptfiles/shipstudio/maneuvers_01.dat`.
- **LOAD ROUTE** — discard unsaved edits, restoring the last saved routes.
- **PREVIEW** — depart from the correct berth (OUT) or exit (IN), following the selected draft without teleportation. The entire envelope must be passenger-free.
- **STOP** — interrupt preview at its current position without teleportation.
- Saved waypoint labels are player-specific 3D text labels. Camera height, clipping and labels need a manual in-game test.

Use placement controls between ADD POINT clicks. A route stores **up to 8 intermediate poses**, including yaw, followed by an existing terminal pose (EXIT for OUT, BERTH for IN). Do **not** record the terminal as an intermediate waypoint (zero-length segments are rejected). Each segment must be between 1 and 600 GTA world units, inclusive. The engine cannot infer obstacle clearance from these bounds.

## Preparation sequence

1. Place the hull at the desired LS berth, click **LS BERTH**.
2. Move the hull through safely navigable intermediate positions with `LS OUT` selected, clicking **ADD POINT** after each.
3. Move the hull to the final clear-water exit pose and click **LS EXIT**.
4. Save the four-route draft with **SAVE ROUTE**; it requires both BERTH and EXIT markers for every nonempty route.
5. Return to the exact saved berth; press **PREVIEW**. Test without passengers and note cliffs or quay collisions. Stop/cancel where needed, adjust points and save again.
6. For `LS IN`, begin at the saved exit and record poses in order toward the saved berth (not reversed automatically).
7. Repeat for SF, select **SF OUT**, **SF IN**.

## Runtime behavior

- `/ship port ls depart`: when the LS OUT route exists, moves slowly through its persisted intermediate points and the configured LS EXIT, then switches to the original ocean preset, **without teleporting passengers**. No route configured: original direct berth→exit behavior remains.
- At the offshore endpoint near SF, if SF IN route is configured and berth/exit are valid, the ship slowly moves to SF EXIT, proceeds through the saved **SF IN** points and docks at SF BERTH. It then stays parked.
- The reverse SF→LS route uses SF OUT and LS IN in the same way.
- `/ship stop` interrupts any live maneuver and clears any pending arrival.
- **No automatic schedule or gangway** yet. Sea-route `/ship run` is still a separate test function that teleports to the first preset waypoint, and should not be used with passengers.
- Movement uses low-speed `MoveObject` and intermediate hull yaw poses. No real swept-hull collision probe exists; use manual water/rock/bridge checks before live passenger service.
- Preserve `cargo_01.dat`, `ports_01.dat`, and `maneuvers_01.dat` in backups. Empty route clears via undo + SAVE ROUTE.

## QA requirements

Check UAT on an isolated server: save → restart → load; mouse TextDraw clicks; marker visibility; preview stop; abort and cancel; multiple player nearby; unexpected disconnect; outbound movement; offshore connection at the destination; reverse direction; old route files; passenger surfing across turns; collision with nearby rock faces and at world bounds.
