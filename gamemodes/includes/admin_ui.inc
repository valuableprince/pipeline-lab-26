#if defined _signal_admin_ui_included
    #endinput
#endif
#define _signal_admin_ui_included

// Только стандартные игровые диалоги без графических наложений.
#define ADMIN_DLG_MAIN (6509)
#define ADMIN_DLG_REPORTS (6510)
#define ADMIN_DLG_PLAYERS (6511)
#define ADMIN_DLG_PLAYER_ACTIONS (6512)
#define ADMIN_DLG_PLAYER_REASON (6513)
#define ADMIN_DLG_PUNISH (6514)
#define ADMIN_DLG_EVENTS (6515)

new gAdminUIReportMap[MAX_PLAYERS][8];
new gAdminUIReportSerial[MAX_PLAYERS][8];
new gAdminUIPlayerMap[MAX_PLAYERS][32];
new gAdminUIPlayerTokens[MAX_PLAYERS][32];
new gAdminUIReportCount[MAX_PLAYERS];
new gAdminUIPlayerCount[MAX_PLAYERS];
new gAdminUIDialogMode[MAX_PLAYERS];
new gAdminUITarget[MAX_PLAYERS];
new gAdminUITargetToken[MAX_PLAYERS];
new gAdminUIAction[MAX_PLAYERS];

stock Admin_UI_Toggle(playerid)
{
    if (!Admin_Allowed(playerid, PERM_REPORT)) return 1;
    new title[90], body[510];
    if (Admin_IsFounder(playerid))
        format(title, sizeof(title), "Tarkenso RPG | VEYRAN · FOUNDER | Duty: %s", gAdminDuty[playerid] ? ("ON") : ("OFF"));
    else
        format(title, sizeof(title), "ADMIN | Уровень %d | Duty: %s",
            gAdminLevel[playerid], gAdminDuty[playerid] ? ("ON") : ("OFF"));
    format(body, sizeof(body),
        "Рабочий режим: %s\nОбращения: %d открыто\nИгроки\nНаказания и команды\nЖурнал действий\nСюжетные события\nТелепортация (карта / координаты)",
        gAdminDuty[playerid] ? ("выключить") : ("включить"), Admin_OpenReportCount());
    gAdminUIDialogMode[playerid] = 1;
    ShowPlayerDialog(playerid, ADMIN_DLG_MAIN, DIALOG_STYLE_LIST,
        title, body, "Выбрать", "Закрыть");
    return 1;
}

stock Admin_UI_ShowReports(playerid)
{
    if (!Admin_Require(playerid, PERM_REPORT)) return 1;
    new body[1024], row[160], count;
    body[0] = EOS;
    for (new i = 0; i < ADMIN_REPORT_LIMIT && count < 8; i++)
    {
        if (gReportState[i] != 1) continue;
        gAdminUIReportMap[playerid][count] = i;
        gAdminUIReportSerial[playerid][count] = gReportNumber[i];
        format(row, sizeof(row), "#%d | %s | %.60s\n",
            gReportNumber[i], gReportAuthor[i], gReportMessage[i]);
        strcat(body, row, sizeof(body));
        count++;
    }
    if (!count) return SendClientMessage(playerid, ADMIN_OK, "Открытых обращений нет.");
    gAdminUIReportCount[playerid] = count;
    gAdminUIDialogMode[playerid] = 2;
    ShowPlayerDialog(playerid, ADMIN_DLG_REPORTS, DIALOG_STYLE_LIST,
        "ADMIN | Обращения", body, "Принять", "Назад");
    return 1;
}

stock Admin_UI_ShowPlayers(playerid)
{
    if (!Admin_Require(playerid, PERM_MODERATE)) return 1;
    new body[1024], row[64], count;
    body[0] = EOS;
    for (new i = 0; i < MAX_PLAYERS && count < 32; i++)
    {
        if (!Admin_CanPunish(playerid, i)) continue;
        gAdminUIPlayerMap[playerid][count] = i;
        gAdminUIPlayerTokens[playerid][count] = gAuthToken[i];
        format(row, sizeof(row), "ID %d | %.20s\n", i, gAccountName[i]);
        strcat(body, row, sizeof(body));
        count++;
    }
    if (!count) return SendClientMessage(playerid, ADMIN_ERROR, "Подходящих игроков нет.");
    gAdminUIPlayerCount[playerid] = count;
    gAdminUIDialogMode[playerid] = 3;
    ShowPlayerDialog(playerid, ADMIN_DLG_PLAYERS, DIALOG_STYLE_LIST,
        "ADMIN | Игроки", body, "Выбрать", "Назад");
    return 1;
}

stock Admin_UI_ShowPunish(playerid)
{
    if (!Admin_Require(playerid, PERM_MODERATE)) return 1;
    gAdminUIDialogMode[playerid] = 6;
    ShowPlayerDialog(playerid, ADMIN_DLG_PUNISH, DIALOG_STYLE_MSGBOX,
        "ADMIN | Наказания",
        "/warn ID причина\n/mute ID минуты причина\n/kick ID причина\n/freeze ID\n\n\
         С уровня 3: /ban ID минуты причина\n\
         С уровня 5: /ban ID 0 причина, /unban ник\n\
         Нужны причина, подходящий уровень и Duty.",
        "Назад", "Закрыть");
    return 1;
}

stock Admin_UI_ShowEvents(playerid)
{
    if (!Admin_Require(playerid, PERM_EVENT)) return 1;
    gAdminUIDialogMode[playerid] = 7;
    ShowPlayerDialog(playerid, ADMIN_DLG_EVENTS, DIALOG_STYLE_LIST,
        "ADMIN | Сюжетные события",
        "Снять эффекты\nТуман\nНочная сцена",
        "Применить", "Назад");
    return 1;
}

stock Admin_UI_OnDialog(playerid, dialogid, response, listitem, const inputtext[])
{
    if (dialogid == ADMIN_DLG_MAIN)
    {
        if (gAdminUIDialogMode[playerid] != 1) return 1;
        gAdminUIDialogMode[playerid] = 0;
        if (!response || listitem < 0 || listitem > 6) return 1;
        if (!Admin_Allowed(playerid, PERM_REPORT)) return 1;
        switch (listitem)
        {
            case 0:
            {
                Admin_Execute(playerid, "/aduty");
                return Admin_UI_Toggle(playerid);
            }
            case 1: return Admin_UI_ShowReports(playerid);
            case 2: return Admin_UI_ShowPlayers(playerid);
            case 3: return Admin_UI_ShowPunish(playerid);
            case 4:
            {
                Admin_RecentLogs(playerid);
                return Admin_UI_Toggle(playerid);
            }
            case 5: return Admin_UI_ShowEvents(playerid);
            case 6: return AdminTP_Info(playerid);
        }
        return 1;
    }
    if (dialogid == ADMIN_DLG_REPORTS)
    {
        if (gAdminUIDialogMode[playerid] != 2) return 1;
        gAdminUIDialogMode[playerid] = 0;
        if (!response) return Admin_UI_Toggle(playerid);
        if (listitem < 0 || listitem >= gAdminUIReportCount[playerid] ||
            !Admin_Require(playerid, PERM_REPORT)) return 1;
        new index = gAdminUIReportMap[playerid][listitem];
        if (index < 0 || index >= ADMIN_REPORT_LIMIT || gReportState[index] != 1 ||
            gReportNumber[index] != gAdminUIReportSerial[playerid][listitem]) return 1;
        new command[32];
        format(command, sizeof(command), "/accept %d", gReportNumber[index]);
        return Admin_Execute(playerid, command);
    }
    if (dialogid == ADMIN_DLG_PLAYERS)
    {
        if (gAdminUIDialogMode[playerid] != 3) return 1;
        gAdminUIDialogMode[playerid] = 0;
        if (!response) return Admin_UI_Toggle(playerid);
        if (listitem < 0 || listitem >= gAdminUIPlayerCount[playerid] ||
            !Admin_Require(playerid, PERM_MODERATE)) return 1;
        new target = gAdminUIPlayerMap[playerid][listitem];
        if (!Admin_ValidTarget(playerid, target) ||
            gAuthToken[target] != gAdminUIPlayerTokens[playerid][listitem]) return 1;
        gAdminUITarget[playerid] = target;
        gAdminUITargetToken[playerid] = gAuthToken[target];
        gAdminUIDialogMode[playerid] = 4;
        ShowPlayerDialog(playerid, ADMIN_DLG_PLAYER_ACTIONS, DIALOG_STYLE_LIST,
            "ADMIN | Игрок: действия",
            "Наблюдать\nТелепортироваться\nПритянуть\nЗаморозить/разморозить\nПредупредить\nИсключить",
            "Выбрать", "Назад");
        return 1;
    }
    if (dialogid == ADMIN_DLG_PLAYER_ACTIONS)
    {
        if (gAdminUIDialogMode[playerid] != 4) return 1;
        gAdminUIDialogMode[playerid] = 0;
        if (!response) return Admin_UI_ShowPlayers(playerid);
        if (listitem < 0 || listitem > 5 || !Admin_Require(playerid, PERM_MODERATE)) return 1;
        new target = gAdminUITarget[playerid];
        if (!Admin_ValidTarget(playerid, target) ||
            gAuthToken[target] != gAdminUITargetToken[playerid]) return 1;
        if (listitem <= 3)
        {
            new command[32];
            switch (listitem)
            {
                case 0: format(command, sizeof(command), "/spec %d", target);
                case 1: format(command, sizeof(command), "/goto %d", target);
                case 2: format(command, sizeof(command), "/gethere %d", target);
                case 3: format(command, sizeof(command), "/freeze %d", target);
            }
            return Admin_Execute(playerid, command);
        }
        gAdminUIAction[playerid] = listitem;
        gAdminUIDialogMode[playerid] = 5;
        ShowPlayerDialog(playerid, ADMIN_DLG_PLAYER_REASON, DIALOG_STYLE_INPUT,
            "ADMIN | Причина",
            "Укажите причину административного действия:",
            "Применить", "Отмена");
        return 1;
    }
    if (dialogid == ADMIN_DLG_PLAYER_REASON)
    {
        if (gAdminUIDialogMode[playerid] != 5) return 1;
        gAdminUIDialogMode[playerid] = 0;
        if (!response || !inputtext[0] || !Admin_Require(playerid, PERM_MODERATE)) return 1;
        new target = gAdminUITarget[playerid];
        if (!Admin_ValidTarget(playerid, target) ||
            gAuthToken[target] != gAdminUITargetToken[playerid]) return 1;
        new safe[ADMIN_REASON_MAX + 1], command[144];
        format(safe, sizeof(safe), "%s", inputtext);
        Admin_SafeText(safe, sizeof(safe));
        if (gAdminUIAction[playerid] == 4)
            format(command, sizeof(command), "/warn %d %s", target, safe);
        else
            format(command, sizeof(command), "/kick %d %s", target, safe);
        return Admin_Execute(playerid, command);
    }
    if (dialogid == ADMIN_DLG_PUNISH)
    {
        if (gAdminUIDialogMode[playerid] != 6) return 1;
        gAdminUIDialogMode[playerid] = 0;
        if (response) return Admin_UI_Toggle(playerid);
        return 1;
    }
    if (dialogid == ADMIN_DLG_EVENTS)
    {
        if (gAdminUIDialogMode[playerid] != 7) return 1;
        gAdminUIDialogMode[playerid] = 0;
        if (!response) return Admin_UI_Toggle(playerid);
        if (listitem < 0 || listitem > 2 || !Admin_Require(playerid, PERM_EVENT)) return 1;
        switch (listitem)
        {
            case 0: return Admin_Execute(playerid, "/event clear");
            case 1: return Admin_Execute(playerid, "/event fog");
            case 2: return Admin_Execute(playerid, "/event night");
        }
        return 1;
    }
    return 0;
}

stock Admin_UI_OnDisconnect(playerid)
{
    gAdminUIDialogMode[playerid] = 0;
    gAdminUIReportCount[playerid] = 0;
    gAdminUIPlayerCount[playerid] = 0;
    gAdminUITarget[playerid] = INVALID_PLAYER_ID;
    for (new i = 0; i < 8; i++) gAdminUIReportMap[playerid][i] = -1;
    for (new i = 0; i < 32; i++) gAdminUIPlayerMap[playerid][i] = -1;
    return 1;
}
