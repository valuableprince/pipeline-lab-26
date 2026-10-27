#if defined _tarkenso_ship_camera_included
    #endinput
#endif
#define _tarkenso_ship_camera_included

// SHIP-CAMERA-01: one command, selectable right-side camera menu.
// No extra collision scans; only active observers receive camera updates.
#define SHIPCAM_TICK_MS (200)
#define SHIPCAM_BUTTONS (12)
#define SHIPCAM_MODE_COUNT (7)
#define SHIPCAM_TOP (0)
#define SHIPCAM_REAR (1)
#define SHIPCAM_FRONT (2)
#define SHIPCAM_PORT (3)
#define SHIPCAM_STARBOARD (4)
#define SHIPCAM_ANGLE (5)
#define SHIPCAM_ORBIT (6)
#define SHIPCAM_ROTATE_LEFT (7)
#define SHIPCAM_ROTATE_RIGHT (8)
#define SHIPCAM_ZOOM_IN (9)
#define SHIPCAM_ZOOM_OUT (10)
#define SHIPCAM_EXIT (11)
#define SHIPCAM_ANCHOR_HEIGHT (165.0)

new bool:gShipCamActive[MAX_PLAYERS];
new gShipCamMode[MAX_PLAYERS];
new Float:gShipCamDistance[MAX_PLAYERS];
new Float:gShipCamOrbit[MAX_PLAYERS];
new Float:gShipCamReturn[MAX_PLAYERS][4];
new Float:gShipCamAnchor[MAX_PLAYERS][2];
new PlayerText:gShipCamButtons[MAX_PLAYERS][SHIPCAM_BUTTONS];
new PlayerText:gShipCamStatus[MAX_PLAYERS];
new gShipCamUsers[MAX_PLAYERS];
new gShipCamCount;
new gShipCamTimer;
new gShipCamTicks;
forward ShipCam_Tick();

stock ShipCam_UpdateView(playerid)
{
    if (!gShipCamActive[playerid] || !Ship_Ready()) return 0;
    new Float:x, Float:y, Float:z, Float:rx, Float:ry, Float:yaw;
    GetObjectPos(gShipRoot, x, y, z);
    GetObjectRot(gShipRoot, rx, ry, yaw);
    new Float:heading = yaw - SHIP_CARGO_HEADING_OFFSET;
    new Float:fx = -floatsin(heading, degrees);
    new Float:fy = floatcos(heading, degrees);
    new Float:sx = fy, Float:sy = -fx;
    new Float:d = gShipCamDistance[playerid];
    new Float:cx = x, Float:cy = y, Float:cz = z + 65.0;
    new Float:lookZ = z + 20.0;
    switch (gShipCamMode[playerid])
    {
        case SHIPCAM_TOP:
        {
            cx = x + 0.2;
            cy = y - 0.2;
            cz = z + d + 80.0;
            lookZ = z + 6.0;
        }
        case SHIPCAM_REAR:
        {
            cx = x - fx*d;
            cy = y - fy*d;
            cz = z + d*0.34;
        }
        case SHIPCAM_FRONT:
        {
            cx = x + fx*d;
            cy = y + fy*d;
            cz = z + d*0.34;
        }
        case SHIPCAM_PORT:
        {
            cx = x - sx*d;
            cy = y - sy*d;
            cz = z + d*0.28;
        }
        case SHIPCAM_STARBOARD:
        {
            cx = x + sx*d;
            cy = y + sy*d;
            cz = z + d*0.28;
        }
        case SHIPCAM_ANGLE:
        {
            cx = x - fx*d*0.72 + sx*d*0.70;
            cy = y - fy*d*0.72 + sy*d*0.70;
            cz = z + d*0.52;
        }
        case SHIPCAM_ORBIT:
        {
            cx = x + floatsin(gShipCamOrbit[playerid], degrees)*d;
            cy = y + floatcos(gShipCamOrbit[playerid], degrees)*d;
            cz = z + d*0.48;
        }
    }
    // Follow the vessel without touching its root or autopilot.
    SetPlayerCameraPos(playerid, cx, cy, cz);
    SetPlayerCameraLookAt(playerid, x, y, lookZ);

    // Stock CreateObject streaming is based on the player's position.
    // Keep the frozen observer above the ship, out of its collision envelope.
    new Float:dx = x - gShipCamAnchor[playerid][0];
    new Float:dy = y - gShipCamAnchor[playerid][1];
    if (dx*dx + dy*dy >= 2500.0)
    {
        SetPlayerPos(playerid, x, y, z + SHIPCAM_ANCHOR_HEIGHT);
        gShipCamAnchor[playerid][0] = x;
        gShipCamAnchor[playerid][1] = y;
    }
    return 1;
}

stock ShipCam_Status(playerid)
{
    if (!gShipCamActive[playerid]) return 0;
    new name[18];
    switch (gShipCamMode[playerid])
    {
        case SHIPCAM_TOP: format(name, sizeof(name), "TOP");
        case SHIPCAM_REAR: format(name, sizeof(name), "REAR");
        case SHIPCAM_FRONT: format(name, sizeof(name), "FRONT");
        case SHIPCAM_PORT: format(name, sizeof(name), "PORT");
        case SHIPCAM_STARBOARD: format(name, sizeof(name), "STARBOARD");
        case SHIPCAM_ANGLE: format(name, sizeof(name), "ANGLE 45");
        case SHIPCAM_ORBIT: format(name, sizeof(name), "ORBIT");
    }
    new state[18];
    if (gShipManeuverActive) format(state, sizeof(state), "MANEUVER");
    else if (gShipPortDeparture >= 0) format(state, sizeof(state), "PORT EXIT");
    else if (gShipRunning) format(state, sizeof(state), "AT SEA");
    else format(state, sizeof(state), "STOPPED");
    new line[160];
    format(line, sizeof(line),
        "Tarkenso RPG | SHIP CAM~n~VIEW: %s | DIST: %.0f~n~VESSEL: %s",
        name, gShipCamDistance[playerid], state);
    PlayerTextDrawSetString(playerid, gShipCamStatus[playerid], line);
    return 1;
}

stock ShipCam_AddButton(playerid, index, Float:y, const title[])
{
    new PlayerText:td = CreatePlayerTextDraw(playerid, 502.0, y, title);
    gShipCamButtons[playerid][index] = td;
    PlayerTextDrawFont(playerid, td, TEXT_DRAW_FONT_1);
    PlayerTextDrawAlignment(playerid, td, TEXT_DRAW_ALIGN_LEFT);
    PlayerTextDrawLetterSize(playerid, td, 0.22, 1.15);
    PlayerTextDrawColour(playerid, td, 0xE8F5FFFF);
    PlayerTextDrawBackgroundColour(playerid, td, 0x102838FF);
    PlayerTextDrawSetOutline(playerid, td, 1);
    PlayerTextDrawUseBox(playerid, td, true);
    PlayerTextDrawBoxColour(playerid, td,
        index == SHIPCAM_EXIT ? 0x80443DE8 : 0x1C455FDF);
    PlayerTextDrawTextSize(playerid, td, 622.0, y + 19.0);
    PlayerTextDrawSetSelectable(playerid, td, true);
    PlayerTextDrawShow(playerid, td);
    return 1;
}

stock ShipCam_Draw(playerid)
{
    gShipCamStatus[playerid] = CreatePlayerTextDraw(playerid, 22.0, 27.0,
        "Tarkenso RPG | SHIP CAM");
    new PlayerText:td = gShipCamStatus[playerid];
    PlayerTextDrawFont(playerid, td, TEXT_DRAW_FONT_1);
    PlayerTextDrawLetterSize(playerid, td, 0.27, 1.35);
    PlayerTextDrawColour(playerid, td, 0xFFFFFFFF);
    PlayerTextDrawSetOutline(playerid, td, 1);
    PlayerTextDrawShow(playerid, td);

    ShipCam_AddButton(playerid, SHIPCAM_TOP, 68.0, "TOP");
    ShipCam_AddButton(playerid, SHIPCAM_REAR, 94.0, "REAR");
    ShipCam_AddButton(playerid, SHIPCAM_FRONT, 120.0, "FRONT");
    ShipCam_AddButton(playerid, SHIPCAM_PORT, 146.0, "LEFT SIDE");
    ShipCam_AddButton(playerid, SHIPCAM_STARBOARD, 172.0, "RIGHT SIDE");
    ShipCam_AddButton(playerid, SHIPCAM_ANGLE, 198.0, "ANGLE 45");
    ShipCam_AddButton(playerid, SHIPCAM_ORBIT, 224.0, "FREE ORBIT");
    ShipCam_AddButton(playerid, SHIPCAM_ROTATE_LEFT, 250.0, "ROTATE -");
    ShipCam_AddButton(playerid, SHIPCAM_ROTATE_RIGHT, 276.0, "ROTATE +");
    ShipCam_AddButton(playerid, SHIPCAM_ZOOM_IN, 302.0, "ZOOM +");
    ShipCam_AddButton(playerid, SHIPCAM_ZOOM_OUT, 328.0, "ZOOM -");
    ShipCam_AddButton(playerid, SHIPCAM_EXIT, 372.0, "EXIT CAMERA");
    ShipCam_Status(playerid);
    SelectTextDraw(playerid, 0x89D4EAFF);
    return 1;
}

stock ShipCam_RemoveObserver(playerid)
{
    for (new i = 0; i < gShipCamCount; i++)
    {
        if (gShipCamUsers[i] != playerid) continue;
        gShipCamCount--;
        gShipCamUsers[i] = gShipCamUsers[gShipCamCount];
        break;
    }
    if (gShipCamCount == 0 && gShipCamTimer)
    {
        KillTimer(gShipCamTimer);
        gShipCamTimer = 0;
    }
    return 1;
}

stock ShipCam_Close(playerid, bool:restore = true)
{
    if (!gShipCamActive[playerid]) return 0;
    gShipCamActive[playerid] = false;
    ShipCam_RemoveObserver(playerid);
    if (!IsPlayerConnected(playerid)) return 1;
    for (new i = 0; i < SHIPCAM_BUTTONS; i++)
    {
        PlayerTextDrawHide(playerid, gShipCamButtons[playerid][i]);
        PlayerTextDrawDestroy(playerid, gShipCamButtons[playerid][i]);
    }
    PlayerTextDrawHide(playerid, gShipCamStatus[playerid]);
    PlayerTextDrawDestroy(playerid, gShipCamStatus[playerid]);
    // Clearing state first prevents ESC re-entering the closing callback.
    CancelSelectTextDraw(playerid);
    if (restore && GetPlayerState(playerid) == PLAYER_STATE_ONFOOT)
    {
        SetPlayerPos(playerid, gShipCamReturn[playerid][0],
            gShipCamReturn[playerid][1], gShipCamReturn[playerid][2]);
        SetPlayerFacingAngle(playerid, gShipCamReturn[playerid][3]);
        SetCameraBehindPlayer(playerid);
        TogglePlayerControllable(playerid, !gAdminFrozen[playerid]);
    }
    return 1;
}

stock ShipCam_Reset(playerid)
{
    if (gShipCamActive[playerid]) ShipCam_Close(playerid, false);
    gShipCamActive[playerid] = false;
    gShipCamMode[playerid] = SHIPCAM_ANGLE;
    gShipCamDistance[playerid] = 205.0;
    gShipCamOrbit[playerid] = 225.0;
    return 1;
}

stock ShipCam_Command(playerid)
{
    if (!Admin_Require(playerid, PERM_MODERATE)) return 1;
    if (gShipCamActive[playerid])
    {
        ShipCam_Close(playerid);
        return SendClientMessage(playerid, ADMIN_COLOR,
            "[Tarkenso RPG] Наблюдение за кораблём завершено.");
    }
    if (!Ship_Ready())
        return SendClientMessage(playerid, ADMIN_ERROR,
            "[Tarkenso RPG] Корабль не загружен.");
    if (gShipEditActive[playerid] || gSortOpen[playerid] ||
        gAdminFly[playerid] || gAdminFrozen[playerid] ||
        Race_IsParticipant(playerid) || AdminPunish_IsJailed(playerid) ||
        GetPlayerState(playerid) != PLAYER_STATE_ONFOOT ||
        GetPlayerInterior(playerid) != 0 || GetPlayerVirtualWorld(playerid) != 0)
        return SendClientMessage(playerid, ADMIN_ERROR,
            "[Tarkenso RPG] Выйдите из редакторов, транспорта и других режимов.");
    if (gShipCamCount >= MAX_PLAYERS) return 1;
    GetPlayerPos(playerid, gShipCamReturn[playerid][0],
        gShipCamReturn[playerid][1], gShipCamReturn[playerid][2]);
    GetPlayerFacingAngle(playerid, gShipCamReturn[playerid][3]);
    gShipCamMode[playerid] = SHIPCAM_ANGLE;
    gShipCamDistance[playerid] = 205.0;
    gShipCamOrbit[playerid] = 225.0;
    gShipCamActive[playerid] = true;
    gShipCamUsers[gShipCamCount++] = playerid;
    new Float:x, Float:y, Float:z;
    GetObjectPos(gShipRoot, x, y, z);
    gShipCamAnchor[playerid][0] = x;
    gShipCamAnchor[playerid][1] = y;
    TogglePlayerControllable(playerid, false);
    SetPlayerPos(playerid, x, y, z + SHIPCAM_ANCHOR_HEIGHT);
    ShipCam_Draw(playerid);
    ShipCam_UpdateView(playerid);
    if (!gShipCamTimer)
        gShipCamTimer = SetTimer("ShipCam_Tick", SHIPCAM_TICK_MS, true);
    return SendClientMessage(playerid, ADMIN_COLOR,
        "[Tarkenso RPG] Камера корабля включена. Выберите ракурс в меню справа; ESC — выход.");
}

stock ShipCam_OnClick(playerid, PlayerText:clickedid)
{
    if (!gShipCamActive[playerid]) return 0;
    for (new i = 0; i < SHIPCAM_BUTTONS; i++)
    {
        if (clickedid != gShipCamButtons[playerid][i]) continue;
        if (i == SHIPCAM_EXIT) return ShipCam_Close(playerid);
        if (i < SHIPCAM_MODE_COUNT) gShipCamMode[playerid] = i;
        else if (i == SHIPCAM_ROTATE_LEFT || i == SHIPCAM_ROTATE_RIGHT)
        {
            gShipCamMode[playerid] = SHIPCAM_ORBIT;
            gShipCamOrbit[playerid] +=
                i == SHIPCAM_ROTATE_LEFT ? -15.0 : 15.0;
            if (gShipCamOrbit[playerid] < 0.0) gShipCamOrbit[playerid] += 360.0;
            if (gShipCamOrbit[playerid] >= 360.0) gShipCamOrbit[playerid] -= 360.0;
        }
        else if (i == SHIPCAM_ZOOM_IN && gShipCamDistance[playerid] > 125.0)
            gShipCamDistance[playerid] -= 25.0;
        else if (i == SHIPCAM_ZOOM_OUT && gShipCamDistance[playerid] < 280.0)
            gShipCamDistance[playerid] += 25.0;
        ShipCam_Status(playerid);
        ShipCam_UpdateView(playerid);
        return 1;
    }
    return 1;
}

stock ShipCam_OnEscape(playerid, Text:clickedid)
{
    if (!gShipCamActive[playerid] || clickedid != Text:INVALID_TEXT_DRAW)
        return 0;
    return ShipCam_Close(playerid);
}

public ShipCam_Tick()
{
    if (!Ship_Ready())
    {
        while (gShipCamCount > 0)
            ShipCam_Close(gShipCamUsers[gShipCamCount-1]);
        return 1;
    }
    gShipCamTicks++;
    for (new i = gShipCamCount-1; i >= 0; i--)
    {
        new playerid = gShipCamUsers[i];
        if (!IsPlayerConnected(playerid) ||
            !Admin_Allowed(playerid, PERM_MODERATE) ||
            !gAdminDuty[playerid] || GetPlayerState(playerid) != PLAYER_STATE_ONFOOT ||
            AdminPunish_IsJailed(playerid))
        {
            ShipCam_Close(playerid, false);
            continue;
        }
        ShipCam_UpdateView(playerid);
        if (gShipCamTicks % 5 == 0) ShipCam_Status(playerid);
    }
    return 1;
}

stock ShipCam_Init()
{
    gShipCamCount = 0;
    gShipCamTimer = 0;
    gShipCamTicks = 0;
    for (new i = 0; i < MAX_PLAYERS; i++)
    {
        gShipCamActive[i] = false;
        gShipCamMode[i] = SHIPCAM_ANGLE;
        gShipCamDistance[i] = 205.0;
        gShipCamOrbit[i] = 225.0;
    }
    return 1;
}

stock ShipCam_Shutdown()
{
    while (gShipCamCount > 0)
        ShipCam_Close(gShipCamUsers[gShipCamCount-1]);
    if (gShipCamTimer) KillTimer(gShipCamTimer);
    gShipCamTimer = 0;
    return 1;
}
