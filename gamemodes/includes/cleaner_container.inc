#if defined _tarkenso_cleaner_container_included
    #endinput
#endif
#define _tarkenso_cleaner_container_included

// JOB-CLEANER-05. Schema v10, bounded world objects and transactional deposits.
#define CLEANER_DUMP_LIMIT (24)
#define CLEANER_DUMP_CAP (60)
#define CLEANER_DUMP_OVERFLOW_CAP (40)
#define CLEANER_DUMP_BAG_OBJECTS (10)
#define CLEANER_DUMP_MODEL (1344)
#define CLEANER_DUMP_BAG_MODEL (1264)
#define CLEANER_DUMP_INTERACT_SQ (16.0)
#define CLEANER_DUMP_MAP_SLOT (73)
#define CLEANER_DUMP_MAP_COLOR (0xEE3333FF)

new bool:gCleanerDumpReady;
new gCleanerDumpCount;
new gCleanerDumpId[CLEANER_DUMP_LIMIT];
new bool:gCleanerDumpEnabled[CLEANER_DUMP_LIMIT];
new Float:gCleanerDumpPos[CLEANER_DUMP_LIMIT][4];
new gCleanerDumpBin[CLEANER_DUMP_LIMIT];
new gCleanerDumpOverflow[CLEANER_DUMP_LIMIT];
new Float:gCleanerDumpBagPos[CLEANER_DUMP_LIMIT][CLEANER_DUMP_BAG_OBJECTS][4];
new bool:gCleanerDumpBagDefined[CLEANER_DUMP_LIMIT][CLEANER_DUMP_BAG_OBJECTS];
new gCleanerDumpObject[CLEANER_DUMP_LIMIT];
new Text3D:gCleanerDumpLabel[CLEANER_DUMP_LIMIT];
new gCleanerDumpBags[CLEANER_DUMP_LIMIT][CLEANER_DUMP_BAG_OBJECTS];
new gCleanerDumpTarget[MAX_PLAYERS]; // selected on proximity check
new bool:gCleanerDumpNav[MAX_PLAYERS];
new gCleanerDumpNavSlot[MAX_PLAYERS];
new bool:gCleanerDumpStaged;
new gCleanerDumpStageSlot = -1;
new gCleanerDumpNextBin;
new gCleanerDumpNextOverflow;
new gCleanerDumpDisplaySlot;

stock CleanerDump_GetBin() { return gCleanerDumpBin[gCleanerDumpDisplaySlot]; }
stock CleanerDump_GetOverflow() { return gCleanerDumpOverflow[gCleanerDumpDisplaySlot]; }
stock CleanerDump_GetBinCap() { return CLEANER_DUMP_CAP; }
stock CleanerDump_GetOverflowCap() { return CLEANER_DUMP_OVERFLOW_CAP; }
stock bool:CleanerDump_IsReady() { return gCleanerDumpReady && gCleanerDumpCount > 0; }

stock CleanerDump_NavClear(playerid)
{
    if (gCleanerDumpNav[playerid]) RemovePlayerMapIcon(playerid, CLEANER_DUMP_MAP_SLOT);
    gCleanerDumpNav[playerid] = false;
    gCleanerDumpNavSlot[playerid] = -1;
    return 1;
}

stock CleanerDump_NavReset(playerid)
{
    gCleanerDumpNav[playerid] = false;
    gCleanerDumpNavSlot[playerid] = -1;
    gCleanerDumpTarget[playerid] = -1;
    gCleanerDumpNavTick[playerid] = 0;
    return 1;
}

stock CleanerDump_Nearest(playerid, count, bool:nearOnly = false)
{
    if (!CleanerDump_IsReady() || !Cleaner_InWorld(playerid)) return -1;
    new Float:px, Float:py, Float:pz;
    GetPlayerPos(playerid, px, py, pz);
    new nearest = -1;
    new Float:best = nearOnly ? CLEANER_DUMP_INTERACT_SQ : 100000000.0;
    for (new i = 0; i < gCleanerDumpCount; i++)
    {
        if (!gCleanerDumpEnabled[i] || (count > 0 &&
            CLEANER_DUMP_CAP + CLEANER_DUMP_OVERFLOW_CAP -
            gCleanerDumpBin[i] - gCleanerDumpOverflow[i] < count)) continue;
        new Float:dx = px - gCleanerDumpPos[i][0];
        new Float:dy = py - gCleanerDumpPos[i][1];
        new Float:dz = pz - gCleanerDumpPos[i][2];
        new Float:distance = dx*dx + dy*dy + dz*dz;
        if (distance < best)
        {
            best = distance;
            nearest = i;
        }
    }
    return nearest;
}

stock bool:CleanerDump_Near(playerid)
{
    if (GetPlayerState(playerid) != PLAYER_STATE_ONFOOT) return false;
    gCleanerDumpTarget[playerid] = CleanerDump_Nearest(playerid, 0, true);
    return gCleanerDumpTarget[playerid] >= 0;
}

stock CleanerDump_Navigate(playerid, count)
{
    if (count <= 0 || !Cleaner_InWorld(playerid))
        return CleanerDump_NavClear(playerid);
    new slot = CleanerDump_Nearest(playerid, count);
    if (slot < 0)
    {
        CleanerDump_NavClear(playerid);
        return SendClientMessage(playerid, COLOR_RED,
            "Свободных контейнеров нет. Ожидайте муниципального вывоза.");
    }
    if (gCleanerDumpNav[playerid] && gCleanerDumpNavSlot[playerid] == slot) return 1;
    CleanerDump_NavClear(playerid);
    SetPlayerMapIcon(playerid, CLEANER_DUMP_MAP_SLOT,
        gCleanerDumpPos[slot][0], gCleanerDumpPos[slot][1], gCleanerDumpPos[slot][2],
        0, CLEANER_DUMP_MAP_COLOR, MAPICON_LOCAL);
    gCleanerDumpNav[playerid] = true;
    gCleanerDumpNavSlot[playerid] = slot;
    return 1;
}

stock CleanerDump_VisualBagCount(slot)
{
    new overflow = gCleanerDumpOverflow[slot];
    if (overflow <= 0) return 0;
    new needed = (overflow * CLEANER_DUMP_BAG_OBJECTS + CLEANER_DUMP_OVERFLOW_CAP - 1)
        / CLEANER_DUMP_OVERFLOW_CAP;
    if (needed > CLEANER_DUMP_BAG_OBJECTS) needed = CLEANER_DUMP_BAG_OBJECTS;
    return needed;
}

stock CleanerDump_Refresh(slot)
{
    if (slot < 0 || slot >= gCleanerDumpCount) return 0;
    new title[122];
    format(title, sizeof(title),
        "{A7D6B5}Мусорный контейнер %d/%d\n{FFFFFF}/trashdrop — сдать мусор",
        gCleanerDumpBin[slot], CLEANER_DUMP_CAP);
    if (gCleanerDumpLabel[slot] != Text3D:INVALID_3DTEXT_ID)
        Update3DTextLabelText(gCleanerDumpLabel[slot], CLEANER_COLOR, title);
    new needed = CleanerDump_VisualBagCount(slot);
    for (new j = 0; j < CLEANER_DUMP_BAG_OBJECTS; j++)
    {
        if (j >= needed || !gCleanerDumpBagDefined[slot][j] || !gCleanerDumpEnabled[slot])
        {
            if (gCleanerDumpBags[slot][j] != INVALID_OBJECT_ID)
                DestroyObject(gCleanerDumpBags[slot][j]);
            gCleanerDumpBags[slot][j] = INVALID_OBJECT_ID;
        }
        else if (gCleanerDumpBags[slot][j] == INVALID_OBJECT_ID)
        {
            gCleanerDumpBags[slot][j] = CreateObject(CLEANER_DUMP_BAG_MODEL,
                gCleanerDumpBagPos[slot][j][0], gCleanerDumpBagPos[slot][j][1],
                gCleanerDumpBagPos[slot][j][2], 0.0, 0.0,
                gCleanerDumpBagPos[slot][j][3], 80.0);
        }
    }
    return 1;
}

stock CleanerDump_ClearObjects()
{
    for (new i = 0; i < CLEANER_DUMP_LIMIT; i++)
    {
        if (gCleanerDumpObject[i] != INVALID_OBJECT_ID) DestroyObject(gCleanerDumpObject[i]);
        gCleanerDumpObject[i] = INVALID_OBJECT_ID;
        if (gCleanerDumpLabel[i] != Text3D:INVALID_3DTEXT_ID)
            Delete3DTextLabel(gCleanerDumpLabel[i]);
        gCleanerDumpLabel[i] = Text3D:INVALID_3DTEXT_ID;
        for (new j = 0; j < CLEANER_DUMP_BAG_OBJECTS; j++)
        {
            if (gCleanerDumpBags[i][j] != INVALID_OBJECT_ID)
                DestroyObject(gCleanerDumpBags[i][j]);
            gCleanerDumpBags[i][j] = INVALID_OBJECT_ID;
        }
    }
    return 1;
}

stock bool:CleanerDump_Load()
{
#if defined TARKENSO_DB_PRIMARY
    if (!DBPrimary_IsReady()) return false;
    gCleanerDumpCount = 0;
    new rows, id, bin, overflow, active, world, interior;
    new Float:x, Float:y, Float:z, Float:rz;
    new Cache:c = mysql_query(gDBPrimary,
        "SELECT container_id,x,y,z,rz,world_id,interior_id,enabled,bin_units,overflow_units FROM cleaner_containers ORDER BY container_id LIMIT 25");
    if (c == Cache:0 || mysql_errno(gDBPrimary) ||
        !cache_get_row_count(rows) || rows < 1 || rows > CLEANER_DUMP_LIMIT)
    {
        if (c != Cache:0) cache_delete(c);
        return false;
    }
    for (new i = 0; i < rows; i++)
    {
        if (!cache_get_value_name_int(i, "container_id", id) ||
            !cache_get_value_name_int(i, "bin_units", bin) ||
            !cache_get_value_name_int(i, "overflow_units", overflow) ||
            !cache_get_value_name_int(i, "enabled", active) ||
            !cache_get_value_name_int(i, "world_id", world) ||
            !cache_get_value_name_int(i, "interior_id", interior) ||
            !cache_get_value_name_float(i, "x", x) ||
            !cache_get_value_name_float(i, "y", y) ||
            !cache_get_value_name_float(i, "z", z) ||
            !cache_get_value_name_float(i, "rz", rz) ||
            id < 1 || bin < 0 || bin > CLEANER_DUMP_CAP ||
            overflow < 0 || overflow > CLEANER_DUMP_OVERFLOW_CAP ||
            (overflow > 0 && bin != CLEANER_DUMP_CAP) || world != 0 || interior != 0)
        {
            cache_delete(c);
            return false;
        }
        gCleanerDumpId[i] = id;
        gCleanerDumpEnabled[i] = active != 0;
        gCleanerDumpBin[i] = bin;
        gCleanerDumpOverflow[i] = overflow;
        gCleanerDumpPos[i][0] = x;
        gCleanerDumpPos[i][1] = y;
        gCleanerDumpPos[i][2] = z;
        gCleanerDumpPos[i][3] = rz;
        for (new j = 0; j < CLEANER_DUMP_BAG_OBJECTS; j++)
            gCleanerDumpBagDefined[i][j] = false;
        gCleanerDumpCount++;
    }
    cache_delete(c);
    c = mysql_query(gDBPrimary,
        "SELECT container_id,slot_id,x,y,z,rz FROM cleaner_container_bag_positions ORDER BY container_id,slot_id LIMIT 241");
    if (c == Cache:0 || mysql_errno(gDBPrimary) || !cache_get_row_count(rows) ||
        rows > CLEANER_DUMP_LIMIT * CLEANER_DUMP_BAG_OBJECTS)
    {
        if (c != Cache:0) cache_delete(c);
        return false;
    }
    for (new i = 0; i < rows; i++)
    {
        new bagId, bagSlot;
        if (!cache_get_value_name_int(i, "container_id", bagId) ||
            !cache_get_value_name_int(i, "slot_id", bagSlot) ||
            bagSlot < 0 || bagSlot >= CLEANER_DUMP_BAG_OBJECTS ||
            !cache_get_value_name_float(i, "x", x) ||
            !cache_get_value_name_float(i, "y", y) ||
            !cache_get_value_name_float(i, "z", z) ||
            !cache_get_value_name_float(i, "rz", rz))
        { cache_delete(c); return false; }
        new at = -1;
        for (new j = 0; j < gCleanerDumpCount; j++)
            if (gCleanerDumpId[j] == bagId) { at = j; break; }
        if (at < 0) { cache_delete(c); return false; }
        gCleanerDumpBagDefined[at][bagSlot] = true;
        gCleanerDumpBagPos[at][bagSlot][0] = x;
        gCleanerDumpBagPos[at][bagSlot][1] = y;
        gCleanerDumpBagPos[at][bagSlot][2] = z;
        gCleanerDumpBagPos[at][bagSlot][3] = rz;
    }
    cache_delete(c);
    return true;
#else
    return false;
#endif
}

stock CleanerDump_BuildObjects()
{
    for (new i = 0; i < gCleanerDumpCount; i++)
    {
        if (!gCleanerDumpEnabled[i]) continue;
        gCleanerDumpObject[i] = CreateObject(CLEANER_DUMP_MODEL,
            gCleanerDumpPos[i][0], gCleanerDumpPos[i][1], gCleanerDumpPos[i][2]-0.85,
            0.0, 0.0, gCleanerDumpPos[i][3], 160.0);
        gCleanerDumpLabel[i] = Create3DTextLabel("Мусорный контейнер",
            CLEANER_COLOR, gCleanerDumpPos[i][0], gCleanerDumpPos[i][1],
            gCleanerDumpPos[i][2]+1.4, 18.0, 0, false);
        CleanerDump_Refresh(i);
    }
    return 1;
}

stock CleanerDump_Init()
{
    gCleanerDumpStaged = false;
    gCleanerDumpStageSlot = -1;
    gCleanerDumpDisplaySlot = 0;
    gCleanerDumpCount = 0;
    for (new i = 0; i < CLEANER_DUMP_LIMIT; i++)
    {
        gCleanerDumpObject[i] = INVALID_OBJECT_ID;
        gCleanerDumpLabel[i] = Text3D:INVALID_3DTEXT_ID;
        for (new j = 0; j < CLEANER_DUMP_BAG_OBJECTS; j++)
            gCleanerDumpBags[i][j] = INVALID_OBJECT_ID;
    }
    gCleanerDumpReady = CleanerDump_Load();
    if (!gCleanerDumpReady) print("[Cleaner] ERROR: schema v10 containers unavailable.");
    else CleanerDump_BuildObjects();
    return 1;
}

stock CleanerDump_Shutdown()
{
    if (gCleanerDumpStaged) DBWorld_Rollback();
    gCleanerDumpStaged = false;
    CleanerDump_ClearObjects();
    gCleanerDumpReady = false;
    return 1;
}

stock bool:CleanerDump_CanAccept(playerid, count)
{
    new slot = gCleanerDumpTarget[playerid];
    return gCleanerDumpReady && !gCleanerDumpStaged &&
        slot >= 0 && slot < gCleanerDumpCount && gCleanerDumpEnabled[slot] &&
        count > 0 && count <= 15 &&
        gCleanerDumpBin[slot] + gCleanerDumpOverflow[slot] + count <=
            CLEANER_DUMP_CAP + CLEANER_DUMP_OVERFLOW_CAP;
}

stock bool:CleanerDump_Stage(playerid, count)
{
    if (!CleanerDump_CanAccept(playerid, count)) return false;
    new slot = gCleanerDumpTarget[playerid];
    new freeSpace = CLEANER_DUMP_CAP - gCleanerDumpBin[slot];
    new inBin = count;
    if (inBin > freeSpace) inBin = freeSpace;
    gCleanerDumpNextBin = gCleanerDumpBin[slot] + inBin;
    gCleanerDumpNextOverflow = gCleanerDumpOverflow[slot] + count - inBin;
#if defined TARKENSO_DB_PRIMARY
    if (!DBWorld_Exec("START TRANSACTION")) return false;
    new query[290], changed;
    mysql_format(gDBPrimary, query, sizeof(query),
        "UPDATE cleaner_containers SET bin_units=%d,overflow_units=%d WHERE container_id=%d AND bin_units=%d AND overflow_units=%d AND enabled=1",
        gCleanerDumpNextBin, gCleanerDumpNextOverflow,
        gCleanerDumpId[slot], gCleanerDumpBin[slot], gCleanerDumpOverflow[slot]);
    if (!DBPrimary_Execute(query, changed) || changed != 1)
    { DBWorld_Rollback(); return false; }
#endif
    gCleanerDumpStaged = true;
    gCleanerDumpStageSlot = slot;
    return true;
}

stock CleanerDump_Rollback()
{
    if (gCleanerDumpStaged) DBWorld_Rollback();
    gCleanerDumpStaged = false;
    gCleanerDumpStageSlot = -1;
    return 1;
}

stock bool:CleanerDump_Commit()
{
    if (!gCleanerDumpStaged) return false;
    if (!DBWorld_Exec("COMMIT"))
    {
        CleanerDump_Rollback();
        gCleanerDumpReady = false; // unknown transaction state: fail closed
        return false;
    }
    new slot = gCleanerDumpStageSlot;
    gCleanerDumpBin[slot] = gCleanerDumpNextBin;
    gCleanerDumpOverflow[slot] = gCleanerDumpNextOverflow;
    gCleanerDumpDisplaySlot = slot;
    gCleanerDumpStaged = false;
    gCleanerDumpStageSlot = -1;
    CleanerDump_Refresh(slot);
    return true;
}

// Only verified founder may empty the nearest bin; use a guarded DB update.
stock CleanerDump_Service(playerid)
{
    if (!Admin_IsFounder(playerid) || !gAdminDuty[playerid])
        return SendClientMessage(playerid, ADMIN_ERROR, "Только Founder с /aduty.");
    if (!CleanerDump_Near(playerid) || !gCleanerDumpReady || gCleanerDumpStaged)
        return SendClientMessage(playerid, ADMIN_ERROR, "Подойдите к доступному контейнеру.");
    new slot = gCleanerDumpTarget[playerid], detail[95], q[270], changed;
    format(detail, sizeof(detail), "container=%d bin=%d overflow=%d",
        gCleanerDumpId[slot], gCleanerDumpBin[slot], gCleanerDumpOverflow[slot]);
    if (!Admin_LogPlayer(playerid, "DUMP_SERVICE", detail))
        return SendClientMessage(playerid, ADMIN_ERROR, "Аудит недоступен.");
    mysql_format(gDBPrimary, q, sizeof(q),
        "UPDATE cleaner_containers SET bin_units=0,overflow_units=0 WHERE container_id=%d AND bin_units=%d AND overflow_units=%d",
        gCleanerDumpId[slot], gCleanerDumpBin[slot], gCleanerDumpOverflow[slot]);
    if (!DBPrimary_Execute(q, changed) || changed != 1)
        return SendClientMessage(playerid, ADMIN_ERROR, "Не удалось очистить контейнер.");
    gCleanerDumpBin[slot] = 0;
    gCleanerDumpOverflow[slot] = 0;
    CleanerDump_Refresh(slot);
    return SendClientMessage(playerid, CLEANER_COLOR, "Контейнер очищен.");
}
