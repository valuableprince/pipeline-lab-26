#if defined _tarkenso_payday_included
    #endinput
#endif
#define _tarkenso_payday_included

// PAYDAY-01: one EXP at each server-clock HH:00; no AFK checks or wages.
// EXP needed for next level = current level * 4.
#define PAYDAY_EXP_PER_HOUR (1)
#define PAYDAY_COLOR (0xA7D6B5FF)
#define PAYDAY_RETRY_SECONDS (10)

new gPayDayTimer;
new gPayDayLastHour;
// Pending hours are scheduled only for authenticated players online at HH:00.
// A failed database write remains pending rather than being lost for that hour.
new gPayDayPendingHour[MAX_PLAYERS];
new gPayDayPendingThrough[MAX_PLAYERS];
new gPayDayNextRetry[MAX_PLAYERS];
forward PayDay_Tick();

stock PayDay_Reset(playerid)
{
    gPayDayPendingHour[playerid] = 0;
    gPayDayPendingThrough[playerid] = 0;
    gPayDayNextRetry[playerid] = 0;
    return 1;
}

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
    if (levelUp)
    {
        new message[100];
        format(message, sizeof(message), "PAYDAY: +1 EXP. Новый уровень: %d! Поздравляем.", level);
        SendClientMessage(playerid, PAYDAY_COLOR, message);
    }
    else
        SendClientMessage(playerid, PAYDAY_COLOR, "PAYDAY: +1 EXP.");
    PayDay_Status(playerid);
    return 1;
#else
    return 0;
#endif
}

// At a real hourly boundary, mark the then-online players as eligible. Failed
// grants are retried outside :00; later hourly awards queue behind older ones.
public PayDay_Tick()
{
    new hour, minute, second;
    new now = gettime(hour, minute, second);
    new currentHourKey = now / 3600;
    if (minute == 0 && currentHourKey != gPayDayLastHour)
    {
        gPayDayLastHour = currentHourKey;
        printf("[PayDay] Scheduling %02d:%02d, hour key %d.", hour, second, currentHourKey);
        for (new playerid = 0; playerid < MAX_PLAYERS; playerid++)
        {
            if (!IsPlayerConnected(playerid) || IsPlayerNPC(playerid) ||
                gAuthState[playerid] != AUTH_READY) continue;
            if (!gPayDayPendingHour[playerid])
                gPayDayPendingHour[playerid] = currentHourKey;
            gPayDayPendingThrough[playerid] = currentHourKey;
        }
    }
    for (new playerid = 0; playerid < MAX_PLAYERS; playerid++)
    {
        new pending = gPayDayPendingHour[playerid];
        if (!pending) continue;
        if (!IsPlayerConnected(playerid) || IsPlayerNPC(playerid) ||
            gAuthState[playerid] != AUTH_READY)
        {
            PayDay_Reset(playerid);
            continue;
        }
        if (now < gPayDayNextRetry[playerid]) continue;
        if (gCharacterLastPaydayHour[playerid] >= pending ||
            PayDay_Grant(playerid, pending))
        {
            pending++;
            if (pending > gPayDayPendingThrough[playerid])
                PayDay_Reset(playerid);
            else
                gPayDayPendingHour[playerid] = pending;
        }
        else
            gPayDayNextRetry[playerid] = now + PAYDAY_RETRY_SECONDS;
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
