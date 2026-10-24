#if defined _tarkenso_race_event_included
    #endinput
#endif
#define _tarkenso_race_event_included

// RACE-EVENT-01. Движущиеся препятствия на скоростном участке S5.
// Только персональные объекты, без глобальных CreateObject и без новых таймеров.
// Движение синхронизировано с временем заезда, не зависит от количества игроков.
#define RACE_EVENT_COUNT           (6)
#define RACE_EVENT_CYCLE_SEC       (8)
#define RACE_EVENT_MOVE_SPEED      (2.0)
#define RACE_EVENT_DRAW_DISTANCE   (200.0)
#define RACE_EVENT_MODEL           (3798)
#define RACE_EVENT_COLOR           (0xE6BB79FF)

// Широкий участок S5: X=690..790, Y=815..845, поверхность Z=165.
// Верх ящика ~167; крайние полосы, центральная полоса остается свободной.
// Последний элемент массива - амплитуда поперечного движения (ось Y).
new const Float:gRaceEventPositions[RACE_EVENT_COUNT][4] =
{
    {704.0, 821.0, 166.0,  3.0},
    {716.0, 839.0, 166.0, -3.0},
    {729.0, 821.0, 166.0,  3.0},
    {741.0, 839.0, 166.0, -3.0},
    {753.0, 821.0, 166.0,  3.0},
    {765.0, 839.0, 166.0, -3.0}
};

new gRaceEventObjects[MAX_PLAYERS][RACE_EVENT_COUNT];
new gRaceEventObjectCount[MAX_PLAYERS];
new gRaceEventPhase[MAX_PLAYERS];
new bool:gRaceEventWarned[MAX_PLAYERS];

stock RaceEvent_ResetPlayer(playerid)
{
    gRaceEventObjectCount[playerid] = 0;
    gRaceEventPhase[playerid] = -1;
    gRaceEventWarned[playerid] = false;
    return 1;
}

// Очистка выполняется до выхода из гонки, при финише, смерти и дисконнекте.
stock RaceEvent_Unload(playerid)
{
    if (IsPlayerConnected(playerid))
    {
        for (new i = 0; i < gRaceEventObjectCount[playerid]; i++)
        {
            if (IsValidPlayerObject(playerid, gRaceEventObjects[playerid][i]))
                DestroyPlayerObject(playerid, gRaceEventObjects[playerid][i]);
        }
    }
    return RaceEvent_ResetPlayer(playerid);
}

// Fail-closed: частичная загрузка не переводит игрока в виртуальный мир.
stock bool:RaceEvent_Load(playerid)
{
    if (!IsPlayerConnected(playerid)) return false;
    if (gRaceEventObjectCount[playerid] == RACE_EVENT_COUNT) return true;
    RaceEvent_Unload(playerid);

    for (new i = 0; i < RACE_EVENT_COUNT; i++)
    {
        new objectid = CreatePlayerObject(playerid, RACE_EVENT_MODEL,
            gRaceEventPositions[i][0], gRaceEventPositions[i][1],
            gRaceEventPositions[i][2], 0.0, 0.0, 0.0,
            RACE_EVENT_DRAW_DISTANCE);
        if (objectid == INVALID_OBJECT_ID)
        {
            RaceEvent_Unload(playerid);
            return false;
        }
        gRaceEventObjects[playerid][gRaceEventObjectCount[playerid]++] = objectid;
    }
    return true;
}

// Вызывается из существующего Race_Tick (раз в секунду).
// Позиции не зависят от кадровой частоты / задержек OnPlayerObjectMoved.
stock RaceEvent_Tick(playerid, elapsed)
{
    if (elapsed < 0 || gRaceEventObjectCount[playerid] != RACE_EVENT_COUNT)
        return 0;

    new phase = (elapsed / RACE_EVENT_CYCLE_SEC) % 2;
    if (phase == gRaceEventPhase[playerid]) return 1;
    gRaceEventPhase[playerid] = phase;
    for (new i = 0; i < RACE_EVENT_COUNT; i++)
    {
        // Фаза 0: ящики сближаются с центром полосы. Фаза 1: отступают.
        new Float:targetY = gRaceEventPositions[i][1];
        if (phase == 0) targetY += gRaceEventPositions[i][3];
        MovePlayerObject(playerid, gRaceEventObjects[playerid][i],
            gRaceEventPositions[i][0], targetY,
            gRaceEventPositions[i][2], RACE_EVENT_MOVE_SPEED);
    }
    return 1;
}

// При прохождении КП-9 гонщик предупреждается перед S5. Один раз за заезд.
stock RaceEvent_OnCheckpoint(playerid, nextCheckpoint)
{
    if (nextCheckpoint < 9 || gRaceEventWarned[playerid]) return 0;
    if (gRaceEventObjectCount[playerid] != RACE_EVENT_COUNT) return 0;
    gRaceEventWarned[playerid] = true;
    SendClientMessage(playerid, RACE_EVENT_COLOR,
        "ОПАСНАЯ ЗОНА: впереди подвижные ящики! Сбрасывайте скорость.");
    GameTextForPlayer(playerid, "~r~DANGER ~w~| MOVING OBSTACLES", 3500, 3);
    return 1;
}
