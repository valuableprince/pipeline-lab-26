#if defined _tarkenso_payday_included
    #endinput
#endif
#define _tarkenso_payday_included

// PAYDAY-01: one EXP at each server-clock HH:00; no AFK checks or wages.
// EXP needed for next level = current level * 4.
#define PAYDAY_EXP_PER_HOUR (1)
#define PAYDAY_COLOR (0xA7D6B5FF)

new gPayDayTimer;
new gPayDayLastHour;
forward PayDay_Tick();

stock PayDay_OnReady(playerid)
{
    if (!IsPlayerConnected(playerid) || IsPlayerNPC(playerid)) return 0;
    SetPlayerScore(playerid, gCharacterLevel[playerid]);
    return 1;
}

stock PayDay_Status(playerid)
{
    new message[104];
    if (gCharacterLevel[playerid] >= PAYDAY_LEVEL_LIMIT)
        format(message, sizeof(message), "Уровень: %d (максимум). EXP: %d.",
            gCharacterLevel[playerid], gCharacterEXP[playerid]);
    else
        format(message, sizeof(message), "Уровень: %d. EXP: %d/%d. PayDay каждый час в :00.",
            gCharacterLevel[playerid], gCharacterEXP[playerid],
            gCharacterLevel[playerid] * PAYDAY_EXP_STEP);
    SendClientMessage(playerid, PAYDAY_COLOR, message);
    return 1;
}

// Chat receipt is shown only after the persisted hourly grant succeeds.
// GameText style 1 is the native lower-right notification, no TextDraw needed.
stock PayDay_Notify(playerid, earnedExp, bool:levelUp)
{
    new message[144];
    SendClientMessage(playerid, PAYDAY_COLOR,
        "{D9AA55}[Tarkenso RPG] {D8AD65}========== TARKENSO | PAYDAY ==========");

    format(message, sizeof(message),
        "{D9AA55}[Tarkenso RPG] {E4ECF3}Опыт за час: {75DFA5}+%d EXP {B2BDCA}| {E4ECF3}Уровень: {EAC885}%d",
        earnedExp, gCharacterLevel[playerid]);
    SendClientMessage(playerid, PAYDAY_COLOR, message);

    if (gCharacterLevel[playerid] >= PAYDAY_LEVEL_LIMIT)
    {
        SendClientMessage(playerid, PAYDAY_COLOR,
            "{D9AA55}[Tarkenso RPG] {EAC885}Достигнут максимальный уровень персонажа.");
    }
    else
    {
        new threshold = gCharacterLevel[playerid] * PAYDAY_EXP_STEP;
        format(message, sizeof(message),
            "{D9AA55}[Tarkenso RPG] {E4ECF3}Прогресс: {EAC885}%d/%d EXP {B2BDCA}| {E4ECF3}Осталось: {FFFFFF}%d EXP",
            gCharacterEXP[playerid], threshold, threshold - gCharacterEXP[playerid]);
        SendClientMessage(playerid, PAYDAY_COLOR, message);
    }

    if (levelUp)
    {
        format(message, sizeof(message),
            "{D9AA55}[Tarkenso RPG] {EAC885}Поздравляем! Вы достигли %d уровня.", gCharacterLevel[playerid]);
        SendClientMessage(playerid, PAYDAY_COLOR, message);
    }

    SendClientMessage(playerid, PAYDAY_COLOR,
        "{D9AA55}[Tarkenso RPG] {D8AD65}========================================");
    GameTextForPlayer(playerid, "~w~TARKENSO~n~~y~PAYDAY", 5000, 1);
    return 1;
}

stock PayDay_Grant(playerid, hourKey)
{
#if defined TARKENSO_DB_PRIMARY
    if (!DBPrimary_IsReady() || !gDBPrimaryLoaded[playerid] ||
        gDBPrimaryAccountId[playerid] <= 0 || gDBPrimaryVersion[playerid] >= 2147483647 ||
        gCharacterLastPaydayHour[playerid] >= hourKey)
        return 0;

    new level = gCharacterLevel[playerid], exp = gCharacterEXP[playerid];
    if (level < 1 || level > PAYDAY_LEVEL_LIMIT || exp < 0 ||
        (level < PAYDAY_LEVEL_LIMIT && exp >= level * PAYDAY_EXP_STEP))
        return 0;

    new bool:levelUp = false;
    new earnedExp = level < PAYDAY_LEVEL_LIMIT ? PAYDAY_EXP_PER_HOUR : 0;
    if (level < PAYDAY_LEVEL_LIMIT)
    {
        exp += PAYDAY_EXP_PER_HOUR;
        if (exp >= level * PAYDAY_EXP_STEP)
        {
            exp -= level * PAYDAY_EXP_STEP;
            level++;
            levelUp = true;
        }
    }
    new query[340], affected;
    mysql_format(gDBPrimary, query, sizeof(query),
        "UPDATE accounts SET character_level=%d,character_exp=%d,last_payday_hour=%d,row_version=row_version+1 WHERE account_id=%d AND row_version=%d AND last_payday_hour<%d",
        level, exp, hourKey, gDBPrimaryAccountId[playerid],
        gDBPrimaryVersion[playerid], hourKey);
    if (!DBPrimary_Execute(query, affected) || affected != 1)
    {
        printf("[PayDay] Database grant failed for player %d; no EXP awarded.", playerid);
        return 0;
    }
    gDBPrimaryVersion[playerid]++;
    gCharacterLevel[playerid] = level;
    gCharacterEXP[playerid] = exp;
    gCharacterLastPaydayHour[playerid] = hourKey;
    SetPlayerScore(playerid, level);
    PayDay_Notify(playerid, earnedExp, levelUp);
    return 1;
#else
    return 0;
#endif
}

public PayDay_Tick()
{
    new hour, minute, second;
    new currentHourKey = gettime(hour, minute, second) / 3600;
    if (minute != 0 || currentHourKey == gPayDayLastHour) return 1;
    // Update the process-wide clock before granting to prevent repeated awards.
    gPayDayLastHour = currentHourKey;
    printf("[PayDay] Processing %02d:%02d, hour key %d.", hour, second, currentHourKey);
    for (new playerid = 0; playerid < MAX_PLAYERS; playerid++)
    {
        if (!IsPlayerConnected(playerid) || IsPlayerNPC(playerid) ||
            gAuthState[playerid] != AUTH_READY) continue;
        PayDay_Grant(playerid, currentHourKey);
    }
    return 1;
}

stock PayDay_Init()
{
    gPayDayLastHour = gettime() / 3600;
    gPayDayTimer = SetTimer("PayDay_Tick", 1000, true);
    print("[PayDay] Hourly EXP scheduler initialized.");
    return 1;
}

stock PayDay_Shutdown()
{
    if (gPayDayTimer) KillTimer(gPayDayTimer);
    gPayDayTimer = 0;
    return 1;
}
