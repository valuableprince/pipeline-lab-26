#if defined _tarkenso_ship_surf_included
    #endinput
#endif
#define _tarkenso_ship_surf_included

// SHIP-SURF-01: read-only native surfing diagnostics on a timer.
// No SetPlayerPos, no forced player velocity, no per-packet OnPlayerUpdate hook.
#define SHIPSURF_INTERVAL_MS   (400)
#define SHIPSURF_COLOR         (0x9AD7AFFF)
#define SHIPSURF_MAX_LOCAL_STEP (100.0)

new gShipSurfTimer;
new bool:gShipSurfWatch[MAX_PLAYERS];
new bool:gShipSurfWasAttached[MAX_PLAYERS];
new bool:gShipSurfLocalValid[MAX_PLAYERS];
new gShipSurfSamples[MAX_PLAYERS];
new gShipSurfRecognized[MAX_PLAYERS];
new gShipSurfTransitions[MAX_PLAYERS];
new gShipSurfLost[MAX_PLAYERS];
new gShipSurfPart[MAX_PLAYERS];
new Float:gShipSurfLocalX[MAX_PLAYERS];
new Float:gShipSurfLocalY[MAX_PLAYERS];
new Float:gShipSurfMaxStep[MAX_PLAYERS];
new Float:gShipSurfLastStep[MAX_PLAYERS];
new Float:gShipSurfOffsetX[MAX_PLAYERS];
new Float:gShipSurfOffsetY[MAX_PLAYERS];
new Float:gShipSurfOffsetZ[MAX_PLAYERS];
new gShipSurfNativeObject[MAX_PLAYERS];

forward ShipSurf_Poll();

stock ShipSurf_Reset(playerid)
{
    gShipSurfWatch[playerid] = false;
    gShipSurfWasAttached[playerid] = false;
    gShipSurfLocalValid[playerid] = false;
    gShipSurfSamples[playerid] = 0;
    gShipSurfRecognized[playerid] = 0;
    gShipSurfTransitions[playerid] = 0;
    gShipSurfLost[playerid] = 0;
    gShipSurfPart[playerid] = -1;
    gShipSurfLocalX[playerid] = 0.0;
    gShipSurfLocalY[playerid] = 0.0;
    gShipSurfMaxStep[playerid] = 0.0;
    gShipSurfLastStep[playerid] = 0.0;
    gShipSurfOffsetX[playerid] = 0.0;
    gShipSurfOffsetY[playerid] = 0.0;
    gShipSurfOffsetZ[playerid] = 0.0;
    gShipSurfNativeObject[playerid] = INVALID_OBJECT_ID;
    return 1;
}

// Returns 0 for the movable root, 1..N for an attached component, -1 otherwise.
stock ShipSurf_Component(objectid)
{
    if (!Ship_Ready() || objectid == INVALID_OBJECT_ID) return -1;
    if (objectid == gShipRoot) return 0;
    for (new i = 0; i < gShipPartCount; i++)
        if (gShipPartObject[i] != INVALID_OBJECT_ID &&
            IsValidObject(gShipPartObject[i]) &&
            objectid == gShipPartObject[i]) return i + 1;
    return -1;
}

// Reset sampling continuity without losing session counters or the watch switch.
stock ShipSurf_BreakContinuity(playerid)
{
    gShipSurfWasAttached[playerid] = false;
    gShipSurfLocalValid[playerid] = false;
    gShipSurfPart[playerid] = -1;
    gShipSurfNativeObject[playerid] = INVALID_OBJECT_ID;
    return 1;
}

public ShipSurf_Poll()
{
    if (!Ship_Ready()) return 1;
    new Float:shipX, Float:shipY, Float:shipZ, Float:rx, Float:ry, Float:rz;
    GetObjectPos(gShipRoot, shipX, shipY, shipZ);
    GetObjectRot(gShipRoot, rx, ry, rz);
    new Float:c = floatcos(rz, degrees), Float:sn = floatsin(rz, degrees);
    for (new playerid = 0; playerid < MAX_PLAYERS; playerid++)
    {
        if (!gShipSurfWatch[playerid]) continue;
        if (!IsPlayerConnected(playerid) ||
            GetPlayerState(playerid) != PLAYER_STATE_ONFOOT ||
            GetPlayerVirtualWorld(playerid) != 0 ||
            GetPlayerInterior(playerid) != 0)
        {
            ShipSurf_BreakContinuity(playerid);
            continue;
        }
        new objectid = GetPlayerSurfingObjectID(playerid);
        new part = ShipSurf_Component(objectid);
        gShipSurfSamples[playerid]++;
        gShipSurfNativeObject[playerid] = objectid;
        gShipSurfPart[playerid] = part;
        new bool:attached = part >= 0;
        if (attached)
        {
            gShipSurfRecognized[playerid]++;
            if (!gShipSurfWasAttached[playerid])
                gShipSurfTransitions[playerid]++;
            // Offsets are telemetry from the native, not used as a movement command.
            GetPlayerSurfingOffsets(playerid,
                gShipSurfOffsetX[playerid], gShipSurfOffsetY[playerid],
                gShipSurfOffsetZ[playerid]);
            new Float:px, Float:py, Float:pz;
            GetPlayerPos(playerid, px, py, pz);
            new Float:dx = px - shipX, Float:dy = py - shipY;
            new Float:lx = dx * c + dy * sn;
            new Float:ly = -dx * sn + dy * c;
            if (gShipSurfLocalValid[playerid])
            {
                new Float:stepX = lx - gShipSurfLocalX[playerid];
                new Float:stepY = ly - gShipSurfLocalY[playerid];
                new Float:step = floatsqroot(stepX * stepX + stepY * stepY);
                if (step <= SHIPSURF_MAX_LOCAL_STEP)
                {
                    gShipSurfLastStep[playerid] = step;
                    if (step > gShipSurfMaxStep[playerid])
                        gShipSurfMaxStep[playerid] = step;
                }
            }
            gShipSurfLocalX[playerid] = lx;
            gShipSurfLocalY[playerid] = ly;
            gShipSurfLocalValid[playerid] = true;
        }
        else
        {
            if (gShipSurfWasAttached[playerid])
            {
                // This is a native surfing transition, NOT proof of falling:
                // jumping, animations and streaming can all cause a transition.
                gShipSurfLost[playerid]++;
                printf("[ShipSurf] player=%d native surf lost (%d transitions).",
                    playerid, gShipSurfLost[playerid]);
            }
            gShipSurfLocalValid[playerid] = false;
        }
        gShipSurfWasAttached[playerid] = attached;
    }
    return 1;
}

stock ShipSurf_Status(playerid, targetid)
{
    if (targetid < 0 || targetid >= MAX_PLAYERS || !IsPlayerConnected(targetid))
        return SendClientMessage(playerid, ADMIN_ERROR, "[Tarkenso RPG] Игрок не подключён.");
    new line[160];
    format(line, sizeof(line),
        "Surf P%d: watch=%d samples=%d hits=%d entries=%d losses=%d",
        targetid, gShipSurfWatch[targetid], gShipSurfSamples[targetid],
        gShipSurfRecognized[targetid], gShipSurfTransitions[targetid],
        gShipSurfLost[targetid]);
    SendClientMessage(playerid, SHIPSURF_COLOR, line);
    format(line, sizeof(line),
        "Object=%d component=%d local step=%.3f max=%.3f",
        gShipSurfNativeObject[targetid], gShipSurfPart[targetid],
        gShipSurfLastStep[targetid], gShipSurfMaxStep[targetid]);
    SendClientMessage(playerid, SHIPSURF_COLOR, line);
    format(line, sizeof(line),
        "Native surf offset: %.2f %.2f %.2f",
        gShipSurfOffsetX[targetid], gShipSurfOffsetY[targetid],
        gShipSurfOffsetZ[targetid]);
    SendClientMessage(playerid, SHIPSURF_COLOR, line);
    return 1;
}

stock ShipSurf_Command(playerid, const params[])
{
    if (!Studio_Require(playerid)) return 1;
    new action[16], targetid;
    if (isnull(params) || sscanf(params, "s[16]i", action, targetid))
    {
        SendClientMessage(playerid, SHIPSURF_COLOR,
            "[Tarkenso RPG] /shipsurf watch [playerid], off [playerid], status [playerid]");
        SendClientMessage(playerid, SHIPSURF_COLOR,
            "[Tarkenso RPG] Native-only diagnostic (400ms). Does NOT teleport, freeze or attach players.");
        return 1;
    }
    if (targetid < 0 || targetid >= MAX_PLAYERS || !IsPlayerConnected(targetid))
        return SendClientMessage(playerid, ADMIN_ERROR, "[Tarkenso RPG] Игрок не подключён.");
    if (!strcmp(action, "status", true))
        return ShipSurf_Status(playerid, targetid);
    if (!strcmp(action, "off", true))
    {
        ShipSurf_Reset(targetid);
        return SendClientMessage(playerid, SHIPSURF_COLOR,
            "[Tarkenso RPG] Диагностика игрока выключена, счётчики сброшены.");
    }
    if (!strcmp(action, "watch", true))
    {
        ShipSurf_Reset(targetid);
        gShipSurfWatch[targetid] = true;
        return SendClientMessage(playerid, SHIPSURF_COLOR,
            "[Tarkenso RPG] Диагностика включена. Следите за объектом и переходами surf.");
    }
    return SendClientMessage(playerid, ADMIN_ERROR,
        "[Tarkenso RPG] Использование: /shipsurf watch|off|status [playerid].");
}

stock ShipSurf_Init()
{
    for (new playerid = 0; playerid < MAX_PLAYERS; playerid++)
        ShipSurf_Reset(playerid);
    gShipSurfTimer = SetTimer("ShipSurf_Poll", SHIPSURF_INTERVAL_MS, true);
    print("[ShipSurf] Read-only diagnostics initialized (default OFF).");
    return 1;
}

stock ShipSurf_Shutdown()
{
    if (gShipSurfTimer) KillTimer(gShipSurfTimer);
    gShipSurfTimer = 0;
    return 1;
}
