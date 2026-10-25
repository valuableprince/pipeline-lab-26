#if defined _tarkenso_studio_ui_included
    #endinput
#endif
#define _tarkenso_studio_ui_included

// STUDIO-UI-02: управление редакторами из стандартных диалогов open.mp.
// Все действия повторно проверяют права на сервере; dialogid не выдаёт доступ.
#define STUDIO_UI_ZONES         (6310)
#define STUDIO_UI_ZONE_ACTION  (6311)
#define STUDIO_UI_ZONE_RADIUS  (6312)

new gStudioUiZoneSlot[MAX_PLAYERS];

stock StudioUI_Reset(playerid)
{
    gStudioUiZoneSlot[playerid] = 0;
    return 1;
}

stock StudioUI_ZoneList(playerid)
{
    if (!Studio_Require(playerid)) return 1;
    new list[768], line[48];
    for (new i = 0; i < STUDIO_ZONE_COUNT; i++)
    {
        format(line, sizeof(line), "%02d | %s%s\n", i,
            gStudioZoneInvalid[i] ? ("Ошибка файла") :
            (gStudioZoneActive[i] ? ("Опубликована") :
            (gStudioZoneExists[i] ? ("Отключена") : ("Свободна"))),
            gStudioZoneOwner != INVALID_PLAYER_ID && gStudioZoneDraftSlot == i ?
            (" | черновик") : (""));
        strcat(list, line, sizeof(list));
    }
    ShowPlayerDialog(playerid, STUDIO_UI_ZONES, DIALOG_STYLE_LIST,
        "Tarkenso Studio | Зоны и контрольные точки",
        list, "Выбрать", "Назад");
    return 1;
}

stock StudioUI_ZoneAction(playerid)
{
    if (!Studio_Require(playerid)) return 1;
    new heading[80];
    format(heading, sizeof(heading), "STUDIO | Слот %d", gStudioUiZoneSlot[playerid]);
    ShowPlayerDialog(playerid, STUDIO_UI_ZONE_ACTION, DIALOG_STYLE_LIST, heading,
        "Открыть черновик из сохранённой зоны / создать\nПеренести центр к моей позиции\nИзменить радиус\nПоказать метки черновика\nСкрыть метки черновика\nОпубликовать черновик\nОтключить публикацию\nОтменить черновик\nТип: обычная зона / контрольная точка",
        "Выбрать", "Назад");
    return 1;
}

// Черновик копирует текущую опубликованную геометрию; сохраняется только publish.
// Один автор на сервере; двойной вход из меню не уничтожает чужой черновик.
stock StudioUI_ZoneBegin(playerid)
{
    new slot = gStudioUiZoneSlot[playerid];
    if (slot < 0 || slot >= STUDIO_ZONE_COUNT || gStudioZoneInvalid[slot])
        return SendClientMessage(playerid, ADMIN_ERROR, "Зона заблокирована: проверьте файл.");
    if (gStudioZoneOwner != INVALID_PLAYER_ID && gStudioZoneOwner != playerid)
        return SendClientMessage(playerid, ADMIN_ERROR, "Черновик занят другим разработчиком.");
    if (gStudioZoneOwner == playerid) StudioZone_Cancel();

    gStudioZoneOwner = playerid;
    gStudioZoneDraftSlot = slot;
    if (gStudioZoneExists[slot])
    {
        gStudioZoneDraftX = gStudioZoneX[slot];
        gStudioZoneDraftY = gStudioZoneY[slot];
        gStudioZoneDraftZ = gStudioZoneZ[slot];
        gStudioZoneDraftRadius = gStudioZoneRadius[slot];
        gStudioZoneDraftWorld = gStudioZoneWorld[slot];
        gStudioZoneDraftInterior = gStudioZoneInterior[slot];
        gStudioZoneDraftMode = gStudioZoneMode[slot];
    }
    else
    {
        gStudioZoneDraftRadius = 2.5;
        gStudioZoneDraftMode = 0;
        if (!StudioZone_Capture(playerid))
        {
            StudioZone_Cancel();
            return SendClientMessage(playerid, ADMIN_ERROR,
                "Для новой зоны требуется допустимая позиция пешком.");
        }
    }
    StudioZone_Show(playerid);
    return SendClientMessage(playerid, STUDIO_ZONE_COLOR,
        "Черновик открыт. Изменения видны только вам до публикации.");
}

stock bool:StudioUI_ZoneOwned(playerid)
{
    return gStudioZoneOwner == playerid &&
        gStudioZoneDraftSlot == gStudioUiZoneSlot[playerid];
}

// Возвращает 1, только если callback принадлежит Studio.
stock StudioUI_OnDialog(playerid, dialogid, response, listitem, const inputtext[])
{
    if (dialogid < STUDIO_UI_ZONES || dialogid > STUDIO_UI_ZONE_RADIUS) return 0;
    if (!Studio_Require(playerid)) return 1;
    if (!response)
    {
        switch (dialogid)
        {
            case STUDIO_UI_ZONES: return Studio_Open(playerid);
            case STUDIO_UI_ZONE_ACTION: return StudioUI_ZoneList(playerid);
        }
        return StudioUI_ZoneAction(playerid);
    }
    if (dialogid == STUDIO_UI_ZONES)
    {
        if (listitem < 0 || listitem >= STUDIO_ZONE_COUNT) return 1;
        gStudioUiZoneSlot[playerid] = listitem;
        return StudioUI_ZoneAction(playerid);
    }
    if (dialogid == STUDIO_UI_ZONE_RADIUS)
    {
        new Float:radius;
        if (!StudioUI_ZoneOwned(playerid)) return StudioUI_ZoneAction(playerid);
        if (sscanf(inputtext, "f", radius) || radius < 1.0 ||
            radius > STUDIO_ZONE_MAX_RADIUS)
        {
            SendClientMessage(playerid, ADMIN_ERROR, "Радиус: число от 1 до 50 метров.");
            return StudioUI_ZoneAction(playerid);
        }
        gStudioZoneDraftRadius = radius;
        StudioZone_Show(playerid);
        return StudioUI_ZoneAction(playerid);
    }
    if (dialogid == STUDIO_UI_ZONE_ACTION)
    {
        new slot = gStudioUiZoneSlot[playerid];
        if (slot < 0 || slot >= STUDIO_ZONE_COUNT) return 1;
        switch (listitem)
        {
            case 0: StudioUI_ZoneBegin(playerid);
            case 1:
            {
                if (!StudioUI_ZoneOwned(playerid))
                    SendClientMessage(playerid, ADMIN_ERROR, "Сначала откройте черновик этого слота.");
                else if (!StudioZone_Capture(playerid))
                    SendClientMessage(playerid, ADMIN_ERROR, "Перенос возможен только пешком.");
                else StudioZone_Show(playerid);
            }
            case 2:
            {
                if (!StudioUI_ZoneOwned(playerid))
                    SendClientMessage(playerid, ADMIN_ERROR, "Сначала откройте черновик этого слота.");
                else
                {
                    ShowPlayerDialog(playerid, STUDIO_UI_ZONE_RADIUS, DIALOG_STYLE_INPUT,
                        "STUDIO | Радиус зоны", "Введите радиус от 1 до 50 м:",
                        "Применить", "Назад");
                    return 1;
                }
            }
            case 3:
            {
                if (StudioUI_ZoneOwned(playerid)) StudioZone_Show(playerid);
                else SendClientMessage(playerid, ADMIN_ERROR, "Откройте черновик.");
            }
            case 4:
            {
                if (StudioUI_ZoneOwned(playerid)) StudioZone_ClearMarkers();
                else SendClientMessage(playerid, ADMIN_ERROR, "Откройте черновик.");
            }
            case 5:
            {
                if (StudioUI_ZoneOwned(playerid)) StudioZone_Command(playerid, "publish");
                else SendClientMessage(playerid, ADMIN_ERROR, "Нет черновика выбранного слота.");
            }
            case 6:
            {
                new cmdtext[32];
                format(cmdtext, sizeof(cmdtext), "disable %d", slot);
                StudioZone_Command(playerid, cmdtext);
            }
            case 7:
            {
                if (StudioUI_ZoneOwned(playerid)) StudioZone_Command(playerid, "cancel");
                else SendClientMessage(playerid, ADMIN_ERROR, "Нет черновика выбранного слота.");
            }
            case 8:
            {
                if (!StudioUI_ZoneOwned(playerid))
                    SendClientMessage(playerid, ADMIN_ERROR, "Откройте черновик.");
                else
                {
                    gStudioZoneDraftMode = 1 - gStudioZoneDraftMode;
                    SendClientMessage(playerid, STUDIO_ZONE_COLOR,
                        gStudioZoneDraftMode == 1 ?
                        ("Тип: виртуальная контрольная точка.") : ("Тип: информационная зона."));
                }
            }
        }
        return StudioUI_ZoneAction(playerid);
    }
    return 1;
}
