#if defined _tarkenso_ship_notice_included
    #endinput
#endif
#define _tarkenso_ship_notice_included

// SHIP-NOTICE-01: calculate an approximate real-time voyage duration from
// the actual saved route and the movement speeds used by the ship controller.
// No scheduled departures, persisted ETA or additional repeating timer.
// This routine can be reused when the departure trigger becomes automatic.

stock Float:ShipNotice_Distance(Float:ax, Float:ay, Float:az,
    Float:bx, Float:by, Float:bz)
{
    new Float:dx = bx - ax, Float:dy = by - ay, Float:dz = bz - az;
    return floatsqroot(dx * dx + dy * dy + dz * dz);
}

// Arrival docking only happens when its route is valid and the sea endpoint
// lies within the existing approach limit of the destination exit.
stock bool:ShipNotice_HasBerthArrival(port)
{
    if (gShipPointCount < 2 ||
        !gShipPortSaved[port][SHIP_PORT_EXIT] ||
        !gShipPortSaved[port][SHIP_PORT_BERTH] ||
        gShipManeuverCount[port][SHIP_MANEUVER_ARRIVE] < 1)
        return false;
    if (!ShipManeuver_Validated(-1, port, SHIP_MANEUVER_ARRIVE, false))
        return false;
    new last = gShipPointCount - 1;
    return ShipNotice_Distance(gShipPoint[last][0], gShipPoint[last][1],
        gShipPoint[last][2], gShipPortCoord[port][SHIP_PORT_EXIT][0],
        gShipPortCoord[port][SHIP_PORT_EXIT][1],
        gShipPortCoord[port][SHIP_PORT_EXIT][2]) <= SHIP_PORT_EXIT_NEAR_ROUTE;
}

stock Float:ShipNotice_TravelSeconds(port)
{
    if (!Ship_Ready() || gShipPointCount < 2) return 0.0;

    new Float:x, Float:y, Float:z;
    GetObjectPos(gShipRoot, x, y, z);
    new Float:duration = 0.0;

    // Berth -> saved intermediate departure points -> port exit.
    if (gShipManeuverCount[port][SHIP_MANEUVER_DEPART] > 0)
    {
        for (new i = 0; i < gShipManeuverCount[port][SHIP_MANEUVER_DEPART]; i++)
        {
            new Float:tx = gShipManeuverPoint[port][SHIP_MANEUVER_DEPART][i][0];
            new Float:ty = gShipManeuverPoint[port][SHIP_MANEUVER_DEPART][i][1];
            new Float:tz = gShipManeuverPoint[port][SHIP_MANEUVER_DEPART][i][2];
            duration += ShipNotice_Distance(x, y, z, tx, ty, tz) / SHIP_MANEUVER_SPEED;
            x = tx;
            y = ty;
            z = tz;
        }
        duration += ShipNotice_Distance(x, y, z,
            gShipPortCoord[port][SHIP_PORT_EXIT][0],
            gShipPortCoord[port][SHIP_PORT_EXIT][1],
            gShipPortCoord[port][SHIP_PORT_EXIT][2]) / SHIP_MANEUVER_SPEED;
    }
    else
    {
        duration += ShipNotice_Distance(x, y, z,
            gShipPortCoord[port][SHIP_PORT_EXIT][0],
            gShipPortCoord[port][SHIP_PORT_EXIT][1],
            gShipPortCoord[port][SHIP_PORT_EXIT][2]) / SHIP_PORT_SLOW_SPEED;
    }

    // Exit -> ocean waypoints, including the link to the first waypoint.
    x = gShipPortCoord[port][SHIP_PORT_EXIT][0];
    y = gShipPortCoord[port][SHIP_PORT_EXIT][1];
    z = gShipPortCoord[port][SHIP_PORT_EXIT][2];
    for (new i = 0; i < gShipPointCount; i++)
    {
        new Float:tx = gShipPoint[i][0];
        new Float:ty = gShipPoint[i][1];
        new Float:tz = gShipPoint[i][2];
        duration += ShipNotice_Distance(x, y, z, tx, ty, tz) / SHIP_CRUISE_SPEED;
        x = tx;
        y = ty;
        z = tz;
    }

    // Optional ocean endpoint -> destination exit -> berth route.
    // Without it the ETA refers only to the offshore route endpoint.
    new destination = port == SHIP_PORT_LS ? SHIP_PORT_SF : SHIP_PORT_LS;
    if (ShipNotice_HasBerthArrival(destination))
    {
        new Float:tx = gShipPortCoord[destination][SHIP_PORT_EXIT][0];
        new Float:ty = gShipPortCoord[destination][SHIP_PORT_EXIT][1];
        new Float:tz = gShipPortCoord[destination][SHIP_PORT_EXIT][2];
        duration += ShipNotice_Distance(x, y, z, tx, ty, tz) / SHIP_MANEUVER_SPEED;
        x = tx;
        y = ty;
        z = tz;
        for (new i = 0; i < gShipManeuverCount[destination][SHIP_MANEUVER_ARRIVE]; i++)
        {
            tx = gShipManeuverPoint[destination][SHIP_MANEUVER_ARRIVE][i][0];
            ty = gShipManeuverPoint[destination][SHIP_MANEUVER_ARRIVE][i][1];
            tz = gShipManeuverPoint[destination][SHIP_MANEUVER_ARRIVE][i][2];
            duration += ShipNotice_Distance(x, y, z, tx, ty, tz) / SHIP_MANEUVER_SPEED;
            x = tx;
            y = ty;
            z = tz;
        }
        duration += ShipNotice_Distance(x, y, z,
            gShipPortCoord[destination][SHIP_PORT_BERTH][0],
            gShipPortCoord[destination][SHIP_PORT_BERTH][1],
            gShipPortCoord[destination][SHIP_PORT_BERTH][2]) / SHIP_MANEUVER_SPEED;
    }
    return duration;
}

// Publish only for a successful, real port departure (not /ship run or preview).
stock ShipNotice_Depart(port)
{
    if (!Ship_Ready() || (port != SHIP_PORT_LS && port != SHIP_PORT_SF))
        return 0;
    new Float:seconds = ShipNotice_TravelSeconds(port);
    if (seconds <= 0.0) return 0;

    new travelMinutes = floatround(seconds / 60.0, floatround_ceil);
    if (travelMinutes < 1) travelMinutes = 1;
    new travelHours = travelMinutes / 60, travelRest = travelMinutes % 60;
    new hour, minute, second;
    gettime(hour, minute, second);
    new etaMinutes = (hour * 60 + minute + travelMinutes) % (24 * 60);
    new etaHour = etaMinutes / 60, etaMinute = etaMinutes % 60;

    new bool:toBerth = ShipNotice_HasBerthArrival(
        port == SHIP_PORT_LS ? SHIP_PORT_SF : SHIP_PORT_LS);
    new message[160];
    format(message, sizeof(message),
        "[Tarkenso RPG] Судоходство: из порта %s вышел грузовой корабль. Курс: %s.",
        port == SHIP_PORT_LS ? ("Лос-Сантоса") : ("Сан-Фиерро"),
        port == SHIP_PORT_LS ? ("Сан-Фиерро") : ("Лос-Сантос"));
    SendClientMessageToAll(SHIP_COLOR, message);
    format(message, sizeof(message),
        "[Tarkenso RPG] Путь ~%d ч %d мин; прибытие через ~%d ч %d мин, около %02d:%02d (время сервера).",
        travelHours, travelRest, travelHours, travelRest, etaHour, etaMinute);
    SendClientMessageToAll(SHIP_COLOR, message);
    if (!toBerth)
        SendClientMessageToAll(SHIP_COLOR,
            "[Tarkenso RPG] Прибытие рассчитано до рейда у порта: заход к причалу ещё не настроен.");
    return 1;
}
