// Runs the REAL ship_notice.inc in an isolated open.mp server with deterministic
// dummy route/object state. No databases, bots, network clients, or plugins.
#include <open.mp>
#pragma dynamic 16384

#define SHIP_PORT_BERTH (0)
#define SHIP_PORT_EXIT (1)
#define SHIP_PORT_LS (0)
#define SHIP_PORT_SF (1)
#define SHIP_MANEUVER_DEPART (0)
#define SHIP_MANEUVER_ARRIVE (1)
#define SHIP_MANEUVER_SPEED (1.0)
#define SHIP_CRUISE_SPEED (4.0)
#define SHIP_PORT_SLOW_SPEED (1.0)
#define SHIP_PORT_EXIT_NEAR_ROUTE (350.0)

new gShipRoot;
new gShipPointCount;
new Float:gShipPoint[80][3];
new bool:gShipPortSaved[2][2];
new Float:gShipPortCoord[2][2][4];
new gShipManeuverCount[2][2];
new Float:gShipManeuverPoint[2][2][16][3];

stock bool:Ship_Ready()
{
    return IsValidObject(gShipRoot);
}
stock bool:ShipManeuver_Validated(playerid, port, mode, bool:notify)
{
    #pragma unused playerid, port, mode, notify
    return true;
}

#include "ship_notice.inc"

main() {}
forward Perf_Run();

public OnGameModeInit()
{
    gShipRoot = CreateObject(971, 0.0, 0.0, 20.0, 0.0, 0.0, 0.0);
    if (!IsValidObject(gShipRoot))
    {
        print("PERF_ERROR: cannot create route origin object");
        return 0;
    }

    gShipPointCount = 60;
    for (new i; i < gShipPointCount; ++i)
    {
        gShipPoint[i][0] = float(i * 31);
        gShipPoint[i][1] = float((i % 11) * 17);
        gShipPoint[i][2] = 20.0;
    }
    for (new port; port < 2; ++port)
    {
        gShipPortSaved[port][SHIP_PORT_BERTH] = true;
        gShipPortSaved[port][SHIP_PORT_EXIT] = true;
        gShipPortCoord[port][SHIP_PORT_BERTH][0] = 20.0;
        gShipPortCoord[port][SHIP_PORT_BERTH][1] = 30.0;
        gShipPortCoord[port][SHIP_PORT_BERTH][2] = 20.0;
        gShipPortCoord[port][SHIP_PORT_EXIT][0] = 50.0;
        gShipPortCoord[port][SHIP_PORT_EXIT][1] = 50.0;
        gShipPortCoord[port][SHIP_PORT_EXIT][2] = 20.0;
        gShipManeuverCount[port][SHIP_MANEUVER_DEPART] = 8;
        gShipManeuverCount[port][SHIP_MANEUVER_ARRIVE] = 8;
        for (new i; i < 8; i++)
        {
            gShipManeuverPoint[port][SHIP_MANEUVER_DEPART][i][0] = float(i * 4);
            gShipManeuverPoint[port][SHIP_MANEUVER_DEPART][i][1] = float(i * 5);
            gShipManeuverPoint[port][SHIP_MANEUVER_DEPART][i][2] = 20.0;
            gShipManeuverPoint[port][SHIP_MANEUVER_ARRIVE][i][0] = float(i * 5);
            gShipManeuverPoint[port][SHIP_MANEUVER_ARRIVE][i][1] = float(i * 4);
            gShipManeuverPoint[port][SHIP_MANEUVER_ARRIVE][i][2] = 20.0;
        }
    }
    SetTimer("Perf_Run", 100, false);
    return 1;
}

public Perf_Run()
{
    new Float:sum;
    // JIT/server warmup, excluded from all measurements.
    for (new i; i < 300; i++)
        sum += ShipNotice_TravelSeconds(SHIP_PORT_LS);
    for (new i; i < 1000; i++)
        sum += ShipNotice_Distance(float(i), 0.0, 0.0, 10.0, 20.0, 30.0);

    for (new pass; pass < 7; pass++)
    {
        new start = GetTickCount();
        new Float:distanceSum = 0.0;
        for (new i; i < 75000; i++)
            distanceSum += ShipNotice_Distance(float(i % 500), 2.0, 4.0,
                                                100.0, 20.0, 10.0);
        new elapsed = GetTickCount() - start;
        printf("PERF_SAMPLE:name=distance,pass=%d,ms=%d,checksum=%d",
            pass, elapsed, floatround(distanceSum));

        start = GetTickCount();
        new Float:travelSum = 0.0;
        for (new i; i < 3500; i++)
            travelSum += ShipNotice_TravelSeconds(SHIP_PORT_LS);
        elapsed = GetTickCount() - start;
        printf("PERF_SAMPLE:name=voyage,pass=%d,ms=%d,checksum=%d",
            pass, elapsed, floatround(travelSum));
    }
    print("PERF_DONE:cases=2,samples=7");
    DestroyObject(gShipRoot);
    return 1;
}
