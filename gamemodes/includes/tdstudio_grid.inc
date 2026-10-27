#if defined _tarkenso_tds_grid
    #endinput
#endif
#define _tarkenso_tds_grid

// A dense 2-unit full-screen grid would exceed the 256-per-player pool.
// Six non-selectable local guides are independent from project layers/data.
#define TDS_GRID_GUIDES (6)
new PlayerText:gTDSGrid[TDS_SESSIONS][TDS_GRID_GUIDES];
new bool:gTDSGridVisible[TDS_SESSIONS], bool:gTDSGridCached[TDS_SESSIONS];
new Float:gTDSGridCellX[TDS_SESSIONS], Float:gTDSGridCellY[TDS_SESSIONS];

stock TDS_GridInit(s)
{
    for (new i; i < TDS_GRID_GUIDES; i++) gTDSGrid[s][i] = INVALID_PLAYER_TEXT_DRAW;
    gTDSGridVisible[s] = false; gTDSGridCached[s] = false;
    return 1;
}

stock TDS_GridHide(s)
{
    if (!gTDSGridVisible[s]) return 1;
    for (new i; i < TDS_GRID_GUIDES; i++) if (gTDSGrid[s][i] != INVALID_PLAYER_TEXT_DRAW &&
        IsValidPlayerTextDraw(gTDS[s][tsOwner], gTDSGrid[s][i])) PlayerTextDrawHide(gTDS[s][tsOwner], gTDSGrid[s][i]);
    gTDSGridVisible[s] = false;
    return 1;
}

stock TDS_GridDestroy(s)
{
    for (new i; i < TDS_GRID_GUIDES; i++)
    {
        new PlayerText:td = gTDSGrid[s][i]; gTDSGrid[s][i] = INVALID_PLAYER_TEXT_DRAW;
        if (td != INVALID_PLAYER_TEXT_DRAW && IsValidPlayerTextDraw(gTDS[s][tsOwner], td)) PlayerTextDrawDestroy(gTDS[s][tsOwner], td);
    }
    gTDSGridVisible[s] = false; gTDSGridCached[s] = false;
    return 1;
}

stock TDS_GridUpdate(s)
{
    if (!gTDS[s][tsGridShow] || !gTDS[s][tsProject] ||
        (gTDS[s][tsMode] != TDS_EDIT_MODE && gTDS[s][tsMode] != TDS_MOVE_MODE)) return TDS_GridHide(s);
    if (gTDS[s][tsGridStep] < 0 || gTDS[s][tsGridStep] >= sizeof(gTDSGridSteps)) return TDS_GridHide(s);
    new Float:x = 320.0, Float:y = 240.0, slots[TDS_ELEMENTS], count = TDS_Targets(s, slots);
    if (count) TDS_GeoPivot(s, slots, count, x, y);
    new Float:step = gTDSGridSteps[gTDS[s][tsGridStep]];
    x = float(floatround(x/step, floatround_floor))*step;
    y = float(floatround(y/step, floatround_floor))*step;
    new bool:changed = !gTDSGridCached[s] || x != gTDSGridCellX[s] || y != gTDSGridCellY[s];
    for (new i; i < TDS_GRID_GUIDES; i++)
    {
        new PlayerText:td = gTDSGrid[s][i];
        if (td == INVALID_PLAYER_TEXT_DRAW || !IsValidPlayerTextDraw(gTDS[s][tsOwner], td))
        { TDS_GridDestroy(s); gTDS[s][tsGridShow] = false; return 0; }
        if (changed)
        {
            new Float:pos = (i < 3 ? x : y)+float((i%3)-1)*step;
            new Float:maximum = i < 3 ? 640.0 : 480.0;
            if (pos < 0.0) pos = 0.0;
            if (pos > maximum) pos = maximum;
            PlayerTextDrawHide(gTDS[s][tsOwner], td);
            PlayerTextDrawSetPos(gTDS[s][tsOwner], td, i < 3 ? pos : 0.0, i < 3 ? 0.0 : pos);
        }
        if (changed || !gTDSGridVisible[s]) PlayerTextDrawShow(gTDS[s][tsOwner], td);
    }
    gTDSGridCellX[s] = x; gTDSGridCellY[s] = y; gTDSGridCached[s] = true; gTDSGridVisible[s] = true;
    return 1;
}

stock bool:TDS_GridSet(playerid, bool:show)
{
    new s = TDS_LayoutSession(playerid);
    if (s < 0) return false;
    if (!show) { gTDS[s][tsGridShow] = false; TDS_GridDestroy(s); return true; }
    if (gTDS[s][tsGridShow]) return true;
    for (new i; i < TDS_GRID_GUIDES; i++)
    {
        new PlayerText:td = CreatePlayerTextDraw(playerid, 0.0, 0.0, "LD_SPAC:white");
        if (td == INVALID_PLAYER_TEXT_DRAW) { TDS_GridDestroy(s); return false; }
        gTDSGrid[s][i] = td;
        PlayerTextDrawFont(playerid, td, TEXT_DRAW_FONT_SPRITE_DRAW);
        PlayerTextDrawColour(playerid, td, 0x7CA6C030);
        PlayerTextDrawTextSize(playerid, td, i < 3 ? 0.35 : 640.0, i < 3 ? 480.0 : 0.35);
        PlayerTextDrawSetSelectable(playerid, td, false);
    }
    gTDS[s][tsGridShow] = true; gTDSGridCached[s] = false; TDS_GridUpdate(s);
    return true;
}

stock bool:TDS_GridStep(playerid, index)
{
    new s = TDS_LayoutSession(playerid);
    if (s < 0 || index < 0 || index >= sizeof(gTDSGridSteps)) return false;
    gTDS[s][tsGridStep] = index; gTDSGridCached[s] = false;
    TDS_GridUpdate(s); TDS_RenderStatus(s);
    return true;
}
