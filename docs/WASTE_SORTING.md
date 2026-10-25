# SORT-01 — Sorting station / TextDraw

Implemented in `gamemodes/includes/waste_sort.inc`, connected from
`gamemodes/signal0317.pwn` and `gamemodes/includes/commands.inc`.

## How to test

1. Connect to TARKENSO, authenticate and begin a municipal collector or truck-driver shift at Fort Carson.
2. Walk **on foot** to provisional coordinates **(-228.0, 1079.0, 19.7422)** in world/interior 0 (near the municipal yard). Check the floating `WASTE SORTING / ALT or /sort` label.
3. Press left ALT or enter `/sort`. A centred dark native player-TextDraw UI opens with the bag model (1265), **six clickable 3D material previews** and categories **PAPER / PLASTIC / METAL**.
4. Click one item, then click the corresponding category. The chosen item disappears, progress increases; an incorrect category leaves the item and displays an error.
5. Complete six items; inspect the result. Click **X** or press **ESC** to close; reopen for a fresh bag.
6. Verify outside shift, far from the station, in a vehicle, or a different interior/world: the activity must not start. Verify disconnect, death and respawn clean up TextDraw instances.

**Interaction:** vanilla open.mp does not expose free cursor drag movements through TextDraw callbacks; the gameplay uses two clicks (item, category). The screenshot design is a target reference rather than a pixel-accurate rendering. Native GTA textdraw fonts cannot reliably show Cyrillic on stock clients, so material names, prompts and hints are ASCII English for now.

**Location:** provisional. Check the position and geometry in-game before making the job public. No objects or SQL migrations are required for this demo. It does **not** alter garbage-delivery accounting, driver cargo, experience, salary or account balance. A paid, persistent processing task should be added separately with transactional accounting to prevent farming.

**Graphics (SORT-02):** six separate clickable `PlayerTextDraw` model previews (`TEXT_DRAW_FONT_MODEL_PREVIEW`, font 5), with name captions; not static PNG sprites. They use verified, stock GTA SA models:

| Category | Item name | Model | GTA SA model |
| --- | --- | --- | --- |
| PAPER | MAGAZINES | 2852 | `gb_bedmags02` |
| PAPER | CARDBOARD | 1220 | `cardboardbox2` |
| PLASTIC | DRINK CUP | 2647 | `CJ_BS_CUP` |
| PLASTIC | SLUSH CUP | 2874 | `CJ_SLUSH_CUPDUM` |
| METAL | JUICE CAN | 2601 | `CJ_JUICE_CAN` |
| METAL | METAL TINS | 2057 | `Flame_tins` |

Model identifiers were checked against the GTA SA object catalogue (CIT Tools / Prineside); all are in the unmodified game. Plastic **bottles** are deliberately not substituted with glass bottles: two disposable cups represent plastic at this stage.

The bag remains a GTA stock trash-bag model (1265), not an opened photorealistic bag. The conceptual screenshot cannot be reproduced pixel for pixel with server-only TextDraws; that would need different artwork/texture strategy.

**Rendering and input UAT:** ensure every item shows the correct 3D object (not yellow-black question mark), all item previews are clickable, captions remain readable, picked items disappear with their captions, category clicks work, ESC closes the game, and at 16:9 and 4:3 no elements spill outside the panel. Pawn compilation checks model API availability but cannot confirm in-game client-side appearance or texture loading.

**Safety:** actions are validated in Pawn on every category click, not inferred from UI state. Per-player selection and completion are reset at each opening and on lifecycle events. The TextDraws are destroyed after closing, and ESC is handled without recursively cancelling selection.
