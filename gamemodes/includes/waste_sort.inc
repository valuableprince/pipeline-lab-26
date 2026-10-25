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

// Verified stock GTA SA objects. No downloaded models, TXD or client mods.
// Paper: magazine pile and cardboard box.
// Plastic: two separate disposable drink cup models (not glass bottles).
// Metal: juice can and a cog-shaped marker. Verify the marker in-game.
new const gSortItemModels[SORT_ITEM_COUNT] =
{
    2852, 1220, 2647, 1484, 2601, 18631
};
new const Float:gSortItemRot[SORT_ITEM_COUNT][4] =
{
    {-18.0, 0.0, 110.0, 0.95}, // magazines
    {-10.0, 0.0, 25.0, 1.00},  // cardboard
    {-12.0, 0.0, -12.0, 0.80}, // drink cup
    {-10.0, 0.0, 18.0, 0.80},  // plastic drink object
    {-18.0, 0.0, -18.0, 0.75},// juice can
    {-10.0, 0.0, 22.0, 1.00}  // cog marker
};
new const gSortItemCategory[SORT_ITEM_COUNT] = {0, 0, 1, 1, 2, 2};
new const gSortCategoryNames[SORT_CATEGORY_COUNT][12] = {"PAPER", "PLASTIC", "METAL"};

new PlayerText:gSortDraws[MAX_PLAYERS][SORT_TD_TOTAL];
new bool:gSortOpen[MAX_PLAYERS];
new gSortSelection[MAX_PLAYERS];
new gSortCompleted[MAX_PLAYERS];
new gSortErrors[MAX_PLAYERS];
new Text3D:gSortStationLabel = Text3D:INVALID_3DTEXT_ID;

stock SortUI_Init()
{
    gSortStationLabel = Create3DTextLabel(
        "{F1C263}WASTE SORTING\n{FFFFFF}ALT or /sort",
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

// Shared creator: all TextDraw strings use ASCII because GTA fonts do not
// reliably render Cyrillic. Selection rectangles and colours are authoritative
// client-side presentation only; the server validates every category click.
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
        new box = SORT_COLOR_METAL;
        if (gSortItemCategory[i] == 0) box = SORT_COLOR_PAPER;
        if (gSortItemCategory[i] == 1) box = SORT_COLOR_PLASTIC;
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
    return 1;
}

stock SortUI_Open(playerid)
{
    if (gSortOpen[playerid]) return 1;
    if (!SortUI_CanUse(playerid))
        return SendClientMessage(playerid, COLOR_RED,
            "Сортировка: требуется смена уборщика/водителя и место у станции в Fort Carson.");

    gSortSelection[playerid] = -1;
    gSortCompleted[playerid] = 0;
    gSortErrors[playerid] = 0;

    // Panel and title, viewport 640 x 448.
    SortUI_Add(playerid, 0, 84.0, 82.0, "_", 472.0, 291.0,
        0.01, 0.01, 0x00000000, 0x1A1D22E8);
    SortUI_Add(playerid, 1, 90.0, 88.0, "_", 460.0, 56.0,
        0.01, 0.01, 0x00000000, 0x20242AEF);
    SortUI_Add(playerid, 2, 108.0, 96.0, "WASTE SORTING", 265.0, 29.0,
        0.47, 1.95, SORT_COLOR_GOLD);
    SortUI_Add(playerid, 3, 109.0, 122.0, "TARKENSO / RECYCLING STATION", 300.0, 19.0,
        0.20, 0.95, SORT_COLOR_MUTED);
    SortUI_Add(playerid, 4, 102.0, 153.0, "_", 214.0, 177.0,
        0.01, 0.01, 0x00000000, 0x171B20E8);

    // Game-native object preview. Model 1265 is a GTA rubbish bag; an opened
    // bag textured illustration would require an additional TXD asset.
    new PlayerText:bag = CreatePlayerTextDraw(playerid, 131.0, 173.0, "_");
    gSortDraws[playerid][5] = bag;
    PlayerTextDrawFont(playerid, bag, TEXT_DRAW_FONT_MODEL_PREVIEW);
    PlayerTextDrawTextSize(playerid, bag, 154.0, 138.0);
    PlayerTextDrawSetPreviewModel(playerid, bag, 1265);
    PlayerTextDrawSetPreviewRot(playerid, bag, -12.0, 0.0, 30.0, 1.0);

    SortUI_Add(playerid, 6, 111.0, 158.0, "01 / MIXED BAG", 185.0, 16.0,
        0.26, 1.15, SORT_COLOR_GOLD);
    SortUI_Add(playerid, 7, 329.0, 174.0, "02 / DROP ZONE", 185.0, 17.0,
        0.26, 1.15, SORT_COLOR_GOLD);
    SortUI_Add(playerid, 8, 108.0, 348.0, "SELECT AN ITEM AND THEN ITS BIN", 421.0, 17.0,
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
        new colour = SORT_COLOR_METAL;
        if (gSortItemCategory[item] == 0) colour = SORT_COLOR_PAPER;
        if (gSortItemCategory[item] == 1) colour = SORT_COLOR_PLASTIC;

        new PlayerText:preview = CreatePlayerTextDraw(playerid, x, y, "_");
        gSortDraws[playerid][SORT_TD_ITEM_START + item] = preview;
        PlayerTextDrawFont(playerid, preview, TEXT_DRAW_FONT_MODEL_PREVIEW);
        PlayerTextDrawTextSize(playerid, preview, 88.0, 43.0);
        PlayerTextDrawUseBox(playerid, preview, true);
        PlayerTextDrawBoxColour(playerid, preview, colour);
        PlayerTextDrawBackgroundColour(playerid, preview, 0x17222DE8);
        PlayerTextDrawSetPreviewModel(playerid, preview, gSortItemModels[item]);
        PlayerTextDrawSetPreviewRot(playerid, preview,
            gSortItemRot[item][0], gSortItemRot[item][1],
            gSortItemRot[item][2], gSortItemRot[item][3]);
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
    SortUI_Add(playerid, 20, 331.0, 157.0, "PAPER  /  PLASTIC  /  METAL", 207.0, 15.0,
        0.19, 0.95, SORT_COLOR_MUTED);
    SortUI_Add(playerid, 21, 328.0, 330.0, "ESC - EXIT", 200.0, 16.0,
        0.22, 1.0, SORT_COLOR_MUTED);

    gSortOpen[playerid] = true;
    for (new i = 0; i < SORT_TD_TOTAL; i++)
        PlayerTextDrawShow(playerid, gSortDraws[playerid][i]);
    SortUI_Refresh(playerid);
    SelectTextDraw(playerid, SORT_COLOR_GOLD);
    SendClientMessage(playerid, 0xFFFFFFFF,
        "{33CC33}[Сортировка] {FFFFFF}Выберите элемент, затем нажмите на соответствующую кнопку.");
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
        SortUI_Status(playerid, "ITEM SELECTED - CHOOSE A BIN");
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
            SortUI_Status(playerid, "SELECT AN ITEM FIRST");
            return 1;
        }
        if (gSortCompleted[playerid] & (1 << selected)) return 1;
        if (gSortItemCategory[selected] != category)
        {
            gSortErrors[playerid]++;
            SortUI_Status(playerid, "WRONG MATERIAL - TRY AGAIN");
            return 1;
        }
        gSortCompleted[playerid] |= (1 << selected);
        gSortSelection[playerid] = -1;
        if (SortUI_Refresh(playerid) == SORT_ITEM_COUNT)
        {
            new message[92];
            format(message, sizeof(message),
                "Сортировка завершена: 6 предметов, ошибок: %d.", gSortErrors[playerid]);
            SendClientMessage(playerid, CLEANER_COLOR, message);
            SortUI_Status(playerid, "COMPLETE! 6 / 6 - PRESS X OR ESC");
        }
        else SortUI_Status(playerid, "CORRECT! SELECT NEXT ITEM");
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
