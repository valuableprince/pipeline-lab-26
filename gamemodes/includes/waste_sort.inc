#if defined _tarkenso_waste_sort_included
    #endinput
#endif
#define _tarkenso_waste_sort_included

// SORT-01: native PlayerTextDraw mini-game, without a client mod.
// GTA/open.mp textdraw selection uses two clicks, not mouse drag events.
// Coordinates are provisional and must be checked in Fort Carson UAT.
#define SORT_STATION_X (-228.0)
#define SORT_STATION_Y (1079.0)
#define SORT_STATION_Z (19.7422)
#define SORT_STATION_RADIUS_SQ (16.0)
#define SORT_ITEM_COUNT (6)
#define SORT_MODELS_PER_CATEGORY (10)
#define SORT_CATALOG_COUNT (SORT_CATEGORY_COUNT * SORT_MODELS_PER_CATEGORY)
#define SORT_CATEGORY_COUNT (3)
#define SORT_TD_ITEM_START (11)
#define SORT_TD_CATEGORY_START (17)
#define SORT_TD_TOTAL (22)

#define SORT_COLOR_GOLD (0xF1C263FF)
#define SORT_COLOR_TEXT (0xEDF4FFFF)
#define SORT_COLOR_MUTED (0xAFBDCAFF)
#define SORT_COLOR_PAPER (0x7B5B27E8)
#define SORT_COLOR_PLASTIC (0x26475EE8)
#define SORT_COLOR_METAL (0x44505CE8)

// 30 curated stock game / SA-MP models, grouped by material: 10 per group.
// Entries are stable catalog indices; a per-player session maps six UI slots
// to selected indices. Art / material recognition still needs client UAT.
new const gSortCatalogModels[SORT_CATALOG_COUNT] =
{
    2852, // Magazines
    1220, // Cardboard box #1
    1221, // Cardboard box #2
    1230, // Cardboard box #3
    2059, // Scattered booklets
    2894, // Rhyme book
    1582, // Pizza box
    2663, // Fast food paper bag
    19561, // Cereal carton #1
    19562, // Cereal carton #2
    2647, // Disposable drink cup
    2767, // Fast food tray
    19560, // Supermarket meat tray
    19567, // Ice cream tub #1
    19568, // Ice cream tub #2
    19570, // Milk bottle
    19587, // Plastic tray
    19592, // Shopping basket
    1238, // Traffic cone
    2437, // Stack of disposable cups
    2601, // Juice can
    18633, // Wrench
    18634, // Crowbar
    18635, // Hammer
    18644, // Screwdriver
    2057, // Metal tins
    19581, // Frying pan
    19583, // Kitchen knife
    19584, // Saucepan
    19586 // Spatula
};
new const Float:gSortCatalogRot[SORT_CATALOG_COUNT][4] =
{
    {35.0, 0.0, 110.0, 1.05}, // Magazines
    {-10.0, 0.0, 25.0, 1.00}, // Cardboard box #1
    {-10.0, 0.0, -20.0, 1.00}, // Cardboard box #2
    {-10.0, 0.0, 20.0, 1.00}, // Cardboard box #3
    {45.0, 0.0, 35.0, 1.00}, // Scattered booklets
    {35.0, 0.0, 40.0, 1.00}, // Rhyme book
    {30.0, 0.0, 20.0, 1.00}, // Pizza box
    {-10.0, 0.0, 25.0, 1.00}, // Fast food paper bag
    {-10.0, 0.0, 25.0, 1.00}, // Cereal carton #1
    {-10.0, 0.0, -20.0, 1.00}, // Cereal carton #2
    {-12.0, 0.0, -12.0, 0.80}, // Disposable drink cup
    {35.0, 0.0, 20.0, 1.00}, // Fast food tray
    {35.0, 0.0, 25.0, 1.00}, // Supermarket meat tray
    {-10.0, 0.0, 35.0, 1.00}, // Ice cream tub #1
    {-10.0, 0.0, -25.0, 1.00}, // Ice cream tub #2
    {-10.0, 0.0, 25.0, 1.00}, // Milk bottle
    {35.0, 0.0, 25.0, 1.00}, // Plastic tray
    {20.0, 0.0, 25.0, 1.00}, // Shopping basket
    {-10.0, 0.0, 25.0, 1.00}, // Traffic cone
    {25.0, 0.0, -20.0, 0.85}, // Stack of disposable cups
    {-18.0, 0.0, -18.0, 0.80}, // Juice can
    {-10.0, 0.0, 22.0, 1.00}, // Wrench
    {-10.0, 0.0, 30.0, 1.00}, // Crowbar
    {-10.0, 0.0, -25.0, 1.00}, // Hammer
    {-10.0, 0.0, 20.0, 1.00}, // Screwdriver
    {-10.0, 0.0, 30.0, 1.00}, // Metal tins
    {25.0, 0.0, 50.0, 1.00}, // Frying pan
    {20.0, 0.0, 35.0, 1.00}, // Kitchen knife
    {20.0, 0.0, -25.0, 1.00}, // Saucepan
    {15.0, 0.0, 45.0, 1.00} // Spatula
};
new const gSortCategoryNames[SORT_CATEGORY_COUNT][12] = {"БУМАГА", "ПЛАСТИК", "МЕТАЛЛ"};

new PlayerText:gSortDraws[MAX_PLAYERS][SORT_TD_TOTAL];
new bool:gSortOpen[MAX_PLAYERS];
new gSortSelection[MAX_PLAYERS];
new gSortCompleted[MAX_PLAYERS];
new gSortErrors[MAX_PLAYERS];
// A random catalog index per UI slot; never trust clients for classification.
new gSortRoundItems[MAX_PLAYERS][SORT_ITEM_COUNT];
new gSortLastRound[MAX_PLAYERS][SORT_ITEM_COUNT];
new bool:gSortHasLastRound[MAX_PLAYERS];
new Text3D:gSortStationLabel = Text3D:INVALID_3DTEXT_ID;

stock SortUI_Init()
{
    gSortStationLabel = Create3DTextLabel(
        "{F1C263}СОРТИРОВКА ОТХОДОВ\n{FFFFFF}ALT или /sort",
        SORT_COLOR_GOLD, SORT_STATION_X, SORT_STATION_Y,
        SORT_STATION_Z + 1.1, 12.0, 0, false);
    return 1;
}

stock SortUI_Shutdown()
{
    if (gSortStationLabel != Text3D:INVALID_3DTEXT_ID)
        Delete3DTextLabel(gSortStationLabel);
    gSortStationLabel = Text3D:INVALID_3DTEXT_ID;
    return 1;
}

stock bool:SortUI_CanUse(playerid)
{
    if (!IsPlayerConnected(playerid) || gAuthState[playerid] != AUTH_READY ||
        (!gCleanerDuty[playerid] && !gTruckOnShift[playerid]) ||
        GetPlayerState(playerid) != PLAYER_STATE_ONFOOT ||
        GetPlayerInterior(playerid) != 0 ||
        GetPlayerVirtualWorld(playerid) != 0) return false;

    new Float:x, Float:y, Float:z;
    GetPlayerPos(playerid, x, y, z);
    x -= SORT_STATION_X;
    y -= SORT_STATION_Y;
    z -= SORT_STATION_Z;
    return x*x + y*y + z*z <= SORT_STATION_RADIUS_SQ;
}

// Cyrillic strings compile from UTF-8 sources to CP1251 in the build;
// rustext v2.0.11 nomemhack handles TextDraw RPC conversion on the server.
// All visible category and status labels use Cyrillic (CP1251 in the build).
// The server still validates every item and category click.
// Font 4 treats TextSize as width/height. Unlike font-1 dummy text with
// near-zero LetterSize Y, this renders a real filled rectangle in stock GTA.
stock SortUI_Rect(playerid, slot, Float:x, Float:y,
    Float:width, Float:height, colour)
{
    new PlayerText:td = CreatePlayerTextDraw(playerid, x, y, "LD_SPAC:white");
    gSortDraws[playerid][slot] = td;
    PlayerTextDrawFont(playerid, td, TEXT_DRAW_FONT_SPRITE);
    PlayerTextDrawAlignment(playerid, td, TEXT_DRAW_ALIGN_LEFT);
    PlayerTextDrawTextSize(playerid, td, width, height);
    PlayerTextDrawColour(playerid, td, colour);
    return 1;
}

stock SortUI_Add(playerid, slot, Float:x, Float:y, const label[],
    Float:width, Float:height, Float:letterX, Float:letterY,
    colour, boxColour = 0, bool:selectable = false)
{
    new PlayerText:td = CreatePlayerTextDraw(playerid, x, y, label);
    gSortDraws[playerid][slot] = td;
    PlayerTextDrawFont(playerid, td, TEXT_DRAW_FONT_1);
    PlayerTextDrawAlignment(playerid, td, TEXT_DRAW_ALIGN_LEFT);
    PlayerTextDrawLetterSize(playerid, td, letterX, letterY);
    PlayerTextDrawColour(playerid, td, colour);
    PlayerTextDrawBackgroundColour(playerid, td, 0x111A22FF);
    PlayerTextDrawSetProportional(playerid, td, true);
    PlayerTextDrawSetOutline(playerid, td, 1);
    PlayerTextDrawTextSize(playerid, td, x + width, y + height);
    if (boxColour)
    {
        PlayerTextDrawUseBox(playerid, td, true);
        PlayerTextDrawBoxColour(playerid, td, boxColour);
    }
    if (selectable) PlayerTextDrawSetSelectable(playerid, td, true);
    return 1;
}

// Guarantee one of each material, then draw 3 additional unique models
// without replacement from the other 27. Shuffle the six visible slots so
// their screen positions do not disclose the category.
stock SortUI_RollItems(playerid)
{
    new pool[SORT_CATALOG_COUNT];
    new poolSize = 0;

    for (new category = 0; category < SORT_CATEGORY_COUNT; category++)
        gSortRoundItems[playerid][category] =
            category * SORT_MODELS_PER_CATEGORY + random(SORT_MODELS_PER_CATEGORY);

    for (new catalog = 0; catalog < SORT_CATALOG_COUNT; catalog++)
    {
        new bool:chosen = false;
        for (new slot = 0; slot < SORT_CATEGORY_COUNT; slot++)
        {
            if (gSortRoundItems[playerid][slot] == catalog)
            {
                chosen = true;
                break;
            }
        }
        if (!chosen) pool[poolSize++] = catalog;
    }
    for (new slot = SORT_CATEGORY_COUNT; slot < SORT_ITEM_COUNT; slot++)
    {
        new pick = random(poolSize);
        gSortRoundItems[playerid][slot] = pool[pick];
        poolSize--;
        pool[pick] = pool[poolSize];
    }

    // If random selection happened to reproduce the previous six-model set,
    // replace one choice with an unused model from the same material group.
    // Thus consecutive sessions always contain at least one different model.
    if (gSortHasLastRound[playerid])
    {
        new bool:unchanged = true;
        for (new oldSlot = 0; oldSlot < SORT_ITEM_COUNT; oldSlot++)
        {
            new bool:found = false;
            for (new slot = 0; slot < SORT_ITEM_COUNT; slot++)
            {
                if (gSortLastRound[playerid][oldSlot] ==
                    gSortRoundItems[playerid][slot])
                {
                    found = true;
                    break;
                }
            }
            if (!found)
            {
                unchanged = false;
                break;
            }
        }
        if (unchanged)
        {
            new category = gSortRoundItems[playerid][0] / SORT_MODELS_PER_CATEGORY;
            for (new catalog = category * SORT_MODELS_PER_CATEGORY;
                 catalog < (category + 1) * SORT_MODELS_PER_CATEGORY; catalog++)
            {
                new bool:alreadyUsed = false;
                for (new slot = 0; slot < SORT_ITEM_COUNT; slot++)
                {
                    if (gSortRoundItems[playerid][slot] == catalog)
                    {
                        alreadyUsed = true;
                        break;
                    }
                }
                if (!alreadyUsed)
                {
                    gSortRoundItems[playerid][0] = catalog;
                    break;
                }
            }
        }
    }

    // Fisher-Yates shuffle of the six clickable model slots.
    for (new slot = SORT_ITEM_COUNT - 1; slot > 0; slot--)
    {
        new pick = random(slot + 1);
        new tmp = gSortRoundItems[playerid][slot];
        gSortRoundItems[playerid][slot] = gSortRoundItems[playerid][pick];
        gSortRoundItems[playerid][pick] = tmp;
    }
    for (new slot = 0; slot < SORT_ITEM_COUNT; slot++)
        gSortLastRound[playerid][slot] = gSortRoundItems[playerid][slot];
    gSortHasLastRound[playerid] = true;
    return 1;
}

stock SortUI_Status(playerid, const message[])
{
    PlayerTextDrawSetString(playerid, gSortDraws[playerid][8], message);
    PlayerTextDrawShow(playerid, gSortDraws[playerid][8]);
    return 1;
}

stock SortUI_Refresh(playerid)
{
    new completed = 0;
    for (new i = 0; i < SORT_ITEM_COUNT; i++)
    {
        new PlayerText:td = gSortDraws[playerid][SORT_TD_ITEM_START + i];
        if (gSortCompleted[playerid] & (1 << i))
        {
            completed++;
            PlayerTextDrawHide(playerid, td);
            continue;
        }
        // Neutral item tiles prevent revealing the answer by material colour.
        new box = 0x27323BEE;
        if (gSortSelection[playerid] == i) box = 0xD2A044FF;
        PlayerTextDrawBoxColour(playerid, td, box);
        PlayerTextDrawShow(playerid, td);
    }
    new progress[32];
    format(progress, sizeof(progress), "%d / %d", completed, SORT_ITEM_COUNT);
    PlayerTextDrawSetString(playerid, gSortDraws[playerid][9], progress);
    PlayerTextDrawShow(playerid, gSortDraws[playerid][9]);
    return completed;
}

stock SortUI_Close(playerid, bool:cancelSelection = true)
{
    if (!gSortOpen[playerid]) return 1;
    // Clear before CancelSelectTextDraw: ESC dispatches OnPlayerClickTextDraw.
    gSortOpen[playerid] = false;
    gSortSelection[playerid] = -1;
    for (new i = 0; i < SORT_TD_TOTAL; i++)
    {
        PlayerTextDrawHide(playerid, gSortDraws[playerid][i]);
        PlayerTextDrawDestroy(playerid, gSortDraws[playerid][i]);
    }
    if (cancelSelection && IsPlayerConnected(playerid))
        CancelSelectTextDraw(playerid);
    return 1;
}

stock SortUI_Reset(playerid)
{
    if (gSortOpen[playerid]) SortUI_Close(playerid);
    gSortOpen[playerid] = false;
    gSortSelection[playerid] = -1;
    gSortCompleted[playerid] = 0;
    gSortErrors[playerid] = 0;
    gSortHasLastRound[playerid] = false;
    return 1;
}

stock SortUI_Open(playerid)
{
    if (TDS_Active(playerid))
        return SendClientMessage(playerid, COLOR_RED, "[Tarkenso RPG] Сначала закройте TextDraw Studio через /tdstudio.");
    if (gSortOpen[playerid]) return 1;
    if (!SortUI_CanUse(playerid))
        return SendClientMessage(playerid, COLOR_RED,
            "Сортировка: требуется смена уборщика/водителя и место у станции в Fort Carson.");

    gSortSelection[playerid] = -1;
    gSortCompleted[playerid] = 0;
    gSortErrors[playerid] = 0;
    SortUI_RollItems(playerid);

    // Panel and title, viewport 640 x 448.
    // Sprite-backed panels: real 472x291 and 460x56 fills, not one-pixel
    // font-1 box strips. Keep all background draws before interactive items.
    SortUI_Rect(playerid, 0, 84.0, 82.0, 472.0, 291.0, 0x171B21F5);
    SortUI_Rect(playerid, 1, 90.0, 88.0, 460.0, 56.0, 0x242930FA);
    SortUI_Add(playerid, 2, 108.0, 96.0, "СОРТИРОВКА ОТХОДОВ", 265.0, 29.0,
        0.47, 1.95, SORT_COLOR_GOLD);
    SortUI_Add(playerid, 3, 109.0, 122.0, "Tarkenso RPG / ПЕРЕРАБОТКА", 300.0, 19.0,
        0.20, 0.95, SORT_COLOR_MUTED);
    // Independent dark zones for six selectable objects and three bins.
    // The large rubbish-bag 3D preview was removed at player request.
    SortUI_Rect(playerid, 4, 102.0, 153.0, 214.0, 177.0, 0x10161DF0);
    SortUI_Rect(playerid, 5, 324.0, 153.0, 214.0, 177.0, 0x131920F0);

    SortUI_Add(playerid, 6, 111.0, 158.0, "01 / ПРЕДМЕТЫ", 185.0, 16.0,
        0.26, 1.15, SORT_COLOR_GOLD);
    SortUI_Add(playerid, 7, 329.0, 174.0, "02 / КАТЕГОРИИ", 185.0, 17.0,
        0.26, 1.15, SORT_COLOR_GOLD);
    SortUI_Add(playerid, 8, 108.0, 348.0, "ВЫБЕРИТЕ ПРЕДМЕТ, ЗАТЕМ КАТЕГОРИЮ", 421.0, 17.0,
        0.22, 1.05, SORT_COLOR_TEXT);
    SortUI_Add(playerid, 9, 484.0, 125.0, "0 / 6", 54.0, 19.0,
        0.30, 1.35, SORT_COLOR_GOLD);
    SortUI_Add(playerid, 10, 522.0, 95.0, "X", 21.0, 26.0,
        0.45, 1.7, SORT_COLOR_GOLD, 0x4B3026E8, true);

    // Six clickable 3D model previews, not text-only placeholders.
    // Their boxes own the entire hit areas; no captions are rendered.
    // This is supported by stock SA-MP/open.mp clients via font 5.
    for (new item = 0; item < SORT_ITEM_COUNT; item++)
    {
        new Float:x = item % 2 ? 215.0 : 113.0;
        new Float:y = 184.0 + float(item / 2) * 47.0;
        new catalog = gSortRoundItems[playerid][item];
        new PlayerText:preview = CreatePlayerTextDraw(playerid, x, y, "_");
        gSortDraws[playerid][SORT_TD_ITEM_START + item] = preview;
        PlayerTextDrawFont(playerid, preview, TEXT_DRAW_FONT_MODEL_PREVIEW);
        PlayerTextDrawTextSize(playerid, preview, 88.0, 43.0);
        PlayerTextDrawUseBox(playerid, preview, true);
        PlayerTextDrawBoxColour(playerid, preview, 0x27323BEE);
        PlayerTextDrawBackgroundColour(playerid, preview, 0x17222DE8);
        PlayerTextDrawSetPreviewModel(playerid, preview, gSortCatalogModels[catalog]);
        PlayerTextDrawSetPreviewRot(playerid, preview,
            gSortCatalogRot[catalog][0], gSortCatalogRot[catalog][1],
            gSortCatalogRot[catalog][2], gSortCatalogRot[catalog][3]);
        PlayerTextDrawSetSelectable(playerid, preview, true);

    }
    for (new category = 0; category < SORT_CATEGORY_COUNT; category++)
    {
        new Float:y = 200.0 + float(category) * 42.0;
        new colour = SORT_COLOR_METAL;
        if (category == 0) colour = SORT_COLOR_PAPER;
        if (category == 1) colour = SORT_COLOR_PLASTIC;
        SortUI_Add(playerid, SORT_TD_CATEGORY_START + category, 330.0, y,
            gSortCategoryNames[category], 201.0, 33.0,
            0.36, 1.52, SORT_COLOR_TEXT, colour, true);
    }
    SortUI_Add(playerid, 20, 331.0, 157.0, "ВЫБЕРИТЕ КАТЕГОРИЮ", 207.0, 15.0,
        0.19, 0.95, SORT_COLOR_MUTED);
    SortUI_Add(playerid, 21, 328.0, 330.0, "ESC - ВЫХОД", 200.0, 16.0,
        0.22, 1.0, SORT_COLOR_MUTED);

    gSortOpen[playerid] = true;
    for (new i = 0; i < SORT_TD_TOTAL; i++)
        PlayerTextDrawShow(playerid, gSortDraws[playerid][i]);
    SortUI_Refresh(playerid);
    SelectTextDraw(playerid, SORT_COLOR_GOLD);
    SendClientMessage(playerid, 0xFFFFFFFF,
        "{33CC33}[Сортировка] {FFFFFF}Выберите предмет, затем выберите категорию.");
    return 1;
}

stock SortUI_Command(playerid)
{
    return SortUI_Open(playerid);
}

stock SortUI_OnKeyStateChange(playerid, KEY:newkeys, KEY:oldkeys)
{
    if (!(newkeys & KEY_WALK) || (oldkeys & KEY_WALK) ||
        !SortUI_CanUse(playerid)) return 0;
    SortUI_Open(playerid);
    return 1;
}

stock SortUI_OnClick(playerid, PlayerText:clickedid)
{
    if (!gSortOpen[playerid]) return 0;
    if (!SortUI_CanUse(playerid))
    {
        SortUI_Close(playerid);
        return 1;
    }
    if (clickedid == gSortDraws[playerid][10])
    {
        SortUI_Close(playerid);
        return 1;
    }
    for (new item = 0; item < SORT_ITEM_COUNT; item++)
    {
        if (clickedid != gSortDraws[playerid][SORT_TD_ITEM_START + item])
            continue;
        if (gSortCompleted[playerid] & (1 << item)) return 1;
        gSortSelection[playerid] = item;
        SortUI_Status(playerid, "ВЫБЕРИТЕ КАТЕГОРИЮ");
        SortUI_Refresh(playerid);
        return 1;
    }
    for (new category = 0; category < SORT_CATEGORY_COUNT; category++)
    {
        if (clickedid != gSortDraws[playerid][SORT_TD_CATEGORY_START + category])
            continue;
        new selected = gSortSelection[playerid];
        if (selected < 0 || selected >= SORT_ITEM_COUNT)
        {
            SortUI_Status(playerid, "СНАЧАЛА ВЫБЕРИТЕ ПРЕДМЕТ");
            return 1;
        }
        if (gSortCompleted[playerid] & (1 << selected)) return 1;
        new catalog = gSortRoundItems[playerid][selected];
        if (catalog < 0 || catalog >= SORT_CATALOG_COUNT) return 1;
        if (catalog / SORT_MODELS_PER_CATEGORY != category)
        {
            gSortErrors[playerid]++;
            SortUI_Status(playerid, "НЕВЕРНАЯ КАТЕГОРИЯ - ПОВТОРИТЕ");
            return 1;
        }
        gSortCompleted[playerid] |= (1 << selected);
        gSortSelection[playerid] = -1;
        if (SortUI_Refresh(playerid) == SORT_ITEM_COUNT)
        {
            new message[92];
            format(message, sizeof(message),
                "Сортировка завершена: 6 предметов, ошибок: %d.", gSortErrors[playerid]);
            // Sixth correctly sorted item: notify in chat, close the TextDraw
            // and release cursor selection immediately. SortUI_Close clears
            // gSortOpen before CancelSelectTextDraw to prevent ESC re-entry.
            SendClientMessage(playerid, CLEANER_COLOR, message);
            SortUI_Close(playerid);
            return 1;
        }
        else SortUI_Status(playerid, "ВЕРНО! ВЫБЕРИТЕ СЛЕДУЮЩИЙ ПРЕДМЕТ");
        return 1;
    }
    // Active sorting consumes only its own elements, not other UI systems.
    return 0;
}

stock SortUI_OnCancel(playerid, Text:clickedid)
{
    if (!gSortOpen[playerid] || clickedid != Text:INVALID_TEXT_DRAW)
        return 0;
    SortUI_Close(playerid, false);
    return 1;
}
