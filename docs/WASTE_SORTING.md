# TARKENSO — Waste sorting (SORT-01)

Implemented in gamemodes/includes/waste_sort.inc. After login, start the cleaner / truck shift, walk to (-228.0, 1079.0, 19.7422) in Fort Carson (interior/world 0) and press ALT or /sort.

The mini-game shows six selectable GTA object previews on a dark background, without object captions or a 3D bag model. Click an item, then the corresponding bin. Bin labels are now in Russian: **БУМАГА / ПЛАСТИК / МЕТАЛЛ**. Incorrect categories produce an error; after the sixth correct selection, the result is shown in chat and the UI automatically closes; ESC or X closes it early. Reward calculations, database storage and bag-delivery flow are unchanged.

**Cyrillic pilot:** the title **СОРТИРОВКА ОТХОДОВ**, section labels, all three material names, instructions and feedback are now Cyrillic TextDraw strings (built as CP1251). These require the open.mp-compatible rustext server plugin. Installation, checksums, rollback and game-client verification: [RUSSIAN_TEXTDRAW.md](RUSSIAN_TEXTDRAW.md). Test on a GTA client: successful server build is not visual proof.

**Server controls:** Sorting is restricted to a signed-in player on a permitted shift, on foot and within range of the station. Every click revalidates category and player's location. PlayerTextDraw objects are destroyed on close and player reset. Mouse-dragging is not supported in the stock TextDraw callbacks; two clicks are used. No SQL migration is required.

**UAT:** verify text is legible in all states, Russian category names remain clickable, finishing 6/6 immediately removes the panel and mouse selection, item previews do not overlap and do disappear on success, wrong categories do not advance, ESC/X work, and client rendering at 4:3 and 16:9.
