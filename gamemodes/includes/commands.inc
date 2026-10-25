#if defined _signal_commands_included
    #endinput
#endif
#define _signal_commands_included

// ROUTER-01: регистрация и быстрый поиск команд выполняются Pawn.CMD.
// Пока административная бизнес-логика находится в Admin_Execute.
// Сборка полного текста используется как адаптер, а не как поиск команды.

public OnPlayerCommandReceived(playerid, cmd[], params[], flags)
{
    if (!Guard_AllowCommand(playerid)) return 0;
    if (gAuthState[playerid] != AUTH_READY)
    {
        SendClientMessage(playerid, COLOR_RED, "Сначала завершите регистрацию или войдите в аккаунт.");
        return 0;
    }
    if (Race_IsParticipant(playerid) && strcmp(cmd, "race", true) &&
        strcmp(cmd, "raceaudio", true) && strcmp(cmd, "help", true))
    {
        SendClientMessage(playerid, COLOR_RED, "В гонке: /race leave, /race status, /raceaudio.");
        return 0;
    }
    Guard_CommandStart(playerid);
    return 1;
}

public OnPlayerCommandPerformed(playerid, cmd[], params[], result, flags)
{
    Guard_CommandFinish(playerid);
    if (result == -1 && gAuthState[playerid] == AUTH_READY)
    {
        SendClientMessage(playerid, COLOR_RED, "Неизвестная команда. Список: /help.");
        return 0;
    }
    return 1;
}

stock Command_ExecuteAdmin(playerid, const command[], const params[])
{
    new input[160];
    if (isnull(params))
        format(input, sizeof(input), "/%s", command);
    else
        format(input, sizeof(input), "/%s %s", command, params);
    return Admin_Execute(playerid, input);
}

cmd:founder(playerid, params[])
{
    return Command_ExecuteAdmin(playerid, "founder", params);
}

cmd:guardstatus(playerid, params[])
{
    return Guard_ShowStatus(playerid);
}

// Справка по ролям и темам вместо одного длинного списка команд.
cmd:help(playerid, params[])
{
    new category[20];
    if (isnull(params))
    {
        SendClientMessage(playerid, COLOR_WHITE,
            "Tarkenso RPG | /help player, /help jobs, /help race, /help factions.");
        if (gAdminLevel[playerid] > 0)
            SendClientMessage(playerid, ADMIN_COLOR,
                "Администрация: /ahelp, /help admin.");
        if (Admin_IsFounder(playerid))
            SendClientMessage(playerid, STUDIO_ZONE_COLOR,
                "Разработка: /help studio; главное меню: /studio.");
        return 1;
    }
    if (sscanf(params, "s[20]", category))
        return SendClientMessage(playerid, COLOR_RED, "Использование: /help [player|jobs|race|factions|admin|studio].");
    if (!strcmp(category, "player", true))
    {
        SendClientMessage(playerid, COLOR_WHITE,
            "Игрок: /mn (/mm, /menu), /story, /position, /hudlogo, /report, /admins.");
        return 1;
    }
    if (!strcmp(category, "jobs", true))
    {
        SendClientMessage(playerid, CLEANER_COLOR,
            "Работы: уборщик — левый ALT рядом с диспетчером (начало / конец смены).");
        SendClientMessage(playerid, CLEANER_COLOR,
            "/trashdrop — сдать мусор у контейнера, /cleanstats — статистика.");
        return 1;
    }
    if (!strcmp(category, "race", true))
    {
        SendClientMessage(playerid, RACE_COLOR,
            "Гонки: /race join|leave|status|stats|top.");
        SendClientMessage(playerid, RACE_AUDIO_COLOR,
            "Музыка: /raceaudio test|playlist|on|off.");
        return 1;
    }
    if (!strcmp(category, "factions", true))
    {
        SendClientMessage(playerid, COLOR_WHITE,
            "Организации: /leaders, /fmenu, /fstats, /leader, /fchat.");
        SendClientMessage(playerid, COLOR_WHITE,
            "Управление: /finvite, /faccept, /funinvite, /frank, /fleave.");
        return 1;
    }
    if (!strcmp(category, "admin", true))
    {
        if (gAdminLevel[playerid] == 0)
            return SendClientMessage(playerid, ADMIN_ERROR, "Раздел доступен только администрации.");
        SendClientMessage(playerid, ADMIN_COLOR,
            "Администрация: /aduty, /ahelp, /admin, /check ID, /admins, /reports.");
        SendClientMessage(playerid, ADMIN_COLOR,
            "Наказания и управление: /warn, /mute, /kick, /ban, /unban, /setadmin.");
        SendClientMessage(playerid, ADMIN_COLOR,
            "Прочее: /veh del ID, /weather, /event; остальные: /ahelp.");
        if (Admin_IsFounder(playerid))
            SendClientMessage(playerid, ADMIN_COLOR,
                "Муниципальный вывоз: /trashservice (очистить контейнер и площадку).");
        return 1;
    }
    if (!strcmp(category, "studio", true))
    {
        if (!Admin_IsFounder(playerid))
            return SendClientMessage(playerid, ADMIN_ERROR, "Studio доступна только основателю.");
        SendClientMessage(playerid, STUDIO_ZONE_COLOR,
            "Tarkenso Studio: включите /aduty и откройте /studio.");
        SendClientMessage(playerid, STUDIO_ZONE_COLOR,
            "/studio zones|status|help — редактор зон.");
        SendClientMessage(playerid, STUDIO_ZONE_COLOR,
            "Прямые команды: /zoneedit; /founder — права основателя.");
        return 1;
    }
    return SendClientMessage(playerid, COLOR_RED,
        "Разделы: player, jobs, race, factions, admin, studio.");
}

cmd:trashdrop(playerid, params[])
{
    return Cleaner_Deposit(playerid);
}

cmd:trashservice(playerid, params[])
{
    return CleanerDump_Service(playerid);
}

cmd:cleanstats(playerid, params[])
{
    return Cleaner_Stats(playerid);
}

cmd:hudlogo(playerid, params[])
{
    if (OrvelarHUD_Toggle(playerid))
        SendClientMessage(playerid, COLOR_AMBER, "Логотип Tarkenso включён.");
    else
        SendClientMessage(playerid, COLOR_GREY, "Логотип Tarkenso скрыт.");
    return 1;
}

cmd:mn(playerid, params[])
{
    return PlayerMenu_Open(playerid);
}

cmd:mm(playerid, params[])
{
    return PlayerMenu_Open(playerid);
}

cmd:menu(playerid, params[])
{
    return PlayerMenu_Open(playerid);
}

cmd:veh(playerid, params[])
{
    return AdminVeh_Command(playerid, params);
}

cmd:delveh(playerid, params[])
{
    return AdminVeh_DeleteCommand(playerid, params);
}

cmd:studio(playerid, params[])
{
    return Studio_Command(playerid, params);
}

cmd:zoneedit(playerid, params[])
{
    return StudioZone_Command(playerid, params);
}

cmd:story(playerid, params[])
{
    return Story_Command(playerid);
}

cmd:position(playerid, params[])
{
    new Float:x, Float:y, Float:z;
    new message[96];
    GetPlayerPos(playerid, x, y, z);
    format(message, sizeof(message), "Координаты: %.2f / %.2f / %.2f", x, y, z);
    SendClientMessage(playerid, COLOR_GREY, message);
    return 1;
}

// Названия команд проверяются Pawn.CMD, права на действия — Admin_Execute.
// Для числовых параметров добавлена строгая типовая проверка через sscanf.
cmd:setadmin(playerid, params[])
{
    new target, level;
    if (sscanf(params, "ii", target, level))
        return SendClientMessage(playerid, ADMIN_ERROR, "Использование: /setadmin ID уровень.");
    return Command_ExecuteAdmin(playerid, "setadmin", params);
}
cmd:setevent(playerid, params[])
{
    new target, enabled;
    if (sscanf(params, "ii", target, enabled))
        return SendClientMessage(playerid, ADMIN_ERROR, "Использование: /setevent ID 0|1.");
    return Command_ExecuteAdmin(playerid, "setevent", params);
}
cmd:ban(playerid, params[])
{
    new target, minutes;
    if (sscanf(params, "ii", target, minutes))
        return SendClientMessage(playerid, ADMIN_ERROR, "Использование: /ban ID минуты причина.");
    return Command_ExecuteAdmin(playerid, "ban", params);
}
cmd:mute(playerid, params[])
{
    new target, minutes;
    if (sscanf(params, "ii", target, minutes))
        return SendClientMessage(playerid, ADMIN_ERROR, "Использование: /mute ID минуты причина.");
    return Command_ExecuteAdmin(playerid, "mute", params);
}
cmd:report(playerid, params[])
{
    return Command_ExecuteAdmin(playerid, "report", params);
}

cmd:admins(playerid, params[])
{
    return Command_ExecuteAdmin(playerid, "admins", params);
}

cmd:aduty(playerid, params[])
{
    return Command_ExecuteAdmin(playerid, "aduty", params);
}

cmd:admin(playerid, params[])
{
    return Command_ExecuteAdmin(playerid, "admin", params);
}

cmd:a(playerid, params[])
{
    return Command_ExecuteAdmin(playerid, "a", params);
}

cmd:reports(playerid, params[])
{
    return Command_ExecuteAdmin(playerid, "reports", params);
}

cmd:alog(playerid, params[])
{
    return Command_ExecuteAdmin(playerid, "alog", params);
}

cmd:accept(playerid, params[])
{
    return Command_ExecuteAdmin(playerid, "accept", params);
}

cmd:close(playerid, params[])
{
    return Command_ExecuteAdmin(playerid, "close", params);
}

cmd:areply(playerid, params[])
{
    return Command_ExecuteAdmin(playerid, "areply", params);
}

cmd:spec(playerid, params[])
{
    return Command_ExecuteAdmin(playerid, "spec", params);
}

cmd:event(playerid, params[])
{
    return Command_ExecuteAdmin(playerid, "event", params);
}

cmd:weather(playerid, params[])
{
    return Command_ExecuteAdmin(playerid, "weather", params);
}

cmd:ahelp(playerid, params[])
{
    return AdminTools_Help(playerid);
}

cmd:check(playerid, params[])
{
    return AdminTools_Check(playerid, params);
}

cmd:heal(playerid, params[])
{
    return AdminTools_ChangeVitals(playerid, params, true);
}

cmd:armour(playerid, params[])
{
    return AdminTools_ChangeVitals(playerid, params, false);
}

cmd:fixveh(playerid, params[])
{
    return AdminTools_Vehicle(playerid, params, false);
}

cmd:flip(playerid, params[])
{
    return AdminTools_Vehicle(playerid, params, true);
}

cmd:unban(playerid, params[])
{
    return Command_ExecuteAdmin(playerid, "unban", params);
}

cmd:warn(playerid, params[])
{
    return Command_ExecuteAdmin(playerid, "warn", params);
}

cmd:kick(playerid, params[])
{
    return Command_ExecuteAdmin(playerid, "kick", params);
}

cmd:freeze(playerid, params[])
{
    return Command_ExecuteAdmin(playerid, "freeze", params);
}

cmd:goto(playerid, params[])
{
    return Command_ExecuteAdmin(playerid, "goto", params);
}

cmd:tpcor(playerid, params[])
{
    return AdminTP_Coordinates(playerid, params);
}

cmd:gethere(playerid, params[])
{
    return Command_ExecuteAdmin(playerid, "gethere", params);
}

cmd:leaders(playerid, params[])
{
    return Leader_Command(playerid, "leaders", params);
}
cmd:fmenu(playerid, params[])
{
    return Leader_Command(playerid, "fmenu", params);
}
cmd:leader(playerid, params[])
{
    return Leader_Command(playerid, "leader", params);
}
cmd:fstats(playerid, params[])
{
    return Leader_Command(playerid, "fstats", params);
}
cmd:fchat(playerid, params[])
{
    return Leader_Command(playerid, "fchat", params);
}
cmd:finvite(playerid, params[])
{
    return Leader_Command(playerid, "finvite", params);
}
cmd:faccept(playerid, params[])
{
    return Leader_Command(playerid, "faccept", params);
}
cmd:funinvite(playerid, params[])
{
    return Leader_Command(playerid, "funinvite", params);
}
cmd:frank(playerid, params[])
{
    return Leader_Command(playerid, "frank", params);
}
cmd:fleave(playerid, params[])
{
    return Leader_Command(playerid, "fleave", params);
}
cmd:setleader(playerid, params[])
{
    return Leader_Command(playerid, "setleader", params);
}
cmd:unleader(playerid, params[])
{
    return Leader_Command(playerid, "unleader", params);
}
cmd:renewleader(playerid, params[])
{
    return Leader_Command(playerid, "renewleader", params);
}

