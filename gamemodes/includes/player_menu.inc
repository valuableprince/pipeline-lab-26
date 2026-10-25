#if defined _tarkenso_player_menu_included
    #endinput
#endif
#define _tarkenso_player_menu_included

// Character menu: simple native open.mp dialogs, no TextDraw dependency.
// The dossier number is the actual MariaDB account_id.
#define PLAYER_MENU_MAIN      (6430)
#define PLAYER_MENU_STATS     (6431)

stock PlayerMenu_Open(playerid)
{
    if (gAuthState[playerid] != AUTH_READY) return 1;
    ShowPlayerDialog(playerid, PLAYER_MENU_MAIN, DIALOG_STYLE_LIST,
        "Tarkenso RPG | Меню персонажа",
        "1. Статистика персонажа",
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

stock PlayerMenu_OnDialog(playerid, dialogid, response, listitem)
{
    if (dialogid != PLAYER_MENU_MAIN && dialogid != PLAYER_MENU_STATS)
        return 0;
    if (gAuthState[playerid] != AUTH_READY) return 1;
    if (dialogid == PLAYER_MENU_MAIN)
    {
        if (response && listitem == 0) return PlayerMenu_Statistics(playerid);
        return 1;
    }
    if (response) return PlayerMenu_Open(playerid);
    return 1;
}
