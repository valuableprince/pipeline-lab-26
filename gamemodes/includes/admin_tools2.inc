#if defined _tarkenso_admin_tools2_included
    #endinput
#endif
#define _tarkenso_admin_tools2_included

// ADMIN-TOOLS-02: moderation, world controls and safe narrative boundaries.
new gAdminChatClearNext;

stock bool:AdminTools2_ParseOne(const params[], &first)
{
    return AdminTools_ReadTarget(params, first);
}

stock bool:AdminTools2_ParseTwo(const params[], &first, &second)
{
    new offset, a[24], b[24], extra[12];
    if (!Admin_Next(params, offset, a, sizeof(a)) ||
        !Admin_Next(params, offset, b, sizeof(b)) ||
        Admin_Next(params, offset, extra, sizeof(extra)) ||
        !Admin_UnsignedNumber(a) || !Admin_UnsignedNumber(b)) return false;
    first = strval(a);
    second = strval(b);
    return true;
}

stock bool:AdminTools2_CanMove(playerid, target)
{
    if (!AdminTools_CanEditPlayer(playerid, target)) return false;
    if (AdminPunish_IsJailed(target))
    {
        SendClientMessage(playerid, ADMIN_ERROR, "Нельзя перемещать заключённого.");
        return false;
    }
    if (gStoryStage[target] < STORY_DONE)
    {
        SendClientMessage(playerid, ADMIN_ERROR,
            "Действие недоступно во время сюжетного пролога.");
        return false;
    }
    if (GetPlayerState(target) != PLAYER_STATE_ONFOOT)
    {
        SendClientMessage(playerid, ADMIN_ERROR,
            "Игрок должен находиться пешком, вне транспорта.");
        return false;
    }
    return true;
}

stock AdminTools2_Unmute(playerid, const params[])
{
    if (!Admin_Require(playerid, PERM_MODERATE)) return 1;
    new target;
    if (!AdminTools2_ParseOne(params, target)) return AdminHelp_Usage(playerid, "unmute");
    if (!Admin_ValidTarget(playerid, target)) return 1;
    if (gAdminMuteUntil[target] <= gettime())
        return SendClientMessage(playerid, ADMIN_ERROR, "У игрока нет действующей блокировки чата.");
    new detail[90];
    format(detail, sizeof(detail), "%s: снятие блокировки чата", gAccountName[target]);
    if (!Admin_LogPlayer(playerid, "UNMUTE", detail))
        return SendClientMessage(playerid, ADMIN_ERROR, "Журнал недоступен. Действие отменено.");
    new oldUntil = gAdminMuteUntil[target];
    gAdminMuteUntil[target] = 0;
    if (!Auth_SaveAccount(target))
    {
        gAdminMuteUntil[target] = oldUntil;
        return SendClientMessage(playerid, ADMIN_ERROR, "Ошибка сохранения. Блокировка сохранена.");
    }
    SendClientMessage(target, ADMIN_OK, "Администратор досрочно разблокировал ваш чат.");
    return SendClientMessage(playerid, ADMIN_OK, "Блокировка чата снята.");
}

stock bool:AdminTools2_HistoryAction(const action[])
{
    return !strcmp(action, "WARN", true) ||
        !strcmp(action, "MUTE", true) ||
        !strcmp(action, "UNMUTE", true) ||
        !strcmp(action, "KICK", true) ||
        !strcmp(action, "BAN", true);
}

stock AdminTools2_History(playerid, const params[])
{
    if (!Admin_Require(playerid, PERM_MODERATE)) return 1;
    new target;
    if (!AdminTools2_ParseOne(params, target)) return AdminHelp_Usage(playerid, "history");
    if (!Admin_Ready(target))
        return SendClientMessage(playerid, ADMIN_ERROR, "Игрок не найден или не авторизован.");
    new detail[86], line[240];
    format(detail, sizeof(detail), "target=%s id=%d", gAccountName[target], target);
    if (!Admin_LogPlayer(playerid, "HISTORY_VIEW", detail))
        return SendClientMessage(playerid, ADMIN_ERROR, "Журнал недоступен. Просмотр отменён.");
    new File:log = fopen(ADMIN_DIRECTORY "/audit.log", io_read);
    if (!log) return SendClientMessage(playerid, ADMIN_ERROR, "Не удалось открыть историю.");

    new entries[5][140], count;
    new targetLength = strlen(gAccountName[target]);
    while (fread(log, line, sizeof(line)))
    {
        Auth_StripLine(line);
        new pos, pipes, action[24], ts[16], start, actionStart = -1, detailStart = -1;
        while (line[pos])
        {
            if (line[pos] == '|')
            {
                pipes++;
                if (pipes == 1) actionStart = pos + 1;
                if (pipes == 2)
                {
                    if (actionStart < 0 || pos - actionStart >= sizeof(action)) break;
                    strmid(action, line, actionStart, pos, sizeof(action));
                }
                if (pipes == 3) { detailStart = pos + 1; break; }
            }
            pos++;
        }
        if (detailStart < 0 || !AdminTools2_HistoryAction(action)) continue;
        if (strlen(line) < detailStart + targetLength + 1 ||
            line[detailStart + targetLength] != ':') continue;
        new bool:matches = true;
        for (new k = 0; k < targetLength; k++)
            if (line[detailStart + k] != gAccountName[target][k])
            {
                matches = false;
                break;
            }
        if (!matches) continue;
        start = strfind(line, "|");
        if (start < 1 || start >= sizeof(ts)) continue;
        strmid(ts, line, 0, start, sizeof(ts));
        new summary[140];
        format(summary, sizeof(summary), "[%s] %s: %.70s", ts, action, line[detailStart + targetLength + 1]);
        for (new j = 0; j < 4; j++)
            format(entries[j], sizeof(entries[]), "%s", entries[j + 1]);
        format(entries[4], sizeof(entries[]), "%s", summary);
        count++;
    }
    fclose(log);
    if (!count) return SendClientMessage(playerid, ADMIN_COLOR, "В журнале наказаний записей не найдено.");
    SendClientMessage(playerid, ADMIN_COLOR, "История: пять последних записей (Unix-время, действие, причина):");
    new first = count >= 5 ? 0 : 5 - count;
    for (new i = 4; i >= first; i--)
        SendClientMessage(playerid, ADMIN_COLOR, entries[i]);
    return 1;
}

stock AdminTools2_Respawn(playerid, const params[])
{
    if (!Admin_Require(playerid, PERM_BAN_DAY)) return 1;
    new target;
    if (!AdminTools2_ParseOne(params, target)) return AdminHelp_Usage(playerid, "respawn");
    if (!AdminTools2_CanMove(playerid, target)) return 1;
    new detail[80];
    format(detail, sizeof(detail), "target=%s id=%d", gAccountName[target], target);
    if (!Admin_LogPlayer(playerid, "RESPAWN", detail))
        return SendClientMessage(playerid, ADMIN_ERROR, "Журнал недоступен. Действие отменено.");
    SpawnPlayer(target);
    return SendClientMessage(playerid, ADMIN_OK, "Игрок отправлен на точку появления.");
}

stock AdminTools2_SetSkin(playerid, const params[])
{
    if (!Admin_Require(playerid, PERM_BAN_DAY)) return 1;
    new target, skin;
    if (!AdminTools2_ParseTwo(params, target, skin) || skin > 311 || skin == 74)
        return AdminHelp_Usage(playerid, "setskin");
    if (!AdminTools2_CanMove(playerid, target)) return 1;
    new detail[90], msg[96];
    format(detail, sizeof(detail), "target=%s id=%d skin=%d", gAccountName[target], target, skin);
    if (!Admin_LogPlayer(playerid, "SETSKIN", detail))
        return SendClientMessage(playerid, ADMIN_ERROR, "Журнал недоступен. Действие отменено.");
    SetPlayerSkin(target, skin);
    format(msg, sizeof(msg), "Скин игрока %d изменён на %d до следующего спавна.", target, skin);
    SendClientMessage(target, ADMIN_COLOR, "Администратор временно изменил вашу внешность.");
    return SendClientMessage(playerid, ADMIN_OK, msg);
}

stock AdminTools2_SetTime(playerid, const params[])
{
    if (!Admin_Require(playerid, PERM_BAN_DAY)) return 1;
    new hour, minute;
    if (!AdminTools2_ParseTwo(params, hour, minute) || hour > 23 || minute > 59)
        return AdminHelp_Usage(playerid, "settime");
    new detail[36], msg[92];
    format(detail, sizeof(detail), "%02d:%02d", hour, minute);
    if (!Admin_LogPlayer(playerid, "SETTIME", detail))
        return SendClientMessage(playerid, ADMIN_ERROR, "Журнал недоступен. Действие отменено.");
    gAdminClockHour = hour;
    gAdminClockMinute = minute;
    for (new i = 0; i < MAX_PLAYERS; i++)
        if (Admin_Ready(i)) SetPlayerTime(i, hour, minute);
    format(msg, sizeof(msg), "Игровое время установлено: %02d:%02d.", hour, minute);
    return SendClientMessage(playerid, ADMIN_OK, msg);
}

stock AdminTools2_ClearChat(playerid, const params[])
{
    if (!Admin_Require(playerid, PERM_BAN_MONTH)) return 1;
    new offset, extra[8];
    if (Admin_Next(params, offset, extra, sizeof(extra)))
        return AdminHelp_Usage(playerid, "clearchat");
    if (gettime() < gAdminChatClearNext)
        return SendClientMessage(playerid, ADMIN_ERROR, "Повторная очистка чата доступна через 30 секунд.");
    if (!Admin_LogPlayer(playerid, "CLEAR_CHAT", "global"))
        return SendClientMessage(playerid, ADMIN_ERROR, "Журнал недоступен. Действие отменено.");
    gAdminChatClearNext = gettime() + 30;
    for (new i = 0; i < 16; i++) SendClientMessageToAll(COLOR_WHITE, " ");
    SendClientMessageToAll(ADMIN_COLOR, "Администратор очистил общий чат.");
    return 1;
}

stock AdminTools2_SetDimension(playerid, const params[], bool:world)
{
    if (!Admin_Require(playerid, PERM_BAN_MONTH)) return 1;
    new target, dimension;
    if (!AdminTools2_ParseTwo(params, target, dimension) ||
        dimension < 0 || dimension > (world ? 65535 : 255))
        return AdminHelp_Usage(playerid, world ? "setvw" : "setint");
    if (!AdminTools2_CanMove(playerid, target)) return 1;
    if (world && dimension == RACE_WORLD_ID)
        return SendClientMessage(playerid, ADMIN_ERROR, "Мир гонок зарезервирован.");
    new detail[100], msg[92];
    format(detail, sizeof(detail), "target=%s id=%d %s=%d",
        gAccountName[target], target, world ? "world" : "interior", dimension);
    if (!Admin_LogPlayer(playerid, world ? "SETVW" : "SETINT", detail))
        return SendClientMessage(playerid, ADMIN_ERROR, "Журнал недоступен. Действие отменено.");
    if (world) SetPlayerVirtualWorld(target, dimension);
    else SetPlayerInterior(target, dimension);
    SetCameraBehindPlayer(target);
    format(msg, sizeof(msg), world ? "Виртуальный мир игрока %d: %d." :
        "Интерьер игрока %d: %d.", target, dimension);
    return SendClientMessage(playerid, ADMIN_OK, msg);
}
