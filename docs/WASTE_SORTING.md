# TARKENSO — Waste sorting (SORT-01)

Implemented in gamemodes/includes/waste_sort.inc. After login, start the cleaner / truck shift, walk to (-228.0, 1079.0, 19.7422) in Fort Carson (interior/world 0) and press ALT or /sort.

The mini-game shows six selectable GTA object previews on a dark background, without object captions or a 3D bag model. Click an item, then the corresponding bin. Bin labels are now in Russian: **БУМАГА / ПЛАСТИК / МЕТАЛЛ**. Incorrect categories produce an error; after the sixth correct selection, the result is shown in chat and the UI automatically closes; ESC or X closes it early. Reward calculations, database storage and bag-delivery flow are unchanged.

**Cyrillic pilot:** the title **СОРТИРОВКА ОТХОДОВ**, section labels, all three material names, instructions and feedback are now Cyrillic TextDraw strings (built as CP1251). These require the open.mp-compatible rustext server plugin. Installation, checksums, rollback and game-client verification: [RUSSIAN_TEXTDRAW.md](RUSSIAN_TEXTDRAW.md). Test on a GTA client: successful server build is not visual proof.

**Server controls:** Sorting is restricted to a signed-in player on a permitted shift, on foot and within range of the station. Every click revalidates category and player's location. PlayerTextDraw objects are destroyed on close and player reset. Mouse-dragging is not supported in the stock TextDraw callbacks; two clicks are used. No SQL migration is required.

## SORT-03: randomized catalog (30 models)

Each opening (ALT or /sort) now generates a new **server-side, per-player** set of six distinct models from a catalog of **30**: 10 PAPER, 10 PLASTIC and 10 METAL. The selection always contains **at least one model of each category**, and then fills the remaining three places without replacement; the six positions are shuffled independently of categories. A new opening is guaranteed to differ in at least one model from the previous opening by that player during the current connection, even in the unlikely event that the random sample repeats.

The UI continues to use the same six clickable 3D previews and three Russian category buttons. Item cards are neutral dark grey so the correct answer cannot be guessed from the old category-specific background colours; the selected item highlights gold. Correct classification is looked up by the server's *catalog index*, not inferred from the client or its visual layout. At 6/6, the existing chat completion result and immediate window close remain unchanged. No external 3D assets, database migrations or client-side modules required.

Catalog:
- **Бумага (10):** magazines 2852; cardboard boxes 1220, 1221, 1230; scattered booklets 2059; rhyme book 2894; pizza box 1582; paper fast-food bag 2663; cereal cartons 19561, 19562.
- **Пластик (10):** disposable cup 2647; food tray 2767; meat tray 19560; ice-cream tubs 19567, 19568; milk bottle 19570; plastic tray 19587; shopping basket 19592; traffic cone 1238; disposable cup stack 2437.
- **Металл (10):** can 2601; wrench 18633; crowbar 18634; hammer 18635; screwdriver 18644; metal tins 2057; frying pan 19581; knife 19583; saucepan 19584; spatula 19586.

**Art UAT required:** model ID existence and the Pawn compilation are checked, but the actual item appearance and physical material cannot be confirmed automatically. The SA/MTA catalog and the [official SA-MP additions list](https://open.mp/docs/scripting/resources/samp_objects) were consulted. Specifically review milk bottle, trays, paper bag and containers for recognizable materials, and test larger models' preview scaling. Any ambiguous models should be replaced before this gameplay is used for paid rewards.

**Regression checks:** independently open /sort on two concurrent players; reopen after ESC, X, or 6/6; verify no repeats within a single selection, at least one of each material, shuffled positions and a differing set across consecutive openings. A disconnect resets that player's previous-set memory. No persistent accounts or job progress are changed.

**UAT:** verify text is legible in all states, Russian category names remain clickable, finishing 6/6 immediately removes the panel and mouse selection, item previews do not overlap and do disappear on success, wrong categories do not advance, ESC/X work, and client rendering at 4:3 and 16:9.
