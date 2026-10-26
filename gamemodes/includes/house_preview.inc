#if defined _tarkenso_house_preview_included
    #endinput
#endif
#define _tarkenso_house_preview_included

// Developer-only preview. No player-owned real estate or DB persistence.
// Wall dimensions verified against GTA/SA-MP model catalog:
// 19450 = 9.63762 x 3.50048 m; 19353 = 3.21515 x 3.50048 m;
// 19426 = 1.60757 x 3.50048 m; 19379 = 9.63762 x 10.50146 x 0.17878 m
// when rotated 90 degrees about Y (horizontal floor/ceiling).
#define HOUSE_PREVIEW_WORLD    (10)
#define HOUSE_PREVIEW_X        (6000.0)
#define HOUSE_PREVIEW_Y        (6000.0)
#define HOUSE_PREVIEW_Z        (20.0)
#define HOUSE_PREVIEW_OBJECTS  (40)

new gHouseObjects[HOUSE_PREVIEW_OBJECTS];
new gHouseObjectCount;
new bool:gHouseReady;
new gHouseDoor[2];
new bool:gHouseDoorOpen[2];

new bool:gHouseInside[MAX_PLAYERS];
new Float:gHouseReturnPos[MAX_PLAYERS][4];
new gHouseReturnWorld[MAX_PLAYERS];
new gHouseReturnInterior[MAX_PLAYERS];

stock House_Add(modelid, Float:x, Float:y, Float:z,
    Float:rx = 0.0, Float:ry = 0.0, Float:rz = 0.0)
{
    if (gHouseObjectCount >= HOUSE_PREVIEW_OBJECTS) return INVALID_STREAMER_ID;
    new objectid = CreateDynamicObject(modelid, x, y, z, rx, ry, rz, HOUSE_PREVIEW_WORLD, 0);
    if (!IsValidDynamicObject(objectid)) return INVALID_STREAMER_ID;
    gHouseObjects[gHouseObjectCount++] = objectid;
    return objectid;
}

stock House_Shutdown()
{
    for (new i = 0; i < gHouseObjectCount; i++)
    {
        if (IsValidDynamicObject(gHouseObjects[i]))
            DestroyDynamicObject(gHouseObjects[i]);
        gHouseObjects[i] = INVALID_STREAMER_ID;
    }
    gHouseObjectCount = 0;
    gHouseDoor[0] = INVALID_STREAMER_ID;
    gHouseDoor[1] = INVALID_STREAMER_ID;
    gHouseDoorOpen[0] = false;
    gHouseDoorOpen[1] = false;
    gHouseReady = false;
    return 1;
}

stock bool:House_Init()
{
    House_Shutdown();

    // Local coordinates: X/Y +/-4.81881. Floor is a Y-rotated wall,
    // not a standing wall mislabeled as a 30x30 slab.
    House_Add(19379, 6000.0, 6000.0, 19.91, 0.0, 90.0, 0.0); // Floor
    House_Add(19379, 6000.0, 6000.0, 23.55, 0.0, -90.0, 0.0); // Ceiling

    // North/south walls run along X (Z rotation 90); east/west along Y.
    House_Add(19450, 6000.0, 6004.8188, 21.75, 0.0, 0.0, 90.0); // North
    House_Add(19450, 5995.1812, 6000.0, 21.75); // West
    House_Add(19450, 6004.8188, 6000.0, 21.75); // East

    // South facade with an actual 1.607 m entrance gap, X=6000..6001.607.
    House_Add(19353, 5996.7848, 5995.1812, 21.75, 0.0, 0.0, 90.0);
    House_Add(19426, 5999.1964, 5995.1812, 21.75, 0.0, 0.0, 90.0);
    House_Add(19353, 6003.2152, 5995.1812, 21.75, 0.0, 0.0, 90.0);

    // Bedroom occupies X=6000.804..6004.819; Y=6000..6004.819.
    // West partition deliberately has a 1.607 m opening, Y=6000..6001.607.
    House_Add(19353, 6000.8036, 6003.2152, 21.75);
    // South partition is continuous; short wall overlaps the last 0.804 m
    // of the long wall, closing the east-wall corner without a gap.
    House_Add(19353, 6002.4112, 6000.0, 21.75, 0.0, 0.0, 90.0);
    House_Add(19426, 6004.0150, 6000.0, 21.75, 0.0, 0.0, 90.0);

    // 1502 = Gen_doorINT04; it has collision and a 1.5 m leaf.
    // Rotation is immediate. Correct hinge pivot must be visually tested.
    gHouseDoor[0] = House_Add(1502, 6000.05, 5995.1812, 20.0, 0.0, 0.0, 0.0);
    gHouseDoor[1] = House_Add(1502, 6000.8036, 6000.05, 20.0, 0.0, 0.0, 90.0);

    // Foyer (southeast); furnishing kept outside the direct entrance route.
    House_Add(1417, 6003.6, 5996.5, 20.05, 0.0, 0.0, 180.0); // DYN_CUPBOARD
    House_Add(949, 6002.6, 5997.4, 20.05); // Pot plant

    // Living area (southwest), clear movement corridor at X~-1..+1.
    House_Add(1753, 5997.1, 5998.5, 20.05, 0.0, 0.0, 90.0); // Couch
    House_Add(1747, 5998.2, 5995.8, 20.50, 0.0, 0.0, 0.0); // Small TV
    House_Add(1740, 5998.2, 5995.8, 20.05); // TV cabinet
    House_Add(949, 5995.9, 5996.0, 20.05);

    // Kitchen / dining (northwest). No unverified material indices applied.
    House_Add(2144, 5996.0, 6003.8, 20.05, 0.0, 0.0, 180.0);
    House_Add(2147, 5997.9, 6003.8, 20.05, 0.0, 0.0, 180.0); // Fridge
    House_Add(1737, 5997.8, 6001.65, 20.05); // Dining table
    House_Add(1739, 5996.7, 6001.65, 20.05, 0.0, 0.0, 90.0);
    House_Add(1739, 5998.9, 6001.65, 20.05, 0.0, 0.0, 270.0);

    // Enclosed bedroom (northeast).
    House_Add(2298, 6002.6, 6003.15, 20.05, 0.0, 0.0, 90.0);
    House_Add(1743, 6004.1, 6003.85, 20.05, 0.0, 0.0, 180.0);

    gHouseReady = IsValidDynamicObject(gHouseDoor[0]) &&
        IsValidDynamicObject(gHouseDoor[1]) && gHouseObjectCount == 26;
    if (!gHouseReady)
    {
        print("[HOUSE] Preview initialization failed; destroying incomplete map.");
        House_Shutdown();
        return false;
    }
    printf("[HOUSE] Preview ready: %d dynamic objects, world %d.",
        gHouseObjectCount, HOUSE_PREVIEW_WORLD);
    return true;
}

stock House_OnConnect(playerid)
{
    gHouseInside[playerid] = false;
    gHouseReturnWorld[playerid] = 0;
    gHouseReturnInterior[playerid] = 0;
    return 1;
}

stock House_OnSpawn(playerid)
{
    // Death/respawn is managed by the existing game mode, not by this preview.
    gHouseInside[playerid] = false;
    return 1;
}

stock House_Command(playerid, const params[])
{
    // Preview intentionally closed to ordinary players until property system exists.
    if (!Studio_Require(playerid)) return 1;

    new action[16];
    if (sscanf(params, "s[16]", action))
    {
        SendClientMessage(playerid, COLOR_AMBER,
            "House preview: /house enter | exit | door | status.");
        return 1;
    }
    if (!strcmp(action, "status", true))
    {
        new message[92];
        format(message, sizeof(message),
            "House preview: ready=%d, objects=%d, world=%d.",
            _:gHouseReady, gHouseObjectCount, HOUSE_PREVIEW_WORLD);
        SendClientMessage(playerid, COLOR_AMBER, message);
        return 1;
    }
    if (!gHouseReady)
        return SendClientMessage(playerid, COLOR_RED, "House preview is not ready.");

    if (!strcmp(action, "enter", true))
    {
        if (gHouseInside[playerid] && GetPlayerVirtualWorld(playerid) == HOUSE_PREVIEW_WORLD)
            return SendClientMessage(playerid, COLOR_GREY, "You are already in the preview.");
        if (GetPlayerState(playerid) != PLAYER_STATE_ONFOOT)
            return SendClientMessage(playerid, COLOR_RED, "Leave your vehicle first.");
        GetPlayerPos(playerid, gHouseReturnPos[playerid][0],
            gHouseReturnPos[playerid][1], gHouseReturnPos[playerid][2]);
        GetPlayerFacingAngle(playerid, gHouseReturnPos[playerid][3]);
        gHouseReturnWorld[playerid] = GetPlayerVirtualWorld(playerid);
        gHouseReturnInterior[playerid] = GetPlayerInterior(playerid);
        gHouseInside[playerid] = true;
        SetPlayerInterior(playerid, 0);
        SetPlayerVirtualWorld(playerid, HOUSE_PREVIEW_WORLD);
        SetPlayerPos(playerid, 6000.8, 5996.8, 20.20);
        SetPlayerFacingAngle(playerid, 0.0);
        SetCameraBehindPlayer(playerid);
        SendClientMessage(playerid, COLOR_AMBER,
            "House preview: /house door near a door, /house exit to return.");
        return 1;
    }
    if (!strcmp(action, "exit", true))
    {
        if (!gHouseInside[playerid])
            return SendClientMessage(playerid, COLOR_GREY, "You are not in the preview.");
        SetPlayerInterior(playerid, gHouseReturnInterior[playerid]);
        SetPlayerVirtualWorld(playerid, gHouseReturnWorld[playerid]);
        SetPlayerPos(playerid, gHouseReturnPos[playerid][0],
            gHouseReturnPos[playerid][1], gHouseReturnPos[playerid][2]);
        SetPlayerFacingAngle(playerid, gHouseReturnPos[playerid][3]);
        SetCameraBehindPlayer(playerid);
        gHouseInside[playerid] = false;
        return 1;
    }
    if (!strcmp(action, "door", true))
    {
        if (!gHouseInside[playerid] || GetPlayerVirtualWorld(playerid) != HOUSE_PREVIEW_WORLD)
            return SendClientMessage(playerid, COLOR_RED, "Enter the preview first.");
        new door = -1;
        if (IsPlayerInRangeOfPoint(playerid, 2.5, 6000.05, 5995.1812, 20.0))
            door = 0;
        else if (IsPlayerInRangeOfPoint(playerid, 2.5, 6000.8036, 6000.05, 20.0))
            door = 1;
        if (door == -1)
            return SendClientMessage(playerid, COLOR_GREY, "Approach a door first.");

        gHouseDoorOpen[door] = !gHouseDoorOpen[door];
        // Streamer MoveDynamicObject with zero positional delta is unsuitable
        // for rotation-only animation. Direct rotation is deterministic here.
        SetDynamicObjectRot(gHouseDoor[door], 0.0, 0.0,
            (door == 0 ? 0.0 : 90.0) + (gHouseDoorOpen[door] ? 90.0 : 0.0));
        SendClientMessage(playerid, COLOR_AMBER,
            gHouseDoorOpen[door] ? "Door open." : "Door closed.");
        return 1;
    }
    return SendClientMessage(playerid, COLOR_GREY,
        "House preview: /house enter | exit | door | status.");
}

cmd:house(playerid, params[])
{
    return House_Command(playerid, params);
}
