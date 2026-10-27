# Ship Camera: one-command read-only observer

Command: **`/shipcam`** (admin level 2+ and active `/aduty`). Do not use `/ship cam top` etc. All operations live in a mouse-selectable **right-hand TextDraw panel**.

## Modes and buttons

TOP / REAR / FRONT / LEFT SIDE / RIGHT SIDE / ANGLE 45 / FREE ORBIT, ROTATE - / ROTATE + (sets orbit and turns by 15 degrees), ZOOM + / ZOOM -, EXIT CAMERA. ESC closes the camera. Issuing `/shipcam` again closes it. The ship carries on as before; no movement, route, berth or collision changes are made.

## Technical approach

Native `SetPlayerCameraPos` and `SetPlayerCameraLookAt` updated every 200 ms. This timer runs **only while someone is observing** and iterates active observers, not all online players. The camera pose is derived from the moving root's latest location and hull rotation. The admin character is frozen, temporarily anchored in the same world above the hull (165 units above its pivot) to support regular CreateObject streaming while far from the original player location. The anchor updates only when it has fallen more than 50 units behind the ship. Exiting restores the original XYZ/heading, camera and controls. No add-on client, ColAndreas or raycasts.

Observation is denied if the ship doesn't exist or the administrator is driving, flying, editing the ship, sorting items, racing or being punished. Each administrator has independent view/magnification; multiple staff can observe simultaneously.

### Known limitations / in-game tests

The stock server-side objects and GTA client may respond to distant camera placement and world streaming differently across open.mp builds. Confirm that the **full hull and its attached deck** remain visible for the entire LS→SF voyage and after route turns, not only at the home port. Because the administrator is temporarily relocated above the ship, watch for any anti-teleport logic and ensure a disconnect, death or staff duty change leaves a valid player state. The mode is only available on foot in world 0/interior 0; test restoration exactly to the original point. Other active camera modes may need future interoperability guards. Timed updates cannot be assumed to look cinematic without testing at real game frame rates.

This is a read-only spectator overlay rather than `/ship edit`. It needs manual acceptance testing in-game even when CI compiles successfully.
