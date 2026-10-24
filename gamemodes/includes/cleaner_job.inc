#if defined _orvelar_cleaner_included
    #endinput
#endif
#define _orvelar_cleaner_included

// JOB-CLEANER-01. Общие уличные объекты; сюжетные находки сюда пока не входят.
// Координаты — пилотная зона Fort Carson, игровая проверка покрытия обязательна.
#define CLEANER_DIRECTORY       "signal_jobs"
#define CLEANER_SPOT_COUNT      (87)
#define CLEANER_VISIBLE_LIMIT   (24)
#define CLEANER_REFILL_MS       (120000) // один новый объект каждые 2 минуты
#define CLEANER_ENV_SAVE_MS     (180000)
#define CLEANER_DISTRICTS       (2)
#define CLEANER_POLLUTION_MAX   (100)
#define CLEANER_NIGHT_GOAL      (3)
#define CLEANER_NIGHT_BONUS     (100)
#define CLEANER_NIGHT_COOLDOWN  (86400)
#define CLEANER_ZONE_TICK_MS    (350)
#define CLEANER_ZONE_ENTER_SQ   (4.0)    // зона сбора 2 м
#define CLEANER_ZONE_RELEASE_SQ (12.25)  // выход на 3,5 м
#define CLEANER_SPACING_SQ      (196.0)  // 14 м между активными объектами
#define CLEANER_ANIM_MS         (3700)
#define CLEANER_COLLECT_DELAY   (4)
#define CLEANER_INTERACT_SQ     (10.24)   // левый ALT в радиусе 3,2 м
#define CLEANER_BAG_MODEL       (1264)    // BlackBag1, модель мусорного мешка
#define CLEANER_PAY_PER_UNIT    (25)
#define CLEANER_MAX_TOTAL       (1000000)
// Точка диспетчера предоставлена по игровому замеру владельца сервера.
#define CLEANER_DEPOT_X         (-219.9823)
#define CLEANER_DEPOT_Y         (1070.8785)
#define CLEANER_DEPOT_Z         (19.7422)
#define CLEANER_DEPOT_A         (87.6387)
#define CLEANER_COLOR           (0xA7D6B5FF)

// 20 исходных и 67 новых пилотных точек по кварталам Fort Carson.
// Координаты удалённых точек требуют подтверждения в игровом UAT:
// минимальная дистанция между реально активными объектами: 14 м.
new const Float:gCleanerSpots[CLEANER_SPOT_COUNT][3] =
{
    {-209.5, 1089.0, 19.6},
    {-205.0, 1089.0, 19.6},
    {-199.5, 1089.0, 19.6},
    {-191.0, 1089.0, 19.6},
    {-210.0, 1077.5, 19.6},
    {-206.0, 1077.5, 19.6},
    {-199.0, 1077.5, 19.6},
    {-190.0, 1077.5, 19.6},
    {-209.0, 1091.0, 19.6},
    {-205.0, 1091.0, 19.6},
    {-200.0, 1091.0, 19.6},
    {-195.0, 1091.0, 19.6},
    {-210.0, 1075.0, 19.6},
    {-206.0, 1075.0, 19.6},
    {-201.0, 1075.0, 19.6},
    {-195.0, 1075.0, 19.6},
    {-215.0, 1089.0, 19.6},
    {-215.0, 1077.0, 19.6},
    {-189.0, 1081.5, 19.6},
    {-189.0, 1085.0, 19.6},
    {-377.0, 972.0, 19.6},
    {-377.0, 1008.0, 19.6},
    {-377.0, 1044.0, 19.6},
    {-377.0, 1080.0, 19.6},
    {-377.0, 1116.0, 19.6},
    {-377.0, 1152.0, 19.6},
    {-377.0, 1188.0, 19.6},
    {-377.0, 1224.0, 19.6},
    {-332.0, 960.0, 19.6},
    {-332.0, 996.0, 19.6},
    {-332.0, 1032.0, 19.6},
    {-332.0, 1068.0, 19.6},
    {-332.0, 1104.0, 19.6},
    {-332.0, 1140.0, 19.6},
    {-332.0, 1176.0, 19.6},
    {-332.0, 1212.0, 19.6},
    {-287.0, 978.0, 19.6},
    {-287.0, 1014.0, 19.6},
    {-287.0, 1050.0, 19.6},
    {-287.0, 1086.0, 19.6},
    {-287.0, 1122.0, 19.6},
    {-287.0, 1158.0, 19.6},
    {-287.0, 1194.0, 19.6},
    {-252.0, 960.0, 19.6},
    {-252.0, 996.0, 19.6},
    {-252.0, 1032.0, 19.6},
    {-252.0, 1068.0, 19.6},
    {-252.0, 1104.0, 19.6},
    {-252.0, 1140.0, 19.6},
    {-252.0, 1176.0, 19.6},
    {-252.0, 1212.0, 19.6},
    {-167.0, 963.0, 19.6},
    {-167.0, 999.0, 19.6},
    {-167.0, 1035.0, 19.6},
    {-167.0, 1071.0, 19.6},
    {-167.0, 1107.0, 19.6},
    {-167.0, 1143.0, 19.6},
    {-167.0, 1179.0, 19.6},
    {-167.0, 1215.0, 19.6},
    {-127.0, 981.0, 19.6},
    {-127.0, 1017.0, 19.6},
    {-127.0, 1053.0, 19.6},
    {-127.0, 1089.0, 19.6},
    {-127.0, 1125.0, 19.6},
    {-127.0, 1161.0, 19.6},
    {-127.0, 1197.0, 19.6},
    {-82.0, 960.0, 19.6},
    {-82.0, 996.0, 19.6},
    {-82.0, 1032.0, 19.6},
    {-82.0, 1068.0, 19.6},
    {-82.0, 1104.0, 19.6},
    {-82.0, 1140.0, 19.6},
    {-82.0, 1176.0, 19.6},
    {-82.0, 1212.0, 19.6},
    {-37.0, 990.0, 19.6},
    {-37.0, 1026.0, 19.6},
    {-37.0, 1062.0, 19.6},
    {-37.0, 1098.0, 19.6},
    {-37.0, 1134.0, 19.6},
    {-37.0, 1170.0, 19.6},
    {-37.0, 1206.0, 19.6},
    {-2.0, 1008.0, 19.6},
    {-2.0, 1044.0, 19.6},
    {-2.0, 1080.0, 19.6},
    {-2.0, 1116.0, 19.6},
    {-2.0, 1152.0, 19.6},
    {-2.0, 1188.0, 19.6}
};

new const gCleanerSpotDistrict[CLEANER_SPOT_COUNT] =
{
    0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 0, 1, 1, 1, 1,
    0, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 1, 1, 1, 1,
    0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 1, 1, 1, 1, 0,
    0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 0, 1, 1, 1, 1, 0,
    0, 0, 1, 1, 1, 1, 0, 0, 0, 0, 1, 1, 1, 0, 0, 0,
    0, 1, 1, 1, 0, 0, 0
};

new gCleanerDistrictDirt[CLEANER_DISTRICTS];
new bool:gCleanerEnvWritable;
new gCleanerEnvironmentTimer;
new gCleanerTrash[CLEANER_SPOT_COUNT];
new gCleanerRefillTimer;
new gCleanerZoneTimer;
// Подписи видны только игроку на активной смене (не глобальные 3D labels).
new PlayerText3D:gCleanerTrashLabel[MAX_PLAYERS][CLEANER_SPOT_COUNT];
new gCleanerActor = INVALID_ACTOR_ID;
new Text3D:gCleanerLabel = Text3D:INVALID_3DTEXT_ID;
new bool:gCleanerDuty[MAX_PLAYERS];
// Плотный список работников на смене: тики зоны O(активные), не O(MAX_PLAYERS).
new gCleanerWorkers[MAX_PLAYERS];
new gCleanerWorkerIndex[MAX_PLAYERS]; // -1 = игрок не зарегистрирован
new gCleanerWorkerCount;
new bool:gCleanerDataError[MAX_PLAYERS];
new gCleanerBag[MAX_PLAYERS];
new gCleanerXP[MAX_PLAYERS];
new gCleanerCollected[MAX_PLAYERS];
new gCleanerEarned[MAX_PLAYERS];
new gCleanerNextCollect[MAX_PLAYERS];
new gCleanerLastInteract[MAX_PLAYERS];
new gCleanerZoneHold[MAX_PLAYERS]; // index+1 = нужно отойти на 3,5 м
new gCleanerBagPropSlot[MAX_PLAYERS]; // -1, если аксессуар не был выдан работой
new PlayerText:gCleanerBagHud[MAX_PLAYERS]; // один клиент-независимый ASCII TextDraw: BAG: X/Y
new gCleanerNightDistrict[MAX_PLAYERS]; // -1 = ночного контракта нет
new gCleanerNightProgress[MAX_PLAYERS];
new gCleanerNightCompleted[MAX_PLAYERS];
new gCleanerNightLastPaid[MAX_PLAYERS];

forward Cleaner_Refill();
forward Cleaner_ZoneTick();
forward bool:Cleaner_SettlePending(playerid);
forward Cleaner_EnvironmentTick();
forward Cleaner_StartNightContract(playerid);
forward bool:Cleaner_FinishNightContract(playerid);
forward Cleaner_Deposit(playerid);

// Включение/выключение за O(1); swap-remove не оставляет дубликатов.
stock Cleaner_WorkerAdd(playerid)
{
    if (gCleanerWorkerIndex[playerid] >= 0) return 1;
    if (gCleanerWorkerCount >= MAX_PLAYERS) return 0;
    new index = gCleanerWorkerCount++;
    gCleanerWorkers[index] = playerid;
    gCleanerWorkerIndex[playerid] = index;
    return 1;
}

stock Cleaner_WorkerRemove(playerid)
{
    new index = gCleanerWorkerIndex[playerid];
    if (index >= 0 && index < gCleanerWorkerCount &&
        gCleanerWorkers[index] == playerid)
    {
        new last = --gCleanerWorkerCount;
        new moved = gCleanerWorkers[last];
        gCleanerWorkers[index] = moved;
        gCleanerWorkerIndex[moved] = index;
    }
    gCleanerWorkerIndex[playerid] = -1;
    return 1;
}

stock Cleaner_Rank(playerid)
{
    new xp = gCleanerXP[playerid];
    if (xp >= 300) return 5;
    if (xp >= 140) return 4;
    if (xp >= 60) return 3;
    if (xp >= 20) return 2;
    return 1;
}

stock Cleaner_Capacity(playerid)
{
    switch (Cleaner_Rank(playerid))
    {
        case 2: return 8;
        case 3: return 10;
        case 4: return 12;
        case 5: return 15;
    }
    return 6;
}

stock bool:Cleaner_InWorld(playerid)
{
    return IsPlayerConnected(playerid) && !IsPlayerNPC(playerid) &&
        gAuthState[playerid] == AUTH_READY && GetPlayerVirtualWorld(playerid) == 0 &&
        GetPlayerInterior(playerid) == 0 && !IsPlayerInAnyVehicle(playerid);
}

stock bool:Cleaner_NearDepot(playerid)
{
    if (!Cleaner_InWorld(playerid)) return false;
    new Float:x, Float:y, Float:z;
    GetPlayerPos(playerid, x, y, z);
    x -= CLEANER_DEPOT_X;
    y -= CLEANER_DEPOT_Y;
    z -= CLEANER_DEPOT_Z;
    return x * x + y * y + z * z <= CLEANER_INTERACT_SQ;
}


// Стандартный TextDraw GTA не поддерживает кириллицу у всех клиентов.
// Пишем BAG: X/Y обычным ASCII в одном компактном элементе.
stock Cleaner_BagHudUpdate(playerid)
{
    if (gCleanerBagHud[playerid] == PlayerText:INVALID_TEXT_DRAW) return 0;
    new label[36];
    format(label, sizeof(label), "BAG: %d/%d",
        gCleanerBag[playerid], Cleaner_Capacity(playerid));
    PlayerTextDrawSetString(playerid, gCleanerBagHud[playerid], label);
    return 1;
}

stock Cleaner_BagHudShow(playerid)
{
    if (gCleanerBagHud[playerid] != PlayerText:INVALID_TEXT_DRAW)
        return Cleaner_BagHudUpdate(playerid);
    gCleanerBagHud[playerid] = CreatePlayerTextDraw(playerid,
        618.0, 404.0, "BAG: 0/6");
    if (gCleanerBagHud[playerid] == PlayerText:INVALID_TEXT_DRAW) return 0;
    PlayerTextDrawFont(playerid, gCleanerBagHud[playerid], TEXT_DRAW_FONT_2);
    PlayerTextDrawAlignment(playerid, gCleanerBagHud[playerid], TEXT_DRAW_ALIGN_RIGHT);
    PlayerTextDrawLetterSize(playerid, gCleanerBagHud[playerid], 0.30, 1.30);
    PlayerTextDrawColour(playerid, gCleanerBagHud[playerid], 0xE1F5E5FF);
    PlayerTextDrawBackgroundColour(playerid, gCleanerBagHud[playerid], 0x11251BFF);
    PlayerTextDrawSetOutline(playerid, gCleanerBagHud[playerid], 1);
    PlayerTextDrawSetProportional(playerid, gCleanerBagHud[playerid], true);
    Cleaner_BagHudUpdate(playerid);
    PlayerTextDrawShow(playerid, gCleanerBagHud[playerid]);
    return 1;
}

stock Cleaner_BagHudHide(playerid)
{
    if (gCleanerBagHud[playerid] == PlayerText:INVALID_TEXT_DRAW) return 1;
    if (IsPlayerConnected(playerid))
        PlayerTextDrawDestroy(playerid, gCleanerBagHud[playerid]);
    gCleanerBagHud[playerid] = PlayerText:INVALID_TEXT_DRAW;
    return 1;
}

// Правая кисть (bone 6); выбираем свободный слот, не перезаписывая аксессуары.
stock bool:Cleaner_GiveBagProp(playerid)
{
    if (gCleanerBagPropSlot[playerid] >= 0) return true;
    for (new slot = 9; slot >= 7; slot--)
    {
        if (IsPlayerAttachedObjectSlotUsed(playerid, slot)) continue;
        if (!SetPlayerAttachedObject(playerid, slot, CLEANER_BAG_MODEL, 6,
            0.07, 0.04, -0.05, 90.0, 0.0, 0.0,
            0.23, 0.23, 0.23)) return false;
        gCleanerBagPropSlot[playerid] = slot;
        return true;
    }
    return false;
}

stock Cleaner_RemoveBagProp(playerid)
{
    new slot = gCleanerBagPropSlot[playerid];
    if (slot >= 0 && IsPlayerConnected(playerid) &&
        IsPlayerAttachedObjectSlotUsed(playerid, slot))
        RemovePlayerAttachedObject(playerid, slot);
    gCleanerBagPropSlot[playerid] = -1;
    return 1;
}

stock Cleaner_RemoveTrashLabels(point)
{
    if (point < 0 || point >= CLEANER_SPOT_COUNT) return 0;
    for (new i = 0; i < gCleanerWorkerCount; i++)
    {
        new playerid = gCleanerWorkers[i];
        if (gCleanerTrashLabel[playerid][point] == PlayerText3D:INVALID_3DTEXT_ID)
            continue;
        if (IsPlayerConnected(playerid))
            DeletePlayer3DTextLabel(playerid, gCleanerTrashLabel[playerid][point]);
        gCleanerTrashLabel[playerid][point] = PlayerText3D:INVALID_3DTEXT_ID;
    }
    return 1;
}

stock Cleaner_HideTrashLabels(playerid)
{
    for (new i = 0; i < CLEANER_SPOT_COUNT; i++)
    {
        if (gCleanerTrashLabel[playerid][i] == PlayerText3D:INVALID_3DTEXT_ID)
            continue;
        if (IsPlayerConnected(playerid))
            DeletePlayer3DTextLabel(playerid, gCleanerTrashLabel[playerid][i]);
        gCleanerTrashLabel[playerid][i] = PlayerText3D:INVALID_3DTEXT_ID;
    }
    return 1;
}

stock Cleaner_SyncTrashLabels(playerid)
{
    // Персональные 3D подписи не имеют надёжной фильтрации virtual world.
    // Поэтому вне внешнего мира их надо явно удалять.
    new bool:visible = IsPlayerConnected(playerid) && gCleanerDuty[playerid] &&
        !gCleanerDataError[playerid] && gAuthState[playerid] == AUTH_READY &&
        GetPlayerVirtualWorld(playerid) == 0 && GetPlayerInterior(playerid) == 0;
    for (new i = 0; i < CLEANER_SPOT_COUNT; i++)
    {
        new bool:needed = visible && gCleanerTrash[i] != INVALID_OBJECT_ID;
        if (!needed)
        {
            if (gCleanerTrashLabel[playerid][i] == PlayerText3D:INVALID_3DTEXT_ID)
                continue;
            DeletePlayer3DTextLabel(playerid, gCleanerTrashLabel[playerid][i]);
            gCleanerTrashLabel[playerid][i] = PlayerText3D:INVALID_3DTEXT_ID;
            continue;
        }
        if (gCleanerTrashLabel[playerid][i] != PlayerText3D:INVALID_3DTEXT_ID)
            continue;
        gCleanerTrashLabel[playerid][i] = CreatePlayer3DTextLabel(playerid,
            "{A7D6B5}МУСОР\n{FFFFFF}Подойдите ближе для сбора",
            CLEANER_COLOR, gCleanerSpots[i][0], gCleanerSpots[i][1],
            gCleanerSpots[i][2] + 0.30, 7.0);
    }
    return 1;
}

stock Cleaner_EndShift(playerid)
{
    gCleanerDuty[playerid] = false;
    Cleaner_HideTrashLabels(playerid);
    Cleaner_BagHudHide(playerid);
    Cleaner_RemoveBagProp(playerid);
    Cleaner_WorkerRemove(playerid);
    gCleanerZoneHold[playerid] = 0;
    gCleanerNightDistrict[playerid] = -1;
    gCleanerNightProgress[playerid] = 0;
    return 1;
}

// Начало и конец смены теперь только через левый ALT у NPC.
// Чекпоинты больше не создаются, поэтому нет повторных срабатываний.
stock Cleaner_OnKeyStateChange(playerid, KEY:newkeys, KEY:oldkeys)
{
    if (!(newkeys & KEY_WALK) || (oldkeys & KEY_WALK)) return 0;
    if (!Cleaner_InWorld(playerid) ||
        GetPlayerState(playerid) != PLAYER_STATE_ONFOOT ||
        gCleanerDataError[playerid] || !Cleaner_NearDepot(playerid)) return 0;
    if (gCleanerLastInteract[playerid] &&
        GetTickCount() - gCleanerLastInteract[playerid] < 750) return 0;
    gCleanerLastInteract[playerid] = GetTickCount();

    if (!gCleanerDuty[playerid])
    {
        if (!CleanerDump_IsReady())
            return SendClientMessage(playerid, COLOR_RED,
                "Выдача смены недоступна: контейнер не готов. Сообщите администрации.");
        if (!Cleaner_GiveBagProp(playerid))
            return SendClientMessage(playerid, COLOR_RED,
                "Не удалось выдать мешок. Освободите слот аксессуара.");
        if (!Cleaner_WorkerAdd(playerid))
        {
            Cleaner_RemoveBagProp(playerid);
            return SendClientMessage(playerid, COLOR_RED, "Смена недоступна: лимит работников.");
        }
        gCleanerDuty[playerid] = true;
        gCleanerZoneHold[playerid] = 0;
        Cleaner_StartNightContract(playerid);
        Cleaner_BagHudShow(playerid);
        Cleaner_SyncTrashLabels(playerid);
        // Прогреваем библиотеку, чтобы первый сбор не был без анимации.
        ApplyAnimation(playerid, "BOMBER", "BOM_Plant_Loop", 4.1,
            false, false, false, false, 0, SYNC_NONE);
        ClearAnimations(playerid);
        SendClientMessage(playerid, CLEANER_COLOR,
            "Смена началась! Мешок выдан. Собирайте мусор, подходя к нему.");
        SendClientMessage(playerid, COLOR_GREY,
            "Сдать мусор: /trashdrop у контейнера. Закончить смену: левый ALT у NPC.");
        return 1;
    }
    // Сдать мусор можно только у контейнера, а не у диспетчера.
    if (gCleanerBag[playerid] > 0)
        return SendClientMessage(playerid, COLOR_GREY,
            "Сначала сдайте мусор у контейнера через /trashdrop, затем завершайте смену.");
    if (!Cleaner_FinishNightContract(playerid)) return 1;
    Cleaner_EndShift(playerid);
    SendClientMessage(playerid, CLEANER_COLOR,
        "Смена завершена. До встречи!");
    return 1;
}

// Экологический показатель квартала 0..100, без бесконечного накопления.
stock Cleaner_AdjustDirt(district, amount)
{
    if (district < 0 || district >= CLEANER_DISTRICTS) return 0;
    new next = gCleanerDistrictDirt[district] + amount;
    if (next < 0) next = 0;
    if (next > CLEANER_POLLUTION_MAX) next = CLEANER_POLLUTION_MAX;
    gCleanerDistrictDirt[district] = next;
    return 1;
}

stock Cleaner_DistrictName(district, dest[], size)
{
    if (district == 0) format(dest, size, "Северный квартал");
    else format(dest, size, "Южный квартал");
    return 1;
}

stock bool:Cleaner_ParseTime(const value[], &number)
{
    new length = strlen(value);
    if (!length || length > 10) return false;
    for (new i = 0; i < length; i++)
        if (value[i] < '0' || value[i] > '9') return false;
    if (length == 10 && strcmp(value, "2147483647", false) > 0) return false;
    number = strval(value);
    return number >= 0;
}

stock Cleaner_Path(playerid, dest[], size)
{
    format(dest, size, "%s/%s.clean", CLEANER_DIRECTORY, gAccountName[playerid]);
    return 1;
}

stock bool:Cleaner_Number(const value[], &number)
{
    new length = strlen(value);
    if (!length || length > 7) return false;
    for (new i = 0; i < length; i++)
        if (value[i] < '0' || value[i] > '9') return false;
    number = strval(value);
    return number >= 0 && number <= CLEANER_MAX_TOTAL;
}

// Новые игроки имеют нулевой прогресс. Повреждённый файл не перезаписывается.
stock bool:Cleaner_Load(playerid)
{
#if defined TARKENSO_DB_PRIMARY
    return DBPrimary_LoadCleaner(playerid);
#else
    new path[96], backup[112], header[32], xp[24], count[24], paid[24];
    new completedText[24], nightLastText[24];
    Cleaner_Path(playerid, path, sizeof(path));
    format(backup, sizeof(backup), "%s.bak", path);
    if (!fexist(path) && fexist(backup)) frename(backup, path);
    if (!fexist(path)) return true;
    new File:file = fopen(path, io_read);
    if (!file) return false;
    new bool:ok = true;
    if (!fread(file, header, sizeof(header))) ok = false;
    if (!fread(file, xp, sizeof(xp))) ok = false;
    if (!fread(file, count, sizeof(count))) ok = false;
    if (!fread(file, paid, sizeof(paid))) ok = false;
    new bool:v2 = false;
    if (ok)
    {
        Auth_StripLine(header);
        v2 = !strcmp(header, "ORVELARCLEANER|2", false);
        if (v2)
        {
            if (!fread(file, completedText, sizeof(completedText))) ok = false;
            if (!fread(file, nightLastText, sizeof(nightLastText))) ok = false;
        }
    }
    fclose(file);
    if (!ok) return false;
    Auth_StripLine(xp);
    Auth_StripLine(count);
    Auth_StripLine(paid);
    new experience, collected, earned, completed, lastPaid;
    if (!Cleaner_Number(xp, experience) ||
        !Cleaner_Number(count, collected) ||
        !Cleaner_Number(paid, earned)) return false;
    if (v2)
    {
        Auth_StripLine(completedText);
        Auth_StripLine(nightLastText);
        if (!Cleaner_Number(completedText, completed) ||
            !Cleaner_ParseTime(nightLastText, lastPaid)) return false;
    }
    else if (strcmp(header, "ORVELARCLEANER|1", false)) return false;
    if (experience != collected ||
        earned != collected * CLEANER_PAY_PER_UNIT + completed * CLEANER_NIGHT_BONUS)
        return false;
    if (v2 && ((completed == 0 && lastPaid != 0) ||
        (completed > 0 && lastPaid == 0))) return false;
    gCleanerNightCompleted[playerid] = completed;
    gCleanerNightLastPaid[playerid] = lastPaid;
    gCleanerXP[playerid] = experience;
    gCleanerCollected[playerid] = collected;
    gCleanerEarned[playerid] = earned;
    return true;
#endif
}

stock bool:Cleaner_Save(playerid)
{
#if defined TARKENSO_DB_PRIMARY
    return DBPrimary_SaveCleaner(playerid);
#else
    new path[96], temp[112], backup[112], line[96];
    Cleaner_Path(playerid, path, sizeof(path));
    format(temp, sizeof(temp), "%s.tmp", path);
    format(backup, sizeof(backup), "%s.bak", path);
    if (fexist(temp)) fremove(temp);
    new File:file = fopen(temp, io_write);
    if (!file) return false;
    new bool:ok = fwrite(file, "ORVELARCLEANER|2\n") > 0;
    format(line, sizeof(line), "%d\n%d\n%d\n%d\n%d\n",
        gCleanerXP[playerid], gCleanerCollected[playerid], gCleanerEarned[playerid],
        gCleanerNightCompleted[playerid], gCleanerNightLastPaid[playerid]);
    if (fwrite(file, line) <= 0) ok = false;
    fclose(file);
    if (!ok)
    {
        fremove(temp);
        return false;
    }
    new bool:hadOriginal = fexist(path) != 0;
    if (hadOriginal)
    {
        if (fexist(backup)) fremove(backup);
        if (!frename(path, backup))
        {
            fremove(temp);
            return false;
        }
    }
    if (!frename(temp, path))
    {
        if (hadOriginal) frename(backup, path);
        fremove(temp);
        return false;
    }
    if (hadOriginal && fexist(backup)) fremove(backup);
    return true;
#endif
}

// Отдельное состояние города не смешивается с аккаунтами.
stock bool:Cleaner_LoadEnvironment()
{
#if defined TARKENSO_DB_PRIMARY
    return DBWorld_DistrictLoad();
#else
    new path[] = CLEANER_DIRECTORY "/districts.env";
    new backup[] = CLEANER_DIRECTORY "/districts.bak";
    new header[32], north[16], south[16];
    // Восстанавливаем предыдущую корректную версию после сбоя между rename.
    if (!fexist(path) && fexist(backup) && !frename(backup, path)) return false;
    if (!fexist(path)) return true;
    new File:handle = fopen(path, io_read);
    if (!handle) return false;
    new bool:ok = true;
    if (!fread(handle, header, sizeof(header))) ok = false;
    if (!fread(handle, north, sizeof(north))) ok = false;
    if (!fread(handle, south, sizeof(south))) ok = false;
    fclose(handle);
    if (!ok) return false;
    Auth_StripLine(header);
    Auth_StripLine(north);
    Auth_StripLine(south);
    new a, b;
    if (strcmp(header, "ORVELARENV|1", false) ||
        !Cleaner_Number(north, a) || !Cleaner_Number(south, b) ||
        a > CLEANER_POLLUTION_MAX || b > CLEANER_POLLUTION_MAX)
        return false;
    gCleanerDistrictDirt[0] = a;
    gCleanerDistrictDirt[1] = b;
    return true;
#endif
}

stock bool:Cleaner_SaveEnvironment()
{
    if (!gCleanerEnvWritable) return false;
#if defined TARKENSO_DB_PRIMARY
    return DBWorld_DistrictSave();
#else
    new path[] = CLEANER_DIRECTORY "/districts.env";
    new temp[] = CLEANER_DIRECTORY "/districts.tmp";
    new backup[] = CLEANER_DIRECTORY "/districts.bak";
    if (fexist(temp)) fremove(temp);
    new File:handle = fopen(temp, io_write);
    if (!handle) return false;
    new bool:ok = fwrite(handle, "ORVELARENV|1\n") > 0;
    new line[40];
    format(line, sizeof(line), "%d\n%d\n", gCleanerDistrictDirt[0], gCleanerDistrictDirt[1]);
    if (fwrite(handle, line) <= 0) ok = false;
    fclose(handle);
    if (!ok) { fremove(temp); return false; }
    new bool:old = fexist(path) != 0;
    if (old)
    {
        if (fexist(backup)) fremove(backup);
        if (!frename(path, backup)) { fremove(temp); return false; }
    }
    if (!frename(temp, path))
    {
        if (old) frename(backup, path);
        fremove(temp);
        return false;
    }
    if (old && fexist(backup)) fremove(backup);
    return true;
#endif
}

public Cleaner_EnvironmentTick()
{
    Cleaner_SaveEnvironment();
    return 1;
}

stock Cleaner_NightTime(playerid)
{
    new hour, minute;
    GetPlayerTime(playerid, hour, minute);
    return hour >= 22 || hour < 6;
}

stock Cleaner_StartNightContract(playerid)
{
    gCleanerNightDistrict[playerid] = -1;
    gCleanerNightProgress[playerid] = 0;
    if (Cleaner_Rank(playerid) < 2 || !Cleaner_NightTime(playerid)) return 0;
    new now = gettime();
    if (gCleanerNightLastPaid[playerid] > 0 &&
        now - gCleanerNightLastPaid[playerid] < CLEANER_NIGHT_COOLDOWN) return 0;
    gCleanerNightDistrict[playerid] = random(CLEANER_DISTRICTS);
    new district[40], message[150];
    Cleaner_DistrictName(gCleanerNightDistrict[playerid], district, sizeof(district));
    format(message, sizeof(message),
        "НОЧНОЕ ЗАДАНИЕ: %s. Соберите %d ед. мусора, премия $%d.",
        district, CLEANER_NIGHT_GOAL, CLEANER_NIGHT_BONUS);
    SendClientMessage(playerid, CLEANER_COLOR, message);
    return 1;
}

stock bool:Cleaner_FinishNightContract(playerid)
{
    if (gCleanerNightDistrict[playerid] < 0) return true;
    if (gCleanerNightProgress[playerid] < CLEANER_NIGHT_GOAL)
    {
        SendClientMessage(playerid, COLOR_GREY,
            "Ночной маршрут не выполнен. Премия не начисляется.");
        gCleanerNightDistrict[playerid] = -1;
        return true;
    }
    if (gCleanerNightCompleted[playerid] >= CLEANER_MAX_TOTAL ||
        gCleanerEarned[playerid] > CLEANER_MAX_TOTAL - CLEANER_NIGHT_BONUS)
        return false;
    new previousTime = gCleanerNightLastPaid[playerid];
    gCleanerNightCompleted[playerid]++;
    gCleanerEarned[playerid] += CLEANER_NIGHT_BONUS;
    gCleanerNightLastPaid[playerid] = gettime();
    if (!Cleaner_Save(playerid))
    {
        gCleanerNightCompleted[playerid]--;
        gCleanerEarned[playerid] -= CLEANER_NIGHT_BONUS;
        gCleanerNightLastPaid[playerid] = previousTime;
        SendClientMessage(playerid, COLOR_RED, "Ошибка сохранения ночного отчёта.");
        return false;
    }
    gCleanerNightDistrict[playerid] = -1;
    // Статистика .clean теперь подтверждённое обязательство к выплате.
    if (!Cleaner_SettlePending(playerid))
        SendClientMessage(playerid, COLOR_RED,
            "Ночная премия учтена. Выплата ожидает восстановления кошелька.");
    SendClientMessage(playerid, CLEANER_COLOR, "Ночной маршрут выполнен! Премия $100.");
    return true;
}

stock Cleaner_OnConnect(playerid)
{
    Cleaner_WorkerRemove(playerid);
    gCleanerBagPropSlot[playerid] = -1;
    gCleanerBagHud[playerid] = PlayerText:INVALID_TEXT_DRAW;
    gCleanerLastInteract[playerid] = 0;
    for (new i = 0; i < CLEANER_SPOT_COUNT; i++)
        gCleanerTrashLabel[playerid][i] = PlayerText3D:INVALID_3DTEXT_ID;
    gCleanerZoneHold[playerid] = 0;
    gCleanerDuty[playerid] = false;
    gCleanerDataError[playerid] = false;
    gCleanerBag[playerid] = 0;
    gCleanerXP[playerid] = 0;
    gCleanerCollected[playerid] = 0;
    gCleanerEarned[playerid] = 0;
    gCleanerNextCollect[playerid] = 0;
    gCleanerNightDistrict[playerid] = -1;
    gCleanerNightProgress[playerid] = 0;
    gCleanerNightCompleted[playerid] = 0;
    gCleanerNightLastPaid[playerid] = 0;
    return 1;
}

stock Cleaner_OnReady(playerid)
{
    // Загрузка только после успешной авторизации и проверки имени аккаунта.
    if (!Cleaner_Load(playerid))
    {
        gCleanerDataError[playerid] = true;
        SendClientMessage(playerid, COLOR_RED,
            "Файл профессии уборщика повреждён или недоступен. Работа заблокирована.");
        return 0;
    }
    // Для существующих .acc v1/v2: предыдущий заработок — уже выплаченная история.
    if (!Economy_CleanerInitialize(playerid, gCleanerEarned[playerid]) ||
        gCleanerEarned[playerid] < gAccountCleanerPaid[playerid])
    {
        gCleanerDataError[playerid] = true;
        SendClientMessage(playerid, COLOR_RED, "Несогласованная зарплата. Работа заблокирована.");
        return 0;
    }
    if (!Cleaner_SettlePending(playerid))
        SendClientMessage(playerid, COLOR_RED, "Ожидается отложенная выплата зарплаты.");
    return 1;
}

// Идемпотентная оплата разницы между .clean и watermark внутри .acc.
stock bool:Cleaner_SettlePending(playerid)
{
    if (gCleanerDataError[playerid] || gAuthState[playerid] != AUTH_READY) return true;
    if (gAccountCleanerPaid[playerid] < 0 ||
        gCleanerEarned[playerid] < gAccountCleanerPaid[playerid]) return false;
    return Economy_CleanerSettle(playerid, gCleanerEarned[playerid]);
}

stock Cleaner_OnDisconnect(playerid)
{
    // Несданный мешок не оплачивается и не сохраняется.
    Cleaner_EndShift(playerid);
    return Cleaner_OnConnect(playerid);
}

stock Cleaner_OnSpawn(playerid)
{
    if (gCleanerDuty[playerid])
    {
        Cleaner_EndShift(playerid);
        gCleanerBag[playerid] = 0;
        SendClientMessage(playerid, COLOR_GREY,
            "После возрождения смена прекращена. Несданный мусор потерян.");
    }
    else Cleaner_EndShift(playerid);
    gCleanerZoneHold[playerid] = 0;
    gCleanerNightDistrict[playerid] = -1;
    gCleanerNightProgress[playerid] = 0;
    return 1;
}

stock Cleaner_ActiveTrash()
{
    new active;
    for (new i = 0; i < CLEANER_SPOT_COUNT; i++)
        if (gCleanerTrash[i] != INVALID_OBJECT_ID) active++;
    return active;
}

stock Cleaner_VisibleTarget()
{
    new target = 16 + (gCleanerDistrictDirt[0] + gCleanerDistrictDirt[1]) / 25;
    if (target > CLEANER_VISIBLE_LIMIT) target = CLEANER_VISIBLE_LIMIT;
    return target;
}

// Спот допускается лишь при отсутствии других активных объектов в 14 м.
stock bool:Cleaner_SpotClear(point)
{
    if (point < 0 || point >= CLEANER_SPOT_COUNT ||
        gCleanerTrash[point] != INVALID_OBJECT_ID) return false;
    for (new i = 0; i < CLEANER_SPOT_COUNT; i++)
    {
        if (gCleanerTrash[i] == INVALID_OBJECT_ID) continue;
        new Float:dx = gCleanerSpots[point][0] - gCleanerSpots[i][0];
        new Float:dy = gCleanerSpots[point][1] - gCleanerSpots[i][1];
        if (dx * dx + dy * dy < CLEANER_SPACING_SQ) return false;
    }
    return true;
}

stock bool:Cleaner_CreateLitter(point, bool:fromNpc = false)
{
    if (!Cleaner_SpotClear(point) ||
        Cleaner_ActiveTrash() >= CLEANER_VISIBLE_LIMIT) return false;
    new model;
    switch (random(4))
    {
        case 0: model = 1264;
        case 1: model = 1265;
        case 2: model = 2670;
        default: model = 2671;
    }
    new objectid = CreateObject(model,
        gCleanerSpots[point][0], gCleanerSpots[point][1],
        gCleanerSpots[point][2] - 0.75, 0.0, 0.0, float(random(360)), 120.0);
    if (objectid == INVALID_OBJECT_ID) return false;
    gCleanerTrash[point] = objectid;
    Cleaner_AdjustDirt(gCleanerSpotDistrict[point], fromNpc ? 3 : 1);
    return true;
}

stock Cleaner_SpawnOne()
{
    new totalWeight;
    for (new i = 0; i < CLEANER_SPOT_COUNT; i++)
        if (Cleaner_SpotClear(i))
            totalWeight += 10 + gCleanerDistrictDirt[gCleanerSpotDistrict[i]];
    if (!totalWeight) return 0;
    new choice = random(totalWeight);
    for (new i = 0; i < CLEANER_SPOT_COUNT; i++)
    {
        if (!Cleaner_SpotClear(i)) continue;
        choice -= 10 + gCleanerDistrictDirt[gCleanerSpotDistrict[i]];
        if (choice < 0) return Cleaner_CreateLitter(i);
    }
    return 0;
}

public Cleaner_Refill()
{
    for (new district = 0; district < CLEANER_DISTRICTS; district++)
        Cleaner_AdjustDirt(district, 1);
    new count = Cleaner_ActiveTrash();
    new target = Cleaner_VisibleTarget();
    if (count < target) Cleaner_SpawnOne();
    return 1;
}

stock Cleaner_Init()
{
    gCleanerWorkerCount = 0;
    for (new i = 0; i < MAX_PLAYERS; i++)
        gCleanerWorkerIndex[i] = -1;
    fcreatedir(CLEANER_DIRECTORY);
    for (new district = 0; district < CLEANER_DISTRICTS; district++)
        gCleanerDistrictDirt[district] = 20;
    gCleanerEnvWritable = Cleaner_LoadEnvironment();
    if (!gCleanerEnvWritable)
        print("[Cleaner] District state file is corrupt; pollution writes disabled.");
    for (new i = 0; i < CLEANER_SPOT_COUNT; i++)
        gCleanerTrash[i] = INVALID_OBJECT_ID;
    gCleanerActor = CreateActor(50, CLEANER_DEPOT_X, CLEANER_DEPOT_Y,
        CLEANER_DEPOT_Z, CLEANER_DEPOT_A);
    gCleanerLabel = Create3DTextLabel(
        "{A7D6B5}Служба благоустройства\n{FFFFFF}Левый ALT — начать / завершить смену",
        CLEANER_COLOR, CLEANER_DEPOT_X, CLEANER_DEPOT_Y,
        CLEANER_DEPOT_Z + 1.2, 16.0, 0, false);
    // Сразу заполняем 16 удалённых и разнесённых точек по всему городу.
    for (new i = 0; i < 16; i++) Cleaner_SpawnOne();
    gCleanerRefillTimer = SetTimer("Cleaner_Refill", CLEANER_REFILL_MS, true);
    gCleanerZoneTimer = SetTimer("Cleaner_ZoneTick", CLEANER_ZONE_TICK_MS, true);
    gCleanerEnvironmentTimer = SetTimer("Cleaner_EnvironmentTick", CLEANER_ENV_SAVE_MS, true);
    print("[Cleaner] Street cleaner job: Fort Carson pilot district enabled.");
    return 1;
}

stock Cleaner_Shutdown()
{
    if (gCleanerEnvironmentTimer) KillTimer(gCleanerEnvironmentTimer);
    gCleanerEnvironmentTimer = 0;
    Cleaner_SaveEnvironment();
    if (gCleanerRefillTimer) KillTimer(gCleanerRefillTimer);
    gCleanerRefillTimer = 0;
    if (gCleanerZoneTimer) KillTimer(gCleanerZoneTimer);
    gCleanerZoneTimer = 0;
    for (new playerid = 0; playerid < MAX_PLAYERS; playerid++)
        if (IsPlayerConnected(playerid)) Cleaner_EndShift(playerid);
    for (new i = 0; i < CLEANER_SPOT_COUNT; i++)
    {
        if (gCleanerTrash[i] != INVALID_OBJECT_ID) DestroyObject(gCleanerTrash[i]);
        gCleanerTrash[i] = INVALID_OBJECT_ID;
    }
    if (gCleanerActor != INVALID_ACTOR_ID) DestroyActor(gCleanerActor);
    gCleanerActor = INVALID_ACTOR_ID;
    if (gCleanerLabel != Text3D:INVALID_3DTEXT_ID) Delete3DTextLabel(gCleanerLabel);
    gCleanerLabel = Text3D:INVALID_3DTEXT_ID;
    return 1;
}

stock Cleaner_Stats(playerid)
{
    new message[144];
    format(message, sizeof(message), "Уборщик: ранг %d/5, опыт %d, собрано %d.",
        Cleaner_Rank(playerid), gCleanerXP[playerid], gCleanerCollected[playerid]);
    SendClientMessage(playerid, CLEANER_COLOR, message);
    format(message, sizeof(message), "Всего начислено: $%d. Мешок: %d/%d.",
        gCleanerEarned[playerid], gCleanerBag[playerid], Cleaner_Capacity(playerid));
    SendClientMessage(playerid, COLOR_GREY, message);
    format(message, sizeof(message),
        "Чистота кварталов: север %d%%, юг %d%% (100%% = идеально).",
        100 - gCleanerDistrictDirt[0], 100 - gCleanerDistrictDirt[1]);
    SendClientMessage(playerid, CLEANER_COLOR, message);
    if (gCleanerNightDistrict[playerid] >= 0)
    {
        new name[40];
        Cleaner_DistrictName(gCleanerNightDistrict[playerid], name, sizeof(name));
        format(message, sizeof(message), "Ночной маршрут: %s, прогресс %d/%d.",
            name, gCleanerNightProgress[playerid], CLEANER_NIGHT_GOAL);
        SendClientMessage(playerid, CLEANER_COLOR, message);
    }
    return 1;
}

// Сбор по серверной зоне без streamer: не используем checkpoint смены.
// Ограниченное число точек делает опрос дешёвым; все эффекты выполняет сервер.
stock Cleaner_ZoneCollect(playerid)
{
    if (!gCleanerDuty[playerid] || gCleanerDataError[playerid] ||
        !Cleaner_InWorld(playerid) || GetPlayerState(playerid) != PLAYER_STATE_ONFOOT)
        return 0;

    new Float:px, Float:py, Float:pz;
    GetPlayerPos(playerid, px, py, pz);

    // Привязка к координатам, а не к живому объекту: повторное появление
    // мусора не приводит к спаму, пока игрок не вышел на 3,5 метра.
    new hold = gCleanerZoneHold[playerid];
    if (hold > 0)
    {
        new heldIndex = hold - 1;
        new Float:hx = px - gCleanerSpots[heldIndex][0];
        new Float:hy = py - gCleanerSpots[heldIndex][1];
        new Float:hz = pz - gCleanerSpots[heldIndex][2];
        if (hx * hx + hy * hy + hz * hz <= CLEANER_ZONE_RELEASE_SQ)
            return 0;
        gCleanerZoneHold[playerid] = 0;
    }

    if (gCleanerBag[playerid] >= Cleaner_Capacity(playerid) ||
        gettime() < gCleanerNextCollect[playerid]) return 0;

    new nearest = -1;
    new Float:best = CLEANER_ZONE_ENTER_SQ;
    for (new i = 0; i < CLEANER_SPOT_COUNT; i++)
    {
        if (gCleanerTrash[i] == INVALID_OBJECT_ID) continue;
        new Float:dx = px - gCleanerSpots[i][0];
        new Float:dy = py - gCleanerSpots[i][1];
        new Float:dz = pz - gCleanerSpots[i][2];
        new Float:distance = dx * dx + dy * dy + dz * dz;
        if (distance <= best)
        {
            best = distance;
            nearest = i;
        }
    }
    if (nearest < 0) return 0;

    // Сначала блокировка и удаление общего объекта, затем начисление мешка.
    // При обходе двух игроков в одном серверном тике предмет выдаётся одному.
    gCleanerZoneHold[playerid] = nearest + 1;
    new objectid = gCleanerTrash[nearest];
    gCleanerTrash[nearest] = INVALID_OBJECT_ID;
    DestroyObject(objectid);
    Cleaner_RemoveTrashLabels(nearest);
    gCleanerBag[playerid]++;
    Cleaner_AdjustDirt(gCleanerSpotDistrict[nearest], -7);
    if (gCleanerNightDistrict[playerid] == gCleanerSpotDistrict[nearest] &&
        gCleanerNightProgress[playerid] < CLEANER_NIGHT_GOAL)
    {
        gCleanerNightProgress[playerid]++;
        if (gCleanerNightProgress[playerid] == CLEANER_NIGHT_GOAL)
            SendClientMessage(playerid, CLEANER_COLOR,
                "Ночной маршрут выполнен. Завершите смену для получения премии.");
    }
    gCleanerNextCollect[playerid] = gettime() + CLEANER_COLLECT_DELAY;
    // Один непрерывный цикл сбора вместо двойного перезапуска анимации.
    ApplyAnimation(playerid, "BOMBER", "BOM_Plant_Loop", 4.1,
        true, false, false, false, CLEANER_ANIM_MS, SYNC_ALL);
    Cleaner_BagHudUpdate(playerid);
    if (gCleanerBag[playerid] >= Cleaner_Capacity(playerid))
        SendClientMessage(playerid, COLOR_GREY,
            "Мешок заполнен. Сдайте его на пункте приёма мусора.");
    return 1;
}

public Cleaner_ZoneTick()
{
    for (new i = 0; i < gCleanerWorkerCount; i++)
    {
        new playerid = gCleanerWorkers[i];
        if (IsPlayerConnected(playerid) && !IsPlayerNPC(playerid) &&
            gCleanerDuty[playerid])
        {
            Cleaner_SyncTrashLabels(playerid);
            Cleaner_ZoneCollect(playerid);
        }
    }
    return 1;
}

stock Cleaner_Deposit(playerid)
{
    if (!gCleanerDuty[playerid])
        return SendClientMessage(playerid, COLOR_RED, "Вы не на смене уборщика.");
    if (!CleanerDump_Near(playerid))
        return SendClientMessage(playerid, COLOR_RED,
            "Для сдачи мусора подойдите к контейнеру и используйте /trashdrop.");
    if (gCleanerDataError[playerid] || !CleanerDump_IsReady())
        return SendClientMessage(playerid, COLOR_RED,
            "Данные профессии или контейнера недоступны.");
    if (!gCleanerBag[playerid])
        return SendClientMessage(playerid, COLOR_GREY, "Ваш мешок пуст.");

    new count = gCleanerBag[playerid];
    if (!CleanerDump_CanAccept(count))
        return SendClientMessage(playerid, COLOR_RED,
            "Контейнер и площадка заполнены. Ожидайте муниципального вывоза.");

    new wage = count * CLEANER_PAY_PER_UNIT;
    if (gCleanerCollected[playerid] > CLEANER_MAX_TOTAL - count ||
        gCleanerEarned[playerid] > CLEANER_MAX_TOTAL - wage)
        return SendClientMessage(playerid, COLOR_RED,
            "Достигнут лимит статистики уборщика.");
    // SQL-first: START TRANSACTION + guarded bin update + profile save + COMMIT.
    // On error the transaction is rolled back and the bag stays with the player.
    if (!CleanerDump_Stage(count))
        return SendClientMessage(playerid, COLOR_RED,
            "Не удалось зарезервировать место в контейнере. Мешок остаётся у вас.");
    gCleanerXP[playerid] += count;
    gCleanerCollected[playerid] += count;
    gCleanerEarned[playerid] += wage;
    if (!Cleaner_Save(playerid))
    {
        CleanerDump_Rollback();
        gCleanerXP[playerid] -= count;
        gCleanerCollected[playerid] -= count;
        gCleanerEarned[playerid] -= wage;
        return SendClientMessage(playerid, COLOR_RED,
            "Не удалось сохранить отчёт. Мешок остаётся у вас.");
    }
    if (!CleanerDump_Commit())
    {
        gCleanerXP[playerid] -= count;
        gCleanerCollected[playerid] -= count;
        gCleanerEarned[playerid] -= wage;
#if !defined TARKENSO_DB_PRIMARY
        if (!Cleaner_Save(playerid))
        {
            gCleanerDataError[playerid] = true;
            print("[Cleaner] ERROR: unable to compensate report after container write failure.");
        }
#endif
        return SendClientMessage(playerid, COLOR_RED,
            "Не удалось сохранить контейнер. Мешок остаётся у вас.");
    }
    // Cleared only after durable storage of both report and shared container.
    gCleanerBag[playerid] = 0;
    Cleaner_BagHudUpdate(playerid);
    if (!Cleaner_SettlePending(playerid))
        SendClientMessage(playerid, COLOR_RED,
            "Зарплата учтена; перевод в кошелёк повторится при восстановлении.");
    new message[144];
    format(message, sizeof(message),
        "Сдано %d ед. ($%d). Контейнер: %d/%d, рядом: %d/%d.",
        count, wage, CleanerDump_GetBin(), CleanerDump_GetBinCap(),
        CleanerDump_GetOverflow(), CleanerDump_GetOverflowCap());
    SendClientMessage(playerid, CLEANER_COLOR, message);
    return 1;
}
