#if defined _tarkenso_truck_persistence_included
    #endinput
#endif
#define _tarkenso_truck_persistence_included

// JOB-TRUCK-04: all persisted transfers and delivery payouts are synchronous,
// guarded InnoDB transactions. Never write local state before COMMIT succeeds.
#define TRUCK_UNLOAD_X (-233.7783)
#define TRUCK_UNLOAD_Y (1083.8329)
#define TRUCK_UNLOAD_Z (20.2933)
#define TRUCK_UNLOAD_RADIUS_SQ (64.0)
#define TRUCK_UNLOAD_MAP_SLOT (76)
#define TRUCK_DRIVER_MAX_XP (1000000)

new bool:gTruckPersistenceReady;
new gTruckCargoAccount[TRUCK_BAYS];
new gTruckCargoPay[TRUCK_BAYS];
new gTruckCargoXP[TRUCK_BAYS];
new gTruckCargoRevision[TRUCK_BAYS];
new Text3D:gTruckUnloadLabel = Text3D:INVALID_3DTEXT_ID;

stock TruckPersist_Rollback()
{
    new affected;
    DBPrimary_Execute("ROLLBACK", affected);
    return 0;
}

stock bool:TruckPersist_Init()
{
    gTruckPersistenceReady = false;
    if (!DBPrimary_IsReady()) return false;
    new rows;
    new Cache:c = mysql_query(gDBPrimary,
        "SELECT version FROM schema_migrations WHERE version=11 LIMIT 1");
    new bool:ok = c != Cache:0 && !mysql_errno(gDBPrimary) &&
        cache_get_row_count(rows) && rows == 1;
    if (c != Cache:0) cache_delete(c);
    if (!ok)
    {
        print("[TRUCK-04] Schema v11 missing. Driver shifts disabled until migration.");
        return false;
    }
    c = mysql_query(gDBPrimary,
        "SELECT bay_id,COALESCE(account_id,0) AS owner,cargo_units,pending_pay,pending_xp,revision FROM municipal_truck_cargo ORDER BY bay_id");
    ok = c != Cache:0 && !mysql_errno(gDBPrimary) &&
        cache_get_row_count(rows) && rows == TRUCK_BAYS;
    if (!ok)
    {
        if (c != Cache:0) cache_delete(c);
        print("[TRUCK-04] Invalid saved fleet state. Driver shifts disabled.");
        return false;
    }
    for (new i = 0; i < TRUCK_BAYS; i++)
    {
        new bay, owner, cargo, pay, xp, version;
        if (!cache_get_value_name_int(i, "bay_id", bay) ||
            !cache_get_value_name_int(i, "owner", owner) ||
            !cache_get_value_name_int(i, "cargo_units", cargo) ||
            !cache_get_value_name_int(i, "pending_pay", pay) ||
            !cache_get_value_name_int(i, "pending_xp", xp) ||
            !cache_get_value_name_int(i, "revision", version) ||
            bay != i || cargo < 0 || cargo > TRUCK_CARGO_CAP ||
            owner < 0 || pay < 0 || pay > 5000 || xp != cargo || version < 0 ||
            (cargo == 0 && (owner || pay || xp)) ||
            (cargo > 0 && (owner <= 0 || pay <= 0)))
        {
            cache_delete(c);
            print("[TRUCK-04] Corrupt saved truck cargo. Driver shifts disabled.");
            return false;
        }
        gTruckCargo[i] = cargo;
        gTruckCargoAccount[i] = owner;
        gTruckCargoPay[i] = pay;
        gTruckCargoXP[i] = xp;
        gTruckCargoRevision[i] = version;
    }
    cache_delete(c);
    gTruckPersistenceReady = true;
    gTruckUnloadLabel = Create3DTextLabel(
        "{A7D6B5}Временная муниципальная разгрузка\n{FFFFFF}Остановитесь и нажмите левый ALT",
        CLEANER_COLOR, TRUCK_UNLOAD_X, TRUCK_UNLOAD_Y,
        TRUCK_UNLOAD_Z + 1.3, 18.0, 0, false);
    print("[TRUCK-04] Persistent cargo and municipal unload enabled.");
    return true;
}

stock TruckPersist_Shutdown()
{
    if (gTruckUnloadLabel != Text3D:INVALID_3DTEXT_ID)
        Delete3DTextLabel(gTruckUnloadLabel);
    gTruckUnloadLabel = Text3D:INVALID_3DTEXT_ID;
    gTruckPersistenceReady = false;
    return 1;
}

stock bool:TruckPersist_LoadProfile(playerid)
{
    gTruckDriverXP[playerid] = 0;
    if (!gTruckPersistenceReady || !gDBPrimaryLoaded[playerid]) return false;
    new query[180], rows, xp;
    mysql_format(gDBPrimary, query, sizeof(query),
        "SELECT experience FROM municipal_driver_profiles WHERE account_id=%d LIMIT 1",
        gDBPrimaryAccountId[playerid]);
    new Cache:c = mysql_query(gDBPrimary, query);
    new bool:ok = c != Cache:0 && !mysql_errno(gDBPrimary) &&
        cache_get_row_count(rows);
    if (ok && rows == 1)
        ok = cache_get_value_name_int(0, "experience", xp) &&
            xp >= 0 && xp <= TRUCK_DRIVER_MAX_XP;
    else if (ok && rows == 0) xp = 0;
    else ok = false;
    if (c != Cache:0) cache_delete(c);
    if (ok) gTruckDriverXP[playerid] = xp;
    return ok;
}

stock bool:TruckPersist_CargoAllowed(playerid, bay)
{
    if (!gTruckPersistenceReady || !gDBPrimaryLoaded[playerid] ||
        bay < 0 || bay >= TRUCK_BAYS) return false;
    return !gTruckCargo[bay] ||
        gTruckCargoAccount[bay] == gDBPrimaryAccountId[playerid];
}

stock bool:TruckPersist_AddCargo(playerid, slot, bool:overflow, amount)
{
    new bay = gTruckBay[playerid];
    if (!gTruckPersistenceReady || !DBPrimary_IsReady() ||
        !gDBPrimaryLoaded[playerid] || slot < 0 || slot >= gCleanerDumpCount ||
        !TruckPersist_CargoAllowed(playerid, bay) ||
        gTruckCargo[bay] + amount > TRUCK_CARGO_CAP ||
        gTruckCargoRevision[bay] >= 2147483647 || amount <= 0)
        return false;

    new current = overflow ? gCleanerDumpOverflow[slot] : gCleanerDumpBin[slot];
    if (amount > current || (!overflow && gCleanerDumpOverflow[slot] > 0))
        return false;
    new pay = amount * (overflow ? TRUCK_OVERFLOW_RATE : TRUCK_BASE_RATE);
    new sql[440], affected;
    if (!DBPrimary_Execute("START TRANSACTION", affected)) return false;

    if (overflow)
        mysql_format(gDBPrimary, sql, sizeof(sql),
            "UPDATE cleaner_containers SET overflow_units=%d WHERE container_id=%d AND bin_units=%d AND overflow_units=%d AND enabled=1",
            current - amount, gCleanerDumpId[slot], gCleanerDumpBin[slot],
            gCleanerDumpOverflow[slot]);
    else
        mysql_format(gDBPrimary, sql, sizeof(sql),
            "UPDATE cleaner_containers SET bin_units=%d WHERE container_id=%d AND bin_units=%d AND overflow_units=%d AND enabled=1",
            current - amount, gCleanerDumpId[slot], gCleanerDumpBin[slot],
            gCleanerDumpOverflow[slot]);
    if (!DBPrimary_Execute(sql, affected) || affected != 1)
        return TruckPersist_Rollback(), false;

    mysql_format(gDBPrimary, sql, sizeof(sql),
        "UPDATE municipal_truck_cargo SET account_id=%d,cargo_units=cargo_units+%d,pending_pay=pending_pay+%d,pending_xp=pending_xp+%d,revision=revision+1 WHERE bay_id=%d AND revision=%d AND cargo_units=%d AND (account_id IS NULL OR account_id=%d) AND cargo_units+%d<=%d",
        gDBPrimaryAccountId[playerid], amount, pay, amount, bay,
        gTruckCargoRevision[bay], gTruckCargo[bay], gDBPrimaryAccountId[playerid],
        amount, TRUCK_CARGO_CAP);
    if (!DBPrimary_Execute(sql, affected) || affected != 1)
        return TruckPersist_Rollback(), false;
    if (!DBPrimary_Execute("COMMIT", affected))
        return TruckPersist_Rollback(), false;

    gTruckCargo[bay] += amount;
    gTruckCargoPay[bay] += pay;
    gTruckCargoXP[bay] += amount;
    gTruckCargoRevision[bay]++;
    gTruckCargoAccount[bay] = gDBPrimaryAccountId[playerid];
    if (overflow) gCleanerDumpOverflow[slot] -= amount;
    else gCleanerDumpBin[slot] -= amount;
    CleanerDump_Refresh(slot);
    Truck_UpdateCargoLabel(bay);
    return true;
}

stock bool:TruckPersist_NearDepot(playerid)
{
    if (GetPlayerState(playerid) != PLAYER_STATE_DRIVER ||
        GetPlayerVirtualWorld(playerid) != 0 || GetPlayerInterior(playerid) != 0)
        return false;
    new bay = gTruckBay[playerid];
    if (bay < 0 || bay >= TRUCK_BAYS ||
        gTruckVehicle[playerid] != gTruckFleet[bay] ||
        GetPlayerVehicleID(playerid) != gTruckFleet[bay] ||
        !IsValidVehicle(gTruckFleet[bay])) return false;
    new Float:x, Float:y, Float:z, Float:vx, Float:vy, Float:vz;
    GetVehiclePos(gTruckFleet[bay], x, y, z);
    GetVehicleVelocity(gTruckFleet[bay], vx, vy, vz);
    x -= TRUCK_UNLOAD_X; y -= TRUCK_UNLOAD_Y; z -= TRUCK_UNLOAD_Z;
    return x*x+y*y+z*z <= TRUCK_UNLOAD_RADIUS_SQ &&
        vx*vx+vy*vy+vz*vz <= TRUCK_DOCK_SPEED_SQ;
}

stock TruckPersist_ReturnGPS(playerid)
{
    if (!gTruckOnShift[playerid] || !gTruckPersistenceReady) return 0;
    new bay = gTruckBay[playerid];
    if (bay < 0 || bay >= TRUCK_BAYS || gTruckCargo[bay] <= 0) return 0;
    SetPlayerMapIcon(playerid, TRUCK_UNLOAD_MAP_SLOT,
        TRUCK_UNLOAD_X, TRUCK_UNLOAD_Y, TRUCK_UNLOAD_Z,
        0, 0x70D0FFFF, MAPICON_LOCAL);
    return SendClientMessage(playerid, CLEANER_COLOR,
        "Груз в кузове. Вернитесь на муниципальную базу и нажмите ALT для сдачи рейса.");
}

stock TruckPersist_Unload(playerid)
{
    if (!gTruckOnShift[playerid] || !gTruckPersistenceReady ||
        gAuthState[playerid] != AUTH_READY || !gDBPrimaryLoaded[playerid])
        return SendClientMessage(playerid, COLOR_RED, "Разгрузка недоступна.");
    new bay = gTruckBay[playerid];
    if (bay < 0 || bay >= TRUCK_BAYS ||
        !TruckPersist_CargoAllowed(playerid, bay) ||
        gTruckCargo[bay] <= 0)
        return SendClientMessage(playerid, COLOR_GREY, "В кузове нет вашего груза.");
    if (TruckLoad_CurrentBusy(playerid) || !TruckPersist_NearDepot(playerid))
        return SendClientMessage(playerid, COLOR_RED,
            "Остановите свой мусоровоз на муниципальной базе и нажмите ALT.");
    new pay = gTruckCargoPay[bay], units = gTruckCargoXP[bay], affected;
    if (pay <= 0 || units <= 0 ||
        pay > ACCOUNT_MAX_MONEY - gAccountMoney[playerid] ||
        gAccountTxnSerial[playerid] >= ACCOUNT_MAX_MONEY ||
        gTruckDriverXP[playerid] > TRUCK_DRIVER_MAX_XP - units ||
        gTruckCargoRevision[bay] >= 2147483647 ||
        gDBPrimaryVersion[playerid] >= 2147483647)
        return SendClientMessage(playerid, COLOR_RED, "Лимит выплат или опыта. Обратитесь к администрации.");

    new sql[560];
    if (!DBPrimary_Execute("START TRANSACTION", affected))
        return SendClientMessage(playerid, COLOR_RED, "Не удалось начать расчёт рейса.");
    mysql_format(gDBPrimary, sql, sizeof(sql),
        "UPDATE municipal_truck_cargo SET account_id=NULL,cargo_units=0,pending_pay=0,pending_xp=0,revision=revision+1 WHERE bay_id=%d AND revision=%d AND account_id=%d AND cargo_units=%d AND pending_pay=%d AND pending_xp=%d",
        bay, gTruckCargoRevision[bay], gDBPrimaryAccountId[playerid],
        gTruckCargo[bay], pay, units);
    if (!DBPrimary_Execute(sql, affected) || affected != 1)
    {
        TruckPersist_Rollback();
        return SendClientMessage(playerid, COLOR_RED, "Данные рейса изменились. Разгрузка отменена.");
    }
    mysql_format(gDBPrimary, sql, sizeof(sql),
        "UPDATE accounts SET balance=balance+%d,txn_serial=txn_serial+1,row_version=row_version+1 WHERE account_id=%d AND row_version=%d AND balance<=%d AND txn_serial<%d",
        pay, gDBPrimaryAccountId[playerid], gDBPrimaryVersion[playerid],
        ACCOUNT_MAX_MONEY-pay, ACCOUNT_MAX_MONEY);
    if (!DBPrimary_Execute(sql, affected) || affected != 1)
    {
        TruckPersist_Rollback();
        return SendClientMessage(playerid, COLOR_RED, "Баланс изменился. Разгрузка отменена.");
    }
    mysql_format(gDBPrimary, sql, sizeof(sql),
        "INSERT INTO municipal_driver_profiles(account_id,experience,delivered_units,total_earned,completed_runs) VALUES (%d,%d,%d,%d,1) ON DUPLICATE KEY UPDATE experience=experience+VALUES(experience),delivered_units=delivered_units+VALUES(delivered_units),total_earned=total_earned+VALUES(total_earned),completed_runs=completed_runs+1",
        gDBPrimaryAccountId[playerid], units, units, pay);
    if (!DBPrimary_Execute(sql, affected))
    {
        TruckPersist_Rollback();
        return SendClientMessage(playerid, COLOR_RED, "Не удалось сохранить опыт. Разгрузка отменена.");
    }
    mysql_format(gDBPrimary, sql, sizeof(sql),
        "INSERT INTO municipal_truck_deliveries(bay_id,account_id,cargo_units,salary,experience) VALUES (%d,%d,%d,%d,%d)",
        bay, gDBPrimaryAccountId[playerid], gTruckCargo[bay], pay, units);
    if (!DBPrimary_Execute(sql, affected) || affected != 1 ||
        !DBPrimary_Execute("COMMIT", affected))
    {
        TruckPersist_Rollback();
        return SendClientMessage(playerid, COLOR_RED, "Не удалось завершить рейс. Данные сохранены.");
    }
    // One committed transaction: no additional account save here.
    gAccountMoney[playerid] += pay;
    gAccountTxnSerial[playerid]++;
    gDBPrimaryVersion[playerid]++;
    gTruckDriverXP[playerid] += units;
    gTruckCargo[bay] = 0;
    gTruckCargoPay[bay] = 0;
    gTruckCargoXP[bay] = 0;
    gTruckCargoAccount[bay] = 0;
    gTruckCargoRevision[bay]++;
    Truck_UpdateCargoLabel(bay);
    RemovePlayerMapIcon(playerid, TRUCK_UNLOAD_MAP_SLOT);
    Economy_SyncHUD(playerid);
    Economy_Log(playerid, pay, "municipal_truck_delivery");
    new msg[130];
    format(msg, sizeof(msg),
        "Рейс завершён! Выгружено %d ед., заработано $%d, опыт +%d. Ранг: %d.",
        units, pay, units, Cleaner_Rank(playerid));
    return SendClientMessage(playerid, CLEANER_COLOR, msg);
}
