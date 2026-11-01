# SHIP-LS-OCEAN-01 — smooth offshore departure test

This is an **opt-in development trial**, not a replacement for the existing production maritime preset. The cargo vessel is a 230-unit-long assembly and the route intentionally extends beyond the ordinary GTA SA ±3000 map square; collision, sea, streaming and passenger behavior are **not certified** by the CI geometry contracts.

## Intended trajectory

The established Los Santos berth remains `X 2682, Y -2273, Z 7, yaw 0.9°`. No save files are changed by this feature.

| Leg | Root X | Root Y | Mesh yaw | Mode |
|---|---:|---:|---:|---|
| Berth / approach | 2682 → 3092 | approximately -2273 → -2266.56 | 0.9° | straight east at 2 u/s |
| Turn 1 (18 tangential segments, R=308) | 3092 → 3400 | -2266.56 → -2574.56 | 0.9° → -89.1° | continuous east-to-south arc at 2.3 u/s |
| Southbound leg | **3400** | -2574.56 → **-2980** | -89.1° | straight south at 3 u/s |
| Safety hold | **3400** | **-2980** | -89.1° | stopped pending visual confirmation |
| Resume south | **3400** | -2980 → **-3100** | -89.1° | 3 u/s |
| Turn 2 (18 tangential segments, R=300) | 3400 → 3100 | -3100 → **-3400** | -89.1° → -179.1° | continuous south-to-west arc at 2.3 u/s |
| South offshore lane | 3100 → 2400 | **-3400** | -179.1° | straight west at 4 u/s, then stop |

Why: using X=3400 keeps the eastbound turn farther away from the LS coastline; running west on Y=-3400 puts the straight southern portion ~400 game units beyond the conventional -3000 boundary, versus the old LS→SF preset near Y=-2985. These are geometric design offsets, **not verified clearances of the full hull or visible map geometry**.

## Commands / on-server trial

Install a compiled AMX containing this change, start as founder in `/aduty`. The existing `LS BERTH` file must still match the approved coordinates within ±12 units and yaw within 5°. The trial may begin either while stopped at the saved berth or on its eastbound straight approach around X=3092, Y=-2267. **Before starting, move all players away from the ship.**

```text
/ship status
/ship oceanprobe on
/ship oceanroute start
```

Watch the ship; it follows the first arc and southward straight. At **X=3400 Y=-2980**, the controller stops on its own, without teleportation. Inspect the hull, water, streaming, turn smoothness and gap to coast.

If this segment is clear and the client remains stable, and players are away:

```text
/ship oceanroute continue
```

This moves farther south, takes the second smooth turn and continues west to **X=2400 Y=-3400** before stopping. For monitoring:

```text
/ship oceanroute status
/ship status
```

**Emergency stop** (works at any point):

```text
/ship stop
```

Cancels the test and stops the root at its current position without a teleport. No automatic restart occurs after a server reboot.

**Recovery**: with no nearby players, after stopping, use `/ship set 2991.96 -2268.13 7.0 0.89` while oceanprobe remains enabled. Then `/ship oceanprobe off`. This is a debug teleport, not a passenger-safe operation. Return to saved berth if required: `/ship set 2682.0 -2273.0 7.0 0.9`. Do **not** use `/ship save` at an out-of-map location.

## Non-goals and safety

- No modifications to original `gShipPresetLsSf` or port `berth/exit` configuration.
- Existing `/ship port ls depart` continues to use old route logic. To start this *experimental* path use `/ship oceanroute start`, not `port ls depart`.
- The trial is guarded by `/ship oceanprobe on`. This flag resets to OFF on server initialization.
- It refuses to run while another ship movement controller or editor is active; the dispatcher rejects other editing commands during the trial.
- The hold requires explicit `continue`, a free hull and exact checkpoint position, so nothing automatically proceeds beyond the map boundary without approval.
- No route/ship/berth file persistence outside vanilla coordinates. Open water and object rendering beyond ±3000 cannot be assumed merely from successful Pawn compilation.
- After visual verification, decide whether to integrate this offshore lane with the remaining westward and northbound SF route. Full automatic LS→SF remains a separate integration milestone.
