# TARKENSO Ship Studio — LS ⇄ SF marine route preset

Status: **DRAFT / UNVERIFIED IN GAME**. Waypoints reconstructed from the public overhead GTA SA map and named in-world dock locations. They are NOT a surveyed collision-free route and are not guaranteed to remain on navigable water under a custom map.

## How to use

1. In an isolated test server, connect as confirmed founder and enable `/aduty`.
2. Create the ship or load an existing saved assembly. Example: `/ship create 10230`. Model 10230 is an existing full-size freighter, not the screenshot's hull.
3. Type `/ship preset lssf` for Los Santos Ocean Docks -> San Fierro Easter Basin holding area. `/ship preset sfls` reverses the route.
4. Review all points. `/ship export` produces `scriptfiles/shipstudio/cargo_01_route.inc` with the waypoint coordinates.
5. `/ship save` persists the route to `scriptfiles/shipstudio/cargo_01.dat`. This does **not** auto-start any vessel.
6. Only in a closed test environment, use `/ship run`. The stage-1 engine **teleports the ship to waypoint #1** and follows the route once at 4 GTA world units per second. Emergency stop: `/ship stop`.

## Route summary

- 36 control points, surface `Z = 0.0` at the root object pivot. Adjust hull pivot height before live operations.
- Geometry: leave Ocean Docks via a wider **east/south offshore arc** rather than the old abrupt south-west turn, then travel west on an outer southern sea lane, go north along the west of San Fierro, round the northern city shore and travel south-eastward through Easter Bay to an **offshore holding position** east of the docks.
- Start: `2640.0, -2740.0, 0.0` (proposed offshore Ocean Docks approach).
- End: `-1090.0, 700.0, 0.0` (proposed offshore Easter Basin waiting area, **not a berth**).
- Total segment distance ~13,689 GTA coordinate units. At 4 units/s, theoretical uninterrupted travel time ~57 minutes, excluding turns and port handling.
- Reverse preset starts at the SF holding area and terminates at the LS approach.
- Stays inside the conservative map coordinate limit of [-3000, +3000] on X and Y.

## Non-negotiable verification before public use

- Confirm ship centre and **entire hull outline**, not just waypoints, is over water.
- Check shorelines, piers, buoys, bridges and any custom objects. This implementation does **not** do live raycasts or hull-swept collision tests.
- Test east coast and harbor narrowing from the north of San Fierro to the Easter Basin waiting area. Known Easter Basin cranes and ships may obstruct close approach.
- Validate object attachments, collision, animation, turning and passenger position on a private build.
- The terminal position is deliberately offshore; actual berth approach must be surveyed in game and recorded as additional final points.
- The present `MoveObject` movement logic does not guarantee smooth turning across all waypoints; a route smoothing and speed-profile pass is a separate stage.
- Existing route draft is **overwritten** by either preset. Use `/ship save` / export beforehand.

References: GTA SA aerial map (gta.com.ua), Ocean Docks placed objects near `2507,-2640` (CIT tools), Easter Basin maritime zone / cargo port (GTA wiki) and freighter model (Prineside). These references locate districts but do not validate water topology or collision clearance.

## Los Santos coastal rock avoidance (v2 draft)

Reports from in-game navigation found the original outbound route too close to coastal cliffs and at least one collision with rock geometry. The corrected first sector takes the hull east to `2825,-2905`, curves toward `2660,-2980`, and follows a southern ocean corridor near `Y=-2985` rather than `Y=-2960/-2950`. The route now contains 36 points and changes the initial hull heading to 330.95 degrees (first navigation heading 240.95 + model correction 90). The offshore diversion adds roughly 235 route units (about one minute at cruise speed).

**Critical limitation:** The stock GTA SA map is conventionally charted only to ±3000 coordinates. A centreline at Y=-2985 places the **edge of the full 230-unit-long vessel** at possible risk of extending beyond the mapped water grid during turning. Merely moving waypoint centres closer to -3000 does not certify hull clearance or water physics. Never advertise this corridor as collision-free without driving the full route in game and checking both coastal rocks and the southern world limit.

The updated geometry is applied only when choosing `/ship preset lssf` or `/ship preset sfls` (and on fresh default assembly). Existing `cargo_01.dat` persists the old route until the preset is reloaded and saved. Stop the ship before changing the preset. With passengers aboard use `/ship port ls depart` from a verified berth; `/ship run` remains a teleporting test action.
