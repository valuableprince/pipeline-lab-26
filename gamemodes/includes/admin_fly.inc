#if defined _tarkenso_admin_fly_included
    #endinput
#endif
#define _tarkenso_admin_fly_included

// ADMIN-FLY-01: server-native flight with per-player gravity disabled.
// The feature is restricted to authorized staff actively on /aduty.
#define ADMIN_FLY_TICK_MS (100)
#define ADMIN_FLY_COLOR ADMIN_COLOR
#define ADMIN_FLY_SPEED (0.24)
#define ADMIN_FLY_LIFT (0.18)

new bool:gAdminFly[MAX_PLAYERS];
new Float:gAdminFlyOldGravity[MAX_PLAYERS];
new gAdminFlyTimer;
forward AdminFly_Tick();

stock AdminFly_Stop(playerid)
{
    if (!gAdminFly[playerid]) return 1;
    gAdminFly[playerid] = false;
    if (IsPlayerConnected(playerid))
    {
        SetPlayerGravity(playerid, gAdminFlyOldGravity[playerid]);
        if (GetPlayerState(playerid) == PLAYER_STATE_ONFOOT)
            SetPlayerVelocity(playerid, 0.0, 0.0, 0.0);
    }
    return 1;
}

stock AdminFly_Reset(playerid)
{
    AdminFly_Stop(playerid);
    gAdminFlyOldGravity[playerid] = 0.008;
    return 1;
}

stock AdminFly_Command(playerid, const params[])
{
    #pragma unused params
    if (!Admin_Require(playerid, PERM_MODERATE)) return 1;
    if (gAdminFly[playerid])
    {
        AdminFly_Stop(playerid);
        return SendClientMessage(playerid, ADMIN_FLY_COLOR,
            "[TARKENSO] Полёт выключен. Обычная гравитация восстановлена.");
    }
    if (GetPlayerState(playerid) != PLAYER_STATE_ONFOOT ||
        GetPlayerSpecialAction(playerid) == SPECIAL_ACTION_USEJETPACK ||
        Race_IsParticipant(playerid) || AdminPunish_IsJailed(playerid))
        return SendClientMessage(playerid, ADMIN_ERROR,
            "[TARKENSO] Полёт доступен только пешком, вне гонок, джетпака и наказаний.");
    gAdminFlyOldGravity[playerid] = GetPlayerGravity(playerid);
    if (!SetPlayerGravity(playerid, 0.0))
        return SendClientMessage(playerid, ADMIN_ERROR, "[TARKENSO] Не удалось включить полёт.");
    gAdminFly[playerid] = true;
    SendClientMessage(playerid, ADMIN_FLY_COLOR,
        "[TARKENSO] Полёт: W/S вперёд-назад, A/D вбок, прыжок вверх, спринт вниз.");
    return SendClientMessage(playerid, ADMIN_FLY_COLOR,
        "[TARKENSO] Камера задаёт направление. /fly — выключить. Справка: /ahelp fly.");
}

public AdminFly_Tick()
{
    for (new playerid = 0; playerid < MAX_PLAYERS; playerid++)
    {
        if (!gAdminFly[playerid]) continue;
        if (!IsPlayerConnected(playerid) ||
            !Admin_Allowed(playerid, PERM_MODERATE) ||
            !gAdminDuty[playerid] ||
            GetPlayerState(playerid) != PLAYER_STATE_ONFOOT ||
            Race_IsParticipant(playerid) ||
            AdminPunish_IsJailed(playerid))
        {
            AdminFly_Stop(playerid);
            continue;
        }
        new KEY:keys, updown, leftright;
        GetPlayerKeys(playerid, keys, updown, leftright);

        new Float:fx, Float:fy, Float:fz;
        GetPlayerCameraFrontVector(playerid, fx, fy, fz);
        new Float:length = floatsqroot(fx * fx + fy * fy);
        if (length < 0.001) continue;
        fx /= length;
        fy /= length;
        new Float:forwardMove = 0.0, Float:sideMove = 0.0, Float:verticalMove = 0.0;
        if (updown == KEY_UP) forwardMove = 1.0;
        else if (updown == KEY_DOWN) forwardMove = -1.0;
        if (leftright == KEY_LEFT) sideMove = -1.0;
        else if (leftright == KEY_RIGHT) sideMove = 1.0;
        if ((keys & KEY_JUMP) != KEY:0) verticalMove = 1.0;
        else if ((keys & KEY_SPRINT) != KEY:0) verticalMove = -1.0;

        new Float:scale = floatsqroot(forwardMove * forwardMove + sideMove * sideMove);
        if (scale > 1.0)
        {
            forwardMove /= scale;
            sideMove /= scale;
        }
        new Float:vx = (fx * forwardMove + fy * sideMove) * ADMIN_FLY_SPEED;
        new Float:vy = (fy * forwardMove - fx * sideMove) * ADMIN_FLY_SPEED;
        SetPlayerVelocity(playerid, vx, vy, verticalMove * ADMIN_FLY_LIFT);
    }
    return 1;
}

stock AdminFly_Init()
{
    for (new playerid = 0; playerid < MAX_PLAYERS; playerid++)
    {
        gAdminFly[playerid] = false;
        gAdminFlyOldGravity[playerid] = 0.008;
    }
    gAdminFlyTimer = SetTimer("AdminFly_Tick", ADMIN_FLY_TICK_MS, true);
    print("[AdminFly] Native flight initialized.");
    return 1;
}

stock AdminFly_Shutdown()
{
    if (gAdminFlyTimer) KillTimer(gAdminFlyTimer);
    gAdminFlyTimer = 0;
    for (new playerid = 0; playerid < MAX_PLAYERS; playerid++)
        AdminFly_Stop(playerid);
    return 1;
}
