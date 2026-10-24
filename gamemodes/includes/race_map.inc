#if defined _tarkenso_race_map_included
    #endinput
#endif
#define _tarkenso_race_map_included

// STREAMER-01: общая динамическая геометрия только в мире гонок.
// RACE-MAP-01: авторская эстакада над Hunter Quarry (Bone County).
// Виртуальный мир назначает race_core.inc; объекты доступны только участникам.
// Только штатные GTA SA/SA-MP модели: 18765, 18766, 1633, 3798, 18761.
#define RACE_WORLD_ID            (1800)
#define RACE_MAP_STREAM_DISTANCE  (380.0)
#define RACE_MAP_X                (450.0)
#define RACE_MAP_Y                (780.0)
#define RACE_MAP_SURFACE_Z        (165.0)
#define RACE_MAP_OBJECT_Z         (162.5) // верх 18765 находится на 165.0
#define RACE_MAP_MAX_OBJECTS      (440)
#define RACE_MAP_GRID_X           (42)
#define RACE_MAP_GRID_Y           (24)
#define RACE_MAP_GRID_X_OFFSET    (6)
#define RACE_MAP_GRID_Y_OFFSET    (2)
#define RACE_MAP_DRAW_DISTANCE    (300.0)

enum E_RACE_MAP_OBJECT
{
    mapModel,
    Float:mapX,
    Float:mapY,
    Float:mapZ,
    Float:mapRX,
    Float:mapRY,
    Float:mapRZ
};

new gRaceMapObjects[RACE_MAP_MAX_OBJECTS][E_RACE_MAP_OBJECT];
new gRaceMapObjectCount;
new bool:gRaceMapTiles[RACE_MAP_GRID_X][RACE_MAP_GRID_Y];
new gRaceMapDynamicObjects[RACE_MAP_MAX_OBJECTS];
new gRaceMapDynamicCount;

stock bool:RaceMap_AddObject(model,
    Float:x, Float:y, Float:z, Float:rx = 0.0, Float:ry = 0.0, Float:rz = 0.0)
{
    if (gRaceMapObjectCount >= RACE_MAP_MAX_OBJECTS) return false;
    new index = gRaceMapObjectCount++;
    gRaceMapObjects[index][mapModel] = model;
    gRaceMapObjects[index][mapX] = x;
    gRaceMapObjects[index][mapY] = y;
    gRaceMapObjects[index][mapZ] = z;
    gRaceMapObjects[index][mapRX] = rx;
    gRaceMapObjects[index][mapRY] = ry;
    gRaceMapObjects[index][mapRZ] = rz;
    return true;
}

stock bool:RaceMap_Tile(gridX, gridY)
{
    new ix = gridX + RACE_MAP_GRID_X_OFFSET;
    new iy = gridY + RACE_MAP_GRID_Y_OFFSET;
    if (ix < 0 || ix >= RACE_MAP_GRID_X ||
        iy < 0 || iy >= RACE_MAP_GRID_Y) return false;
    if (gRaceMapTiles[ix][iy]) return true;
    if (!RaceMap_AddObject(18765, RACE_MAP_X + float(gridX) * 10.0,
        RACE_MAP_Y + float(gridY) * 10.0, RACE_MAP_OBJECT_Z)) return false;
    gRaceMapTiles[ix][iy] = true;
    return true;
}

stock bool:RaceMap_Deck(xMin, xMax, yMin, yMax)
{
    for (new x = xMin; x <= xMax; x++)
        for (new y = yMin; y <= yMax; y++)
            if (!RaceMap_Tile(x, y)) return false;
    return true;
}

stock bool:RaceMap_Build()
{
    gRaceMapObjectCount = 0;
    for (new x = 0; x < RACE_MAP_GRID_X; x++)
        for (new y = 0; y < RACE_MAP_GRID_Y; y++)
            gRaceMapTiles[x][y] = false;

    // S1: широкая стартовая эстакада и первый разгон.
    if (!RaceMap_Deck(-5, 13, -1, 1)) return false;
    // S2: восходящий с точки зрения плана серпантин: север, запад, север.
    if (!RaceMap_Deck(11, 13, 0, 9)) return false;
    if (!RaceMap_Deck(5, 13, 7, 9)) return false;
    if (!RaceMap_Deck(4, 6, 8, 17)) return false;
    // S3: 90-метровый узкий мост, одна полоса шириной 10 м.
    if (!RaceMap_Deck(5, 8, 15, 17)) return false;
    if (!RaceMap_Deck(9, 17, 16, 16)) return false;
    if (!RaceMap_Deck(18, 21, 15, 17)) return false;
    // S4: резкий спуск на плане, пересечение по разным точкам маршрута.
    if (!RaceMap_Deck(19, 21, 4, 16)) return false;
    // S5: длинная скоростная секция с трамплинами и сужением.
    if (!RaceMap_Deck(19, 34, 4, 6)) return false;
    // S6: северный разворот и финальный узкий мост.
    if (!RaceMap_Deck(32, 34, 5, 20)) return false;
    if (!RaceMap_Deck(24, 34, 18, 20)) return false;
    // Два визуально опасных кармана: средняя полоса остаётся проездной.
    // Настоящие разрывы дорожного полотна требуют физического UAT.
    // Барьеры на узких мостах: чередование защищённых и открытых кромок.
    for (new i = 9; i <= 17; i++)
    {
        new Float:x = RACE_MAP_X + float(i) * 10.0;
        new Float:y = RACE_MAP_Y + 160.0 + ((i % 2) ? 5.4 : -5.4);
        if (!RaceMap_AddObject(18766, x, y,
            RACE_MAP_SURFACE_Z - 0.6)) return false;
    }
    for (new i = 25; i <= 29; i++)
    {
        if (!RaceMap_AddObject(18766, RACE_MAP_X + float(i) * 10.0,
            RACE_MAP_Y + 185.4, RACE_MAP_SURFACE_Z - 0.6)) return false;
    }
    // На широких поворотах — отдельные ударные препятствия, не закрывающие путь.
    for (new i = 0; i < 3; i++)
    {
        if (!RaceMap_AddObject(3798, RACE_MAP_X + 45.0 + float(i) * 16.0,
            RACE_MAP_Y - 8.0, RACE_MAP_SURFACE_Z + 1.0)) return false;
        if (!RaceMap_AddObject(3798, RACE_MAP_X + 210.0,
            RACE_MAP_Y + 48.0 + float(i) * 9.0,
            RACE_MAP_SURFACE_Z + 1.0)) return false;
    }
    // Два каскадных трамплина: геометрия и направления проверяются в UAT.
    if (!RaceMap_AddObject(1633, RACE_MAP_X + 240.0,
        RACE_MAP_Y + 50.0, RACE_MAP_SURFACE_Z - 0.2, 0.0, 0.0, 90.0)) return false;
    if (!RaceMap_AddObject(1633, RACE_MAP_X + 295.0,
        RACE_MAP_Y + 50.0, RACE_MAP_SURFACE_Z - 0.2, 0.0, 0.0, 90.0)) return false;
    // Финишная арка (визуальный ориентир), сам финиш засчитывает race_core.
    if (!RaceMap_AddObject(18761, RACE_MAP_X + 280.0,
        RACE_MAP_Y + 190.0, RACE_MAP_SURFACE_Z + 0.2, 0.0, 0.0, 90.0)) return false;
    return true;
}

// Один комплект динамической геометрии на виртуальный мир, а не на игрока.
// При частичной неудаче откатываем все созданные объекты.
stock RaceMap_DestroyGeometry()
{
    for (new i = 0; i < gRaceMapDynamicCount; i++)
        if (IsValidDynamicObject(gRaceMapDynamicObjects[i]))
            DestroyDynamicObject(gRaceMapDynamicObjects[i]);
    gRaceMapDynamicCount = 0;
    return 1;
}

stock bool:RaceMap_CreateGeometry()
{
    RaceMap_DestroyGeometry();
    for (new i = 0; i < gRaceMapObjectCount; i++)
    {
        new objectid = CreateDynamicObject(
            gRaceMapObjects[i][mapModel],
            gRaceMapObjects[i][mapX], gRaceMapObjects[i][mapY],
            gRaceMapObjects[i][mapZ], gRaceMapObjects[i][mapRX],
            gRaceMapObjects[i][mapRY], gRaceMapObjects[i][mapRZ],
            RACE_WORLD_ID, 0, -1, RACE_MAP_STREAM_DISTANCE,
            RACE_MAP_DRAW_DISTANCE);
        if (!IsValidDynamicObject(objectid))
        {
            RaceMap_DestroyGeometry();
            return false;
        }
        gRaceMapDynamicObjects[gRaceMapDynamicCount++] = objectid;
    }
    return true;
}

// Освобождение участника не должно уничтожать общий маршрут для других.
stock RaceMap_Unload(playerid)
{
    if (playerid < 0 || playerid >= MAX_PLAYERS) return 0;
    return 1;
}

stock bool:RaceMap_Load(playerid)
{
    if (!IsPlayerConnected(playerid) || gRaceMapObjectCount == 0) return false;
    return gRaceMapDynamicCount == gRaceMapObjectCount;
}

stock RaceMap_Init()
{
    if (!RaceMap_Build() || !RaceMap_CreateGeometry())
    {
        RaceMap_DestroyGeometry();
        print("[RACE-MAP-01] Failed to create dynamic race track.");
        return 0;
    }
    printf("[RACE-MAP-01] Spawned %d dynamic track objects (world %d).",
        gRaceMapDynamicCount, RACE_WORLD_ID);
    return 1;
}

stock RaceMap_Shutdown()
{
    RaceMap_DestroyGeometry();
    gRaceMapObjectCount = 0;
    return 1;
}
