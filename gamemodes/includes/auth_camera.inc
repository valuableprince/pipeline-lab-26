// AUTH-UI-01: ночная панорама Los Santos. Точки допускают последующую настройку.
#define AUTH_CAMERA_INTERVAL_MS (14000)
#define AUTH_CAMERA_WORLD_BASE (2000)

new gAuthCameraTimer[MAX_PLAYERS];
new gAuthCameraPhase[MAX_PLAYERS];

forward AuthCamera_Initialize(playerid, token);
forward AuthCamera_Advance(playerid, token);

stock AuthCamera_SetOpeningView(playerid)
{
    SetPlayerCameraPos(playerid, 1572.0, -1722.0, 86.0);
    SetPlayerCameraLookAt(playerid, 1468.0, -1715.0, 34.0, CAMERA_CUT);
    return 1;
}

stock AuthCamera_Start(playerid)
{
    if (gAuthCameraTimer[playerid]) KillTimer(gAuthCameraTimer[playerid]);
    gAuthCameraTimer[playerid] = 0;
    gAuthCameraPhase[playerid] = 0;
    SetPlayerInterior(playerid, 0);
    SetPlayerVirtualWorld(playerid, AUTH_CAMERA_WORLD_BASE + playerid);
    TogglePlayerControllable(playerid, false);
    TogglePlayerSpectating(playerid, true);
    SetPlayerTime(playerid, 3, 17);
    SetPlayerWeather(playerid, 9);
    // После TogglePlayerSpectating камера устанавливается на следующем тике.
    gAuthCameraTimer[playerid] = SetTimerEx("AuthCamera_Initialize", 350,
        false, "ii", playerid, gAuthToken[playerid]);
    return 1;
}

public AuthCamera_Initialize(playerid, token)
{
    if (!IsPlayerConnected(playerid) || gAuthToken[playerid] != token ||
        gAuthState[playerid] == AUTH_NONE || gAuthState[playerid] == AUTH_READY) return 1;
    gAuthCameraTimer[playerid] = 0;
    AuthCamera_SetOpeningView(playerid);
    gAuthCameraTimer[playerid] = SetTimerEx("AuthCamera_Advance",
        1700, false, "ii", playerid, token);
    return 1;
}

public AuthCamera_Advance(playerid, token)
{
    if (!IsPlayerConnected(playerid) || gAuthToken[playerid] != token ||
        gAuthState[playerid] == AUTH_NONE || gAuthState[playerid] == AUTH_READY) return 1;

    gAuthCameraTimer[playerid] = 0;
    if (!gAuthCameraPhase[playerid])
    {
        InterpolateCameraPos(playerid, 1572.0, -1722.0, 86.0,
            1285.0, -1461.0, 100.0, AUTH_CAMERA_INTERVAL_MS, CAMERA_MOVE);
        InterpolateCameraLookAt(playerid, 1468.0, -1715.0, 34.0,
            1386.0, -1530.0, 48.0, AUTH_CAMERA_INTERVAL_MS, CAMERA_MOVE);
        gAuthCameraPhase[playerid] = 1;
    }
    else
    {
        InterpolateCameraPos(playerid, 1285.0, -1461.0, 100.0,
            1572.0, -1722.0, 86.0, AUTH_CAMERA_INTERVAL_MS, CAMERA_MOVE);
        InterpolateCameraLookAt(playerid, 1386.0, -1530.0, 48.0,
            1468.0, -1715.0, 34.0, AUTH_CAMERA_INTERVAL_MS, CAMERA_MOVE);
        gAuthCameraPhase[playerid] = 0;
    }
    gAuthCameraTimer[playerid] = SetTimerEx("AuthCamera_Advance",
        AUTH_CAMERA_INTERVAL_MS + 100, false, "ii", playerid, token);
    return 1;
}

stock AuthCamera_Stop(playerid)
{
    if (gAuthCameraTimer[playerid]) KillTimer(gAuthCameraTimer[playerid]);
    gAuthCameraTimer[playerid] = 0;
    gAuthCameraPhase[playerid] = 0;
    return 1;
}
