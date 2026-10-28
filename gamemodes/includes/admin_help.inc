#if defined _tarkenso_admin_help_included
    #endinput
#endif
#define _tarkenso_admin_help_included

// ADMIN-TOOLS-02. Single source for syntax, purpose and example strings.
// Level 6 denotes Founder exclusively; level 7 denotes Event Team.
#define ADMIN_HELP_PAGE_SIZE (9)
#define ADMIN_HELP_DLG_CATEGORIES (6550)
#define ADMIN_HELP_DLG_LIST (6551)
#define ADMIN_HELP_DLG_DETAIL (6552)

new gAdminHelpCategory[MAX_PLAYERS];
new gAdminHelpPage[MAX_PLAYERS];
new gAdminHelpSelected[MAX_PLAYERS];
new gAdminHelpRow[MAX_PLAYERS][12];
new gAdminHelpRowCount[MAX_PLAYERS];

enum E_ADMIN_HELP_ENTRY
{
    ahName[18],
    ahUsage[66],
    ahPurpose[85],
    ahExample[84],
    ahCategory,
    ahAccess
};
new const gAdminHelp[][E_ADMIN_HELP_ENTRY] =
{
    {"ahelp", "/ahelp [страница|команда]", "Справка по разрешённым командам", "/ahelp mute", 1, 1},
    {"aduty", "/aduty", "Включить или выключить режим администратора", "/aduty", 1, 1},
    {"admin", "/admin", "Открыть административное меню", "/admin", 1, 1},
    {"admins", "/admins", "Посмотреть администрацию онлайн", "/admins", 1, 1},
    {"a", "/a текст", "Написать в чат администрации", "/a Проверяю репорт", 1, 1},
    {"report", "/report текст", "Отправить обращение администрации", "/report Не работает маркер", 1, 1},
    {"reports", "/reports", "Посмотреть обращения игроков", "/reports", 1, 1},
    {"accept", "/accept номер", "Принять обращение", "/accept 7", 1, 1},
    {"areply", "/areply номер текст", "Ответить на обращение", "/areply 7 Проверим", 1, 1},
    {"close", "/close номер", "Закрыть обращение", "/close 7", 1, 1},
    {"check", "/check ID", "Посмотреть данные персонажа", "/check 12", 2, 1},
    {"fly", "/fly", "Включить или выключить свободный полёт", "/fly", 4, 2},
    {"shipcam", "/shipcam", "Камера слежения за кораблём с меню справа", "/shipcam", 4, 2},
    {"veh", "/veh модель [цвет1] [цвет2]", "Создать временный транспорт", "/veh 411 1 1", 4, 1},
    {"delveh", "/delveh ID", "Удалить созданный админом транспорт", "/delveh 125", 4, 1},
    {"guardstatus", "/guardstatus", "Показать защиту сервера", "/guardstatus", 1, 1},
    {"tpcor", "/tpcor X Y Z", "Телепортироваться по координатам", "/tpcor -220 1065 20", 4, 1},
    {"heal", "/heal ID", "Восстановить здоровье персонажа", "/heal 12", 2, 2},
    {"armour", "/armour ID 0-100", "Установить броню персонажа", "/armour 12 70", 2, 2},
    {"fixveh", "/fixveh [ID]", "Отремонтировать автомобиль", "/fixveh 125", 4, 2},
    {"flip", "/flip [ID]", "Поставить автомобиль на колёса", "/flip 125", 4, 2},
    {"warn", "/warn ID причина", "Выдать предупреждение", "/warn 12 Нарушение правил", 3, 2},
    {"mute", "/mute ID минуты причина", "Заблокировать игроку чат на время", "/mute 12 30 Флуд", 3, 2},
    {"unmute", "/unmute ID", "Досрочно разблокировать чат", "/unmute 12", 3, 2},
    {"history", "/history ID", "Посмотреть историю наказаний", "/history 12", 3, 2},
    {"kick", "/kick ID причина", "Отключить игрока от сервера", "/kick 12 Нарушение правил", 3, 2},
    {"freeze", "/freeze ID", "Заморозить или разморозить игрока", "/freeze 12", 2, 2},
    {"spec", "/spec ID|off", "Наблюдать за игроком", "/spec 12", 2, 2},
    {"weather", "/weather 0-45", "Сменить погоду на сервере", "/weather 9", 4, 2},
    {"respawn", "/respawn ID", "Возродить игрока на точке появления", "/respawn 12", 2, 3},
    {"setskin", "/setskin ID skin", "Временно изменить скин персонажа", "/setskin 12 29", 2, 3},
    {"settime", "/settime HH MM | auto", "Время: вручную или часы сервера", "/settime auto", 4, 3},
    {"goto", "/goto ID", "Телепортироваться к игроку", "/goto 12", 2, 3},
    {"gethere", "/gethere ID", "Переместить игрока к себе", "/gethere 12", 2, 3},
    {"ban", "/ban ID минуты причина", "Заблокировать аккаунт игрока", "/ban 12 60 Читы", 3, 3},
    {"clearchat", "/clearchat", "Очистить общий чат", "/clearchat", 3, 4},
    {"setvw", "/setvw ID мир", "Перенести игрока в виртуальный мир", "/setvw 12 2", 4, 4},
    {"setint", "/setint ID интерьер", "Перенести игрока в интерьер", "/setint 12 3", 4, 4},
    {"alog", "/alog", "Посмотреть последние действия администрации", "/alog", 3, 4},
    {"ajail", "/ajail ID минуты причина", "Заключить игрока в административную тюрьму", "/ajail 12 30 Флуд", 3, 3},
    {"unjail", "/unjail ID", "Досрочно освободить из админ-тюрьмы", "/unjail 12", 3, 3},
    {"jailinfo", "/jailinfo ID", "Показать срок и причину заключения", "/jailinfo 12", 3, 2},
    {"warns", "/warns ID", "Показать активные предупреждения", "/warns 12", 3, 2},
    {"unwarn", "/unwarn ID номер причина", "Отменить предупреждение", "/unwarn 12 5 Ошибка", 3, 4},
    {"anote", "/anote ID текст", "Добавить служебную заметку об игроке", "/anote 12 Проверить", 3, 2},
    {"anotes", "/anotes ID", "Посмотреть служебные заметки", "/anotes 12", 3, 2},
    {"offlineban", "/offlineban ник минуты причина", "Заблокировать аккаунт оффлайн", "/offlineban Player_Name 60 Читы", 3, 5},
    {"unban", "/unban имя", "Разблокировать аккаунт", "/unban Player_Name", 3, 5},
    {"givemoney", "/givemoney ID сумма", "Выдать деньги игроку (до $1 000 000 за раз)", "/givemoney 12 50000", 3, 5},
    {"setadmin", "/setadmin ID уровень", "Назначить права администратора", "/setadmin 12 2", 3, 5},
    {"setevent", "/setevent ID 0|1", "Управлять ролью Event Team", "/setevent 12 1", 3, 5},
    {"event", "/event fog|night|clear", "Активировать сюжетную атмосферу", "/event fog", 4, 7},
    {"founder", "/founder [announce текст]", "Инструменты основателя проекта", "/founder announce Сбор", 5, 6},
    {"studio", "/studio [режим]", "Открыть редактор мира", "/studio", 5, 6},
    {"zoneedit", "/zoneedit", "Редактировать игровые зоны", "/zoneedit", 5, 6},
    {"trashservice", "/trashservice", "Обслужить контейнер мусора", "/trashservice", 6}
};

stock AdminHelp_Find(const command[])
{
    for (new i = 0; i < sizeof(gAdminHelp); i++)
        if (!strcmp(gAdminHelp[i][ahName], command, true)) return i;
    return -1;
}

stock bool:AdminHelp_Access(playerid, index)
{
    if (!Admin_Allowed(playerid, PERM_REPORT)) return false;
    if (index < 0 || index >= sizeof(gAdminHelp)) return false;
    new level = gAdminHelp[index][ahAccess];
    if (level == 6) return Admin_IsFounder(playerid);
    if (level == 7) return Admin_Allowed(playerid, PERM_EVENT);
    return gAdminLevel[playerid] >= level;
}

stock AdminHelp_Print(playerid, index)
{
    if (!AdminHelp_Access(playerid, index))
        return SendClientMessage(playerid, ADMIN_ERROR, "Команда вам недоступна.");
    gAdminHelpSelected[playerid] = index;
    new title[80], body[520];
    format(title, sizeof(title), "Tarkenso RPG | Справка: /%s", gAdminHelp[index][ahName]);
    format(body, sizeof(body),
        "{D9AA55}СИНТАКСИС{FFFFFF}\n%s\n\n{D9AA55}ОПИСАНИЕ{FFFFFF}\n%s\n\n{D9AA55}ПРИМЕР ИСПОЛЬЗОВАНИЯ{FFFFFF}\n{B6E4B3}%s\n\n{9FAAB8}Доступ: уровень %d{FFFFFF}",
        gAdminHelp[index][ahUsage], gAdminHelp[index][ahPurpose],
        gAdminHelp[index][ahExample], gAdminHelp[index][ahAccess]);
    return ShowPlayerDialog(playerid, ADMIN_HELP_DLG_DETAIL, DIALOG_STYLE_MSGBOX,
        title, body, "К списку", "Закрыть");
}

stock AdminHelp_Categories(playerid)
{
    return ShowPlayerDialog(playerid, ADMIN_HELP_DLG_CATEGORIES, DIALOG_STYLE_LIST,
        "Tarkenso RPG | Справочник администрации",
        "Все доступные команды\nОбщие и обращения\nУправление игроками\nМодерация и наказания\nМир и транспорт\nTarkenso RPG Studio",
        "Открыть", "Закрыть");
}

stock AdminHelp_List(playerid)
{
    if (!Admin_Require(playerid, PERM_REPORT)) return 1;
    new body[1700] = "{D9AA55}Команда\t{D9AA55}Назначение{FFFFFF}\n", row[180], visible, first;
    first = gAdminHelpPage[playerid] * ADMIN_HELP_PAGE_SIZE;
    gAdminHelpRowCount[playerid] = 0;
    for (new i = 0; i < sizeof(gAdminHelp); i++)
    {
        if (!AdminHelp_Access(playerid, i)) continue;
        if (gAdminHelpCategory[playerid] &&
            gAdminHelp[i][ahCategory] != gAdminHelpCategory[playerid]) continue;
        if (visible >= first && visible < first + ADMIN_HELP_PAGE_SIZE)
        {
            format(row, sizeof(row), "{B6E4B3}/%s\t{FFFFFF}%s\n",
                gAdminHelp[i][ahName], gAdminHelp[i][ahPurpose]);
            strcat(body, row, sizeof(body));
            gAdminHelpRow[playerid][gAdminHelpRowCount[playerid]++] = i;
        }
        visible++;
    }
    if (gAdminHelpPage[playerid] > 0)
    {
        strcat(body, "{9FAAB8}Предыдущая страница\t<\n", sizeof(body));
        gAdminHelpRow[playerid][gAdminHelpRowCount[playerid]++] = -1;
    }
    if ((gAdminHelpPage[playerid] + 1) * ADMIN_HELP_PAGE_SIZE < visible)
    {
        strcat(body, "{9FAAB8}Следующая страница\t>\n", sizeof(body));
        gAdminHelpRow[playerid][gAdminHelpRowCount[playerid]++] = -2;
    }
    new title[100];
    format(title, sizeof(title), "Tarkenso RPG | Команды (%d) | Страница %d/%d",
        visible, gAdminHelpPage[playerid] + 1,
        (visible + ADMIN_HELP_PAGE_SIZE - 1) / ADMIN_HELP_PAGE_SIZE);
    return ShowPlayerDialog(playerid, ADMIN_HELP_DLG_LIST, DIALOG_STYLE_TABLIST_HEADERS,
        title, body, "Подробнее", "Категории");
}

stock AdminHelp_OnDialog(playerid, dialogid, response, listitem)
{
    if (dialogid < ADMIN_HELP_DLG_CATEGORIES || dialogid > ADMIN_HELP_DLG_DETAIL) return 0;
    if (!Admin_Require(playerid, PERM_REPORT)) return 1;
    if (dialogid == ADMIN_HELP_DLG_CATEGORIES)
    {
        if (!response || listitem < 0 || listitem > 5) return 1;
        gAdminHelpCategory[playerid] = listitem;
        gAdminHelpPage[playerid] = 0;
        return AdminHelp_List(playerid);
    }
    if (dialogid == ADMIN_HELP_DLG_LIST)
    {
        if (!response) return AdminHelp_Categories(playerid);
        if (listitem < 0 || listitem >= gAdminHelpRowCount[playerid]) return 1;
        new index = gAdminHelpRow[playerid][listitem];
        if (index == -1)
        {
            if (gAdminHelpPage[playerid] > 0) gAdminHelpPage[playerid]--;
            return AdminHelp_List(playerid);
        }
        if (index == -2)
        {
            gAdminHelpPage[playerid]++;
            return AdminHelp_List(playerid);
        }
        return AdminHelp_Print(playerid, index);
    }
    if (response) return AdminHelp_List(playerid);
    return 1;
}

stock bool:AdminHelp_RequiresParams(const command[])
{
    // Optional commands and menu openers keep their default behavior.
    for (new i = 0; i < sizeof(gAdminHelp); i++)
    {
        if (strcmp(gAdminHelp[i][ahName], command, true)) continue;
        return !strcmp(command, "accept", true) ||
            !strcmp(command, "areply", true) ||
            !strcmp(command, "close", true) ||
            !strcmp(command, "a", true) ||
            !strcmp(command, "spec", true) ||
            !strcmp(command, "warn", true) ||
            !strcmp(command, "mute", true) ||
            !strcmp(command, "kick", true) ||
            !strcmp(command, "freeze", true) ||
            !strcmp(command, "weather", true) ||
            !strcmp(command, "event", true) ||
            !strcmp(command, "goto", true) ||
            !strcmp(command, "gethere", true) ||
            !strcmp(command, "ban", true) ||
            !strcmp(command, "unban", true) ||
            !strcmp(command, "setadmin", true) ||
            !strcmp(command, "setevent", true) ||
            !strcmp(command, "report", true);
    }
    return false;
}

// Incomplete / invalid admin commands should not interrupt gameplay with a dialog.
stock AdminHelp_Usage(playerid, const command[])
{
    new index = AdminHelp_Find(command);
    if (index == -1) return SendClientMessage(playerid, ADMIN_ERROR, "Неизвестная команда.");
    if (!AdminHelp_Access(playerid, index))
        return SendClientMessage(playerid, ADMIN_ERROR, "Команда вам недоступна.");

    new message[192];
    format(message, sizeof(message), "{9FAAB8}[Tarkenso RPG] {FFFFFF}Синтаксис: {B6E4B3}%s",
        gAdminHelp[index][ahUsage]);
    SendClientMessage(playerid, -1, message);
    format(message, sizeof(message), "{9FAAB8}[Tarkenso RPG] {FFFFFF}Описание: %s",
        gAdminHelp[index][ahPurpose]);
    SendClientMessage(playerid, -1, message);
    format(message, sizeof(message), "{9FAAB8}[Tarkenso RPG] {FFFFFF}Пример: {B6E4B3}%s {9FAAB8}| Подробнее: /ahelp %s",
        gAdminHelp[index][ahExample], gAdminHelp[index][ahName]);
    return SendClientMessage(playerid, -1, message);
}

stock AdminHelp_FromSlash(playerid, const slashCommand[])
{
    new plain[24];
    if (slashCommand[0] != '/' || strlen(slashCommand) < 2)
        return SendClientMessage(playerid, ADMIN_ERROR, "Неизвестная команда.");
    strmid(plain, slashCommand, 1, strlen(slashCommand), sizeof(plain));
    return AdminHelp_Usage(playerid, plain);
}

stock AdminHelp_Open(playerid, const params[])
{
    if (!Admin_Require(playerid, PERM_REPORT)) return 1;
    new offset, argument[32], extra[8];
    if (!Admin_Next(params, offset, argument, sizeof(argument)))
        return AdminHelp_Categories(playerid);
    if (Admin_Next(params, offset, extra, sizeof(extra)))
        return AdminHelp_Print(playerid, AdminHelp_Find("ahelp"));
    if (!Admin_UnsignedNumber(argument))
        return AdminHelp_Print(playerid, AdminHelp_Find(argument));
    new page = strval(argument);
    if (page < 1 || page > 99) return AdminHelp_Print(playerid, AdminHelp_Find("ahelp"));
    gAdminHelpCategory[playerid] = 0;
    gAdminHelpPage[playerid] = page - 1;
    return AdminHelp_List(playerid);
}
