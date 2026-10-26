# Ship Studio — built-in cargo ship assembly (SHIP-ASSEMBLY-01)

The default ship is now **a real assembled GTA San Andreas freighter**, rather than a route with no vessel.

## Startup & control

- At server start, if `scriptfiles/shipstudio/cargo_01.dat` exists, that file is loaded unchanged. The bundled ship **does not replace** an existing design. If the save is malformed the auto-spawn is blocked instead of clobbering it.
- With no saved blueprint the server creates a freighter near the LS start point `2640, -2740`, root Z `7.0`, rotation `0`, using vanilla GTA SA object IDs; **does not start moving automatically**.
- A founder who is on `/aduty` can use `/ship status`, `/ship preset lssf`, `/ship run`, `/ship stop`, `/ship save`, `/ship export`.
- To recreate the bundled default when another design is loaded, first save/export it, then `/ship clear` and `/ship cargo`. The `clear` command only deletes in-memory objects; it does not delete the save file.
- Use `/ship save` to persist the built-in ship; otherwise a missing-file server restart reconstructs it.
- `/ship preset sfls` loads the reverse route. The built-in hull pivot is 7.0 above sea level, **not 0.0**: only the built-in model 9585 receives this route Z correction.

## Components reconstructed from San Fierro cargo freighter

Root model `9585 freight_SFW33` — original IPL origin `(-2409.84, 1544.95, 7.0)`.
Every other part uses its original in-game IPL position minus the root's origin. This reproduces the base-game grouping, with no arbitrary 25/50/75 metre container spacing:

| ID | Component | IPL position |
| --- | --- | --- |
| 9586 | freight_deck_SFW | -2412.12, 1544.95, 17.05 |
| 9761 | freight_alfa_SFW | -2411.39, 1544.95, 27.0781 |
| 9584 | freight_SFW31 | -2485.08, 1544.95, 26.1953 |
| 9590 | freight_interiorsfw | -2403.51, 1544.95, 8.7188 |
| 9587 | freight_box_SFW01 | -2401.14, 1544.94, 23.5938 |
| 9588 | freightbox_inSFw | -2404.21, 1544.94, 7.5859 |
| 9589 | frght_BOXES08 | -2389.40, 1544.88, 4.03 |
| 9604 | frght_BOXES19 | -2392.59, 1545.91, 24.8594 |
| 9698 | BRIDGE_argh | -2473.59, 1543.77, 29.0781 |
| 9819 | shpbridge_sfw02 | -2470.45, 1551.12, 33.1406 |
| 9821 | shpbridge_sfw03 | -2474.36, 1547.24, 24.7500 |
| 9822 | shpbridge_sfw08 | -2470.94, 1550.75, 32.9062 |

IPL positions corroborated by object catalogs: https://citwiki.com/CITtools/Objects/9585/ , https://citwiki.com/CITtools/Objects/9586/ , https://dev.prineside.com/en/gtasa_samp_model_id/model/9604 and associated model detail pages. Some original mapping records do not expose full Euler rotations, so **visual alignment and attachment rotation must still be verified** on an open.mp client.

## Hazards / next validation

1. Model 9585 has approx. **230 m length, 36 m width**; a root position in open water does not by itself prove the whole hull misses the shore or the map boundary.
2. The route is an **unverified navigation draft**. It still needs a swept-hull check against coasts, docks, bridges and dynamic vehicles. The built-in freighter is not considered collision-certified.
3. Players on a moving multi-object deck have not been validated. Keep test voyages private and do not transport players yet.
4. Existing route test logic teleports to the first waypoint then sails at 4 units/sec; it is not a public scheduled freight service.
5. No automatic removal of the original static ship in San Fierro; this implementation creates a second copy near Los Santos, leaving the base-game map intact.
6. This is a **13-model assembled ship** (root + 12 components), not a consolidated .dff model or a native vehicle.

The initial source supplied by the user used these models with mostly zero offsets. This version reconstructs offsets from the game's original ship rather than using those placeholders.
