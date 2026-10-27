#if defined _tarkenso_ship_editor_included
    #endinput
#endif
#define _tarkenso_ship_editor_included

// SHIP-EDITOR-01: mouse-driven top-down placement; no custom client.
// 18 selectable PlayerTextDraw controls and a static overhead camera.
// Safety: deny moving if ANY player occupies the ship collision envelope.
#define SHIPEDIT_BUTTONS (18)
#define SHIPEDIT_ZOOM_DEFAULT (280.0)
#define SHIPEDIT_COLOR (0xA8D4E8FF)
#define SHIPEDIT_POLL_MS (250)

#define SE_NORTH (0)
#define SE_SOUTH (1)
#define SE_WEST (2)
#define SE_EAST (3)
#define SE_ROTATE_L (4)
#define SE_ROTATE_R (5)
#define SE_ZUP (6)
#define SE_ZDOWN (7)
#define SE_FINE (8)
#define SE_COARSE (9)
#define SE_ZOOM_IN (10)
#define SE_ZOOM_OUT (11)
#define SE_SAVE (12)
#define SE_CANCEL (13)
#define SE_LS_BERTH (14)
#define SE_LS_EXIT (15)
#define SE_SF_BERTH (16)
#define SE_SF_EXIT (17)

new gShipEditOwner = INVALID_PLAYER_ID;
new bool:gShipEditActive[MAX_PLAYERS];
new PlayerText:gShipEditButtons[MAX_PLAYERS][SHIPEDIT_BUTTONS];
new PlayerText:gShipEditInfo[MAX_PLAYERS];
new Float:gShipEditOriginal[MAX_PLAYERS][4];
new Float:gShipEditStep[MAX_PLAYERS];
new Float:gShipEditZoom[MAX_PLAYERS];
new gShipEditTimer;
forward ShipEditor_Poll();

stock bool:ShipEditor_Aboard(playerid)
{
    if (!IsPlayerConnected(playerid) || !Ship_Ready()) return false;
    if (GetPlayerVirtualWorld(playerid) != 0 || GetPlayerInterior(playerid) != 0)
        return false;
    // Native surfing is definitive, but detached jumping players also count.
    if (ShipSurf_Component(GetPlayerSurfingObjectID(playerid)) >= 0)
        return true;
    if (GetPlayerState(playerid) == PLAYER_STATE_SPECTATING)
        return false;
    new Float:x, Float:y, Float:z, Float:sx, Float:sy, Float:sz;
    new Float:rx, Float:ry, Float:yaw;
    GetPlayerPos(playerid, x, y, z);
    GetObjectPos(gShipRoot, sx, sy, sz);
    if (z < sz-13.0 || z > sz+75.0) return false;
    GetObjectRot(gShipRoot, rx, ry, yaw);
    new Float:dx = x-sx, Float:dy = y-sy;
    new Float:c = floatcos(yaw, degrees), Float:sn = floatsin(yaw, degrees);
    new Float:localX = dx*c+dy*sn;
    new Float:localY = -dx*sn+dy*c;
    // Conservative footprint of the entire ~230-unit hull; also covers jumps.
    return floatabs(localX) <= 145.0 && floatabs(localY) <= 65.0;
}

stock bool:ShipEditor_Occupied()
{
    if (!Ship_Ready()) return true;
    for (new i = 0; i < MAX_PLAYERS; i++)
        if (ShipEditor_Aboard(i)) return true;
    return false;
}

stock ShipEditor_Camera(playerid)
{
    new Float:x, Float:y, Float:z;
    GetObjectPos(gShipRoot, x, y, z);
    SetPlayerCameraPos(playerid, x+0.2, y-0.2, z+gShipEditZoom[playerid]);
    SetPlayerCameraLookAt(playerid, x, y, z);
    return 1;
}

stock ShipEditor_Refresh(playerid)
{
    if (!gShipEditActive[playerid]) return 0;
    new Float:x, Float:y, Float:z, Float:rx, Float:ry, Float:yaw;
    GetObjectPos(gShipRoot, x, y, z);
    GetObjectRot(gShipRoot, rx, ry, yaw);
    new line[145];
    format(line, sizeof(line),
        "Tarkenso RPG | Ship Studio~n~X %.2f  Y %.2f  Z %.2f  A %.1f~n~Step %.1f | Zoom %.0f",
        x, y, z, yaw, gShipEditStep[playerid], gShipEditZoom[playerid]);
    PlayerTextDrawSetString(playerid, gShipEditInfo[playerid], line);
    ShipEditor_Camera(playerid);
    return 1;
}

stock ShipEditor_AddButton(playerid, slot, Float:x, Float:y,
    const text[], Float:w = 52.0)
{
    new PlayerText:td = CreatePlayerTextDraw(playerid, x, y, text);
    gShipEditButtons[playerid][slot] = td;
    PlayerTextDrawFont(playerid, td, TEXT_DRAW_FONT_1);
    PlayerTextDrawAlignment(playerid, td, TEXT_DRAW_ALIGN_LEFT);
    PlayerTextDrawLetterSize(playerid, td, 0.19, 1.10);
    PlayerTextDrawColour(playerid, td, 0xF3F8FFFF);
    PlayerTextDrawBackgroundColour(playerid, td, 0x142B3EFF);
    PlayerTextDrawSetOutline(playerid, td, 1);
    PlayerTextDrawUseBox(playerid, td, true);
    PlayerTextDrawBoxColour(playerid, td, 0x193C53D9);
    PlayerTextDrawTextSize(playerid, td, x+w, y+19.0);
    PlayerTextDrawSetSelectable(playerid, td, true);
    PlayerTextDrawShow(playerid, td);
    return 1;
}

stock ShipEditor_Draw(playerid)
{
    gShipEditInfo[playerid] = CreatePlayerTextDraw(playerid, 22.0, 25.0,
        "Tarkenso RPG | Ship Studio");
    new PlayerText:td = gShipEditInfo[playerid];
    PlayerTextDrawFont(playerid, td, TEXT_DRAW_FONT_1);
    PlayerTextDrawLetterSize(playerid, td, 0.28, 1.25);
    PlayerTextDrawColour(playerid, td, 0xFFFFFFFF);
    PlayerTextDrawSetOutline(playerid, td, 1);
    PlayerTextDrawShow(playerid, td);
    // Mouse-click arrows work even when the player's keyboard is remapped.
    ShipEditor_AddButton(playerid, SE_NORTH, 499.0, 275.0, "NORTH", 55.0);
    ShipEditor_AddButton(playerid, SE_WEST, 440.0, 300.0, "WEST", 55.0);
    ShipEditor_AddButton(playerid, SE_EAST, 558.0, 300.0, "EAST", 55.0);
    ShipEditor_AddButton(playerid, SE_SOUTH, 499.0, 325.0, "SOUTH", 55.0);
    ShipEditor_AddButton(playerid, SE_ROTATE_L, 22.0, 285.0, "TURN -", 72.0);
    ShipEditor_AddButton(playerid, SE_ROTATE_R, 99.0, 285.0, "TURN +", 72.0);
    ShipEditor_AddButton(playerid, SE_ZUP, 22.0, 315.0, "Z UP", 72.0);
    ShipEditor_AddButton(playerid, SE_ZDOWN, 99.0, 315.0, "Z DOWN", 72.0);
    ShipEditor_AddButton(playerid, SE_FINE, 185.0, 285.0, "STEP -", 70.0);
    ShipEditor_AddButton(playerid, SE_COARSE, 262.0, 285.0, "STEP +", 70.0);
    ShipEditor_AddButton(playerid, SE_ZOOM_IN, 185.0, 315.0, "ZOOM +", 70.0);
    ShipEditor_AddButton(playerid, SE_ZOOM_OUT, 262.0, 315.0, "ZOOM -", 70.0);
    ShipEditor_AddButton(playerid, SE_SAVE, 22.0, 392.0, "SAVE SHIP", 100.0);
    ShipEditor_AddButton(playerid, SE_CANCEL, 130.0, 392.0, "CANCEL", 100.0);
    ShipEditor_AddButton(playerid, SE_LS_BERTH, 238.0, 392.0, "LS BERTH", 85.0);
    ShipEditor_AddButton(playerid, SE_LS_EXIT, 330.0, 392.0, "LS EXIT", 85.0);
    ShipEditor_AddButton(playerid, SE_SF_BERTH, 422.0, 392.0, "SF BERTH", 85.0);
    ShipEditor_AddButton(playerid, SE_SF_EXIT, 514.0, 392.0, "SF EXIT", 85.0);
    ShipEditor_Refresh(playerid);
    SelectTextDraw(playerid, SHIPEDIT_COLOR);
    return 1;
}

// Rollback only when absolutely no one has approached the hull; never drag
// a newly arrived passenger by snapping the ship to its original coordinates.
stock ShipEditor_End(playerid, bool:rollback)
{
    if (!gShipEditActive[playerid]) return 0;
    gShipEditActive[playerid] = false;
    if (gShipEditOwner == playerid) gShipEditOwner = INVALID_PLAYER_ID;
    if (rollback && Ship_Ready() && !ShipEditor_Occupied())
    {
        SetObjectPos(gShipRoot, gShipEditOriginal[playerid][0],
            gShipEditOriginal[playerid][1], gShipEditOriginal[playerid][2]);
        SetObjectRot(gShipRoot, 0.0, 0.0, gShipEditOriginal[playerid][3]);
    }
    if (IsPlayerConnected(playerid))
    {
        for (new i = 0; i < SHIPEDIT_BUTTONS; i++)
        {
            PlayerTextDrawHide(playerid, gShipEditButtons[playerid][i]);
            PlayerTextDrawDestroy(playerid, gShipEditButtons[playerid][i]);
        }
        PlayerTextDrawHide(playerid, gShipEditInfo[playerid]);
        PlayerTextDrawDestroy(playerid, gShipEditInfo[playerid]);
        // Clear active flag BEFORE canceling selection: ESC invokes a callback.
        CancelSelectTextDraw(playerid);
        SetCameraBehindPlayer(playerid);
        TogglePlayerControllable(playerid, !gAdminFrozen[playerid]);
    }
    return 1;
}

stock ShipEditor_Reset(playerid)
{
    if (gShipEditActive[playerid]) ShipEditor_End(playerid, false);
    gShipEditActive[playerid] = false;
    if (gShipEditOwner == playerid) gShipEditOwner = INVALID_PLAYER_ID;
    gShipEditStep[playerid] = 1.0;
    gShipEditZoom[playerid] = SHIPEDIT_ZOOM_DEFAULT;
    return 1;
}

stock ShipEditor_Begin(playerid)
{
    if (!Studio_Require(playerid)) return 1;
    if (!Ship_Ready() || gShipRunning || gShipPortDeparture >= 0)
        return SendClientMessage(playerid, ADMIN_ERROR,
            "[Tarkenso RPG] Для редактора нужен остановленный корабль.");
    if (gShipEditOwner != INVALID_PLAYER_ID || gShipEditActive[playerid] ||
        GetPlayerState(playerid) != PLAYER_STATE_ONFOOT ||
        gAdminFly[playerid] || gAdminFrozen[playerid] ||
        Race_IsParticipant(playerid) || AdminPunish_IsJailed(playerid) ||
        GetPlayerVirtualWorld(playerid) != 0 || GetPlayerInterior(playerid) != 0)
        return SendClientMessage(playerid, ADMIN_ERROR,
            "[Tarkenso RPG] Редактор занят или игрок не в подходящем состоянии.");
    if (ShipEditor_Occupied())
        return SendClientMessage(playerid, ADMIN_ERROR,
            "[Tarkenso RPG] Освободите палубу и зону корпуса перед редактированием.");
    if (!Ship_Lock(playerid)) return 1;
    new Float:x, Float:y, Float:z, Float:rx, Float:ry, Float:yaw;
    GetObjectPos(gShipRoot, x, y, z);
    GetObjectRot(gShipRoot, rx, ry, yaw);
    gShipEditOriginal[playerid][0] = x;
    gShipEditOriginal[playerid][1] = y;
    gShipEditOriginal[playerid][2] = z;
    gShipEditOriginal[playerid][3] = yaw;
    gShipEditStep[playerid] = 1.0;
    gShipEditZoom[playerid] = SHIPEDIT_ZOOM_DEFAULT;
    gShipEditActive[playerid] = true;
    gShipEditOwner = playerid;
    TogglePlayerControllable(playerid, false);
    ShipEditor_Draw(playerid);
    return SendClientMessage(playerid, SHIP_COLOR,
        "[Tarkenso RPG] Вид сверху: нажимайте кнопки мышью; SAVE SHIP сохранит cargo_01.dat.");
}

stock ShipEditor_Action(playerid, action)
{
    if (!gShipEditActive[playerid] || gShipEditOwner != playerid) return 0;
    if (ShipEditor_Occupied())
    {
        ShipEditor_End(playerid, false);
        return SendClientMessage(playerid, ADMIN_ERROR,
            "[Tarkenso RPG] Редактор закрыт: кто-то приблизился к палубе. Судно не перемещалось.");
    }
    if (action == SE_CANCEL)
    {
        ShipEditor_End(playerid, true);
        return SendClientMessage(playerid, SHIP_COLOR,
            "[Tarkenso RPG] Изменения положения отменены.");
    }
    if (action == SE_SAVE)
    {
        if (!Ship_Save())
            return SendClientMessage(playerid, ADMIN_ERROR,
                "[Tarkenso RPG] Не удалось сохранить положение корабля.");
        ShipEditor_End(playerid, false);
        return SendClientMessage(playerid, SHIP_COLOR,
            "[Tarkenso RPG] Положение и маршрут сохранены в cargo_01.dat.");
    }
    if (action >= SE_LS_BERTH && action <= SE_SF_EXIT)
    {
        // These actions persist the CURRENT root transform as a harbor marker.
        new op[20];
        switch (action)
        {
            case SE_LS_BERTH: format(op, sizeof(op), "ls berth");
            case SE_LS_EXIT: format(op, sizeof(op), "ls exit");
            case SE_SF_BERTH: format(op, sizeof(op), "sf berth");
            case SE_SF_EXIT: format(op, sizeof(op), "sf exit");
        }
        ShipPort_Command(playerid, op);
        return 1;
    }
    if (action == SE_FINE || action == SE_COARSE)
    {
        if (action == SE_FINE && gShipEditStep[playerid] > 0.1)
            gShipEditStep[playerid] /= 10.0;
        if (action == SE_COARSE && gShipEditStep[playerid] < 10.0)
            gShipEditStep[playerid] *= 10.0;
        ShipEditor_Refresh(playerid);
        return 1;
    }
    if (action == SE_ZOOM_IN || action == SE_ZOOM_OUT)
    {
        if (action == SE_ZOOM_IN && gShipEditZoom[playerid] > 180.0)
            gShipEditZoom[playerid] -= 50.0;
        if (action == SE_ZOOM_OUT && gShipEditZoom[playerid] < 430.0)
            gShipEditZoom[playerid] += 50.0;
        ShipEditor_Refresh(playerid);
        return 1;
    }
    new Float:x, Float:y, Float:z, Float:rx, Float:ry, Float:yaw;
    GetObjectPos(gShipRoot, x, y, z);
    GetObjectRot(gShipRoot, rx, ry, yaw);
    new Float:step = gShipEditStep[playerid];
    switch (action)
    {
        case SE_NORTH: y += step;
        case SE_SOUTH: y -= step;
        case SE_WEST: x -= step;
        case SE_EAST: x += step;
        case SE_ROTATE_L: yaw -= step;
        case SE_ROTATE_R: yaw += step;
        case SE_ZUP: z += step;
        case SE_ZDOWN: z -= step;
        default: return 0;
    }
    if (!Ship_ValidCoord(x, y, z))
        return SendClientMessage(playerid, ADMIN_ERROR,
            "[Tarkenso RPG] Достигнута граница допустимых координат.");
    // UI edits ONLY a stationary and passenger-free root; attachments follow.
    SetObjectPos(gShipRoot, x, y, z);
    SetObjectRot(gShipRoot, 0.0, 0.0, yaw);
    return ShipEditor_Refresh(playerid);
}

stock ShipEditor_OnClick(playerid, PlayerText:clicked)
{
    if (!gShipEditActive[playerid]) return 0;
    for (new i = 0; i < SHIPEDIT_BUTTONS; i++)
        if (clicked == gShipEditButtons[playerid][i])
            return ShipEditor_Action(playerid, i);
    return 1;
}

stock ShipEditor_OnEscape(playerid, Text:clicked)
{
    if (!gShipEditActive[playerid] || clicked != Text:INVALID_TEXT_DRAW)
        return 0;
    return ShipEditor_Action(playerid, SE_CANCEL);
}

stock ShipEditor_Command(playerid, const params[])
{
    if (!Studio_Require(playerid)) return 1;
    if (gShipEditActive[playerid])
    {
        if (!strcmp(params, "save", true)) return ShipEditor_Action(playerid, SE_SAVE);
        if (!strcmp(params, "cancel", true)) return ShipEditor_Action(playerid, SE_CANCEL);
        return SendClientMessage(playerid, SHIP_COLOR,
            "[Tarkenso RPG] В редакторе: /ship edit save или /ship edit cancel.");
    }
    if (isnull(params) || !strcmp(params, "start", true))
        return ShipEditor_Begin(playerid);
    return SendClientMessage(playerid, ADMIN_ERROR,
        "[Tarkenso RPG] Синтаксис: /ship edit [start|save|cancel].");
}

public ShipEditor_Poll()
{
    if (gShipEditOwner == INVALID_PLAYER_ID) return 1;
    new p = gShipEditOwner;
    if (!gShipEditActive[p]) { gShipEditOwner = INVALID_PLAYER_ID; return 1; }
    if (!IsPlayerConnected(p) || !Admin_IsFounder(p) || !gAdminDuty[p] ||
        GetPlayerState(p) != PLAYER_STATE_ONFOOT || !Ship_Ready() ||
        gShipRunning || gShipPortDeparture >= 0)
    {
        ShipEditor_End(p, false);
        return 1;
    }
    if (ShipEditor_Occupied())
    {
        ShipEditor_End(p, false); // no rollback if a passenger might be aboard
        SendClientMessage(p, ADMIN_ERROR,
            "[Tarkenso RPG] Редактор закрыт: обнаружен игрок в зоне корабля.");
    }
    return 1;
}

stock ShipEditor_Init()
{
    gShipEditOwner = INVALID_PLAYER_ID;
    for (new p = 0; p < MAX_PLAYERS; p++) gShipEditActive[p] = false;
    gShipEditTimer = SetTimer("ShipEditor_Poll", SHIPEDIT_POLL_MS, true);
    return 1;
}

stock ShipEditor_Shutdown()
{
    if (gShipEditTimer) KillTimer(gShipEditTimer);
    gShipEditTimer = 0;
    if (gShipEditOwner != INVALID_PLAYER_ID)
        ShipEditor_End(gShipEditOwner, false);
    return 1;
}
