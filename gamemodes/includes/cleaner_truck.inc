#if defined _tarkenso_cleaner_truck_included
    #endinput
#endif
#define _tarkenso_cleaner_truck_included

// JOB-TRUCK-01: выбор специализации и живой перечень заказов по контейнерам.
// Рейсы, машины и анимация загрузки будут внедрены отдельными этапами.
#define TRUCK_DLG_DEPOT (6680)
#define TRUCK_DLG_ORDERS (6681)
#define TRUCK_DLG_INFO (6682)
#define TRUCK_ORDER_THRESHOLD (60)
#define TRUCK_BASE_RATE (10)
#define TRUCK_OVERFLOW_RATE (20)
#define TRUCK_MAP_SLOT (74)
#define TRUCK_MAP_COLOR (0xF1A554FF)

new bool:gTruckOnShift[MAX_PLAYERS];
new gTruckDialogContainer[MAX_PLAYERS][CLEANER_DUMP_LIMIT];
new gTruckDialogCount[MAX_PLAYERS];
new bool:gTruckMapMarked[MAX_PLAYERS];

stock Truck_Reset(playerid)
{
    gTruckOnShift[playerid] = false;
    gTruckDialogCount[playerid] = 0;
    if (IsPlayerConnected(playerid) && gTruckMapMarked[playerid])
        RemovePlayerMapIcon(playerid, TRUCK_MAP_SLOT);
    gTruckMapMarked[playerid] = false;
    return 1;
}

// Координатная классификация: ограниченные области, вне их — общий регион.
// При расширении карты зоны можно вынести в SQL без изменения заказов.
stock Truck_LocationName(Float:x, Float:y, output[], size)
{
    if (x >= -400.0 && x <= 0.0 && y >= 900.0 && y <= 1250.0)
        format(output, size, "Fort Carson");
    else if (x >= -400.0 && x <= 250.0 && y >= 1400.0 && y <= 1750.0)
        format(output, size, "Las Payasadas");
    else if (x >= -950.0 && x <= -600.0 && y >= 900.0 && y <= 1200.0)
        format(output, size, "Las Barrancas");
    else
        format(output, size, "Bone County / вне зон");
    return 1;
}

stock Truck_ShowDepot(playerid)
{
    if (gAuthState[playerid] != AUTH_READY || !Cleaner_NearDepot(playerid) ||
        gCleanerDataError[playerid]) return 0;
    if (gTruckOnShift[playerid])
        return ShowPlayerDialog(playerid, TRUCK_DLG_DEPOT, DIALOG_STYLE_LIST,
            "Муниципальная служба", "Завершить смену водителя", "Выбрать", "Закрыть");
    if (gCleanerDuty[playerid])
        return ShowPlayerDialog(playerid, TRUCK_DLG_DEPOT, DIALOG_STYLE_LIST,
            "Муниципальная служба", "Завершить смену сборщика", "Выбрать", "Закрыть");
    return ShowPlayerDialog(playerid, TRUCK_DLG_DEPOT, DIALOG_STYLE_LIST,
        "Муниципальная служба",
        "Сборщик мусора (с 1 ранга)\nВодитель мусоровоза (со 2 ранга)",
        "Выбрать", "Закрыть");
}

stock Truck_OnKeyStateChange(playerid, KEY:newkeys, KEY:oldkeys)
{
    if (!(newkeys & KEY_WALK) || (oldkeys & KEY_WALK) ||
        GetPlayerState(playerid) != PLAYER_STATE_ONFOOT ||
        !Cleaner_NearDepot(playerid)) return 0;
    if (gCleanerLastInteract[playerid] &&
        GetTickCount() - gCleanerLastInteract[playerid] < 750) return 1;
    gCleanerLastInteract[playerid] = GetTickCount();
    Truck_ShowDepot(playerid);
    return 1;
}

// Предварительная стоимость заказа, фактическая выплата — после реализации рейса.
stock Truck_EstimatedPay(slot)
{
    return gCleanerDumpBin[slot] * TRUCK_BASE_RATE +
        gCleanerDumpOverflow[slot] * TRUCK_OVERFLOW_RATE;
}

stock Truck_ShowOrders(playerid)
{
    if (!gTruckOnShift[playerid])
        return SendClientMessage(playerid, COLOR_RED, "Сначала устройтесь водителем мусоровоза.");
    if (!CleanerDump_IsReady())
        return SendClientMessage(playerid, COLOR_RED, "База контейнеров недоступна.");
    new body[2300], line[130], location[48], total;
    format(body, sizeof(body), "Контейнер\tМесто\tЗаполнение\tОплата\n");
    gTruckDialogCount[playerid] = 0;
    for (new i = 0; i < gCleanerDumpCount; i++)
    {
        if (!gCleanerDumpEnabled[i]) continue;
        total = gCleanerDumpBin[i] + gCleanerDumpOverflow[i];
        if (total < TRUCK_ORDER_THRESHOLD) continue;
        Truck_LocationName(gCleanerDumpPos[i][0], gCleanerDumpPos[i][1],
            location, sizeof(location));
        format(line, sizeof(line), "#%d\t%s\t%d/100\t$%d\n",
            gCleanerDumpId[i], location, total, Truck_EstimatedPay(i));
        strcat(body, line, sizeof(body));
        gTruckDialogContainer[playerid][gTruckDialogCount[playerid]++] = gCleanerDumpId[i];
    }
    if (!gTruckDialogCount[playerid])
        return SendClientMessage(playerid, COLOR_GREY, "Активных заказов пока нет.");
    return ShowPlayerDialog(playerid, TRUCK_DLG_ORDERS, DIALOG_STYLE_TABLIST_HEADERS,
        "Активные заказы", body, "На карту", "Назад");
}

stock Truck_OnDialog(playerid, dialogid, response, listitem)
{
    if (dialogid == TRUCK_DLG_DEPOT)
    {
        if (!response || !Cleaner_NearDepot(playerid) ||
            gAuthState[playerid] != AUTH_READY || gCleanerDataError[playerid]) return 1;
        if (gTruckOnShift[playerid])
        {
            if (listitem == 0)
            {
                Truck_Reset(playerid);
                SendClientMessage(playerid, CLEANER_COLOR, "Смена водителя завершена.");
            }
            return 1;
        }
        if (gCleanerDuty[playerid])
        {
            if (listitem == 0)
            {
                if (gCleanerBag[playerid] > 0)
                    return SendClientMessage(playerid, COLOR_GREY,
                        "Сначала сдайте мусор из мешка.");
                if (!Cleaner_FinishNightContract(playerid)) return 1;
                Cleaner_EndShift(playerid);
                SendClientMessage(playerid, CLEANER_COLOR, "Смена сборщика завершена.");
            }
            return 1;
        }
        if (listitem == 0)
        {
            // Existing validated employment logic: trigger only after selection.
            gCleanerLastInteract[playerid] = 0;
            return Cleaner_OnKeyStateChange(playerid, KEY_WALK, KEY:0);
        }
        if (listitem == 1)
        {
            if (Cleaner_Rank(playerid) < 2)
                return SendClientMessage(playerid, COLOR_RED,
                    "Водитель мусоровоза доступен со 2 ранга (20 опыта).");
            if (!CleanerDump_IsReady())
                return SendClientMessage(playerid, COLOR_RED,
                    "База контейнеров недоступна.");
            gTruckOnShift[playerid] = true;
            SendClientMessage(playerid, CLEANER_COLOR,
                "Вы назначены водителем. /orders - активные заказы.");
        }
        return 1;
    }
    if (dialogid == TRUCK_DLG_ORDERS)
    {
        if (!response || !gTruckOnShift[playerid]) return 1;
        if (listitem < 0 || listitem >= gTruckDialogCount[playerid]) return 1;
        new id = gTruckDialogContainer[playerid][listitem];
        for (new i = 0; i < gCleanerDumpCount; i++)
        {
            if (gCleanerDumpId[i] != id || !gCleanerDumpEnabled[i]) continue;
            if (gCleanerDumpBin[i] + gCleanerDumpOverflow[i] < TRUCK_ORDER_THRESHOLD)
                return SendClientMessage(playerid, COLOR_GREY, "Заказ больше не актуален.");
            if (gTruckMapMarked[playerid]) RemovePlayerMapIcon(playerid, TRUCK_MAP_SLOT);
            SetPlayerMapIcon(playerid, TRUCK_MAP_SLOT,
                gCleanerDumpPos[i][0], gCleanerDumpPos[i][1], gCleanerDumpPos[i][2],
                0, TRUCK_MAP_COLOR, MAPICON_LOCAL);
            gTruckMapMarked[playerid] = true;
            SendClientMessage(playerid, CLEANER_COLOR,
                "Контейнер отмечен на карте. Перевозка будет доступна в JOB-TRUCK-02.");
            return 1;
        }
        return SendClientMessage(playerid, COLOR_GREY, "Контейнер удалён или недоступен.");
    }
    return 0;
}
