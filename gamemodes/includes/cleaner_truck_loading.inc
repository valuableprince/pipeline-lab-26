#if defined _tarkenso_cleaner_truck_loading_included
    #endinput
#endif
#define _tarkenso_cleaner_truck_loading_included

// JOB-TRUCK-03: animation is visual; every actual waste deduction uses
// an atomic MariaDB compare-and-swap AFTER the corresponding animation.
#define TRUCK_BAG_SLOT (8)
#define TRUCK_BAG_UNITS (4)
#define TRUCK_PHASE_IDLE (0)
#define TRUCK_PHASE_LIFT (1)
#define TRUCK_PHASE_TIP (2)
#define TRUCK_PHASE_RETURN (3)
#define TRUCK_PHASE_THROW_UP (4)
#define TRUCK_PHASE_THROW_IN (5)
#define TRUCK_LOAD_BAG_RADIUS_SQ (10.0)
#define TRUCK_LOAD_REAR_RADIUS_SQ (16.0)

new gTruckLoadPhase[MAX_PLAYERS];
new gTruckLoadEpoch[MAX_PLAYERS];
new gTruckLoadSlot[MAX_PLAYERS];
new gTruckCarrySlot[MAX_PLAYERS];
new gTruckCarryIndex[MAX_PLAYERS];
new gTruckHeldUnits[MAX_PLAYERS];
new gTruckLoadBinUnits[MAX_PLAYERS];
new gTruckThrowObject[MAX_PLAYERS];
forward TruckLoad_Step(playerid, epoch);

stock TruckLoad_InitPlayer(playerid)
{
    gTruckLoadEpoch[playerid]++;
    gTruckLoadPhase[playerid] = TRUCK_PHASE_IDLE;
    gTruckLoadSlot[playerid] = -1;
    gTruckCarrySlot[playerid] = -1;
    gTruckCarryIndex[playerid] = -1;
    gTruckHeldUnits[playerid] = 0;
    gTruckLoadBinUnits[playerid] = 0;
    gTruckThrowObject[playerid] = INVALID_OBJECT_ID;
    return 1;
}

stock TruckLoad_Reset(playerid)
{
    gTruckLoadEpoch[playerid]++; // Invalidate pending SetTimerEx callbacks.
    new slot = gTruckLoadSlot[playerid];
    gTruckLoadPhase[playerid] = TRUCK_PHASE_IDLE;
    gTruckLoadSlot[playerid] = -1;
    gTruckLoadBinUnits[playerid] = 0;
    if (slot >= 0 && slot < gCleanerDumpCount &&
        gCleanerDumpObject[slot] != INVALID_OBJECT_ID &&
        IsValidObject(gCleanerDumpObject[slot]))
    {
        StopObject(gCleanerDumpObject[slot]);
        SetObjectPos(gCleanerDumpObject[slot], gCleanerDumpPos[slot][0],
            gCleanerDumpPos[slot][1], gCleanerDumpPos[slot][2] - 0.85);
        SetObjectRot(gCleanerDumpObject[slot], 0.0, 0.0, gCleanerDumpPos[slot][3]);
    }
    if (gTruckThrowObject[playerid] != INVALID_OBJECT_ID)
    {
        if (IsValidObject(gTruckThrowObject[playerid]))
            DestroyObject(gTruckThrowObject[playerid]);
        gTruckThrowObject[playerid] = INVALID_OBJECT_ID;
    }
    if (gTruckHeldUnits[playerid] && IsPlayerConnected(playerid))
        RemovePlayerAttachedObject(playerid, TRUCK_BAG_SLOT);
    slot = gTruckCarrySlot[playerid];
    gTruckCarrySlot[playerid] = -1;
    gTruckCarryIndex[playerid] = -1;
    gTruckHeldUnits[playerid] = 0;
    if (slot >= 0 && slot < gCleanerDumpCount && gCleanerDumpReady)
        CleanerDump_Refresh(slot);
    return 1;
}

stock bool:TruckLoad_CurrentBusy(playerid)
{
    return gTruckLoadPhase[playerid] != TRUCK_PHASE_IDLE ||
        gTruckHeldUnits[playerid] > 0;
}

stock bool:TruckLoad_Parked(playerid)
{
    if (!gTruckOnShift[playerid] || !gTruckDockReady[playerid]) return false;
    new bay = gTruckBay[playerid];
    if (bay < 0 || bay >= TRUCK_BAYS ||
        gTruckBayOwner[bay] != playerid + 1) return false;
    new vehicleid = gTruckFleet[bay];
    if (!IsValidVehicle(vehicleid) || GetVehicleVirtualWorld(vehicleid) != 0)
        return false;
    new slot = Truck_FindContainer(gTruckReservedId[playerid]);
    if (slot < 0) return false;
    new Float:x, Float:y, Float:z, Float:angle, Float:vx, Float:vy, Float:vz;
    GetVehiclePos(vehicleid, x, y, z);
    GetVehicleZAngle(vehicleid, angle);
    GetVehicleVelocity(vehicleid, vx, vy, vz);
    if (vx*vx + vy*vy + vz*vz > TRUCK_DOCK_SPEED_SQ) return false;
    new Float:dx = gCleanerDumpPos[slot][0] - x;
    new Float:dy = gCleanerDumpPos[slot][1] - y;
    new Float:dz = gCleanerDumpPos[slot][2] - z;
    if (floatabs(dz) > TRUCK_DOCK_HEIGHT_MAX) return false;
    new Float:distanceSq = dx*dx + dy*dy;
    if (distanceSq < TRUCK_DOCK_MIN_SQ ||
        distanceSq > TRUCK_DOCK_MAX_SQ) return false;
    return dx * floatsin(angle, degrees) - dy * floatcos(angle, degrees)
        >= TRUCK_DOCK_REAR_COS * floatsqroot(distanceSq);
}

stock TruckLoad_CanLeaveVehicle(playerid)
{
    return TruckLoad_Parked(playerid);
}

stock TruckLoad_Notice(playerid)
{
    if (!gTruckOnShift[playerid]) return 0;
    new bay = gTruckBay[playerid], message[102];
    if (bay < 0 || bay >= TRUCK_BAYS) return 0;
    format(message, sizeof(message),
        "Кузов: %d/%d. /truckload, /truckbag, /truckthrow.",
        gTruckCargo[bay], TRUCK_CARGO_CAP);
    return SendClientMessage(playerid, CLEANER_COLOR, message);
}

// Completes only after a successful SQL compare-and-swap. Each success credits
// the corresponding static truck's cargo once in this server process.
stock bool:TruckLoad_Commit(playerid, slot, bool:overflow, amount)
{
    if (!CleanerDump_IsReady() || gCleanerDumpStaged || amount < 1 ||
        slot < 0 || slot >= gCleanerDumpCount ||
        !gCleanerDumpEnabled[slot] ||
        !gTruckOnShift[playerid] || gTruckReservedId[playerid] != gCleanerDumpId[slot] ||
        !TruckLoad_Parked(playerid)) return false;
    new bay = gTruckBay[playerid];
    if (bay < 0 || bay >= TRUCK_BAYS ||
        gTruckCargo[bay] + amount > TRUCK_CARGO_CAP) return false;
    new current = overflow ? gCleanerDumpOverflow[slot] : gCleanerDumpBin[slot];
    if (amount > current) return false;
    new query[330], changed;
    if (overflow)
        mysql_format(gDBPrimary, query, sizeof(query),
            "UPDATE cleaner_containers SET overflow_units=%d WHERE container_id=%d AND bin_units=%d AND overflow_units=%d AND enabled=1",
            current - amount, gCleanerDumpId[slot],
            gCleanerDumpBin[slot], gCleanerDumpOverflow[slot]);
    else
        mysql_format(gDBPrimary, query, sizeof(query),
            "UPDATE cleaner_containers SET bin_units=%d WHERE container_id=%d AND bin_units=%d AND overflow_units=%d AND enabled=1",
            current - amount, gCleanerDumpId[slot],
            gCleanerDumpBin[slot], gCleanerDumpOverflow[slot]);
    if (!DBPrimary_Execute(query, changed) || changed != 1) return false;
    if (overflow) gCleanerDumpOverflow[slot] -= amount;
    else gCleanerDumpBin[slot] -= amount;
    gTruckCargo[bay] += amount;
    CleanerDump_Refresh(slot);
    return true;
}

stock TruckLoad_FinishIfEmpty(playerid, slot)
{
    if (slot < 0 || slot >= gCleanerDumpCount ||
        gTruckReservedId[playerid] != gCleanerDumpId[slot]) return 1;
    if (gCleanerDumpBin[slot] || gCleanerDumpOverflow[slot]) return 1;
    Truck_ReleaseOrder(playerid);
    SendClientMessage(playerid, CLEANER_COLOR,
        "Площадка очищена! Груз в кузове; пункт разгрузки появится в следующем этапе.");
    return 1;
}

stock TruckLoad_Bin(playerid)
{
    if (!gTruckOnShift[playerid] || !gTruckReservedId[playerid])
        return SendClientMessage(playerid, COLOR_RED, "Сначала примите заказ /orders.");
    if (TruckLoad_CurrentBusy(playerid))
        return SendClientMessage(playerid, COLOR_GREY, "Сначала завершите текущую погрузку.");
    if (!Truck_CanDock(playerid) || !gTruckDockReady[playerid])
        return SendClientMessage(playerid, COLOR_RED,
            "Остановитесь задним бортом к контейнеру и подтвердите /truckpark.");
    new slot = Truck_FindContainer(gTruckReservedId[playerid]);
    if (slot < 0 || gCleanerDumpBin[slot] <= 0 ||
        gCleanerDumpObject[slot] == INVALID_OBJECT_ID ||
        !IsValidObject(gCleanerDumpObject[slot]))
        return SendClientMessage(playerid, COLOR_GREY, "Контейнер пуст или недоступен.");
    if (gCleanerDumpStaged)
        return SendClientMessage(playerid, COLOR_RED, "Контейнер занят операцией.");
    new bay = gTruckBay[playerid], amount = gCleanerDumpBin[slot];
    if (gTruckCargo[bay] + amount > TRUCK_CARGO_CAP)
        return SendClientMessage(playerid, COLOR_RED, "В кузове недостаточно свободного места.");
    gTruckLoadSlot[playerid] = slot;
    gTruckLoadBinUnits[playerid] = amount;
    gTruckLoadPhase[playerid] = TRUCK_PHASE_LIFT;
    new objectid = gCleanerDumpObject[slot];
    new travelMs = MoveObject(objectid, gCleanerDumpPos[slot][0],
        gCleanerDumpPos[slot][1], gCleanerDumpPos[slot][2] + 1.45,
        1.1, 0.0, 0.0, gCleanerDumpPos[slot][3]);
    if (travelMs <= 0)
    {
        TruckLoad_Reset(playerid);
        return SendClientMessage(playerid, COLOR_RED, "Не удалось поднять контейнер.");
    }
    SetTimerEx("TruckLoad_Step", travelMs + 150, false, "ii",
        playerid, gTruckLoadEpoch[playerid]);
    return SendClientMessage(playerid, CLEANER_COLOR,
        "Гидравлический захват поднимает контейнер...");
}

stock TruckLoad_Bag(playerid)
{
    if (!gTruckOnShift[playerid] || !gTruckReservedId[playerid] ||
        GetPlayerState(playerid) != PLAYER_STATE_ONFOOT ||
        GetPlayerVirtualWorld(playerid) != 0 || GetPlayerInterior(playerid) != 0 ||
        !TruckLoad_Parked(playerid) || TruckLoad_CurrentBusy(playerid))
        return SendClientMessage(playerid, COLOR_RED,
            "Сначала припаркуйте мусоровоз, выйдите и подойдите к мешку.");
    new slot = Truck_FindContainer(gTruckReservedId[playerid]);
    if (slot < 0 || gCleanerDumpOverflow[slot] <= 0)
        return SendClientMessage(playerid, COLOR_GREY, "Мешков возле контейнера нет.");
    new bay = gTruckBay[playerid];
    new units = gCleanerDumpOverflow[slot];
    if (units > TRUCK_BAG_UNITS) units = TRUCK_BAG_UNITS;
    if (gTruckCargo[bay] + units > TRUCK_CARGO_CAP)
        return SendClientMessage(playerid, COLOR_RED, "Кузов заполнен.");
    new Float:px, Float:py, Float:pz;
    GetPlayerPos(playerid, px, py, pz);
    for (new j = 0; j < CLEANER_DUMP_BAG_OBJECTS; j++)
    {
        if (gCleanerDumpBags[slot][j] == INVALID_OBJECT_ID ||
            !IsValidObject(gCleanerDumpBags[slot][j])) continue;
        new Float:dx = px - gCleanerDumpBagPos[slot][j][0];
        new Float:dy = py - gCleanerDumpBagPos[slot][j][1];
        new Float:dz = pz - gCleanerDumpBagPos[slot][j][2];
        if (dx*dx + dy*dy + dz*dz > TRUCK_LOAD_BAG_RADIUS_SQ) continue;
        DestroyObject(gCleanerDumpBags[slot][j]);
        gCleanerDumpBags[slot][j] = INVALID_OBJECT_ID;
        gTruckCarrySlot[playerid] = slot;
        gTruckCarryIndex[playerid] = j;
        gTruckHeldUnits[playerid] = units;
        SetPlayerAttachedObject(playerid, TRUCK_BAG_SLOT, 1265, 6,
            0.16, 0.02, -0.17, 0.0, -5.0, 90.0, 0.70, 0.70, 0.70);
        ApplyAnimation(playerid, "BOMBER", "BOM_Plant_Loop", 4.1,
            false, false, false, false, 1100, SYNC_ALL);
        return SendClientMessage(playerid, CLEANER_COLOR,
            "Мешок в руке. Подойдите к заднему борту и используйте /truckthrow.");
    }
    return SendClientMessage(playerid, COLOR_GREY, "Подойдите к видимому мешку.");
}

stock TruckLoad_Throw(playerid)
{
    if (!gTruckOnShift[playerid] || gTruckHeldUnits[playerid] <= 0 ||
        gTruckLoadPhase[playerid] != TRUCK_PHASE_IDLE ||
        GetPlayerState(playerid) != PLAYER_STATE_ONFOOT ||
        !TruckLoad_Parked(playerid))
        return SendClientMessage(playerid, COLOR_RED, "Вы не готовы к броску мешка.");
    new bay = gTruckBay[playerid];
    if (gTruckCargo[bay] + gTruckHeldUnits[playerid] > TRUCK_CARGO_CAP)
        return SendClientMessage(playerid, COLOR_RED, "В кузове недостаточно места.");
    new Float:x, Float:y, Float:z, Float:angle;
    GetVehiclePos(gTruckFleet[bay], x, y, z);
    GetVehicleZAngle(gTruckFleet[bay], angle);
    new Float:rearX = x + floatsin(angle, degrees) * 3.1;
    new Float:rearY = y - floatcos(angle, degrees) * 3.1;
    new Float:px, Float:py, Float:pz;
    GetPlayerPos(playerid, px, py, pz);
    new Float:dx = px - rearX, Float:dy = py - rearY;
    if (dx*dx + dy*dy > TRUCK_LOAD_REAR_RADIUS_SQ ||
        floatabs(pz - z) > 3.0)
        return SendClientMessage(playerid, COLOR_RED, "Подойдите к задней части мусоровоза.");
    new objectid = CreateObject(1265, px, py, pz + 0.5,
        0.0, 0.0, 0.0, 80.0);
    if (objectid == INVALID_OBJECT_ID)
        return SendClientMessage(playerid, COLOR_RED, "Ошибка создания мешка.");
    gTruckThrowObject[playerid] = objectid;
    RemovePlayerAttachedObject(playerid, TRUCK_BAG_SLOT);
    ApplyAnimation(playerid, "BASKETBALL", "BBALL_shoot", 4.1,
        false, false, false, false, 1000, SYNC_ALL);
    gTruckLoadPhase[playerid] = TRUCK_PHASE_THROW_UP;
    new flightMs = MoveObject(objectid, rearX, rearY, z + 2.5, 3.5);
    if (flightMs <= 0)
    {
        TruckLoad_Reset(playerid);
        return SendClientMessage(playerid, COLOR_RED, "Не удалось бросить мешок.");
    }
    SetTimerEx("TruckLoad_Step", flightMs + 150, false, "ii",
        playerid, gTruckLoadEpoch[playerid]);
    return SendClientMessage(playerid, CLEANER_COLOR, "Мешок летит в кузов...");
}

public TruckLoad_Step(playerid, epoch)
{
    if (playerid < 0 || playerid >= MAX_PLAYERS ||
        !IsPlayerConnected(playerid) || epoch != gTruckLoadEpoch[playerid] ||
        !gTruckOnShift[playerid]) return 1;
    new phase = gTruckLoadPhase[playerid];
    new slot = (phase >= TRUCK_PHASE_THROW_UP) ?
        gTruckCarrySlot[playerid] : gTruckLoadSlot[playerid];
    if (slot < 0 || slot >= gCleanerDumpCount ||
        gTruckReservedId[playerid] != gCleanerDumpId[slot] ||
        !TruckLoad_Parked(playerid))
    {
        TruckLoad_Reset(playerid);
        return 1;
    }
    new bay = gTruckBay[playerid];
    if (phase == TRUCK_PHASE_LIFT)
    {
        new Float:x, Float:y, Float:z;
        GetVehiclePos(gTruckFleet[bay], x, y, z);
        new Float:angle;
        GetVehicleZAngle(gTruckFleet[bay], angle);
        x += floatsin(angle, degrees) * 3.2;
        y -= floatcos(angle, degrees) * 3.2;
        new travelMs = MoveObject(gCleanerDumpObject[slot], x, y, z + 2.1,
            1.6, 110.0, 0.0, gCleanerDumpPos[slot][3]);
        if (travelMs <= 0) { TruckLoad_Reset(playerid); return 1; }
        gTruckLoadPhase[playerid] = TRUCK_PHASE_TIP;
        SetTimerEx("TruckLoad_Step", travelMs + 350, false, "ii", playerid, epoch);
        return 1;
    }
    if (phase == TRUCK_PHASE_TIP)
    {
        if (!TruckLoad_Commit(playerid, slot, false, gTruckLoadBinUnits[playerid]))
        {
            TruckLoad_Reset(playerid);
            SendClientMessage(playerid, COLOR_RED, "Погрузка отменена: данные контейнера изменились.");
            return 1;
        }
        gTruckLoadPhase[playerid] = TRUCK_PHASE_RETURN;
        new travelMs = MoveObject(gCleanerDumpObject[slot], gCleanerDumpPos[slot][0],
            gCleanerDumpPos[slot][1], gCleanerDumpPos[slot][2] - 0.85,
            2.1, 0.0, 0.0, gCleanerDumpPos[slot][3]);
        if (travelMs <= 0) { TruckLoad_Reset(playerid); return 1; }
        SetTimerEx("TruckLoad_Step", travelMs + 150, false, "ii", playerid, epoch);
        return 1;
    }
    if (phase == TRUCK_PHASE_RETURN)
    {
        TruckLoad_Reset(playerid);
        TruckLoad_Notice(playerid);
        if (gCleanerDumpOverflow[slot] > 0)
            SendClientMessage(playerid, CLEANER_COLOR,
                "Контейнер пуст. Выйдите и соберите мешки: /truckbag.");
        TruckLoad_FinishIfEmpty(playerid, slot);
        return 1;
    }
    if (phase == TRUCK_PHASE_THROW_UP)
    {
        if (gTruckThrowObject[playerid] == INVALID_OBJECT_ID ||
            !IsValidObject(gTruckThrowObject[playerid]))
        {
            TruckLoad_Reset(playerid);
            return 1;
        }
        new Float:x, Float:y, Float:z;
        GetVehiclePos(gTruckFleet[bay], x, y, z);
        new flightMs = MoveObject(gTruckThrowObject[playerid], x, y, z + 1.25, 3.0);
        if (flightMs <= 0) { TruckLoad_Reset(playerid); return 1; }
        gTruckLoadPhase[playerid] = TRUCK_PHASE_THROW_IN;
        SetTimerEx("TruckLoad_Step", flightMs + 150, false, "ii", playerid, epoch);
        return 1;
    }
    if (phase == TRUCK_PHASE_THROW_IN)
    {
        new units = gTruckHeldUnits[playerid];
        if (!TruckLoad_Commit(playerid, slot, true, units))
            SendClientMessage(playerid, COLOR_RED, "Погрузка мешка не подтверждена. Мусор сохранён.");
        else
            SendClientMessage(playerid, CLEANER_COLOR, "Мешок загружен.");
        TruckLoad_Reset(playerid);
        TruckLoad_Notice(playerid);
        TruckLoad_FinishIfEmpty(playerid, slot);
    }
    return 1;
}

stock TruckLoad_OnTick(playerid)
{
    if (!gTruckOnShift[playerid]) return 0;
    if (gTruckLoadPhase[playerid] == TRUCK_PHASE_IDLE &&
        gTruckDockReady[playerid] && !TruckLoad_Parked(playerid))
        gTruckDockReady[playerid] = false;
    return 1;
}

stock TruckLoad_OnVehicleDestroyed(vehicleid)
{
    #pragma unused vehicleid
    // The owning driver's reservation cleanup resets the moving bin and bag.
    return 1;
}

stock TruckLoad_HandleKey(playerid)
{
    if (!gTruckOnShift[playerid] || !gTruckReservedId[playerid]) return 0;
    if (GetPlayerState(playerid) == PLAYER_STATE_DRIVER)
    {
        if (!gTruckDockReady[playerid])
            return Truck_ParkCommand(playerid);
        return TruckLoad_Bin(playerid);
    }
    if (GetPlayerState(playerid) == PLAYER_STATE_ONFOOT)
    {
        if (gTruckHeldUnits[playerid]) return TruckLoad_Throw(playerid);
        return TruckLoad_Bag(playerid);
    }
    return 0;
}
