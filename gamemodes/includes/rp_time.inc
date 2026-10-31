#if defined _tarkenso_rp_time_included
    #endinput
#endif
#define _tarkenso_rp_time_included

// RP-TIME-01: one-shot wristwatch animation and RP action.
// The lower-right PlayerTextDraw uses the player world clock, including admin overrides.
#define RP_TIME_ANIM_MS  (2100)
#define RP_TIME_TEXT_MS  (3500)
#define RP_TIME_COOLDOWN (4)

new gRPTimeNextUse[MAX_PLAYERS];
new PlayerText:gRPTimeDraw[MAX_PLAYERS];
new gRPTimeSerial[MAX_PLAYERS];
// COP_AMBIENT needs one extra ApplyAnimation on first use after spawn.
new bool:gRPTimeAnimPrimed[MAX_PLAYERS];

forward RPTime_HideLater(playerid, serial);

stock RPTime_Init()
{
    for (new i = 0; i < MAX_PLAYERS; i++)
    {
        gRPTimeDraw[i] = PlayerText:INVALID_TEXT_DRAW;
        gRPTimeSerial[i] = 0;
        gRPTimeNextUse[i] = 0;
        gRPTimeAnimPrimed[i] = false;
    }
    return 1;
}

stock RPTime_Hide(playerid)
{
    if (gRPTimeDraw[playerid] != PlayerText:INVALID_TEXT_DRAW)
    {
        if (IsPlayerConnected(playerid))
            PlayerTextDrawDestroy(playerid, gRPTimeDraw[playerid]);
        gRPTimeDraw[playerid] = PlayerText:INVALID_TEXT_DRAW;
    }
    return 1;
}

stock RPTime_Reset(playerid)
{
    // Invalidate pending callbacks before a player ID can be reused.
    gRPTimeSerial[playerid]++;
    RPTime_Hide(playerid);
    gRPTimeNextUse[playerid] = 0;
    gRPTimeAnimPrimed[playerid] = false;
    return 1;
}

public RPTime_HideLater(playerid, serial)
{
    if (IsPlayerConnected(playerid) && gRPTimeSerial[playerid] == serial)
        RPTime_Hide(playerid);
    return 1;
}

stock bool:RPTime_ShowClock(playerid, hour, minute)
{
    RPTime_Hide(playerid);
    new clockText[36];
    format(clockText, sizeof(clockText), "~w~TIME: ~y~%02d:%02d", hour, minute);
    // Native GameText has fixed screen anchors. TextDraw is used so the
    // same clock can sit in the lower-right corner (640x448 canvas).
    gRPTimeDraw[playerid] = CreatePlayerTextDraw(playerid,
        618.0, 374.0, clockText);
    if (gRPTimeDraw[playerid] == PlayerText:INVALID_TEXT_DRAW) return false;
    PlayerTextDrawAlignment(playerid, gRPTimeDraw[playerid], TEXT_DRAW_ALIGN_RIGHT);
    PlayerTextDrawFont(playerid, gRPTimeDraw[playerid], TEXT_DRAW_FONT_2);
    PlayerTextDrawLetterSize(playerid, gRPTimeDraw[playerid], 0.40, 1.70);
    PlayerTextDrawColour(playerid, gRPTimeDraw[playerid], 0xFFFFFFFF);
    PlayerTextDrawBackgroundColour(playerid, gRPTimeDraw[playerid], 0x11251BFF);
    PlayerTextDrawSetOutline(playerid, gRPTimeDraw[playerid], 1);
    PlayerTextDrawSetProportional(playerid, gRPTimeDraw[playerid], true);
    PlayerTextDrawShow(playerid, gRPTimeDraw[playerid]);
    gRPTimeSerial[playerid]++;
    if (!SetTimerEx("RPTime_HideLater", RP_TIME_TEXT_MS, false,
        "ii", playerid, gRPTimeSerial[playerid]))
    {
        RPTime_Hide(playerid);
        return false;
    }
    return true;
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

    new label[110];
    format(label, sizeof(label), "* %s посмотрел(а) на часы", gAccountName[playerid]);
    if (!RP_ShowActionLabel(playerid, label))
        return SendClientMessage(playerid, COLOR_RED,
            "Не удалось отобразить действие над персонажем.");

    // GTA SA may only load an animation library on its first ApplyAnimation.
    // Prime COP_AMBIENT once per spawn with no forced sync; then play the
    // visible two-second watch gesture for the player and nearby observers.
    // No timers, task clearing or additional animation on later /time calls.
    if (!gRPTimeAnimPrimed[playerid])
    {
        ApplyAnimation(playerid, "COP_AMBIENT", "Coplook_watch", 4.1,
            false, false, false, false, RP_TIME_ANIM_MS, SYNC_NONE);
        gRPTimeAnimPrimed[playerid] = true;
    }
    ApplyAnimation(playerid, "COP_AMBIENT", "Coplook_watch", 4.1,
        false, false, false, false, RP_TIME_ANIM_MS, SYNC_ALL);

    if (!RPTime_ShowClock(playerid, hour, minute))
        SendClientMessage(playerid, COLOR_RED, "Не удалось показать время на экране.");
    RP_SendLocal(playerid, RP_COLOR_ACTION, label, RP_RADIUS_ACTION);
    gRPTimeNextUse[playerid] = gettime() + RP_TIME_COOLDOWN;
    return 1;
}
