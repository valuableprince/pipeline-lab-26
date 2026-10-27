#if defined _tarkenso_ship_port_included
    #endinput
#endif
#define _tarkenso_ship_port_included

// SHIP-PORT-01. Geometry is recorded IN GAME, never guessed.
// This stage provides berth/exit storage and a NON-TELEPORTING outbound leg.
// Final arrival berthing and scheduling remain disabled until route clearance.
#define SHIP_PORT_FILE SHIP_DIR "/ports_01.dat"
#define SHIP_PORT_BERTH (0)
#define SHIP_PORT_EXIT  (1)
#define SHIP_PORT_LS    (0)
#define SHIP_PORT_SF    (1)
#define SHIP_PORT_SLOW_SPEED (1.0)
#define SHIP_PORT_EXIT_NEAR_ROUTE (350.0)
#define SHIP_PORT_MAX_APPROACH (600.0)
#define SHIP_PORT_ARRIVAL_EPS (0.64)

new bool:gShipPortSaved[2][2];
new Float:gShipPortCoord[2][2][4];

stock ShipPort_Save()
{
    new File:f = fopen(SHIP_PORT_FILE ".tmp", io_write);
    if (!f) return 0;
    new bool:ok = fwrite(f, "TARKENSO_PORT|1\n") > 0;
    new line[128];
    for (new i = 0; i < 2; i++)
    {
        for (new k = 0; k < 2; k++)
        {
            format(line, sizeof(line), "%d %.4f %.4f %.4f %.4f\n",
                _:gShipPortSaved[i][k], gShipPortCoord[i][k][0],
                gShipPortCoord[i][k][1], gShipPortCoord[i][k][2],
                gShipPortCoord[i][k][3]);
            if (fwrite(f, line) <= 0) ok = false;
        }
    }
    fclose(f);
    if (!ok)
    {
        fremove(SHIP_PORT_FILE ".tmp");
        return 0;
    }
    // A failed rename must not delete the previously persisted config.
    if (fexist(SHIP_PORT_FILE)) fremove(SHIP_PORT_FILE);
    if (!frename(SHIP_PORT_FILE ".tmp", SHIP_PORT_FILE))
    {
        return 0;
    }
    return 1;
}

stock ShipPort_Load()
{
    if (!fexist(SHIP_PORT_FILE)) return 1;
    new File:f = fopen(SHIP_PORT_FILE, io_read);
    if (!f) return 0;
    new header[48], line[128];
    new bool:ok = fread(f, header, sizeof(header)) != 0;
    if (ok) Auth_StripLine(header);
    if (ok && strcmp(header, "TARKENSO_PORT|1", false)) ok = false;
    for (new i = 0; ok && i < 2; i++)
    {
        for (new k = 0; ok && k < 2; k++)
        {
            new flag;
            new Float:x, Float:y, Float:z, Float:yaw;
            if (!fread(f, line, sizeof(line)) ||
                sscanf(line, "iffff", flag, x, y, z, yaw) ||
                (flag != 0 && flag != 1) || !Ship_ValidCoord(x, y, z) ||
                yaw < -360.0 || yaw > 720.0)
            {
                ok = false;
                continue;
            }
            gShipPortSaved[i][k] = bool:flag;
            gShipPortCoord[i][k][0] = x;
            gShipPortCoord[i][k][1] = y;
            gShipPortCoord[i][k][2] = z;
            gShipPortCoord[i][k][3] = yaw;
        }
    }
    fclose(f);
    if (!ok) print("[ShipPort] Invalid saved port configuration; use editor to record it again.");
    return ok;
}

stock ShipPort_Cancel()
{
    gShipPortDeparture = -1;
    return 1;
}

stock ShipPort_Status(playerid)
{
    new line[150];
    for (new i = 0; i < 2; i++)
    {
        format(line, sizeof(line),
            "[Tarkenso RPG] %s: berth=%s, exit=%s.",
            i == 0 ? ("Los Santos") : ("San Fierro"),
            gShipPortSaved[i][SHIP_PORT_BERTH] ? ("SET") : ("NOT SET"),
            gShipPortSaved[i][SHIP_PORT_EXIT] ? ("SET") : ("NOT SET"));
        SendClientMessage(playerid, SHIP_COLOR, line);
    }
    format(line, sizeof(line), "[Tarkenso RPG] Port departure: %s",
        gShipPortDeparture < 0 ? ("idle") :
        (gShipPortDeparture == SHIP_PORT_LS ? ("LS outbound") : ("SF outbound")));
    return SendClientMessage(playerid, SHIP_COLOR, line);
}

public ShipPort_Command(playerid, const params[])
{
    if (!Studio_Require(playerid)) return 1;
    new place[12], operation[16];
    if (isnull(params) || sscanf(params, "s[12]s[16]", place, operation))
    {
        SendClientMessage(playerid, SHIP_COLOR,
            "[Tarkenso RPG] /ship port ls|sf berth|exit|depart");
        return ShipPort_Status(playerid);
    }
    new port = -1;
    if (!strcmp(place, "ls", true)) port = SHIP_PORT_LS;
    else if (!strcmp(place, "sf", true)) port = SHIP_PORT_SF;
    if (port < 0) return SendClientMessage(playerid, ADMIN_ERROR,
        "[Tarkenso RPG] Порт: ls или sf.");
    if (!Ship_Ready()) return SendClientMessage(playerid, ADMIN_ERROR,
        "[Tarkenso RPG] Корабль ещё не создан.");
    if (gShipRunning || gShipPortDeparture >= 0 || gShipManeuverActive)
        return SendClientMessage(playerid, ADMIN_ERROR,
            "[Tarkenso RPG] Остановите корабль перед редактированием порта.");
    if (!strcmp(operation, "berth", true) || !strcmp(operation, "exit", true))
    {
        // Save the CURRENT ship transform, not the admin's location.
        new k = !strcmp(operation, "berth", true) ? SHIP_PORT_BERTH : SHIP_PORT_EXIT;
        if (!Ship_Lock(playerid)) return 1;
        new Float:x, Float:y, Float:z, Float:rx, Float:ry, Float:yaw;
        GetObjectPos(gShipRoot, x, y, z);
        GetObjectRot(gShipRoot, rx, ry, yaw);
        gShipPortCoord[port][k][0] = x;
        gShipPortCoord[port][k][1] = y;
        gShipPortCoord[port][k][2] = z;
        gShipPortCoord[port][k][3] = yaw;
        gShipPortSaved[port][k] = true;
        if (!ShipPort_Save())
            return SendClientMessage(playerid, ADMIN_ERROR,
                "[Tarkenso RPG] Координаты записаны в память, но сохранить файл не удалось.");
        return SendClientMessage(playerid, SHIP_COLOR,
            "[Tarkenso RPG] Положение корпуса сохранено. Проверьте зазоры в игре.");
    }
    if (!strcmp(operation, "depart", true))
    {
        if (gShipModel != SHIP_CARGO_ROOT_ID)
            return SendClientMessage(playerid, ADMIN_ERROR,
                "[Tarkenso RPG] Портовый режим поддерживает только грузовой корпус 9585.");
        if (!gShipPortSaved[port][SHIP_PORT_BERTH] ||
            !gShipPortSaved[port][SHIP_PORT_EXIT])
            return SendClientMessage(playerid, ADMIN_ERROR,
                "[Tarkenso RPG] Сначала запишите berth и exit для выбранного порта.");
        if (!Ship_Lock(playerid)) return 1;
        new Float:x, Float:y, Float:z;
        GetObjectPos(gShipRoot, x, y, z);
        new Float:bx = gShipPortCoord[port][SHIP_PORT_BERTH][0];
        new Float:by = gShipPortCoord[port][SHIP_PORT_BERTH][1];
        new Float:ex = gShipPortCoord[port][SHIP_PORT_EXIT][0];
        new Float:ey = gShipPortCoord[port][SHIP_PORT_EXIT][1];
        new Float:startDist = floatsqroot((x-bx)*(x-bx)+(y-by)*(y-by));
        new Float:exitDist = floatsqroot((ex-bx)*(ex-bx)+(ey-by)*(ey-by));
        new targetIndex = port == SHIP_PORT_LS ? 0 : sizeof(gShipPresetLsSf)-1;
        new Float:tx = gShipPresetLsSf[targetIndex][0];
        new Float:ty = gShipPresetLsSf[targetIndex][1];
        new Float:routeDist = floatsqroot((ex-tx)*(ex-tx)+(ey-ty)*(ey-ty));
        if (startDist > 8.0 || floatabs(z-gShipPortCoord[port][SHIP_PORT_BERTH][2]) > 3.0)
            return SendClientMessage(playerid, ADMIN_ERROR,
                "[Tarkenso RPG] Судно не на сохранённой стоянке (допуск 8 м).");
        if (exitDist < 10.0 || exitDist > SHIP_PORT_MAX_APPROACH ||
            routeDist > SHIP_PORT_EXIT_NEAR_ROUTE)
            return SendClientMessage(playerid, ADMIN_ERROR,
                "[Tarkenso RPG] Exit: 10–600 м от причала и до 350 м от начала морского пути.");
        if (!Ship_SetRoutePreset(port == SHIP_PORT_SF))
            return SendClientMessage(playerid, ADMIN_ERROR,
                "[Tarkenso RPG] Не удалось установить морской маршрут.");
        // Persist the destination only for a full outbound trip.
        // A configured, validated maneuver takes precedence over straight exit.
        if (gShipManeuverCount[port][SHIP_MANEUVER_DEPART] > 0)
        {
            if (!ShipManeuver_Start(port, SHIP_MANEUVER_DEPART, false))
                return SendClientMessage(playerid, ADMIN_ERROR,
                    "[Tarkenso RPG] Манёвр не запущен: проверьте точки и положение у причала.");
            gShipPortDeparture = port;
            gShipManeuverArrivePort = port == SHIP_PORT_LS ? SHIP_PORT_SF : SHIP_PORT_LS;
            ShipHorn_Play();
            return SendClientMessage(playerid, SHIP_COLOR,
                "[Tarkenso RPG] Запущен маршрут отхода: промежуточные точки, затем море.");
        }
        // Legacy direct exit remains for ports with no authored route.
        // NEVER snap the root to the route start: passengers are on board.
        StopObject(gShipRoot);
        gShipRunning = false;
        gShipPortDeparture = port;
        gShipManeuverArrivePort = port == SHIP_PORT_LS ? SHIP_PORT_SF : SHIP_PORT_LS;
        ShipHorn_Play();
        MoveObject(gShipRoot, ex, ey,
            gShipPortCoord[port][SHIP_PORT_EXIT][2], SHIP_PORT_SLOW_SPEED,
            0.0, 0.0, gShipPortCoord[port][SHIP_PORT_EXIT][3]);
        return SendClientMessage(playerid, SHIP_COLOR,
            "[Tarkenso RPG] Отход от причала начался (медленный ход). Без телепортации.");
    }
    return SendClientMessage(playerid, ADMIN_ERROR,
        "[Tarkenso RPG] Использование: /ship port ls|sf berth|exit|depart.");
}

public ShipPort_Tick()
{
    if (gShipPortDeparture < 0 || !Ship_Ready()) return 0;
    new p = gShipPortDeparture;
    new Float:x, Float:y, Float:z;
    GetObjectPos(gShipRoot, x, y, z);
    new Float:dx = x-gShipPortCoord[p][SHIP_PORT_EXIT][0];
    new Float:dy = y-gShipPortCoord[p][SHIP_PORT_EXIT][1];
    new Float:dz = z-gShipPortCoord[p][SHIP_PORT_EXIT][2];
    if (dx*dx+dy*dy+dz*dz > SHIP_PORT_ARRIVAL_EPS) return 1;
    // Crossing from the port manoeuvre to cruise reuses the existing engine.
    gShipPortDeparture = -1;
    gShipNextPoint = 0;
    gShipRunning = true;
    Ship_Advance();
    print("[ShipPort] Port exit reached; switched to cruise waypoints.");
    return 1;
}

stock ShipPort_Init()
{
    gShipPortDeparture = -1;
    for (new p = 0; p < 2; p++)
        for (new k = 0; k < 2; k++)
            gShipPortSaved[p][k] = false;
    ShipPort_Load();
    print("[ShipPort] Stage 1 available: manual berth and port departure.");
    return 1;
}
