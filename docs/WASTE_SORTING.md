# SORT-01 — Sorting station / TextDraw

Implemented in `gamemodes/includes/waste_sort.inc`, connected from
`gamemodes/signal0317.pwn` and `gamemodes/includes/commands.inc`.

## How to test

1. Connect to TARKENSO, authenticate and begin a municipal collector or truck-driver shift at Fort Carson.
2. Walk **on foot** to provisional coordinates **(-228.0, 1079.0, 19.7422)** in world/interior 0 (near the municipal yard). Check the floating `WASTE SORTING / ALT or /sort` label.
3. Press left ALT or enter `/sort`. A centred dark native player-TextDraw UI opens with one GTA bag model preview (1265), six item labels and categories **PAPER / PLASTIC / METAL**.
4. Click one item, then click the corresponding category. The chosen item disappears, progress increases; an incorrect category leaves the item and displays an error.
5. Complete six items; inspect the result. Click **X** or press **ESC** to close; reopen for a fresh bag.
6. Verify outside shift, far from the station, in a vehicle, or a different interior/world: the activity must not start. Verify disconnect, death and respawn clean up TextDraw instances.

**Interaction:** vanilla open.mp does not expose free cursor drag movements through TextDraw callbacks; the gameplay uses two clicks (item, category). The screenshot design is a target reference rather than a pixel-accurate rendering. Native GTA textdraw fonts cannot reliably show Cyrillic on stock clients, so material names, prompts and hints are ASCII English for now.

**Location:** provisional. Check the position and geometry in-game before making the job public. No objects or SQL migrations are required for this demo. It does **not** alter garbage-delivery accounting, driver cargo, experience, salary or account balance. A paid, persistent processing task should be added separately with transactional accounting to prevent farming.

**Graphics:** built entirely from server-side textdraw boxes, labels and the built-in rubbish bag model. A realistic *open* bag with item thumbnails like the concept image would require a compatible texture/sprite asset and a different client strategy; no asset or client module is included in SORT-01.

**Safety:** actions are validated in Pawn on every category click, not inferred from UI state. Per-player selection and completion are reset at each opening and on lifecycle events. The TextDraws are destroyed after closing, and ESC is handled without recursively cancelling selection.
