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
    if (AdminPunish_IsJailed(playerid) &&
        strcmp(cmd, "help", true) && strcmp(cmd, "report", true))
    {
        SendClientMessage(playerid, ADMIN_ERROR, "Вы отбываете административное наказание. Команды заблокированы.");
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
    if (isnull(params) && AdminHelp_RequiresParams(command) &&
        AdminHelp_Access(playerid, AdminHelp_Find(command)))
        return AdminHelp_Usage(playerid, command);
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

// Both names are available; /load is the short operator command.
cmd:load(playerid, params[])
{
    return LoadTest_Command(playerid, params);
}

cmd:loadtest(playerid, params[])
{
    return LoadTest_Command(playerid, params);
}

// Каталог радиостанций для личного прослушивания вне гоночных событий.
cmd:play(playerid, params[])
{
    return Radio_Open(playerid);
}

// Справка по ролям и темам вместо одного длинного списка команд.
cmd:help(playerid, params[])
{
    new category[20];
    if (isnull(params))
    {
        SendClientMessage(playerid, COLOR_WHITE,
            "Tarkenso RPG | /help player, /help rp, /help jobs, /help race, /help factions.");
        if (gAdminLevel[playerid] > 0)
            SendClientMessage(playerid, ADMIN_COLOR,
                "Администрация: /ahelp, /help admin.");
        if (Admin_IsFounder(playerid))
            SendClientMessage(playerid, STUDIO_ZONE_COLOR,
                "Разработка: /help studio; главное меню: /studio.");
        return 1;
    }
    if (sscanf(params, "s[20]", category))
        return SendClientMessage(playerid, COLOR_RED, "Использование: /help [player|rp|jobs|race|factions|admin|studio].");
    if (!strcmp(category, "player", true))
    {
        SendClientMessage(playerid, COLOR_WHITE,
            "Игрок: /mn (/mm, /menu), /story, /position, /play, /hudlogo, /report, /admins.");
        return 1;
    }
    if (!strcmp(category, "rp", true))
    {
        SendClientMessage(playerid, RP_COLOR_ACTION, "RP-действия: /me, /do, /try, /todo, /ame (текст над персонажем).");
        SendClientMessage(playerid, RP_COLOR_OOC, "Общение: /b (OOC), /s (крик 45 м), /w (шёпот 4 м), /low (тихо 8 м).");
        SendClientMessage(playerid, COLOR_GREY, "Обычная речь и /me, /do, /try, /todo, /b — локально до 20 м. Пример: /todo Привет!*протянув руку.");
        return 1;
    }
    if (!strcmp(category, "jobs", true))
    {
        SendClientMessage(playerid, CLEANER_COLOR,
            "Работы: левый ALT у диспетчера — сборщик или водитель (со 2 ранга).");
        SendClientMessage(playerid, CLEANER_COLOR,
            "/trashdrop — сдать мусор у контейнера, /cleanstats — статистика.");
        SendClientMessage(playerid, CLEANER_COLOR,
            "/sort — сортировка отходов у станции на муниципальной базе (во время смены).");
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
            "Администрация: /ahelp [страница|команда] — синтаксис, назначение, пример.");
        SendClientMessage(playerid, ADMIN_COLOR,
            "Наказания и управление: /warn, /mute, /kick, /ban, /unban, /setadmin.");
        SendClientMessage(playerid, ADMIN_COLOR,
            "Транспорт: /veh del ID, /delveh ID. Инструменты: /unmute, /history, /respawn, /setskin, /settime, /clearchat, /setvw, /setint.");
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
        "Разделы: player, rp, jobs, race, factions, admin, studio.");
}

// RP-CHAT-01: commands available to every authenticated, spawned player.
cmd:me(playerid, params[]) { return RP_Command(playerid, 0, params); }
cmd:do(playerid, params[]) { return RP_Command(playerid, 1, params); }
cmd:try(playerid, params[]) { return RP_Command(playerid, 2, params); }
cmd:todo(playerid, params[]) { return RP_Command(playerid, 3, params); }
cmd:ame(playerid, params[]) { return RP_Command(playerid, 4, params); }
cmd:b(playerid, params[]) { return RP_Command(playerid, 5, params); }
cmd:s(playerid, params[]) { return RP_Command(playerid, 6, params); }
cmd:w(playerid, params[]) { return RP_Command(playerid, 7, params); }
cmd:low(playerid, params[]) { return RP_Command(playerid, 8, params); }

cmd:trashdrop(playerid, params[])
{
    return Cleaner_Deposit(playerid);
}

// Native PlayerTextDraw sorting station. Also opens with left ALT nearby.
cmd:sort(playerid, params[])
{
    return SortUI_Command(playerid);
}

cmd:trashservice(playerid, params[])
{
    return CleanerDump_Service(playerid);
}

cmd:cleanstats(playerid, params[])
{
    return Cleaner_Stats(playerid);
}

cmd:orders(playerid, params[])
{
    return Truck_ShowOrders(playerid);
}

cmd:truckpark(playerid, params[])
{
    return Truck_ParkCommand(playerid);
}

cmd:truckload(playerid, params[])
{
    return TruckLoad_Bin(playerid);
}

cmd:truckbag(playerid, params[])
{
    return TruckLoad_Bag(playerid);
}

cmd:truckthrow(playerid, params[])
{
    return TruckLoad_Throw(playerid);
}

cmd:truckcargo(playerid, params[])
{
    return TruckLoad_Notice(playerid);
}

cmd:truckunload(playerid, params[])
{
    return TruckPersist_Unload(playerid);
}

cmd:truckcancel(playerid, params[])
{
    return Truck_CancelCommand(playerid);
}

cmd:truckveh(playerid, params[])
{
    return Truck_VehicleCommand(playerid);
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
        return AdminHelp_Usage(playerid, "setadmin");
    return Command_ExecuteAdmin(playerid, "setadmin", params);
}
cmd:setevent(playerid, params[])
{
    new target, enabled;
    if (sscanf(params, "ii", target, enabled))
        return AdminHelp_Usage(playerid, "setevent");
    return Command_ExecuteAdmin(playerid, "setevent", params);
}
cmd:ban(playerid, params[])
{
    new target, minutes;
    if (sscanf(params, "ii", target, minutes))
        return AdminHelp_Usage(playerid, "ban");
    return Command_ExecuteAdmin(playerid, "ban", params);
}
cmd:mute(playerid, params[])
{
    new target, minutes;
    if (sscanf(params, "ii", target, minutes))
        return AdminHelp_Usage(playerid, "mute");
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
    return AdminHelp_Open(playerid, params);
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

cmd:unmute(playerid, params[])
{
    return AdminTools2_Unmute(playerid, params);
}

cmd:history(playerid, params[])
{
    return AdminTools2_History(playerid, params);
}

cmd:respawn(playerid, params[])
{
    return AdminTools2_Respawn(playerid, params);
}

cmd:setskin(playerid, params[])
{
    return AdminTools2_SetSkin(playerid, params);
}

cmd:settime(playerid, params[])
{
    return AdminTools2_SetTime(playerid, params);
}

cmd:clearchat(playerid, params[])
{
    return AdminTools2_ClearChat(playerid, params);
}

cmd:setvw(playerid, params[])
{
    return AdminTools2_SetDimension(playerid, params, true);
}

cmd:setint(playerid, params[])
{
    return AdminTools2_SetDimension(playerid, params, false);
}

cmd:ajail(playerid, params[])
{
    return AdminPunish_Jail(playerid, params);
}

cmd:unjail(playerid, params[])
{
    return AdminPunish_Unjail(playerid, params);
}

cmd:jailinfo(playerid, params[])
{
    return AdminPunish_JailInfo(playerid, params);
}

cmd:warns(playerid, params[])
{
    return AdminPunish_Warns(playerid, params);
}

cmd:unwarn(playerid, params[])
{
    return AdminPunish_Unwarn(playerid, params);
}

cmd:anote(playerid, params[])
{
    return AdminPunish_ANote(playerid, params);
}

cmd:anotes(playerid, params[])
{
    return AdminPunish_ANotes(playerid, params);
}

cmd:offlineban(playerid, params[])
{
    return AdminPunish_OfflineBan(playerid, params);
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

