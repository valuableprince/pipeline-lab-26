# Admin flight /fly (experimental)

Use `/aduty` and then `/fly`. Allowed for staff level 2+ using existing `PERM_MODERATE` permission. Toggle off with another `/fly` command.

Native open.mp flight uses zero per-player gravity and server-side velocity pulses every 100 ms. Controls: movement inputs W/S and A/D relative to camera direction, jump to ascend and sprint to descend (actual key bindings follow player client configuration). Turn the camera to steer. The administrator must remain on foot, outside races, disciplinary confinement, jetpack use, and with `/aduty` active. Switching states, dying, disconnecting, ending admin duty or ending the game mode restores captured gravity.

This is a **first playable prototype**, not a cheat or custom client modification. Player anti-cheat or remote vehicle interactions may require additional exceptions. Flight controls and gravity RPC need a live open.mp test. Do not use it to carry passengers or drive a vehicle.
