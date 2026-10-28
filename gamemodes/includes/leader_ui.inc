#if defined _signal_leader_ui_included
    #endinput
#endif
#define _signal_leader_ui_included

// Стандартные SA-MP/open.mp диалоги, без TextDraw.
#define LEADER_DLG_MAIN (6600)
#define LEADER_DLG_MANAGE (6601)
#define LEADER_DLG_INPUT (6602)
#define LEADER_DLG_ROSTER (6603)
new gFactionDialogMode[MAX_PLAYERS];
new gFactionDialogAction[MAX_PLAYERS];

stock Leader_UI_Reset(playerid)
{
    gFactionDialogMode[playerid] = 0;
    gFactionDialogAction[playerid] = 0;
    return 1;
}

stock Leader_UI_Main(playerid)
{
    new org = Leader_PlayerOrg(playerid), name[48], title[96];
    if (!org) return SendClientMessage(playerid, FACTION_ERROR, "Вы не состоите в организации.");
    Leader_OrgTitle(org, name, sizeof(name));
    format(title, sizeof(title), "%s | Ранг %d", name,
        Leader_PublicRank(org,Leader_PlayerRank(playerid)));
    gFactionDialogMode[playerid] = 1;
    ShowPlayerDialog(playerid, LEADER_DLG_MAIN, DIALOG_STYLE_LIST,
        title, "Информация о составе\nСписок сотрудников\nСвязь организации\nПокинуть организацию\nРуководство",
        "Выбрать", "Закрыть");
    return 1;
}

stock Leader_UI_Leader(playerid)
{
    if (!Leader_RequireStaff(playerid)) return 1;
    gFactionDialogMode[playerid] = 2;
    ShowPlayerDialog(playerid, LEADER_DLG_MANAGE, DIALOG_STYLE_LIST,
        "LEADER | Управление составом",
        "Список сотрудников\nПригласить игрока\nУволить сотрудника\nИзменить ранг\nСтатистика\nНазад",
        "Выбрать", "Закрыть");
    return 1;
}

stock Leader_UI_Roster(playerid)
{
    new org = Leader_PlayerOrg(playerid), body[1024], line[64], count;
    if (!org) return 1;
    body[0] = EOS;
    for (new i = 0; i < gFactionCount[org - 1] && count < 25; i++)
    {
        format(line, sizeof(line), "%s | Ранг %d%s\n",
            gFactionMemberName[org - 1][i],
            Leader_PublicRank(org,gFactionMemberRank[org - 1][i]),
            Leader_FindOnline(gFactionMemberName[org - 1][i]) != INVALID_PLAYER_ID ? (" [online]") : (""));
        strcat(body, line, sizeof(body));
        count++;
    }
    if (!count) format(body, sizeof(body), "Пока нет сотрудников.");
    gFactionDialogMode[playerid] = 4;
    ShowPlayerDialog(playerid, LEADER_DLG_ROSTER, DIALOG_STYLE_MSGBOX,
        "Сотрудники организации", body, "Назад", "Закрыть");
    return 1;
}

stock Leader_UI_Input(playerid, action)
{
    if (!Leader_RequireStaff(playerid)) return 1;
    gFactionDialogAction[playerid] = action;
    gFactionDialogMode[playerid] = 3;
    switch (action)
    {
        case 1: ShowPlayerDialog(playerid, LEADER_DLG_INPUT, DIALOG_STYLE_INPUT,
            "LEADER | Приглашение", "Укажите ID игрока:", "Применить", "Отмена");
        case 2: ShowPlayerDialog(playerid, LEADER_DLG_INPUT, DIALOG_STYLE_INPUT,
            "LEADER | Увольнение", "Укажите ID сотрудника:", "Применить", "Отмена");
        case 3:
        {
            new org=Leader_PlayerOrg(playerid);
            ShowPlayerDialog(playerid, LEADER_DLG_INPUT, DIALOG_STYLE_INPUT,
                "LEADER | Повышение",
                org==FACTION_FINANCE ?
                    ("Минфин: ID и ранг 1-3 (3=заместитель), например: 12 2") :
                    ("Укажите ID и новый ранг (1-9), например: 12 4"),
                "Применить", "Отмена");
        }
    }
    return 1;
}

stock Leader_UI_OnDialog(playerid, dialogid, response, listitem, const inputtext[])
{
    if (dialogid == LEADER_DLG_MAIN)
    {
        if (gFactionDialogMode[playerid] != 1) return 1;
        gFactionDialogMode[playerid] = 0;
        if (!response || listitem < 0 || listitem > 4 || !Leader_PlayerOrg(playerid)) return 1;
        switch (listitem)
        {
            case 0: return Leader_List(playerid);
            case 1: return Leader_UI_Roster(playerid);
            case 2: return SendClientMessage(playerid, FACTION_COLOR, "Чат организации: /fchat сообщение.");
            case 3: return Leader_Command(playerid, "fleave", "");
            case 4: return Leader_UI_Leader(playerid);
        }
        return 1;
    }
    if (dialogid == LEADER_DLG_MANAGE)
    {
        if (gFactionDialogMode[playerid] != 2) return 1;
        gFactionDialogMode[playerid] = 0;
        if (!response || listitem < 0 || listitem > 5 || !Leader_RequireStaff(playerid)) return 1;
        switch (listitem)
        {
            case 0: return Leader_UI_Roster(playerid);
            case 1: return Leader_UI_Input(playerid, 1);
            case 2: return Leader_UI_Input(playerid, 2);
            case 3: return Leader_UI_Input(playerid, 3);
            case 4: return Leader_List(playerid);
            case 5: return Leader_UI_Main(playerid);
        }
        return 1;
    }
    if (dialogid == LEADER_DLG_INPUT)
    {
        if (gFactionDialogMode[playerid] != 3) return 1;
        gFactionDialogMode[playerid] = 0;
        if (!response) return Leader_UI_Leader(playerid);
        if (!Leader_RequireStaff(playerid) || isnull(inputtext)) return 1;
        switch (gFactionDialogAction[playerid])
        {
            case 1: return Leader_Command(playerid, "finvite", inputtext);
            case 2: return Leader_Command(playerid, "funinvite", inputtext);
            case 3: return Leader_Command(playerid, "frank", inputtext);
        }
        return 1;
    }
    if (dialogid == LEADER_DLG_ROSTER)
    {
        if (gFactionDialogMode[playerid] != 4) return 1;
        gFactionDialogMode[playerid] = 0;
        if (!response) return 1;
        if (Leader_StaffAccess(playerid)) return Leader_UI_Leader(playerid);
        return Leader_UI_Main(playerid);
    }
    return 0;
}
