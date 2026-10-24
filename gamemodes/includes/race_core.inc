#if defined _tarkenso_race_core_included
    #endinput
#endif
#define _tarkenso_race_core_included

// RACE-CORE-01. Отдельный игровой мир, одиночные и групповые заезды до 8 участников.
// RACE-MAP-01: геометрия и контрольные точки над карьером Hunter Quarry.
#define RACE_MAX_PLAYERS       (8)
#define RACE_MIN_PLAYERS       (1)
#define RACE_VEHICLE_MODEL     (495) // Sandking
#define RACE_COUNTDOWN         (15)
#define RACE_TIME_LIMIT        (300)
#define RACE_CHECKPOINT_RADIUS (8.0)
#define RACE_VERIFY_SQ         (100.0)
#define RACE_COLOR             (0xE6BB79FF)
#define RACE_STAGE_COUNT       (14)

#define RACE_IDLE              (0)
#define RACE_WAIT_START        (1)
#define RACE_IN_PROGRESS       (2)

new const Float:gRacePoints[RACE_STAGE_COUNT][3] =
{
    {510.0, 780.0, 166.5},
    {570.0, 800.0, 166.5},
    {570.0, 860.0, 166.5},
    {530.0, 860.0, 166.5},
    {500.0, 890.0, 166.5},
    {500.0, 940.0, 166.5},
    {580.0, 940.0, 166.5},
    {650.0, 920.0, 166.5},
    {650.0, 840.0, 166.5},
    {700.0, 830.0, 166.5},
    {770.0, 830.0, 166.5},
    {780.0, 910.0, 166.5},
    {780.0, 970.0, 166.5},
    {730.0, 970.0, 166.5}
};

new bool:gRaceJoined[MAX_PLAYERS];
new bool:gRaceRespawnRestore[MAX_PLAYERS];
new gRaceCars[MAX_PLAYERS];
new gRaceSlots[MAX_PLAYERS];
new bool:gRaceSlotTaken[RACE_MAX_PLAYERS];
new gRaceProgress[MAX_PLAYERS];
new Float:gRaceReturn[MAX_PLAYERS][4];
new Float:gRaceReturnHealth[MAX_PLAYERS];
new Float:gRaceReturnArmour[MAX_PLAYERS];
new gRaceReturnWorld[MAX_PLAYERS];
new gRaceReturnInterior[MAX_PLAYERS];

new gRaceCount;
new gRaceMode;
new gRaceStartAt;
new gRaceFinishAt;
new gRacePlaces;
new gRaceStartingCount;
new gRaceTimer;
new bool:gRaceMapReady;

forward Race_Tick();

stock bool:Race_IsParticipant(playerid)
{
    return gRaceJoined[playerid];
}

stock Race_Broadcast(const message[])
{
    for (new playerid = 0; playerid < MAX_PLAYERS; playerid++)
        if (gRaceJoined[playerid] && IsPlayerConnected(playerid))
            SendClientMessage(playerid, RACE_COLOR, message);
    return 1;
}

stock Race_SetNextCheckpoint(playerid)
{
    new next = gRaceProgress[playerid];
    if (next < 0 || next >= RACE_STAGE_COUNT) return 0;
    new following = next + 1;
    if (following >= RACE_STAGE_COUNT) following = next;
    SetPlayerRaceCheckpoint(playerid,
        (next == RACE_STAGE_COUNT - 1) ? CP_TYPE_GROUND_FINISH : CP_TYPE_GROUND_NORMAL,
        gRacePoints[next][0], gRacePoints[next][1], gRacePoints[next][2],
        gRacePoints[following][0], gRacePoints[following][1], gRacePoints[following][2],
        RACE_CHECKPOINT_RADIUS);
    return 1;
}

stock Race_ResetSession()
{
    gRaceMode = RACE_IDLE;
    gRaceStartAt = 0;
    gRaceFinishAt = 0;
    gRacePlaces = 0;
    gRaceStartingCount = 0;
    return 1;
}

stock Race_ReconcileSession()
{
    if (gRaceCount == 0) return Race_ResetSession();
    if (gRaceMode == RACE_WAIT_START && gRaceCount < RACE_MIN_PLAYERS)
    {
        gRaceMode = RACE_IDLE;
        gRaceStartAt = 0;
        Race_Broadcast("Отсчёт отменён: недостаточно участников.");
    }
    return 1;
}

// Прежде всего снять членство: DestroyVehicle может вызвать смену состояния игрока.
stock Race_Remove(playerid, bool:restore = true)
{
    if (!gRaceJoined[playerid]) return 0;
    gRaceJoined[playerid] = false;
    RaceAudio_Stop(playerid);
    RaceEvent_Unload(playerid);
    DisablePlayerRaceCheckpoint(playerid);
    if (gRaceSlots[playerid] >= 0 && gRaceSlots[playerid] < RACE_MAX_PLAYERS)
        gRaceSlotTaken[gRaceSlots[playerid]] = false;
    gRaceSlots[playerid] = -1;
    if (gRaceCars[playerid] != INVALID_VEHICLE_ID)
    {
        DestroyVehicle(gRaceCars[playerid]);
        gRaceCars[playerid] = INVALID_VEHICLE_ID;
    }
    RaceMap_Unload(playerid);
    if (restore && IsPlayerConnected(playerid))
    {
        SetPlayerInterior(playerid, gRaceReturnInterior[playerid]);
        SetPlayerVirtualWorld(playerid, gRaceReturnWorld[playerid]);
        SetPlayerPos(playerid, gRaceReturn[playerid][0],
            gRaceReturn[playerid][1], gRaceReturn[playerid][2]);
        SetPlayerFacingAngle(playerid, gRaceReturn[playerid][3]);
        SetPlayerHealth(playerid, gRaceReturnHealth[playerid]);
        SetPlayerArmour(playerid, gRaceReturnArmour[playerid]);
        TogglePlayerControllable(playerid, !gAdminFrozen[playerid]);
        SetCameraBehindPlayer(playerid);
        Cleaner_SyncTrashLabels(playerid);
    }
    gRaceProgress[playerid] = 0;
    if (gRaceCount > 0) gRaceCount--;
    Race_ReconcileSession();
    return 1;
}

stock Race_StartCountdown()
{
    if (gRaceMode != RACE_IDLE || gRaceCount < RACE_MIN_PLAYERS) return 0;
    gRaceMode = RACE_WAIT_START;
    gRaceStartAt = gettime() + RACE_COUNTDOWN;
    Race_Broadcast("Регистрация принята. Старт через 15 секунд!");
    return 1;
}

stock Race_Join(playerid)
{
    if (gAuthState[playerid] != AUTH_READY || !gAccountSpawned[playerid] ||
        gAccountDead[playerid] || GetPlayerState(playerid) != PLAYER_STATE_ONFOOT)
        return SendClientMessage(playerid, COLOR_RED, "Для участия нужно быть живым и выйти из транспорта.");
    if (gRaceJoined[playerid])
        return SendClientMessage(playerid, COLOR_GREY, "Вы уже зарегистрированы. Выход: /race leave.");
    if (!gRaceMapReady)
        return SendClientMessage(playerid, COLOR_RED, "Гоночная трасса недоступна.");
    if (gRaceMode == RACE_IN_PROGRESS)
        return SendClientMessage(playerid, COLOR_RED, "Гонка идёт. Дождитесь следующего заезда.");
    if (gRaceCount >= RACE_MAX_PLAYERS)
        return SendClientMessage(playerid, COLOR_RED, "На стартовой решётке нет мест.");
    if (GetPlayerVirtualWorld(playerid) != 0 || GetPlayerInterior(playerid) != 0 ||
        gCleanerDuty[playerid] || gCleanerBag[playerid] > 0 ||
        gAdminFrozen[playerid])
        return SendClientMessage(playerid, COLOR_RED, "Завершите текущую работу или событие перед гонкой.");

    new slot = -1;
    for (new i = 0; i < RACE_MAX_PLAYERS; i++)
        if (!gRaceSlotTaken[i]) { slot = i; break; }
    if (slot < 0) return 0;

    // Проверяем существование общей динамической геометрии до телепорта.
    if (!RaceMap_Load(playerid))
        return SendClientMessage(playerid, COLOR_RED, "Ошибка загрузки трассы.");
    if (!RaceEvent_Load(playerid))
    {
        RaceMap_Unload(playerid);
        return SendClientMessage(playerid, COLOR_RED,
            "Не удалось создать подвижные препятствия.");
    }
    // Пробное воспроизведение прекращается до участия в гонке.
    RaceAudio_Stop(playerid);
    new Float:x = RACE_MAP_X + 2.0 - float(slot / 2) * 8.0;
    new Float:y = RACE_MAP_Y - 4.0 + float(slot % 2) * 8.0;
    new vehicle = CreateVehicle(RACE_VEHICLE_MODEL, x, y,
        RACE_MAP_SURFACE_Z + 1.6, 90.0, -1, -1, -1);
    if (vehicle == INVALID_VEHICLE_ID)
    {
        RaceEvent_Unload(playerid);
        RaceMap_Unload(playerid);
        return SendClientMessage(playerid, COLOR_RED, "Не удалось создать гоночный автомобиль.");
    }

    GetPlayerPos(playerid, gRaceReturn[playerid][0],
        gRaceReturn[playerid][1], gRaceReturn[playerid][2]);
    GetPlayerFacingAngle(playerid, gRaceReturn[playerid][3]);
    GetPlayerHealth(playerid, gRaceReturnHealth[playerid]);
    GetPlayerArmour(playerid, gRaceReturnArmour[playerid]);
    Auth_CaptureVitals(playerid);
    gRaceReturnWorld[playerid] = GetPlayerVirtualWorld(playerid);
    gRaceReturnInterior[playerid] = GetPlayerInterior(playerid);

    gRaceJoined[playerid] = true;
    gRaceCars[playerid] = vehicle;
    gRaceSlots[playerid] = slot;
    gRaceSlotTaken[slot] = true;
    gRaceProgress[playerid] = 0;
    gRaceCount++;
    SetVehicleVirtualWorld(vehicle, RACE_WORLD_ID);
    SetPlayerInterior(playerid, 0);
    SetPlayerVirtualWorld(playerid, RACE_WORLD_ID);
    SetPlayerPos(playerid, x, y, RACE_MAP_SURFACE_Z + 2.6);
    PutPlayerInVehicle(playerid, vehicle, 0);
    // Принудительное обновление после смены мира и координат перед стартом.
    Streamer_Update(playerid);
    TogglePlayerControllable(playerid, false);

    new message[112];
    format(message, sizeof(message), "Гоночный мир %d. Место %d/%d. Выход: /race leave.",
        RACE_WORLD_ID, gRaceCount, RACE_MAX_PLAYERS);
    SendClientMessage(playerid, RACE_COLOR, message);
    Race_StartCountdown();
    return 1;
}

stock Race_Status(playerid)
{
    new message[112];
    if (gRaceMode == RACE_IN_PROGRESS)
        format(message, sizeof(message), "Гонка идёт, участников: %d. Ожидайте нового заезда.", gRaceCount);
    else if (gRaceMode == RACE_WAIT_START)
        format(message, sizeof(message), "Старт через %d с; участников %d/%d.",
            gRaceStartAt - gettime(), gRaceCount, RACE_MAX_PLAYERS);
    else
        format(message, sizeof(message), "Набор на гонку: %d/%d. Старт при %d участниках.",
            gRaceCount, RACE_MAX_PLAYERS, RACE_MIN_PLAYERS);
    SendClientMessage(playerid, RACE_COLOR, message);
    return 1;
}

stock Race_Start()
{
    if (gRaceMode != RACE_WAIT_START || gRaceCount < RACE_MIN_PLAYERS) return 0;
    gRaceMode = RACE_IN_PROGRESS;
    gRaceStartAt = gettime();
    gRaceFinishAt = gRaceStartAt + RACE_TIME_LIMIT;
    gRacePlaces = 0;
    gRaceStartingCount = gRaceCount;
    RaceAudio_OnRaceStart();
    Race_Broadcast("СТАРТ! Эстакада Hunter Quarry. Не пропускайте чекпоинты!");
    for (new playerid = 0; playerid < MAX_PLAYERS; playerid++)
    {
        if (!gRaceJoined[playerid] || !IsPlayerConnected(playerid)) continue;
        gRaceProgress[playerid] = 0;
        if (!RaceProgress_OnStart(playerid))
        {
            SendClientMessage(playerid, COLOR_RED,
                "Старт не сохранён в аккаунт. Регистрация отменена.");
            Race_Remove(playerid);
            continue;
        }
        Race_SetNextCheckpoint(playerid);
        TogglePlayerControllable(playerid, true);
        RaceAudio_Begin(playerid);
    }
    return 1;
}

public Race_Tick()
{
    RaceAudio_PreviewTick();
    if (gRaceMode == RACE_IDLE) return 1;
    for (new playerid = 0; playerid < MAX_PLAYERS; playerid++)
    {
        if (!gRaceJoined[playerid]) continue;
        if (!IsPlayerConnected(playerid) ||
            GetPlayerVirtualWorld(playerid) != RACE_WORLD_ID ||
            GetPlayerState(playerid) != PLAYER_STATE_DRIVER ||
            GetPlayerVehicleID(playerid) != gRaceCars[playerid])
        {
            Race_Remove(playerid, IsPlayerConnected(playerid));
            continue;
        }
        if (gRaceMode == RACE_IN_PROGRESS)
        {
            RaceAudio_Tick(playerid, gettime() - gRaceStartAt);
            RaceEvent_Tick(playerid, gettime() - gRaceStartAt);
        }
        new Float:raceX, Float:raceY, Float:raceZ;
        GetPlayerPos(playerid, raceX, raceY, raceZ);
        if (raceZ < RACE_MAP_SURFACE_Z - 30.0)
        {
            SendClientMessage(playerid, COLOR_RED, "Вы упали с эстакады и выбыли.");
            Race_Remove(playerid);
        }
    }
    if (gRaceMode == RACE_WAIT_START)
    {
        new left = gRaceStartAt - gettime();
        if (left <= 0) Race_Start();
        else if (left <= 5)
        {
            new message[36];
            format(message, sizeof(message), "Старт через %d...", left);
            Race_Broadcast(message);
        }
    }
    else if (gRaceMode == RACE_IN_PROGRESS && gettime() >= gRaceFinishAt)
    {
        Race_Broadcast("Время заезда истекло. Возвращение в основной мир.");
        for (new playerid = 0; playerid < MAX_PLAYERS; playerid++)
            if (gRaceJoined[playerid]) Race_Remove(playerid);
    }
    return 1;
}

stock Race_OnCheckpoint(playerid)
{
    if (!gRaceJoined[playerid] || gRaceMode != RACE_IN_PROGRESS ||
        GetPlayerVirtualWorld(playerid) != RACE_WORLD_ID ||
        GetPlayerState(playerid) != PLAYER_STATE_DRIVER ||
        GetPlayerVehicleID(playerid) != gRaceCars[playerid]) return 0;
    new point = gRaceProgress[playerid];
    if (point < 0 || point >= RACE_STAGE_COUNT) return 0;
    new Float:dx, Float:dy, Float:dz;
    GetPlayerPos(playerid, dx, dy, dz);
    dx -= gRacePoints[point][0];
    dy -= gRacePoints[point][1];
    dz -= gRacePoints[point][2];
    if (dx * dx + dy * dy + dz * dz > RACE_VERIFY_SQ) return 0;

    // Смена точки выполняется до любых сообщений, повторный callback не засчитается.
    gRaceProgress[playerid]++;
    RaceEvent_OnCheckpoint(playerid, gRaceProgress[playerid]);
    if (gRaceProgress[playerid] < RACE_STAGE_COUNT)
    {
        Race_SetNextCheckpoint(playerid);
        return 1;
    }
    gRacePlaces++;
    new message[120], name[MAX_PLAYER_NAME];
    GetPlayerName(playerid, name, sizeof(name));
    format(message, sizeof(message), "%s финишировал: место %d, время %d с.",
        name, gRacePlaces, gettime() - gRaceStartAt);
    Race_Broadcast(message);
    RaceProgress_OnFinish(playerid, gRacePlaces, gRaceStartingCount,
        gettime() - gRaceStartAt);
    Race_Remove(playerid);
    return 1;
}

stock Race_OnDeath(playerid)
{
    if (!gRaceJoined[playerid]) return 0;
    gRaceRespawnRestore[playerid] = true;
    Race_Remove(playerid, false);
    return 1;
}

stock Race_OnSpawn(playerid)
{
    if (gRaceJoined[playerid]) Race_Remove(playerid, false);
    if (gRaceRespawnRestore[playerid])
    {
        gRaceRespawnRestore[playerid] = false;
        SetPlayerHealth(playerid, gAccountHealth[playerid]);
        SetPlayerArmour(playerid, gAccountArmour[playerid]);
    }
    return 1;
}

stock Race_OnStateChange(playerid, PLAYER_STATE:newstate, PLAYER_STATE:oldstate)
{
    if (gRaceJoined[playerid] &&
        oldstate == PLAYER_STATE_DRIVER && newstate != PLAYER_STATE_DRIVER)
    {
        SendClientMessage(playerid, COLOR_RED, "Вы покинули гоночную машину и выбыли из заезда.");
        Race_Remove(playerid);
    }
    return 1;
}

stock Race_OnConnect(playerid)
{
    gRaceJoined[playerid] = false;
    gRaceRespawnRestore[playerid] = false;
    gRaceCars[playerid] = INVALID_VEHICLE_ID;
    gRaceSlots[playerid] = -1;
    gRaceProgress[playerid] = 0;
    RaceEvent_ResetPlayer(playerid);
    RaceAudio_OnConnect(playerid);
    return 1;
}

stock Race_OnDisconnect(playerid)
{
    if (gRaceJoined[playerid]) Race_Remove(playerid, false);
    RaceAudio_OnDisconnect(playerid);
    return Race_OnConnect(playerid);
}

stock Race_Init()
{
    Race_ResetSession();
    RaceProgress_Init();
    gRaceMapReady = RaceMap_Init() != 0;
    RacePlaylist_Load();
    for (new i = 0; i < RACE_MAX_PLAYERS; i++) gRaceSlotTaken[i] = false;
    gRaceCount = 0;
    gRaceTimer = SetTimer("Race_Tick", 1000, true);
    return 1;
}

stock Race_Shutdown()
{
    if (gRaceTimer) KillTimer(gRaceTimer);
    gRaceTimer = 0;
    for (new playerid = 0; playerid < MAX_PLAYERS; playerid++)
        if (gRaceJoined[playerid]) Race_Remove(playerid, false);
    Race_ResetSession();
    RaceMap_Shutdown();
    RacePlaylist_Reset();
    gRaceMapReady = false;
    return 1;
}

cmd:race(playerid, params[])
{
    if (isnull(params))
        return SendClientMessage(playerid, RACE_COLOR, "Гонки: /race join, leave, status, stats, top.");
    if (!strcmp(params, "join", true)) return Race_Join(playerid);
    if (!strcmp(params, "status", true)) return Race_Status(playerid);
    if (!strcmp(params, "stats", true)) return RaceProgress_Stats(playerid);
    if (!strcmp(params, "top", true)) return RaceProgress_Leaders(playerid);
    if (!strcmp(params, "leave", true))
    {
        if (!gRaceJoined[playerid])
            return SendClientMessage(playerid, COLOR_GREY, "Вы не участвуете в гонке.");
        Race_Remove(playerid);
        SendClientMessage(playerid, RACE_COLOR, "Вы вернулись в основной мир.");
        return 1;
    }
    return SendClientMessage(playerid, RACE_COLOR, "Гонки: /race join, leave, status, stats, top.");
}

cmd:raceaudio(playerid, params[])
{
    return RaceAudio_Command(playerid, params, gRaceJoined[playerid],
        gRaceMode == RACE_IN_PROGRESS,
        gRaceMode == RACE_IN_PROGRESS ? (gettime() - gRaceStartAt) : 0);
}
