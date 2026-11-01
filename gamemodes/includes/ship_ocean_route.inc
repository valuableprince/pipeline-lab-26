#if defined _tarkenso_ship_ocean_route_included
    #endinput
#endif
#define _tarkenso_ship_ocean_route_included

// SHIP-LS-OCEAN-01: opt-in trial ONLY. Two tangential quarter-circle turns.
// No production sailing schedule, no automatic docking, no file writes.
// The conventional +/-3000 map remains unchanged. Out-of-map water, scene
// streaming and collision must be verified in a real GTA/open.mp client.
#define SHIP_OCEAN_TURN_X          (3092.0)
#define SHIP_OCEAN_EAST_LANE_X     (3400.0)
#define SHIP_OCEAN_TURN_RADIUS     (308.0)
#define SHIP_OCEAN_SOUTH_TURN_Y    (-3100.0)
#define SHIP_OCEAN_SOUTH_LANE_Y    (-3400.0)
#define SHIP_OCEAN_WEST_LANE_X     (2400.0)
#define SHIP_OCEAN_TURN_STEPS      (18)
#define SHIP_OCEAN_HOLD_STEP       (19)
#define SHIP_OCEAN_FINAL_STEP      (39)
#define SHIP_OCEAN_ARRIVAL_EPS     (0.64)

// Shared active/hold flags are declared in ship_studio.inc for global guards.
new gShipOceanNext;
new Float:gShipOceanTurnY;
new Float:gShipOceanBaseYaw;
new Float:gShipOceanRootZ;

stock bool:ShipOcean_TrialCoord(Float:x, Float:y, Float:z)
{
    return gShipOceanProbe && gShipModel == SHIP_CARGO_ROOT_ID &&
        x >= -3000.0 && x <= SHIP_OCEAN_TEST_MAX_X &&
        y >= -3500.0 && y <= 3000.0 &&
        z >= -100.0 && z <= 1000.0;
}

stock ShipOcean_Stop()
{
    gShipOceanActive = false;
    gShipOceanHold = false;
    gShipOceanNext = 0;
    return 1;
}

// Each segment has a matching tangent heading. The root mesh needs the
// navigation-to-mesh 90 degree offset already encoded in the berth yaw.
// Step 0: along the berth's existing straight eastward course.
// Steps 1..18: clockwise arc east -> south, end X=3400.
// Step 19: straight south to a safe *inspection stop* at Y=-2980.
// Step 20: continue south at X=3400 to Y=-3100 (after explicit resume).
// Steps 21..38: tangential south -> west arc to Y=-3400.
// Step 39: straight offshore west at constant Y=-3400.
stock bool:ShipOcean_Target(step, &Float:x, &Float:y,
    &Float:z, &Float:yaw, &Float:speed)
{
    z = gShipOceanRootZ;
    if (step == 0)
    {
        x = SHIP_OCEAN_TURN_X;
        y = gShipOceanTurnY;
        yaw = gShipOceanBaseYaw;
        speed = 2.0;
        return true;
    }
    if (step >= 1 && step <= SHIP_OCEAN_TURN_STEPS)
    {
        new Float:t = float(step) * 90.0 / float(SHIP_OCEAN_TURN_STEPS);
        x = SHIP_OCEAN_TURN_X + SHIP_OCEAN_TURN_RADIUS * floatsin(t, degrees);
        y = gShipOceanTurnY - SHIP_OCEAN_TURN_RADIUS *
            (1.0 - floatcos(t, degrees));
        yaw = gShipOceanBaseYaw - t;
        speed = 2.3;
        return true;
    }
    if (step == SHIP_OCEAN_HOLD_STEP || step == 20)
    {
        x = SHIP_OCEAN_EAST_LANE_X;
        y = step == SHIP_OCEAN_HOLD_STEP ? (-2980.0) : SHIP_OCEAN_SOUTH_TURN_Y;
        yaw = gShipOceanBaseYaw - 90.0;
        speed = 3.0;
        return true;
    }
    if (step >= 21 && step <= 38)
    {
        new Float:t = float(step - 20) * 90.0 / float(SHIP_OCEAN_TURN_STEPS);
        new Float:r = 300.0;
        x = SHIP_OCEAN_EAST_LANE_X - r * (1.0 - floatcos(t, degrees));
        y = SHIP_OCEAN_SOUTH_TURN_Y - r * floatsin(t, degrees);
        yaw = gShipOceanBaseYaw - 90.0 - t;
        speed = 2.3;
        return true;
    }
    if (step == SHIP_OCEAN_FINAL_STEP)
    {
        x = SHIP_OCEAN_WEST_LANE_X;
        y = SHIP_OCEAN_SOUTH_LANE_Y;
        yaw = gShipOceanBaseYaw - 180.0;
        speed = SHIP_CRUISE_SPEED;
        return true;
    }
    return false;
}

stock ShipOcean_Pause()
{
    gShipOceanActive = false;
    gShipOceanHold = true;
    if (gShipOwner != INVALID_PLAYER_ID && IsPlayerConnected(gShipOwner))
        SendClientMessage(gShipOwner, SHIP_COLOR,
            "[Tarkenso RPG] OCEAN: контрольная точка X=3400 Y=-2980. Проверьте воду, затем /ship oceanroute continue.");
    return 1;
}

stock ShipOcean_MoveNext()
{
    if (!gShipOceanActive || !Ship_Ready()) return 0;
    new Float:x, Float:y, Float:z, Float:yaw, Float:speed;
    while (ShipOcean_Target(gShipOceanNext, x, y, z, yaw, speed))
    {
        if (!ShipOcean_TrialCoord(x, y, z))
        {
            Ship_Stop();
            return 0;
        }
        new Float:cx, Float:cy, Float:cz;
        GetObjectPos(gShipRoot, cx, cy, cz);
        new Float:dx = cx - x, Float:dy = cy - y, Float:dz = cz - z;
        new step = gShipOceanNext++;
        if (dx*dx + dy*dy + dz*dz <= SHIP_OCEAN_ARRIVAL_EPS)
        {
            // Keep tangent yaw in sync even when the target was reached
            // before the previous segment's end-of-movement callback.
            SetObjectRot(gShipRoot, 0.0, 0.0, yaw);
            if (step == SHIP_OCEAN_HOLD_STEP) return ShipOcean_Pause();
            continue;
        }
        MoveObject(gShipRoot, x, y, z, speed, 0.0, 0.0, yaw);
        return 1;
    }

    if (gShipOceanNext == SHIP_OCEAN_HOLD_STEP + 1) return ShipOcean_Pause();
    gShipOceanActive = false;
    gShipOceanHold = false;
    if (gShipOwner != INVALID_PLAYER_ID && IsPlayerConnected(gShipOwner))
        SendClientMessage(gShipOwner, SHIP_COLOR,
            "[Tarkenso RPG] OCEAN: южная обходная трасса завершена на X=2400 Y=-3400. Корабль остановлен.");
    return 1;
}

stock ShipOcean_Tick()
{
    if (!gShipOceanActive) return 0;
    if (!Ship_Ready()) { ShipOcean_Stop(); return 0; }
    new Float:x, Float:y, Float:z, Float:tx, Float:ty, Float:tz;
    new Float:yaw, Float:speed;
    if (!ShipOcean_Target(gShipOceanNext - 1, tx, ty, tz, yaw, speed))
    {
        Ship_Stop();
        return 0;
    }
    GetObjectPos(gShipRoot, x, y, z);
    new Float:dx = x-tx, Float:dy = y-ty, Float:dz = z-tz;
    if (dx*dx + dy*dy + dz*dz <= SHIP_OCEAN_ARRIVAL_EPS)
    {
        if (gShipOceanNext - 1 == SHIP_OCEAN_HOLD_STEP)
            return ShipOcean_Pause();
        return ShipOcean_MoveNext();
    }
    return 1;
}

stock bool:ShipOcean_StartAllowed(playerid)
{
    if (!gShipOceanProbe || !Ship_Ready() ||
        gShipModel != SHIP_CARGO_ROOT_ID ||
        gShipRunning || gShipPortDeparture >= 0 ||
        gShipManeuverActive || gShipEditOwner != INVALID_PLAYER_ID ||
        gShipOceanActive || gShipOceanHold ||
        !gShipPortSaved[SHIP_PORT_LS][SHIP_PORT_BERTH])
        return false;
    if (ShipEditor_Occupied()) return false;
    return Ship_Lock(playerid);
}

stock ShipOcean_Command(playerid, const arguments[])
{
    if (!strcmp(arguments, "status", true))
    {
        new line[140];
        format(line, sizeof(line),
            "[Tarkenso RPG] OCEAN: active=%d, hold=%d, next=%d; X=3400 -> south -> Y=-3400 west.",
            _:gShipOceanActive, _:gShipOceanHold, gShipOceanNext);
        return SendClientMessage(playerid, SHIP_COLOR, line);
    }
    if (!strcmp(arguments, "stop", true))
    {
        Ship_Stop();
        return SendClientMessage(playerid, SHIP_COLOR,
            "[Tarkenso RPG] OCEAN: испытательный маршрут остановлен, положение не сохранено.");
    }
    if (!strcmp(arguments, "start", true))
    {
        if (!ShipOcean_StartAllowed(playerid))
            return SendClientMessage(playerid, ADMIN_ERROR,
                "[Tarkenso RPG] Нужен /ship oceanprobe on, LS BERTH, остановленное судно и свободный корпус.");
        new Float:bx = gShipPortCoord[SHIP_PORT_LS][SHIP_PORT_BERTH][0];
        new Float:by = gShipPortCoord[SHIP_PORT_LS][SHIP_PORT_BERTH][1];
        new Float:bz = gShipPortCoord[SHIP_PORT_LS][SHIP_PORT_BERTH][2];
        new Float:ba = gShipPortCoord[SHIP_PORT_LS][SHIP_PORT_BERTH][3];
        // This trial is calibrated to the confirmed LS freight berth.
        if (floatabs(bx - 2682.0) > 12.0 || floatabs(by + 2273.0) > 12.0 ||
            floatabs(bz - SHIP_CARGO_ROOT_Z) > 3.0 || floatabs(ba - 0.9) > 5.0)
            return SendClientMessage(playerid, ADMIN_ERROR,
                "[Tarkenso RPG] Стоянка LS отличается от утвержденной. Маршрут не запускается.");
        new Float:cx, Float:cy, Float:cz, Float:rx, Float:ry, Float:ca;
        GetObjectPos(gShipRoot, cx, cy, cz);
        GetObjectRot(gShipRoot, rx, ry, ca);
        new Float:turnY = by + (SHIP_OCEAN_TURN_X - bx) *
            floatsin(ba, degrees) / floatcos(ba, degrees);
        new Float:expectedY = by + (cx-bx) *
            floatsin(ba, degrees) / floatcos(ba, degrees);
        if (cx < bx-6.0 || cx > SHIP_OCEAN_TURN_X+6.0 ||
            floatabs(cy - expectedY) > 15.0 ||
            floatabs(cz - bz) > 3.0 || floatabs(ca - ba) > 6.0)
            return SendClientMessage(playerid, ADMIN_ERROR,
                "[Tarkenso RPG] Начните от сохраненной стоянки или восточного прямого подхода (X<=3098).");
        gShipOceanBaseYaw = ba;
        gShipOceanRootZ = bz;
        gShipOceanTurnY = turnY;
        gShipOceanNext = 0;
        gShipOceanActive = true;
        return ShipOcean_MoveNext();
    }
    if (!strcmp(arguments, "continue", true))
    {
        if (!gShipOceanHold || !gShipOceanProbe || gShipOceanActive ||
            !Ship_Ready() || gShipModel != SHIP_CARGO_ROOT_ID ||
            gShipRunning || gShipPortDeparture >= 0 ||
            gShipManeuverActive || gShipEditOwner != INVALID_PLAYER_ID ||
            ShipEditor_Occupied() || !Ship_Lock(playerid))
            return SendClientMessage(playerid, ADMIN_ERROR,
                "[Tarkenso RPG] Нужна контрольная остановка X=3400 Y=-2980 и свободный корпус.");
        new Float:x, Float:y, Float:z, Float:rx, Float:ry, Float:yaw;
        GetObjectPos(gShipRoot, x, y, z);
        GetObjectRot(gShipRoot, rx, ry, yaw);
        if (floatabs(x - SHIP_OCEAN_EAST_LANE_X) > 2.0 ||
            floatabs(y + 2980.0) > 2.0 ||
            floatabs(z - gShipOceanRootZ) > 2.0 ||
            floatabs(yaw - (gShipOceanBaseYaw - 90.0)) > 5.0)
            return SendClientMessage(playerid, ADMIN_ERROR,
                "[Tarkenso RPG] Положение изменилось после контрольной остановки. Возобновление запрещено.");
        gShipOceanNext = 20;
        gShipOceanHold = false;
        gShipOceanActive = true;
        return ShipOcean_MoveNext();
    }
    return SendClientMessage(playerid, ADMIN_ERROR,
        "[Tarkenso RPG] Использование: /ship oceanroute start|continue|status|stop.");
}
