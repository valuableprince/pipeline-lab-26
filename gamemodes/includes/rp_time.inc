#if defined _tarkenso_rp_time_included
    #endinput
#endif
#define _tarkenso_rp_time_included

// RP-TIME-01: one-shot wristwatch animation and RP action.
// GameText uses the player's world clock, including administrator overrides.
#define RP_TIME_ANIM_MS  (2100)
#define RP_TIME_TEXT_MS  (3500)
#define RP_TIME_COOLDOWN (4)

new gRPTimeNextUse[MAX_PLAYERS];

stock RPTime_Reset(playerid)
{
    gRPTimeNextUse[playerid] = 0;
    return 1;
}

stock bool:RPTime_Allowed(playerid)
{
    return IsPlayerConnected(playerid) && !IsPlayerNPC(playerid) &&
        gAuthState[playerid] == AUTH_READY && gAccountSpawned[playerid] &&
        !gAccountDead[playerid] && !gAdminFrozen[playerid] &&
        !gAdminFly[playerid] && !TDS_Active(playerid) &&
        !Race_IsParticipant(playerid) && !gCleanerDuty[playerid] &&
        !RPAnim_Active(playerid) &&
        GetPlayerState(playerid) == PLAYER_STATE_ONFOOT &&
        GetPlayerSpecialAction(playerid) == SPECIAL_ACTION_NONE;
}

stock RPTime_Command(playerid)
{
    if (!RPTime_Allowed(playerid))
        return SendClientMessage(playerid, COLOR_AMBER,
            "Посмотреть на часы можно только пешком, вне работы и других действий.");

    new Float:vx, Float:vy, Float:vz;
    GetPlayerVelocity(playerid, vx, vy, vz);
    if (vx * vx + vy * vy + vz * vz > 0.0025)
        return SendClientMessage(playerid, COLOR_AMBER,
            "Остановитесь, чтобы посмотреть на часы.");

    new index = GetPlayerAnimationIndex(playerid);
    if (index)
    {
        new library[32], clip[32];
        GetAnimationName(index, library, sizeof(library), clip, sizeof(clip));
        if (strcmp(library, "PED", true) ||
            (strfind(clip, "IDLE_", true) != 0 &&
             strfind(clip, "idlestance", true) != 0))
            return SendClientMessage(playerid, COLOR_AMBER,
                "Сначала завершите текущее действие.");
    }

    if (gettime() < gRPTimeNextUse[playerid])
        return SendClientMessage(playerid, COLOR_AMBER,
            "Посмотреть на часы повторно можно через несколько секунд.");

    new hour, minute;
    if (!GetPlayerTime(playerid, hour, minute))
        return SendClientMessage(playerid, COLOR_RED,
            "Не удалось определить игровое время.");
    if (hour < 0 || hour > 23 || minute < 0 || minute > 59)
        return SendClientMessage(playerid, COLOR_RED,
            "Игровое время временно недоступно.");

    new label[110], clockText[36];
    format(label, sizeof(label), "* %s посмотрел(а) на часы", gAccountName[playerid]);
    if (!RP_ShowActionLabel(playerid, label))
        return SendClientMessage(playerid, COLOR_RED,
            "Не удалось отобразить действие над персонажем.");

    // Two-second GTA SA clip; no loop, freeze, timers or ClearAnimations.
    ApplyAnimation(playerid, "COP_AMBIENT", "Coplook_watch", 4.1,
        false, false, false, false, RP_TIME_ANIM_MS, SYNC_ALL);

    format(clockText, sizeof(clockText), "~w~TIME: ~y~%02d:%02d", hour, minute);
    GameTextForPlayer(playerid, clockText, RP_TIME_TEXT_MS, 3);
    RP_SendLocal(playerid, RP_COLOR_ACTION, label, RP_RADIUS_ACTION);
    gRPTimeNextUse[playerid] = gettime() + RP_TIME_COOLDOWN;
    return 1;
}
