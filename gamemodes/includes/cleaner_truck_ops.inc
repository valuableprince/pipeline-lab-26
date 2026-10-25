#if defined _tarkenso_cleaner_truck_ops_included
    #endinput
#endif
#define _tarkenso_cleaner_truck_ops_included

// JOB-TRUCK-02. Server-authoritative one-driver reservations, dedicated
// Trashmaster fleet and geometric reverse-parking validation.
// This stage never empties containers or credits money.
#define TRUCK_VEHICLE_MODEL (408)
#define TRUCK_BAYS (2)
#define TRUCK_TICK_MS (1500)
#define TRUCK_DOCK_MIN_SQ (9.0)
#define TRUCK_DOCK_MAX_SQ (64.0)
#define TRUCK_DOCK_HEIGHT_MAX (2.5)
#define TRUCK_DOCK_SPEED_SQ (0.0016)
#define TRUCK_DOCK_REAR_COS (0.80)

new gTruckVehicle[MAX_PLAYERS];
new gTruckBay[MAX_PLAYERS];
new gTruckBayOwner[TRUCK_BAYS];
// Approved Fort Carson municipal truck parking bays: X, Y, Z, heading.
new Float:gTruckSpawnPositions[TRUCK_BAYS][4] =
{
    {-233.7885, 1086.0983, 20.2871, 90.8476},
    {-233.7681, 1081.5675, 20.2994, 88.8619}
};
new gTruckClaimId[CLEANER_DUMP_LIMIT];
new gTruckClaimOwner[CLEANER_DUMP_LIMIT]; // playerid+1; zero is free
new gTruckReservedId[MAX_PLAYERS];
new bool:gTruckDockReady[MAX_PLAYERS];
new bool:gTruckDockHint[MAX_PLAYERS];
new gTruckTimer;
forward Truck_Tick();

stock Truck_ReleaseOrder(playerid)
{
    new id = gTruckReservedId[playerid];
    if (id > 0)
    {
        for (new i = 0; i < CLEANER_DUMP_LIMIT; i++)
        {
            if (gTruckClaimId[i] == id && gTruckClaimOwner[i] == playerid + 1)
            {
                gTruckClaimOwner[i] = 0;
                gTruckClaimId[i] = 0;
                break;
            }
        }
    }
    gTruckReservedId[playerid] = 0;
    gTruckDockReady[playerid] = false;
    gTruckDockHint[playerid] = false;
    if (IsPlayerConnected(playerid) && gTruckMapMarked[playerid])
        RemovePlayerMapIcon(playerid, TRUCK_MAP_SLOT);
    gTruckMapMarked[playerid] = false;
    return 1;
}

stock Truck_DestroyVehicle(playerid)
{
    new vehicleid = gTruckVehicle[playerid];
    gTruckVehicle[playerid] = INVALID_VEHICLE_ID;
    new bay = gTruckBay[playerid];
    gTruckBay[playerid] = -1;
    if (bay >= 0 && bay < TRUCK_BAYS && gTruckBayOwner[bay] == playerid + 1)
        gTruckBayOwner[bay] = 0;
    if (vehicleid != INVALID_VEHICLE_ID && IsValidVehicle(vehicleid))
        DestroyVehicle(vehicleid);
    return 1;
}

stock Truck_StopShift(playerid)
{
    Truck_ReleaseOrder(playerid);
    Truck_DestroyVehicle(playerid);
    gTruckOnShift[playerid] = false;
    gTruckDialogCount[playerid] = 0;
    return 1;
}

stock bool:Truck_IssueVehicle(playerid)
{
    if (!Cleaner_NearDepot(playerid) || gCleanerDataError[playerid] ||
        gTruckVehicle[playerid] != INVALID_VEHICLE_ID) return false;
    new bay = -1;
    for (new i = 0; i < TRUCK_BAYS; i++)
    {
        if (gTruckBayOwner[i]) continue;
        bay = i;
        break;
    }
    if (bay < 0) return false;

    // Use precisely configured positions; do not offset the supplied Z value.
    new vehicleid = CreateVehicle(TRUCK_VEHICLE_MODEL,
        gTruckSpawnPositions[bay][0], gTruckSpawnPositions[bay][1],
        gTruckSpawnPositions[bay][2], gTruckSpawnPositions[bay][3],
        1, 1, -1);
    if (vehicleid == INVALID_VEHICLE_ID) return false;
    SetVehicleVirtualWorld(vehicleid, 0);
    LinkVehicleToInterior(vehicleid, 0);
    gTruckVehicle[playerid] = vehicleid;
    gTruckBay[playerid] = bay;
    gTruckBayOwner[bay] = playerid + 1;
    return true;
}

stock bool:Truck_StartShift(playerid)
{
    if (gTruckOnShift[playerid] || gCleanerDuty[playerid] ||
        Cleaner_Rank(playerid) < 2 || !CleanerDump_IsReady() ||
        !Cleaner_NearDepot(playerid)) return false;
    if (!Truck_IssueVehicle(playerid)) return false;
    gTruckOnShift[playerid] = true;
    return true;
}

stock Truck_ReservedByOther(id, playerid)
{
    for (new i = 0; i < CLEANER_DUMP_LIMIT; i++)
        if (gTruckClaimId[i] == id && gTruckClaimOwner[i] &&
            gTruckClaimOwner[i] != playerid + 1) return 1;
    return 0;
}

// Return the live claimant's player ID, or -1 for an unreserved container.
// The reservation table stores playerid + 1 (zero means no claimant).
stock Truck_OrderOwner(id)
{
    for (new i = 0; i < CLEANER_DUMP_LIMIT; i++)
    {
        if (gTruckClaimId[i] != id || !gTruckClaimOwner[i]) continue;
        new owner = gTruckClaimOwner[i] - 1;
        if (owner >= 0 && owner < MAX_PLAYERS && IsPlayerConnected(owner) &&
            gTruckOnShift[owner] && gTruckReservedId[owner] == id)
            return owner;
    }
    return -1;
}

stock Truck_FindContainer(id)
{
    if (!CleanerDump_IsReady() || id <= 0) return -1;
    for (new i = 0; i < gCleanerDumpCount; i++)
        if (gCleanerDumpId[i] == id && gCleanerDumpEnabled[i] &&
            gCleanerDumpBin[i] + gCleanerDumpOverflow[i] >= TRUCK_ORDER_THRESHOLD)
            return i;
    return -1;
}

stock Truck_SetGPS(playerid, slot)
{
    if (gTruckMapMarked[playerid])
        RemovePlayerMapIcon(playerid, TRUCK_MAP_SLOT);
    SetPlayerMapIcon(playerid, TRUCK_MAP_SLOT,
        gCleanerDumpPos[slot][0], gCleanerDumpPos[slot][1], gCleanerDumpPos[slot][2],
        0, TRUCK_MAP_COLOR, MAPICON_LOCAL);
    gTruckMapMarked[playerid] = true;
    return 1;
}

// All claims are handled in the main Pawn event loop: the check and assignment
// cannot interleave. Jobs are NOT completed in stage 02 and are not persisted;
// restart clears all claims; disconnect/death/unemployment releases each claim.
stock Truck_ClaimOrder(playerid, id)
{
    if (!gTruckOnShift[playerid] ||
        gTruckVehicle[playerid] == INVALID_VEHICLE_ID ||
        !IsValidVehicle(gTruckVehicle[playerid])) return 0;
    new slot = Truck_FindContainer(id);
    if (slot < 0)
        return SendClientMessage(playerid, COLOR_RED, "Заказ недоступен или уже вывезен.");
    if (gTruckReservedId[playerid] && gTruckReservedId[playerid] != id)
        return SendClientMessage(playerid, COLOR_RED,
            "У вас уже есть заказ. /truckcancel - отказаться.");
    if (Truck_ReservedByOther(id, playerid))
        return SendClientMessage(playerid, COLOR_RED, "Заказ уже закреплён за другим водителем.");
    if (gTruckReservedId[playerid] == id)
    {
        Truck_SetGPS(playerid, slot);
        return SendClientMessage(playerid, CLEANER_COLOR, "Ваш заказ отмечен на карте.");
    }
    new free = -1;
    for (new i = 0; i < CLEANER_DUMP_LIMIT; i++)
        if (!gTruckClaimOwner[i]) { free = i; break; }
    if (free < 0)
        return SendClientMessage(playerid, COLOR_RED, "Лимит активных заказов достигнут.");
    gTruckClaimId[free] = id;
    gTruckClaimOwner[free] = playerid + 1;
    gTruckReservedId[playerid] = id;
    gTruckDockReady[playerid] = false;
    gTruckDockHint[playerid] = false;
    Truck_SetGPS(playerid, slot);
    new message[108];
    format(message, sizeof(message),
        "Заказ #%d закреплён за вами. Подайте мусоровоз задом к контейнеру.", id);
    return SendClientMessage(playerid, CLEANER_COLOR, message);
}

// A centered vehicle position within 3-8 m of the container, with the
// container in its REAR hemisphere (cos >= 0.8). Avoids checking front approach.
// GTA heading convention: front (-sin(heading), +cos(heading)).
stock bool:Truck_CanDock(playerid)
{
    if (gAuthState[playerid] != AUTH_READY || !gTruckOnShift[playerid] ||
        GetPlayerState(playerid) != PLAYER_STATE_DRIVER ||
        !IsPlayerConnected(playerid) || !gTruckReservedId[playerid] ||
        GetPlayerVirtualWorld(playerid) != 0 || GetPlayerInterior(playerid) != 0)
        return false;
    new vehicleid = gTruckVehicle[playerid];
    if (vehicleid == INVALID_VEHICLE_ID || !IsValidVehicle(vehicleid) ||
        GetPlayerVehicleID(playerid) != vehicleid ||
        GetVehicleModel(vehicleid) != TRUCK_VEHICLE_MODEL ||
        GetVehicleVirtualWorld(vehicleid) != 0) return false;
    new slot = Truck_FindContainer(gTruckReservedId[playerid]);
    if (slot < 0) return false;
    new Float:x, Float:y, Float:z, Float:angle;
    new Float:vx, Float:vy, Float:vz;
    if (!GetVehiclePos(vehicleid, x, y, z) ||
        !GetVehicleZAngle(vehicleid, angle) ||
        !GetVehicleVelocity(vehicleid, vx, vy, vz)) return false;
    if (vx*vx + vy*vy + vz*vz > TRUCK_DOCK_SPEED_SQ) return false;
    new Float:dx = gCleanerDumpPos[slot][0] - x;
    new Float:dy = gCleanerDumpPos[slot][1] - y;
    new Float:dz = gCleanerDumpPos[slot][2] - z;
    if (floatabs(dz) > TRUCK_DOCK_HEIGHT_MAX) return false;
    new Float:distanceSq = dx*dx + dy*dy;
    if (distanceSq < TRUCK_DOCK_MIN_SQ || distanceSq > TRUCK_DOCK_MAX_SQ)
        return false;
    new Float:backDot = dx * floatsin(angle, degrees) -
        dy * floatcos(angle, degrees);
    if (backDot < TRUCK_DOCK_REAR_COS * floatsqroot(distanceSq)) return false;
    return true;
}

stock Truck_ParkCommand(playerid)
{
    if (!gTruckOnShift[playerid])
        return SendClientMessage(playerid, COLOR_RED, "Вы не водитель муниципальной службы.");
    if (!gTruckReservedId[playerid])
        return SendClientMessage(playerid, COLOR_RED, "Сначала выберите заказ: /orders.");
    if (!Truck_CanDock(playerid))
    {
        gTruckDockReady[playerid] = false;
        return SendClientMessage(playerid, COLOR_RED,
            "Поставьте свой Trashmaster задом к контейнеру (3-8 м) и остановитесь.");
    }
    gTruckDockReady[playerid] = true;
    return SendClientMessage(playerid, CLEANER_COLOR,
        "Парковка подтверждена. Механизм погрузки появится в JOB-TRUCK-03.");
}

stock Truck_CancelCommand(playerid)
{
    if (!gTruckOnShift[playerid])
        return SendClientMessage(playerid, COLOR_RED, "Вы не водитель мусоровоза.");
    if (!gTruckReservedId[playerid])
        return SendClientMessage(playerid, COLOR_GREY, "У вас нет активного заказа.");
    Truck_ReleaseOrder(playerid);
    return SendClientMessage(playerid, CLEANER_COLOR, "Заказ освобождён. /orders - другие заявки.");
}

stock Truck_VehicleCommand(playerid)
{
    if (!gTruckOnShift[playerid])
        return SendClientMessage(playerid, COLOR_RED, "Вы не водитель мусоровоза.");
    if (gTruckVehicle[playerid] != INVALID_VEHICLE_ID &&
        IsValidVehicle(gTruckVehicle[playerid]))
        return SendClientMessage(playerid, COLOR_GREY, "Ваш мусоровоз уже выдан.");
    if (!Truck_IssueVehicle(playerid))
        return SendClientMessage(playerid, COLOR_RED,
            "Подойдите пешком к диспетчеру; нужна свободная стоянка.");
    return SendClientMessage(playerid, CLEANER_COLOR, "Выдан служебный Trashmaster.");
}

stock Truck_OnStateChange(playerid, PLAYER_STATE:newstate, PLAYER_STATE:oldstate)
{
    #pragma unused oldstate
    if (newstate != PLAYER_STATE_DRIVER)
    {
        gTruckDockReady[playerid] = false;
        return 1;
    }
    new vehicleid = GetPlayerVehicleID(playerid);
    // A municipal truck may only be driven by its assigned employee.
    for (new owner = 0; owner < MAX_PLAYERS; owner++)
    {
        if (gTruckVehicle[owner] != vehicleid) continue;
        if (owner == playerid && gTruckOnShift[playerid]) return 1;
        RemovePlayerFromVehicle(playerid);
        SendClientMessage(playerid, COLOR_RED,
            "Этот мусоровоз закреплён за другим работником.");
        return 1;
    }
    return 1;
}

stock Truck_OnVehicleDeath(vehicleid)
{
    for (new playerid = 0; playerid < MAX_PLAYERS; playerid++)
    {
        if (gTruckVehicle[playerid] != vehicleid) continue;
        gTruckVehicle[playerid] = INVALID_VEHICLE_ID;
        new bay = gTruckBay[playerid];
        gTruckBay[playerid] = -1;
        if (bay >= 0 && bay < TRUCK_BAYS && gTruckBayOwner[bay] == playerid + 1)
            gTruckBayOwner[bay] = 0;
        Truck_ReleaseOrder(playerid);
        if (IsPlayerConnected(playerid))
            SendClientMessage(playerid, COLOR_RED,
                "Мусоровоз уничтожен. Заказ снят. /truckveh у диспетчера.");
        return 1;
    }
    return 0;
}

public Truck_Tick()
{
    for (new playerid = 0; playerid < MAX_PLAYERS; playerid++)
    {
        if (!gTruckOnShift[playerid] || !IsPlayerConnected(playerid)) continue;
        if (gTruckReservedId[playerid] &&
            Truck_FindContainer(gTruckReservedId[playerid]) < 0)
        {
            Truck_ReleaseOrder(playerid);
            SendClientMessage(playerid, COLOR_GREY,
                "Заказ стал недоступен. Бронь отменена.");
            continue;
        }
        new bool:canDock = Truck_CanDock(playerid);
        if (!canDock)
        {
            gTruckDockHint[playerid] = false;
            gTruckDockReady[playerid] = false;
            continue;
        }
        if (!gTruckDockHint[playerid] && !gTruckDockReady[playerid])
        {
            SendClientMessage(playerid, CLEANER_COLOR,
                "Мусоровоз стоит правильно! Левый ALT или /truckpark.");
            gTruckDockHint[playerid] = true;
        }
    }
    return 1;
}

stock Truck_Init()
{
    for (new i = 0; i < TRUCK_BAYS; i++) gTruckBayOwner[i] = 0;
    for (new i = 0; i < CLEANER_DUMP_LIMIT; i++)
    {
        gTruckClaimId[i] = 0;
        gTruckClaimOwner[i] = 0;
    }
    for (new i = 0; i < MAX_PLAYERS; i++)
    {
        gTruckVehicle[i] = INVALID_VEHICLE_ID;
        gTruckBay[i] = -1;
        gTruckReservedId[i] = 0;
        gTruckOnShift[i] = false;
        gTruckMapMarked[i] = false;
        gTruckDockReady[i] = false;
        gTruckDockHint[i] = false;
    }
    gTruckTimer = SetTimer("Truck_Tick", TRUCK_TICK_MS, true);
    print("[JOB-TRUCK-02] Driver vehicles, reservations and rear-dock checks ready.");
    return 1;
}

stock Truck_Shutdown()
{
    if (gTruckTimer) KillTimer(gTruckTimer);
    gTruckTimer = 0;
    for (new i = 0; i < MAX_PLAYERS; i++)
        Truck_StopShift(i);
    return 1;
}
