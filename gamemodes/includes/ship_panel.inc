#if defined _tarkenso_ship_panel_included
    #endinput
#endif
#define _tarkenso_ship_panel_included

// SHIP-PANEL-01: one /ship entry point, menu-driven workflows.
// Dedicated 7100-7105 dialog IDs: 6340-6345 belong to CleanerEditor.
// Read-only pages/camera: admin 2+. All ship modifications: Founder /aduty.
// Keep legacy /ship subcommands and /ship stop for compatibility and emergencies.
#define SHIP_PANEL_MAIN (7100)
#define SHIP_PANEL_INFO (7101)
#define SHIP_PANEL_VOYAGE (7102)
#define SHIP_PANEL_ROUTE (7103)
#define SHIP_PANEL_TOOLS (7104)
#define SHIP_PANEL_CLEAR_CONFIRM (7105)
new bool:gShipPanelClearPending[MAX_PLAYERS];

stock ShipPanel_Reset(playerid)
{
    gShipPanelClearPending[playerid] = false;
    return 1;
}

stock bool:ShipPanel_Allowed(playerid)
{
    return Admin_Allowed(playerid, PERM_MODERATE) && gAdminDuty[playerid];
}

stock ShipPanel_Open(playerid)
{
    gShipPanelClearPending[playerid] = false;
    if (!Admin_Require(playerid, PERM_MODERATE)) return 1;
    if (gShipEditActive[playerid])
        return SendClientMessage(playerid, ADMIN_ERROR,
            "[Tarkenso RPG] Сначала завершите визуальное редактирование корабля.");
    if (gShipCamActive[playerid])
        return SendClientMessage(playerid, ADMIN_ERROR,
            "[Tarkenso RPG] Закройте наблюдение клавишей ESC или кнопкой EXIT CAMERA.");
    ShowPlayerDialog(playerid, SHIP_PANEL_MAIN, DIALOG_STYLE_LIST,
        "Tarkenso RPG | Управление кораблём",
        "Общая информация\nУправление рейсами\nВизуальный редактор\nПортовые маршруты\nНаблюдение за кораблём\nДополнительные инструменты",
        "Выбрать", "Закрыть");
    return 1;
}

stock ShipPanel_Info(playerid)
{
    if (!ShipPanel_Allowed(playerid)) return 1;
    new text[750], line[125];
    if (!Ship_Ready())
    {
        format(text, sizeof(text), "Судно не загружено.\nДля создания нужен основатель: Дополнительные инструменты.");
    }
    else
    {
        new Float:x, Float:y, Float:z, Float:rx, Float:ry, Float:yaw;
        GetObjectPos(gShipRoot, x, y, z);
        GetObjectRot(gShipRoot, rx, ry, yaw);
        // The labels describe the ACTUAL movement controller state.
        format(text, sizeof(text), "Модель корпуса: %d\nДеталей: %d / %d\nМорских точек: %d / %d\n",
            gShipModel, gShipPartCount, SHIP_MAX_PARTS, gShipPointCount, SHIP_MAX_POINTS);
        if (gShipManeuverActive)
            strcat(text, "Состояние: портовое маневрирование\n", sizeof(text));
        else if (gShipPortDeparture >= 0)
            strcat(text, "Состояние: отход от причала\n", sizeof(text));
        else if (gShipRunning)
            strcat(text, "Состояние: морской переход\n", sizeof(text));
        else
            strcat(text, "Состояние: остановлен\n", sizeof(text));
        format(line, sizeof(line), "X: %.2f  Y: %.2f  Z: %.2f\nПоворот: %.1f\n",
            x, y, z, yaw);
        strcat(text, line, sizeof(text));
        format(line, sizeof(line), "LS: стоянка %s, выход %s\n",
            gShipPortSaved[SHIP_PORT_LS][SHIP_PORT_BERTH] ? ("есть") : ("нет"),
            gShipPortSaved[SHIP_PORT_LS][SHIP_PORT_EXIT] ? ("есть") : ("нет"));
        strcat(text, line, sizeof(text));
        format(line, sizeof(line), "SF: стоянка %s, выход %s\n",
            gShipPortSaved[SHIP_PORT_SF][SHIP_PORT_BERTH] ? ("есть") : ("нет"),
            gShipPortSaved[SHIP_PORT_SF][SHIP_PORT_EXIT] ? ("есть") : ("нет"));
        strcat(text, line, sizeof(text));
        format(line, sizeof(line), "Маршруты: LS отход %d / заход %d; SF отход %d / заход %d",
            gShipManeuverCount[SHIP_PORT_LS][SHIP_MANEUVER_DEPART],
            gShipManeuverCount[SHIP_PORT_LS][SHIP_MANEUVER_ARRIVE],
            gShipManeuverCount[SHIP_PORT_SF][SHIP_MANEUVER_DEPART],
            gShipManeuverCount[SHIP_PORT_SF][SHIP_MANEUVER_ARRIVE]);
        strcat(text, line, sizeof(text));
    }
    ShowPlayerDialog(playerid, SHIP_PANEL_INFO, DIALOG_STYLE_MSGBOX,
        "Tarkenso RPG | Информация о корабле", text, "Назад", "Закрыть");
    return 1;
}

stock ShipPanel_Voyage(playerid)
{
    if (!ShipPanel_Allowed(playerid)) return 1;
    ShowPlayerDialog(playerid, SHIP_PANEL_VOYAGE, DIALOG_STYLE_LIST,
        "Tarkenso RPG | Управление рейсами",
        "Отправить из Лос-Сантоса\nОтправить из Сан-Фиерро\nОстановить корабль\nПосмотреть состояние\nПодать корабельный гудок",
        "Выбрать", "Назад");
    return 1;
}

stock ShipPanel_Route(playerid)
{
    if (!ShipPanel_Allowed(playerid)) return 1;
    ShowPlayerDialog(playerid, SHIP_PANEL_ROUTE, DIALOG_STYLE_LIST,
        "Tarkenso RPG | Портовые маршруты",
        "Открыть редактор сверху (LS/SF, отход/заход)\nМорской маршрут: LS -> SF\nМорской маршрут: SF -> LS\nЭкспортировать морской маршрут",
        "Выбрать", "Назад");
    return 1;
}

stock ShipPanel_Tools(playerid)
{
    if (!ShipPanel_Allowed(playerid)) return 1;
    ShowPlayerDialog(playerid, SHIP_PANEL_TOOLS, DIALOG_STYLE_LIST,
        "Tarkenso RPG | Инструменты корабля",
        "Создать грузовой корабль\nСохранить корабль\nЗагрузить корабль\nЭкспортировать морской маршрут\nОсвободить блокировку редактора\nСправка по старым командам\nУдалить корабль из памяти",
        "Выбрать", "Назад");
    return 1;
}

stock ShipPanel_OpenEditor(playerid)
{
    if (!Studio_Require(playerid)) return 1;
    return ShipEditor_Command(playerid, "");
}

stock ShipPanel_Dispatch(playerid, const command[])
{
    // The legacy command re-checks Founder /aduty and the editor lock.
    if (!Studio_Require(playerid)) return 1;
    return Ship_Command(playerid, command);
}

// Every dialog callback checks authorization and listitem independently.
// Buttons are not privileges: arbitrary client dialog replies grant nothing.
stock ShipPanel_OnDialog(playerid, dialogid, response, listitem)
{
    if (dialogid < SHIP_PANEL_MAIN || dialogid > SHIP_PANEL_CLEAR_CONFIRM) return 0;
    if (!ShipPanel_Allowed(playerid)) return 1;
    if (gShipEditActive[playerid] || gShipCamActive[playerid]) return 1;
    if (!response)
    {
        if (dialogid == SHIP_PANEL_CLEAR_CONFIRM) gShipPanelClearPending[playerid] = false;
        if (dialogid == SHIP_PANEL_MAIN) return 1;
        return ShipPanel_Open(playerid);
    }
    if (dialogid == SHIP_PANEL_MAIN)
    {
        if (listitem < 0 || listitem > 5) return 1;
        switch (listitem)
        {
            case 0: return ShipPanel_Info(playerid);
            case 1: return ShipPanel_Voyage(playerid);
            case 2: return ShipPanel_OpenEditor(playerid);
            case 3: return ShipPanel_Route(playerid);
            case 4: return ShipCam_Command(playerid);
            case 5: return ShipPanel_Tools(playerid);
        }
        return 1;
    }
    if (dialogid == SHIP_PANEL_INFO) return ShipPanel_Open(playerid);
    if (dialogid == SHIP_PANEL_VOYAGE)
    {
        if (listitem < 0 || listitem > 4) return 1;
        if (!Studio_Require(playerid)) return 1;
        switch (listitem)
        {
            case 0: ShipPanel_Dispatch(playerid, "port ls depart");
            case 1: ShipPanel_Dispatch(playerid, "port sf depart");
            case 2: ShipPanel_Dispatch(playerid, "stop");
            case 3: return ShipPanel_Info(playerid);
            case 4: ShipHorn_Manual(playerid);
        }
        return ShipPanel_Voyage(playerid);
    }
    if (dialogid == SHIP_PANEL_ROUTE)
    {
        if (listitem < 0 || listitem > 3) return 1;
        if (!Studio_Require(playerid)) return 1;
        switch (listitem)
        {
            case 0: return ShipPanel_OpenEditor(playerid);
            case 1: ShipPanel_Dispatch(playerid, "preset lssf");
            case 2: ShipPanel_Dispatch(playerid, "preset sfls");
            case 3: ShipPanel_Dispatch(playerid, "export");
        }
        return ShipPanel_Route(playerid);
    }
    if (dialogid == SHIP_PANEL_TOOLS)
    {
        if (listitem < 0 || listitem > 6) return 1;
        if (!Studio_Require(playerid)) return 1;
        switch (listitem)
        {
            case 0: ShipPanel_Dispatch(playerid, "cargo");
            case 1: ShipPanel_Dispatch(playerid, "save");
            case 2: ShipPanel_Dispatch(playerid, "load");
            case 3: ShipPanel_Dispatch(playerid, "export");
            case 4: ShipPanel_Dispatch(playerid, "release");
            case 5: ShipPanel_Dispatch(playerid, "help");
            case 6:
            {
                if (!Ship_Ready()) return ShipPanel_Tools(playerid);
                if (ShipEditor_Occupied())
                    return SendClientMessage(playerid, ADMIN_ERROR,
                        "[Tarkenso RPG] Нельзя удалить корабль, пока рядом с корпусом есть игроки.");
                gShipPanelClearPending[playerid] = true;
                ShowPlayerDialog(playerid, SHIP_PANEL_CLEAR_CONFIRM,
                    DIALOG_STYLE_MSGBOX, "Tarkenso RPG | Подтверждение",
                    "Удалить корабль из памяти?\nСохранённый файл останется на диске.",
                    "Удалить", "Отмена");
                return 1;
            }
        }
        return ShipPanel_Tools(playerid);
    }
    if (dialogid == SHIP_PANEL_CLEAR_CONFIRM)
    {
        if (!gShipPanelClearPending[playerid]) return 1;
        gShipPanelClearPending[playerid] = false;
        if (!Studio_Require(playerid)) return 1;
        if (ShipEditor_Occupied())
            return SendClientMessage(playerid, ADMIN_ERROR,
                "[Tarkenso RPG] Удаление отменено: кто-то находится у корпуса.");
        ShipPanel_Dispatch(playerid, "clear");
        return ShipPanel_Open(playerid);
    }
    return 1;
}
