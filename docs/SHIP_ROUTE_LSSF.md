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

- 32 control points, surface `Z = 0.0` at the root object pivot. Adjust hull pivot height before live operations.
- Geometry: leave Ocean Docks toward the south, travel west in the open sea, go north along the west of San Fierro, round the northern city shore and travel south-eastward through Easter Bay to an **offshore holding position** east of the docks.
- Start: `2640.0, -2740.0, 0.0` (proposed offshore Ocean Docks approach).
- End: `-1090.0, 700.0, 0.0` (proposed offshore Easter Basin waiting area, **not a berth**).
- Total segment distance ~13,454 GTA coordinate units. At 4 units/s, theoretical uninterrupted travel time ~56 minutes, excluding turns and port handling.
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
