# SHIP-PORT-01 — manual docking configuration and safe outbound stage

This is the **first implementation stage**, not automatic berthing or a round-trip scheduler. It does not invent harbor wall coordinates or certify clearance for the approximately 230-unit freighter.

## Commands
Founder in `/aduty`. Keep the freighter stationary while configuring:
- `/ship port ls berth` — save the **current hull transform** as the Los Santos berth.
- `/ship port ls exit` — save the hull transform in a tested clear-water exit position.
- `/ship port ls depart` — only if currently within 8 GTA units of recorded berth. Slowly move to the recorded exit at 1.0 units/sec, then seamlessly continue on the 36-point LS→SF sea route.
- `/ship port sf berth|exit|depart` — same for outbound SF→LS.
- `/ship port` — report configured status.
- `/ship stop` — immediately cancel the maneuver and stop the root object.

Port markers are saved separately in `scriptfiles/shipstudio/ports_01.dat` so they cannot overwrite the existing hull/route saved in `cargo_01.dat`. Preserve this file when moving hosts.

## How to set up
1. With the ship stationary, move its root via `/ship set X Y Z angle` until the ENTIRE hull is safely aligned with the LS quay. Record `/ship port ls berth`.
2. Move the hull to a confirmed open-water exit that has no piers, cranes, shallow water or bridges along its straight trajectory. Record `/ship port ls exit`.
3. Return to the **same saved berth** using `/ship set`, then issue `/ship port ls depart`. No player/root teleport occurs when departing.
4. Test while one passenger stands/walks on deck; inspect with `/shipsurf watch <id>`.
5. Repeat configuration in SF after validating approach geometry.

Exit must be 10–600 units from the berth and within 350 units of the sea-route endpoint. This guards against accidentally using a far-away or wrong port exit. The entire LINE between berth and exit must still be tested in GTA San Andreas; these checks do not validate collision clearance.

## Not included yet
- No scheduled automatic departure.
- No arrival docking or berth approach trajectory; existing sea route still terminates at offshore holding coordinates.
- No automated gangway, mooring animation or cargo lifting.
- Original test command `/ship run` can still reposition the vessel at the first waypoint. **Use /ship port ls depart for passengers** instead.

Other ship objects and the pre-existing saved plan remain compatible.

> Route notice: the v2 Los Santos outer-sea corridor is preliminary and still needs full-vessel collision and world-boundary testing before passenger service.
