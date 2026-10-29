#if defined _tarkenso_command_help_included
    #endinput
#endif
#define _tarkenso_command_help_included

// Player-facing analogue of /ahelp. Visibility is filtered by affiliation.
#define CMD_HELP_CATEGORIES (7902)
#define CMD_HELP_LIST (7903)
#define CMD_HELP_DETAIL (7904)
#define CMD_HELP_PAGE_SIZE (7)

enum E_COMMAND_HELP
{
    chName[20],
    chUsage[90],
    chPurpose[116],
    chExample[110],
    chCategory,
    chAccess // 0=all, 1=faction member, 2=Finance, 3=administrator
};
new const gCommandHelp[][E_COMMAND_HELP]=
{
    {"mn", "/mn", "Открыть меню персонажа", "/mn", 1, 0},
    {"menu", "/menu", "Открыть меню персонажа", "/menu", 1, 0},
    {"help", "/help [раздел]", "Подсказки в чате по направлениям игры", "/help rp", 1, 0},
    {"story", "/story", "Сюжетное вступление и история персонажа", "/story", 1, 0},
    {"position", "/position", "Показать текущие координаты", "/position", 1, 0},
    {"report", "/report текст", "Написать обращение администрации", "/report Нужна помощь", 1, 0},
    {"admins", "/admins", "Показать администрацию онлайн", "/admins", 1, 0},
    {"me", "/me действие", "Описать действие своего персонажа", "/me достал блокнот", 2, 0},
    {"do", "/do описание", "Описать событие или обстановку", "/do На столе лежит ключ.", 2, 0},
    {"try", "/try действие", "Отыграть попытку со случайным результатом", "/try открывает дверь", 2, 0},
    {"todo", "/todo речь*действие", "Сказать фразу и одновременно выполнить действие", "/todo Привет!*подав руку", 2, 0},
    {"ame", "/ame действие", "Действие в чате и надпись над персонажем", "/ame осматривает документы", 2, 0},
    {"b", "/b текст", "Сообщение вне роли персонажа (OOC)", "/b Сейчас вернусь", 2, 0},
    {"s", "/s текст", "Крик на увеличенное расстояние", "/s Осторожно!", 2, 0},
    {"w", "/w текст", "Шёпот ближайшим игрокам", "/w Подойди сюда", 2, 0},
    {"low", "/low текст", "Тихая речь рядом с персонажем", "/low Слышишь меня?", 2, 0},
    {"trashdrop", "/trashdrop", "Сдать мусор в подходящем контейнере", "/trashdrop", 3, 0},
    {"sort", "/sort", "Открыть сортировку отходов на станции", "/sort", 3, 0},
    {"cleanstats", "/cleanstats", "Статистика работы с отходами", "/cleanstats", 3, 0},
    {"orders", "/orders", "Посмотреть заказы на вывоз мусора", "/orders", 3, 0},
    {"truckpark", "/truckpark", "Припарковать служебный грузовик", "/truckpark", 3, 0},
    {"truckload", "/truckload", "Погрузить отходы в грузовик", "/truckload", 3, 0},
    {"truckcargo", "/truckcargo", "Посмотреть загрузку грузовика", "/truckcargo", 3, 0},
    {"truckunload", "/truckunload", "Разгрузить отходы на базе", "/truckunload", 3, 0},
    {"truckcancel", "/truckcancel", "Отменить текущую работу водителя", "/truckcancel", 3, 0},
    {"leaders", "/leaders", "Посмотреть руководителей организаций", "/leaders", 4, 0},
    {"faccept", "/faccept", "Принять приглашение в организацию", "/faccept", 4, 0},
    {"fmenu", "/fmenu", "Открыть меню организации", "/fmenu", 4, 1},
    {"fstats", "/fstats", "Информация об организации и членстве", "/fstats", 4, 1},
    {"r", "/r текст", "Написать в рацию организации", "/r Сбор в кабинете", 4, 1},
    {"finvite", "/finvite ID", "Пригласить игрока в организацию", "/finvite 4", 4, 1},
    {"funinvite", "/funinvite ID", "Исключить сотрудника из организации", "/funinvite 4", 4, 1},
    {"frank", "/frank ID ранг", "Изменить должность сотрудника", "/frank 4 2", 4, 1},
    {"fleave", "/fleave", "Покинуть организацию", "/fleave", 4, 1},
    {"bonds", "/bonds", "Покупка и продажа за 98%, портфель, купоны", "/bonds", 5, 0},
    {"bondgov", "/bondgov", "Выпуски, подписи, фонд и отчёт о выкупе", "/bondgov", 5, 2},
    {"play", "/play", "Открыть список музыкальных радиостанций", "/play", 6, 0},
    {"race", "/race status", "Информация и действия для гонок", "/race status", 6, 0},
    {"raceaudio", "/raceaudio on|off", "Настроить музыку во время гонки", "/raceaudio on", 6, 0},
    {"hudlogo", "/hudlogo", "Показать или скрыть логотип игры", "/hudlogo", 6, 0},
    {"ahelp", "/ahelp", "Подробный справочник администратора", "/ahelp", 7, 3}
};
new gCmdHelpCategory[MAX_PLAYERS];
new gCmdHelpPage[MAX_PLAYERS];
new gCmdHelpRows[MAX_PLAYERS][9];
new gCmdHelpRowCount[MAX_PLAYERS];
new gCmdHelpSelected[MAX_PLAYERS];

stock bool:CommandHelp_Access(playerid,index)
{
    if(index<0 || index>=sizeof(gCommandHelp) ||
       gAuthState[playerid]!=AUTH_READY)return false;
    switch(gCommandHelp[index][chAccess])
    {
        case 1: return Leader_PlayerOrg(playerid)>0;
        case 2: return Leader_PlayerOrg(playerid)==FACTION_FINANCE;
        case 3: return gAdminLevel[playerid]>0;
    }
    return true;
}

stock CommandHelp_Categories(playerid)
{
    if(gAuthState[playerid]!=AUTH_READY)return 1;
    gCmdHelpCategory[playerid]=0;
    gCmdHelpPage[playerid]=0;
    return ShowPlayerDialog(playerid,CMD_HELP_CATEGORIES,DIALOG_STYLE_LIST,
        "{FFD15C}TARKENSO | Помощь по командам",
        gAdminLevel[playerid]>0?"0. Все доступные команды\n1. Персонаж и общая помощь\n2. Ролевая игра и чат\n3. Работы и транспорт\n4. Организации\n5. Облигации и казначейство\n6. Развлечения и интерфейс\n7. Администрирование":
        "0. Все доступные команды\n1. Персонаж и общая помощь\n2. Ролевая игра и чат\n3. Работы и транспорт\n4. Организации\n5. Облигации и казначейство\n6. Развлечения и интерфейс",
        "Открыть","Назад");
}

stock CommandHelp_List(playerid)
{
    if(gAuthState[playerid]!=AUTH_READY)return 1;
    new body[1700]="{FFD15C}Команда\t{FFD15C}Назначение{FFFFFF}\n";
    new row[195],total=0,shown=0;
    gCmdHelpRowCount[playerid]=0;
    for(new i=0;i<sizeof(gCommandHelp);i++)
    {
        if(!CommandHelp_Access(playerid,i))continue;
        if(gCmdHelpCategory[playerid]!=0 &&
           gCommandHelp[i][chCategory]!=gCmdHelpCategory[playerid])continue;
        if(total>=gCmdHelpPage[playerid]*CMD_HELP_PAGE_SIZE &&
           shown<CMD_HELP_PAGE_SIZE)
        {
            format(row,sizeof(row),"{FFD15C}/%s\t{FFFFFF}%s\n",
                gCommandHelp[i][chName],gCommandHelp[i][chPurpose]);
            strcat(body,row,sizeof(body));
            gCmdHelpRows[playerid][gCmdHelpRowCount[playerid]++]=i;
            shown++;
        }
        total++;
    }
    if(gCmdHelpPage[playerid]>0)
    {
        strcat(body,"{FFFFFF}Предыдущая страница\t<\n",sizeof(body));
        gCmdHelpRows[playerid][gCmdHelpRowCount[playerid]++]=-1;
    }
    if((gCmdHelpPage[playerid]+1)*CMD_HELP_PAGE_SIZE<total)
    {
        strcat(body,"{FFFFFF}Следующая страница\t>\n",sizeof(body));
        gCmdHelpRows[playerid][gCmdHelpRowCount[playerid]++]=-2;
    }
    if(!total)strcat(body,"{FFFFFF}Пока нет доступных команд\t-\n",sizeof(body));
    new title[100];
    format(title,sizeof(title),"{FFD15C}Команды | Страница %d/%d",
        gCmdHelpPage[playerid]+1,(total+CMD_HELP_PAGE_SIZE-1)/CMD_HELP_PAGE_SIZE);
    return ShowPlayerDialog(playerid,CMD_HELP_LIST,DIALOG_STYLE_TABLIST_HEADERS,
        title,body,"Подробнее","Разделы");
}

stock CommandHelp_Detail(playerid,index)
{
    if(!CommandHelp_Access(playerid,index))
        return CommandHelp_List(playerid);
    gCmdHelpSelected[playerid]=index;
    new title[88],body[580];
    format(title,sizeof(title),"{FFD15C}Справка: /%s",
        gCommandHelp[index][chName]);
    format(body,sizeof(body),
        "{FFD15C}КАК ВВОДИТЬ{FFFFFF}\n%s\n\n{FFD15C}ДЛЯ ЧЕГО НУЖНА{FFFFFF}\n%s\n\n{FFD15C}ПРИМЕР{FFFFFF}\n%s",
        gCommandHelp[index][chUsage],gCommandHelp[index][chPurpose],
        gCommandHelp[index][chExample]);
    // Bond education is shared by the player menu and the Ministry.
    if(!strcmp(gCommandHelp[index][chName],"bonds"))
        strcat(body,"\n\n{FFD15C}ПОДРОБНАЯ ИНСТРУКЦИЯ{FFFFFF}\n/mn -> Помощь по игре -> Облигации -> урок 7 (досрочная продажа).",sizeof(body));
    else if(!strcmp(gCommandHelp[index][chName],"bondgov"))
        strcat(body,"\n\n{FFD15C}ПОДРОБНАЯ ИНСТРУКЦИЯ{FFFFFF}\n/bondgov -> Обучение и полномочия министерства.",sizeof(body));
    return ShowPlayerDialog(playerid,CMD_HELP_DETAIL,DIALOG_STYLE_MSGBOX,
        title,body,"К списку","Закрыть");
}

stock CommandHelp_OnDialog(playerid,dialogid,response,listitem)
{
    if(dialogid<CMD_HELP_CATEGORIES || dialogid>CMD_HELP_DETAIL)return 0;
    if(gAuthState[playerid]!=AUTH_READY)return 1;
    if(dialogid==CMD_HELP_CATEGORIES)
    {
        if(!response)return PlayerMenu_Open(playerid);
        if(listitem<0 || listitem>7 ||
           (listitem==7 && gAdminLevel[playerid]==0))return 1;
        gCmdHelpCategory[playerid]=listitem;
        gCmdHelpPage[playerid]=0;
        return CommandHelp_List(playerid);
    }
    if(dialogid==CMD_HELP_LIST)
    {
        if(!response)return CommandHelp_Categories(playerid);
        if(listitem<0 || listitem>=gCmdHelpRowCount[playerid])return 1;
        new item=gCmdHelpRows[playerid][listitem];
        if(item==-1 && gCmdHelpPage[playerid]>0)
        {
            gCmdHelpPage[playerid]--;
            return CommandHelp_List(playerid);
        }
        if(item==-2)
        {
            gCmdHelpPage[playerid]++;
            return CommandHelp_List(playerid);
        }
        return CommandHelp_Detail(playerid,item);
    }
    if(response)return CommandHelp_List(playerid);
    return 1;
}
