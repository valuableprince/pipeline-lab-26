#if defined _tarkenso_afk_status_included
    #endinput
#endif
#define _tarkenso_afk_status_included

// AFK-01. A missing stream of client updates indicates pause/alt-tab,
// not merely standing still. A single timer serves all online players.
#define AFK_TIMEOUT_SECONDS       (30)
#define AFK_UPDATE_INTERVAL_MS    (5000)
#define AFK_LABEL_DISTANCE        (20.0)
#define AFK_LABEL_COLOR           (0xB7BEC8FF)

new gAFKTimer;
new gAFKLastUpdate[MAX_PLAYERS];
new Text3D:gAFKLabel[MAX_PLAYERS];
new gAFKLabelWorld[MAX_PLAYERS];

forward AFK_Tick();

stock AFK_ClearLabel(playerid)
{
    if (gAFKLabel[playerid] != Text3D:INVALID_3DTEXT_ID)
        Delete3DTextLabel(gAFKLabel[playerid]);
    gAFKLabel[playerid] = Text3D:INVALID_3DTEXT_ID;
    gAFKLabelWorld[playerid] = -1;
    return 1;
}

stock AFK_Reset(playerid)
{
    AFK_ClearLabel(playerid);
    gAFKLastUpdate[playerid] = 0;
    return 1;
}

stock AFK_Init()
{
    for (new i = 0; i < MAX_PLAYERS; i++)
    {
        gAFKLabel[i] = Text3D:INVALID_3DTEXT_ID;
        gAFKLabelWorld[i] = -1;
        gAFKLastUpdate[i] = 0;
    }
    gAFKTimer = SetTimer("AFK_Tick", AFK_UPDATE_INTERVAL_MS, true);
    if (!gAFKTimer) print("[AFK] ERROR: status timer failed to start.");
    return 1;
}

stock AFK_Shutdown()
{
    if (gAFKTimer) KillTimer(gAFKTimer);
    gAFKTimer = 0;
    for (new i = 0; i < MAX_PLAYERS; i++)
        AFK_Reset(i);
    return 1;
}

stock bool:AFK_CanTrack(playerid)
{
    return IsPlayerConnected(playerid) && !IsPlayerNPC(playerid) &&
        gAuthState[playerid] == AUTH_READY && gAccountSpawned[playerid] &&
        !gAccountDead[playerid] &&
        GetPlayerState(playerid) != PLAYER_STATE_SPECTATING;
}

stock AFK_OnSpawn(playerid)
{
    AFK_Reset(playerid);
    if (AFK_CanTrack(playerid)) gAFKLastUpdate[playerid] = gettime();
    return 1;
}

// OnPlayerUpdate is hot (~30 calls/s/player). Never format labels here.
stock AFK_OnUpdate(playerid)
{
    if (!AFK_CanTrack(playerid)) return 1;
    gAFKLastUpdate[playerid] = gettime();
    // One client update is enough to prove return from pause.
    if (gAFKLabel[playerid] != Text3D:INVALID_3DTEXT_ID)
        AFK_ClearLabel(playerid);
    return 1;
}

public AFK_Tick()
{
    new now = gettime();
    for (new i = 0; i < MAX_PLAYERS; i++)
    {
        if (!AFK_CanTrack(i) || gAFKLastUpdate[i] <= 0)
        {
            if (gAFKLabel[i] != Text3D:INVALID_3DTEXT_ID)
                AFK_ClearLabel(i);
            continue;
        }

        new elapsed = now - gAFKLastUpdate[i];
        // Protect against clock corrections, relogs and short packet gaps.
        if (elapsed < AFK_TIMEOUT_SECONDS)
        {
            if (gAFKLabel[i] != Text3D:INVALID_3DTEXT_ID)
                AFK_ClearLabel(i);
            continue;
        }

        // Keep the label in the same virtual world as its player, including
        // when administrators move AFK characters between virtual worlds.
        new world = GetPlayerVirtualWorld(i);
        if (gAFKLabel[i] != Text3D:INVALID_3DTEXT_ID &&
            gAFKLabelWorld[i] != world) AFK_ClearLabel(i);

        new labelText[64];
        format(labelText, sizeof(labelText), "{D9AA55}AFK\n{B7BEC8}%02d:%02d:%02d",
            elapsed / 3600, (elapsed / 60) % 60, elapsed % 60);

        if (gAFKLabel[i] == Text3D:INVALID_3DTEXT_ID)
        {
            new Float:x, Float:y, Float:z;
            GetPlayerPos(i, x, y, z);
            gAFKLabel[i] = Create3DTextLabel(labelText, AFK_LABEL_COLOR, x, y, z,
                AFK_LABEL_DISTANCE, world, true);
            if (gAFKLabel[i] == Text3D:INVALID_3DTEXT_ID) continue;
            if (!Attach3DTextLabelToPlayer(gAFKLabel[i], i, 0.0, 0.0, 0.95))
            {
                AFK_ClearLabel(i);
                continue;
            }
            gAFKLabelWorld[i] = world;
        }
        else Update3DTextLabelText(gAFKLabel[i], AFK_LABEL_COLOR, labelText);
    }
    return 1;
}
