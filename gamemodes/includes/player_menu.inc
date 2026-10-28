#if defined _tarkenso_player_menu_included
    #endinput
#endif
#define _tarkenso_player_menu_included

// Character menu: simple native open.mp dialogs, no TextDraw dependency.
// The dossier number is the actual MariaDB account_id.
#define PLAYER_MENU_MAIN      (6430)
#define PLAYER_MENU_STATS     (6431)
#define PLAYER_MENU_GAME_HELP (6432)
#define PLAYER_MENU_GAME_TOPIC (6433)

stock PlayerMenu_Open(playerid)
{
    if (gAuthState[playerid] != AUTH_READY) return 1;
    ShowPlayerDialog(playerid, PLAYER_MENU_MAIN, DIALOG_STYLE_LIST,
        "Tarkenso RPG | Меню персонажа",
        "1. Статистика персонажа\n2. Помощь по командам\n3. Помощь по игре",
        "Открыть", "Закрыть");
    return 1;
}

stock PlayerMenu_Statistics(playerid)
{
    if (gAuthState[playerid] != AUTH_READY) return 1;
#if defined TARKENSO_DB_PRIMARY
    if (!gDBPrimaryLoaded[playerid] || gDBPrimaryAccountId[playerid] <= 0 ||
        strlen(gDBPrimaryRegistrationDate[playerid]) != 10)
        return SendClientMessage(playerid, COLOR_RED,
            "Досье недоступно: данные аккаунта ещё не загружены.");

    new registration[16], experience[32], body[480];
    new level = gCharacterLevel[playerid];
    new rawDate[11];
    format(rawDate, sizeof(rawDate), "%s", gDBPrimaryRegistrationDate[playerid]);
    format(registration, sizeof(registration), "%c%c.%c%c.%c%c%c%c",
        rawDate[8], rawDate[9], rawDate[5], rawDate[6],
        rawDate[0], rawDate[1], rawDate[2], rawDate[3]);

    if (level >= PAYDAY_LEVEL_LIMIT)
        format(experience, sizeof(experience), "%d (максимум)", gCharacterEXP[playerid]);
    else
        format(experience, sizeof(experience), "%d/%d", gCharacterEXP[playerid],
            level * PAYDAY_EXP_STEP);

    format(body, sizeof(body),
        "{8EACBC}Имя персонажа:  {FFFFFF}%s\n\n{8EACBC}Уровень:  {D5B477}%d\n\n{8EACBC}Опыт (EXP):  {FFFFFF}%s\n\n{8EACBC}Дата регистрации:  {FFFFFF}%s\n\n{8EACBC}Досье №:  {D5B477}%d",
        gAccountName[playerid], level, experience, registration,
        gDBPrimaryAccountId[playerid]);
    ShowPlayerDialog(playerid, PLAYER_MENU_STATS, DIALOG_STYLE_MSGBOX,
        "Статистика персонажа", body, "Назад", "Закрыть");
    return 1;
#else
    return SendClientMessage(playerid, COLOR_RED,
        "Статистика персонажа доступна в сборке с MariaDB.");
#endif
}

stock PlayerMenu_GameHelp(playerid)
{
    if(gAuthState[playerid]!=AUTH_READY)return 1;
    return ShowPlayerDialog(playerid,PLAYER_MENU_GAME_HELP,DIALOG_STYLE_LIST,
        "{FFD15C}TARKENSO | Помощь по игре",
        "{FFFFFF}1. Государственные облигации: обучение\n{FFFFFF}2. PayDay и развитие персонажа\n{FFFFFF}3. Ролевая игра и общение",
        "Читать","Назад");
}

stock PlayerMenu_GameTopic(playerid,topic)
{
    if(gAuthState[playerid]!=AUTH_READY)return 1;
    new body[800];
    switch(topic)
    {
        case 1: format(body,sizeof(body),
            "{FFD15C}PAYDAY{FFFFFF} — регулярное событие сервера. При нём начисляется опыт (EXP), связанный с ростом уровня персонажа.\n\n{FFD15C}EXP{FFFFFF} — очки опыта. Текущий прогресс смотри через /mn → Статистика персонажа.\n\n{FFD15C}ОБЛИГАЦИИ{FFFFFF} — купонный доход пересчитывается на ближайшем PayDay после покупки. Проверяй /bonds → Купонный счёт; это не то же самое, что EXP.");
        case 2: format(body,sizeof(body),
            "{FFD15C}RP{FFFFFF} — ролевая игра: общайся и действуй так, как поступал бы твой персонаж.\n\n{FFD15C}/me{FFFFFF} — действие: /me открыл папку.\n{FFD15C}/do{FFFFFF} — обстановка: /do Папка лежит на столе.\n{FFD15C}/try{FFFFFF} — попытка со случайным результатом.\n{FFD15C}/b{FFFFFF} — сообщение вне роли (OOC).\n\nДругие команды с примерами смотри в /mn → Помощь по командам.");
        default: return PlayerMenu_GameHelp(playerid);
    }
    return ShowPlayerDialog(playerid,PLAYER_MENU_GAME_TOPIC,DIALOG_STYLE_MSGBOX,
        "{FFD15C}TARKENSO | Помощь по игре",body,"К разделам","Закрыть");
}

stock PlayerMenu_OnDialog(playerid, dialogid, response, listitem)
{
    if (dialogid != PLAYER_MENU_MAIN && dialogid != PLAYER_MENU_STATS &&
        dialogid != PLAYER_MENU_GAME_HELP &&
        dialogid != PLAYER_MENU_GAME_TOPIC)return 0;
    if (gAuthState[playerid] != AUTH_READY) return 1;
    if (dialogid == PLAYER_MENU_MAIN)
    {
        if (!response)return 1;
        if(listitem==0)return PlayerMenu_Statistics(playerid);
        if(listitem==1)return CommandHelp_Categories(playerid);
        if(listitem==2)return PlayerMenu_GameHelp(playerid);
        return 1;
    }
    if(dialogid==PLAYER_MENU_GAME_HELP)
    {
        if(!response)return PlayerMenu_Open(playerid);
        if(listitem==0)return BondEducation_Open(playerid,0);
        if(listitem==1 || listitem==2)
            return PlayerMenu_GameTopic(playerid,listitem);
        return 1;
    }
    if(dialogid==PLAYER_MENU_GAME_TOPIC)
    {
        if(response)return PlayerMenu_GameHelp(playerid);
        return 1;
    }
    if (response) return PlayerMenu_Open(playerid);
    return 1;
}
