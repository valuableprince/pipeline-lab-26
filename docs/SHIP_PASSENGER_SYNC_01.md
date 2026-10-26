# Ship Passenger Sync — stage 1: surfing diagnostics

**Status:** read-only test instrumentation. Disabled by default, included only in `feat/cargo-ship-assembly`. The public `main` branch is unchanged.

The cargo ship from Ship Studio is one moving root plus native attached objects. The client may report surfing the moving root or one of its attached children via `GetPlayerSurfingObjectID`. open.mp supports native surfing on moving objects, but that alone does not establish reliable player retention on attached decks during turns.

## Testing commands

Founder account with `/aduty`:
- `/shipsurf watch 4` — enable a fresh 400 ms diagnostic session for player ID 4.
- `/shipsurf status 4` — show samples, recognized ship surf samples, entries, losses, active object/component, last/max local step and native surfing offsets.
- `/shipsurf off 4` — disable and reset metrics.

The observed passenger may be any connected player on foot in world 0, interior 0. They do not need founder privileges; only the developer who issues the control command does.

**The module does not teleport, freeze, move, or attach the player.** There is deliberately no `SetPlayerPos`, `SetPlayerVelocity`, or `OnPlayerUpdate` hook. Corrections can be implemented later **only after observing a reproducible failure**; forced compensation during jumping or walking creates more problems than it fixes.

## What the counters mean

- `samples`: polling iterations while the watched player was connected, on foot, in world 0/interior 0.
- `hits`: ticks where the native surfing object matches the ship root or one of the current attached objects.
- `entries`: transitions into a confirmed ship surfing state.
- `losses`: transitions out of confirmed ship surfing. **Not equivalent to falling overboard**: a jump, client streaming, or an animation can also clear surfing.
- `component`: 0 = root, 1..N = one of attached parts; -1 = not a ship object.
- `local step`: change of player coordinates relative to the rotated ship frame between consecutive confirmed surfing samples. Deliberate walking can increase it; large values are only a signal to inspect, not proof of slippage.
- `native surf offset`: open.mp output from `GetPlayerSurfingOffsets`, gathered only on confirmed ship surfing samples.

The system only reports **recognized object IDs**. Players standing near the ship but not recognized as surfing will not be assumed passengers.

## Manual test matrix

1. **Still ship:** one player stands on the root or deck. Monitor recognition and component.
2. **Straight line:** `/ship move 50`, check whether the player's client follows the moving assembly.
3. **Rotation:** `/ship turn 10`, check for unexpected lateral displacement and surfing loss.
4. **Walking:** walk across attached object boundaries while sailing and watch whether reported object IDs switch reliably.
5. **Jumping:** confirm a legitimate airborne surfing loss is not treated as a reason to teleport.
6. **Multiple clients:** compare observations from 2 then 10-20 players under varying pings; observe other players' positions, not just the local screen.
7. **Disconnect/reconnect:** verify the watcher state and metrics reset. Stop the ship immediately if there is clipping or desynchronization.

Diagnostics are cheap but not free: they scan the fixed player slot array once per 400 ms, and only obtain surfing/position data for explicitly watched players.

## Follow-up

Only if tests reproduce surfing loss, implement a separate opt-in experimental compensation mode. It must require confirmed prior surfing, track object transform and measured player input/velocity, reject vehicles, spectators, jumping and distant players, cap corrections, and never run uncontrolled on every player update. Evaluate with in-game regression first.
