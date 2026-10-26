#if defined _tarkenso_tds_render
    #endinput
#endif
#define _tarkenso_tds_render

new PlayerText:gTDSDraw[TDS_SESSIONS][TDS_ELEMENTS];
new PlayerText:gTDSTool[TDS_SESSIONS][TDS_TOOLS];
new const gTDSToolText[][] =
{
    "TARKENSO TD STUDIO", "MENU", "ELEMENTS", "X -", "X +", "Y -", "Y +", "STEP", "PREVIEW"
};

stock TDS_RenderInit(session)
{
    for (new i; i < TDS_ELEMENTS; i++) gTDSDraw[session][i] = INVALID_PLAYER_TEXT_DRAW;
    for (new i; i < TDS_TOOLS; i++) gTDSTool[session][i] = INVALID_PLAYER_TEXT_DRAW;
    return 1;
}

stock TDS_RenderDestroyElement(session, slot)
{
    new PlayerText:td = gTDSDraw[session][slot];
    gTDSDraw[session][slot] = INVALID_PLAYER_TEXT_DRAW;
    if (td != INVALID_PLAYER_TEXT_DRAW &&
        IsValidPlayerTextDraw(gTDS[session][tsOwner], td))
        PlayerTextDrawDestroy(gTDS[session][tsOwner], td);
    return 1;
}

// TextSize uses endpoints for left/right text, but dimensions for fonts 4/5.
// Store width/height in the model; alignment conversion belongs only here.
stock TDS_RenderSize(playerid, PlayerText:td, const data[E_TDS_ELEMENT])
{
    new Float:w = data[tdW];
    new Float:h = data[tdH];
    if (data[tdType] == TDS_TEXT_TYPE)
    {
        switch (data[tdAlign])
        {
            case 1: w = data[tdX] + w;
            case 2:
            {
                // Center alignment swaps TextSize axes (height, box width).
                PlayerTextDrawTextSize(playerid, td, h, w);
                return 1;
            }
            case 3: w = data[tdX] - w;
        }
    }
    PlayerTextDrawTextSize(playerid, td, w, h);
    return 1;
}

stock TDS_RenderString(playerid, PlayerText:td, const data[E_TDS_ELEMENT])
{
    switch (data[tdType])
    {
        case TDS_TEXT_TYPE:
            PlayerTextDrawSetString(playerid, td, "%s", data[tdText]);
        case TDS_RECT_TYPE: PlayerTextDrawSetString(playerid, td, "LD_SPAC:white");
        case TDS_SPRITE_TYPE:
            PlayerTextDrawSetString(playerid, td, "%s", gTDSSprites[data[tdSprite]]);
        case TDS_MODEL_TYPE: PlayerTextDrawSetString(playerid, td, "_");
    }
    return 1;
}

// Reuse the handle. Re-show just this element so non-string settings reach GTA.
stock bool:TDS_RenderData(playerid, const data[E_TDS_ELEMENT], &PlayerText:td, bool:forceSelect, bool:show = true)
{
    if (td == INVALID_PLAYER_TEXT_DRAW)
    {
        td = CreatePlayerTextDraw(playerid, data[tdX],
            data[tdY], "_");
        if (td == INVALID_PLAYER_TEXT_DRAW) return false;
    }
    else if (!IsValidPlayerTextDraw(playerid, td)) return false;
    PlayerTextDrawHide(playerid, td);
    PlayerTextDrawSetPos(playerid, td, data[tdX], data[tdY]);
    new font = data[tdFont];
    if (data[tdType] != TDS_TEXT_TYPE) font = 4;
    if (data[tdType] == TDS_MODEL_TYPE) font = 5;
    PlayerTextDrawFont(playerid, td, TEXT_DRAW_FONT:font);
    PlayerTextDrawAlignment(playerid, td, TEXT_DRAW_ALIGN:data[tdAlign]);
    PlayerTextDrawLetterSize(playerid, td, data[tdLetterX], data[tdLetterY]);
    PlayerTextDrawColour(playerid, td, data[tdColour]);
    PlayerTextDrawBackgroundColour(playerid, td, data[tdBackground]);
    PlayerTextDrawBoxColour(playerid, td, data[tdBoxColour]);
    PlayerTextDrawUseBox(playerid, td, data[tdBox]);
    PlayerTextDrawSetOutline(playerid, td, data[tdOutline]);
    PlayerTextDrawSetShadow(playerid, td, data[tdShadow]);
    PlayerTextDrawSetProportional(playerid, td, data[tdProportional]);
    TDS_RenderSize(playerid,td,data);
    TDS_RenderString(playerid,td,data);
    if (font == 5)
    {
        PlayerTextDrawSetPreviewModel(playerid, td, data[tdModel]);
        PlayerTextDrawSetPreviewRot(playerid, td, data[tdRotX],
            data[tdRotY], data[tdRotZ],
            data[tdZoom]);
        PlayerTextDrawSetPreviewVehicleColours(playerid, td,
            data[tdVehColour1], data[tdVehColour2]);
    }
    PlayerTextDrawSetSelectable(playerid, td, forceSelect ||
        data[tdSelectable]);
    if (show) PlayerTextDrawShow(playerid, td);
    return true;
}

stock bool:TDS_RenderElement(session, slot)
{
    if (!TDS_ElementValid(session,slot)) return false;
    return TDS_RenderData(gTDS[session][tsOwner],gTDSElement[session][slot],gTDSDraw[session][slot],gTDS[session][tsMode] == TDS_EDIT_MODE);
}

stock bool:TDS_RenderRebuild(session)
{
    new slots[TDS_ELEMENTS], count = TDS_Order(session,slots), PlayerText:draws[TDS_ELEMENTS];
    for (new i; i < TDS_ELEMENTS; i++) draws[i] = INVALID_PLAYER_TEXT_DRAW;
    new playerid = gTDS[session][tsOwner];
    for (new i; i < count; i++)
    {
        if (TDS_RenderData(playerid,gTDSElement[session][slots[i]],draws[i],gTDS[session][tsMode] == TDS_EDIT_MODE,false)) continue;
        for (new j; j <= i; j++) if (draws[j] != INVALID_PLAYER_TEXT_DRAW) PlayerTextDrawDestroy(playerid,draws[j]);
        return false;
    }
    for (new i; i < TDS_ELEMENTS; i++) TDS_RenderDestroyElement(session,i);
    for (new i; i < count; i++)
    {
        gTDSDraw[session][slots[i]] = draws[i];
        PlayerTextDrawShow(playerid,draws[i]);
    }
    return true;
}

stock TDS_RenderStatus(session)
{
    new PlayerText:td = gTDSTool[session][0];
    if (td == INVALID_PLAYER_TEXT_DRAW) return 0;
    new label[128], slot = gTDS[session][tsSelected];
    if (TDS_ElementValid(session, slot))
        format(label, sizeof(label), "TDS | ID %d | X %.1f Y %.1f | STEP %.1f | %d/%d",
            gTDSElement[session][slot][tdId], gTDSElement[session][slot][tdX],
            gTDSElement[session][slot][tdY], gTDSSteps[gTDS[session][tsStep]],
            gTDS[session][tsCount], TDS_ELEMENTS);
    else format(label, sizeof(label), "TARKENSO TD STUDIO | %d/%d | STEP %.1f",
        gTDS[session][tsCount], TDS_ELEMENTS, gTDSSteps[gTDS[session][tsStep]]);
    PlayerTextDrawSetString(gTDS[session][tsOwner], td, "%s", label);
    return 1;
}

stock bool:TDS_RenderTools(session)
{
    new playerid = gTDS[session][tsOwner];
    for (new i; i < TDS_TOOLS; i++)
    {
        new Float:x = 5.0, Float:y = 5.0;
        if (i > 0) { x += float(i - 1) * 78.0; y = 424.0; }
        new PlayerText:td = CreatePlayerTextDraw(playerid, x, y, gTDSToolText[i]);
        if (td == INVALID_PLAYER_TEXT_DRAW) return false;
        gTDSTool[session][i] = td;
        PlayerTextDrawFont(playerid, td, TEXT_DRAW_FONT_1);
        PlayerTextDrawLetterSize(playerid, td, 0.22, 1.0);
        PlayerTextDrawColour(playerid, td, TDS_COLOR);
        PlayerTextDrawUseBox(playerid, td, true);
        PlayerTextDrawBoxColour(playerid, td, 0x111820EE);
        PlayerTextDrawTextSize(playerid, td, i ? x + 72.0 : 635.0, 16.0);
        PlayerTextDrawSetSelectable(playerid, td, i != 0);
        // Hidden until edit mode; do not consume mouse ownership from another UI.
    }
    return true;
}

stock TDS_RenderToolsVisible(session, bool:visible)
{
    for (new i; i < TDS_TOOLS; i++)
    {
        if (gTDSTool[session][i] == INVALID_PLAYER_TEXT_DRAW) continue;
        if (visible) PlayerTextDrawShow(gTDS[session][tsOwner], gTDSTool[session][i]);
        else PlayerTextDrawHide(gTDS[session][tsOwner], gTDSTool[session][i]);
    }
    return 1;
}

stock TDS_RenderDestroy(session)
{
    for (new i; i < TDS_ELEMENTS; i++) TDS_RenderDestroyElement(session, i);
    for (new i; i < TDS_TOOLS; i++)
    {
        new PlayerText:td = gTDSTool[session][i];
        gTDSTool[session][i] = INVALID_PLAYER_TEXT_DRAW;
        if (td != INVALID_PLAYER_TEXT_DRAW && IsValidPlayerTextDraw(gTDS[session][tsOwner], td))
            PlayerTextDrawDestroy(gTDS[session][tsOwner], td);
    }
    return 1;
}
