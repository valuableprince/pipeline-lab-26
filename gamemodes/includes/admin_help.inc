#if defined _tarkenso_admin_help_included
    #endinput
#endif
#define _tarkenso_admin_help_included

// ADMIN-TOOLS-02. Single source for syntax, purpose and example strings.
// Level 6 denotes Founder exclusively; level 7 denotes Event Team.
#define ADMIN_HELP_PAGE_SIZE (4)

enum E_ADMIN_HELP_ENTRY
{
    ahName[18],
    ahUsage[66],
    ahPurpose[85],
    ahExample[84],
    ahAccess
};
new const gAdminHelp[][E_ADMIN_HELP_ENTRY] =
{
    {"ahelp", "/ahelp [страница|команда]", "Справка по разрешённым командам", "/ahelp mute", 1},
    {"aduty", "/aduty", "Включить или выключить режим администратора", "/aduty", 1},
    {"admin", "/admin", "Открыть административное меню", "/admin", 1},
    {"admins", "/admins", "Посмотреть администрацию онлайн", "/admins", 1},
    {"a", "/a текст", "Написать в чат администрации", "/a Проверяю репорт", 1},
    {"report", "/report текст", "Отправить обращение администрации", "/report Не работает маркер", 1},
    {"reports", "/reports", "Посмотреть обращения игроков", "/reports", 1},
    {"accept", "/accept номер", "Принять обращение", "/accept 7", 1},
    {"areply", "/areply номер текст", "Ответить на обращение", "/areply 7 Проверим", 1},
    {"close", "/close номер", "Закрыть обращение", "/close 7", 1},
    {"check", "/check ID", "Посмотреть данные персонажа", "/check 12", 1},
    {"veh", "/veh модель [цвет1] [цвет2]", "Создать временный транспорт", "/veh 411 1 1", 1},
    {"delveh", "/delveh ID", "Удалить созданный админом транспорт", "/delveh 125", 1},
    {"guardstatus", "/guardstatus", "Показать защиту сервера", "/guardstatus", 1},
    {"tpcor", "/tpcor X Y Z", "Телепортироваться по координатам", "/tpcor -220 1065 20", 1},
    {"heal", "/heal ID", "Восстановить здоровье персонажа", "/heal 12", 2},
    {"armour", "/armour ID 0-100", "Установить броню персонажа", "/armour 12 70", 2},
    {"fixveh", "/fixveh [ID]", "Отремонтировать автомобиль", "/fixveh 125", 2},
    {"flip", "/flip [ID]", "Поставить автомобиль на колёса", "/flip 125", 2},
    {"warn", "/warn ID причина", "Выдать предупреждение", "/warn 12 Нарушение правил", 2},
    {"mute", "/mute ID минуты причина", "Заблокировать игроку чат на время", "/mute 12 30 Флуд", 2},
    {"unmute", "/unmute ID", "Досрочно разблокировать чат", "/unmute 12", 2},
    {"history", "/history ID", "Посмотреть историю наказаний", "/history 12", 2},
    {"kick", "/kick ID причина", "Отключить игрока от сервера", "/kick 12 Нарушение правил", 2},
    {"freeze", "/freeze ID", "Заморозить или разморозить игрока", "/freeze 12", 2},
    {"spec", "/spec ID|off", "Наблюдать за игроком", "/spec 12", 2},
    {"weather", "/weather 0-45", "Сменить погоду на сервере", "/weather 9", 2},
    {"respawn", "/respawn ID", "Возродить игрока на точке появления", "/respawn 12", 3},
    {"setskin", "/setskin ID skin", "Временно изменить скин персонажа", "/setskin 12 29", 3},
    {"settime", "/settime часы минуты", "Установить время суток для всех", "/settime 18 30", 3},
    {"goto", "/goto ID", "Телепортироваться к игроку", "/goto 12", 3},
    {"gethere", "/gethere ID", "Переместить игрока к себе", "/gethere 12", 3},
    {"ban", "/ban ID минуты причина", "Заблокировать аккаунт игрока", "/ban 12 60 Читы", 3},
    {"clearchat", "/clearchat", "Очистить общий чат", "/clearchat", 4},
    {"setvw", "/setvw ID мир", "Перенести игрока в виртуальный мир", "/setvw 12 2", 4},
    {"setint", "/setint ID интерьер", "Перенести игрока в интерьер", "/setint 12 3", 4},
    {"alog", "/alog", "Посмотреть последние действия администрации", "/alog", 4},
    {"ajail", "/ajail ID минуты причина", "Заключить игрока в административную тюрьму", "/ajail 12 30 Флуд", 3},
    {"unjail", "/unjail ID", "Досрочно освободить из админ-тюрьмы", "/unjail 12", 3},
    {"jailinfo", "/jailinfo ID", "Показать срок и причину заключения", "/jailinfo 12", 2},
    {"warns", "/warns ID", "Показать активные предупреждения", "/warns 12", 2},
    {"unwarn", "/unwarn ID номер причина", "Отменить предупреждение", "/unwarn 12 5 Ошибка", 4},
    {"anote", "/anote ID текст", "Добавить служебную заметку об игроке", "/anote 12 Проверить", 2},
    {"anotes", "/anotes ID", "Посмотреть служебные заметки", "/anotes 12", 2},
    {"offlineban", "/offlineban ник минуты причина", "Заблокировать аккаунт оффлайн", "/offlineban Player_Name 60 Читы", 5},
    {"unban", "/unban имя", "Разблокировать аккаунт", "/unban Player_Name", 5},
    {"setadmin", "/setadmin ID уровень", "Назначить права администратора", "/setadmin 12 2", 5},
    {"setevent", "/setevent ID 0|1", "Управлять ролью Event Team", "/setevent 12 1", 5},
    {"event", "/event fog|night|clear", "Активировать сюжетную атмосферу", "/event fog", 7},
    {"founder", "/founder [announce текст]", "Инструменты основателя проекта", "/founder announce Сбор", 6},
    {"studio", "/studio [режим]", "Открыть редактор мира", "/studio", 6},
    {"zoneedit", "/zoneedit", "Редактировать игровые зоны", "/zoneedit", 6},
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
    new line[170];
    format(line, sizeof(line), "Синтаксис: %s", gAdminHelp[index][ahUsage]);
    SendClientMessage(playerid, ADMIN_COLOR, line);
    format(line, sizeof(line), "Назначение: %s", gAdminHelp[index][ahPurpose]);
    SendClientMessage(playerid, ADMIN_COLOR, line);
    format(line, sizeof(line), "Пример: %s", gAdminHelp[index][ahExample]);
    return SendClientMessage(playerid, ADMIN_COLOR, line);
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

stock AdminHelp_Usage(playerid, const command[])
{
    new index = AdminHelp_Find(command);
    if (index == -1) return SendClientMessage(playerid, ADMIN_ERROR, "Неизвестная команда.");
    return AdminHelp_Print(playerid, index);
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
        format(argument, sizeof(argument), "1");
    if (Admin_Next(params, offset, extra, sizeof(extra)))
        return AdminHelp_Usage(playerid, "ahelp");
    if (!Admin_UnsignedNumber(argument))
        return AdminHelp_Usage(playerid, argument);
    new page = strval(argument);
    if (page < 1 || page > 99) return AdminHelp_Usage(playerid, "ahelp");
    new visible, first = (page - 1) * ADMIN_HELP_PAGE_SIZE;
    new label[110];
    format(label, sizeof(label), "Справка администрации: страница %d. /ahelp [страница|команда]", page);
    SendClientMessage(playerid, ADMIN_COLOR, label);
    for (new i = 0; i < sizeof(gAdminHelp); i++)
    {
        if (!AdminHelp_Access(playerid, i)) continue;
        if (visible >= first && visible < first + ADMIN_HELP_PAGE_SIZE)
            AdminHelp_Print(playerid, i);
        visible++;
    }
    if (first >= visible)
        SendClientMessage(playerid, ADMIN_ERROR, "Страница не существует.");
    else
    {
        format(label, sizeof(label), "Доступно команд: %d. Страниц: %d. Пример: /ahelp mute",
            visible, (visible + ADMIN_HELP_PAGE_SIZE - 1) / ADMIN_HELP_PAGE_SIZE);
        SendClientMessage(playerid, ADMIN_COLOR, label);
    }
    return 1;
}
