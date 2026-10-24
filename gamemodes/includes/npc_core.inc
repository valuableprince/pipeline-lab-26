#if defined _tarkenso_npc_core_included
    #endinput
#endif
#define _tarkenso_npc_core_included

// NPC-CORE-01: only MariaDB records instantiate NPCs. No hardcoded city population.
// A database npc_id is stable and is never an open.mp NPC ID or a route slot.
#define NPC_CORE_LIMIT       (64)
#define NPC_CORE_MAX_WALKERS (8)
#define NPC_CORE_COLOR      (0x8ED9C8FF)
#define NPC_CORE_IDLE       (0)
#define NPC_CORE_ROUTE      (1)

#if defined TARKENSO_DB_PRIMARY
new bool:gNpcCoreReady;
new gNpcCoreStreamTimer;
forward NpcCore_StreamTick();
new gNpcCoreCount;
new gNpcCoreId[NPC_CORE_LIMIT];
new gNpcCoreSkin[NPC_CORE_LIMIT];
new gNpcCoreBehavior[NPC_CORE_LIMIT];
new gNpcCoreRoute[NPC_CORE_LIMIT];
new gNpcCoreActor[NPC_CORE_LIMIT];
new gNpcCoreBot[NPC_CORE_LIMIT];
new gNpcCoreNextPoint[NPC_CORE_LIMIT];
new bool:gNpcCoreReverse[NPC_CORE_LIMIT];
new Float:gNpcCoreX[NPC_CORE_LIMIT];
new Float:gNpcCoreY[NPC_CORE_LIMIT];
new Float:gNpcCoreZ[NPC_CORE_LIMIT];
new Float:gNpcCoreAngle[NPC_CORE_LIMIT];
new gNpcCoreWorld[NPC_CORE_LIMIT];
new gNpcCoreInterior[NPC_CORE_LIMIT];
new gNpcCoreName[NPC_CORE_LIMIT][25];

stock bool:NpcCore_Require(playerid)
{
    if (!Admin_IsFounder(playerid) || !gAdminDuty[playerid])
    {
        SendClientMessage(playerid, ADMIN_ERROR,
            "NPC Studio: только подтверждённый основатель в режиме /aduty.");
        return false;
    }
    if (!gNpcCoreReady)
    {
        SendClientMessage(playerid, ADMIN_ERROR,
            "NPC Studio: требуется SQL миграция 005_npc_core.sql и перезапуск сервера.");
        return false;
    }
    return true;
}

stock bool:NpcCore_Query(const query[], &Cache:result)
{
    result = mysql_query(gDBPrimary, query);
    if (result == Cache:0) return false;
    if (mysql_errno(gDBPrimary))
    {
        cache_delete(result);
        result = Cache:0;
        return false;
    }
    return true;
}

stock bool:NpcCore_Exec(const query[])
{
    new Cache:c;
    if (!NpcCore_Query(query, c)) return false;
    cache_delete(c);
    return true;
}

stock NpcCore_ClearWorld()
{
    for (new i = 0; i < gNpcCoreCount; i++)
    {
        if (gNpcCoreActor[i] != INVALID_ACTOR_ID)
        {
            DestroyActor(gNpcCoreActor[i]);
            gNpcCoreActor[i] = INVALID_ACTOR_ID;
        }
        if (NPC_IsValid(gNpcCoreBot[i]))
        {
            NPC_Destroy(gNpcCoreBot[i]);
            gNpcCoreBot[i] = INVALID_NPC_ID;
        }
    }
    gNpcCoreCount = 0;
    return 1;
}

stock bool:NpcCore_MoveNext(index)
{
    if (index < 0 || index >= gNpcCoreCount ||
        !NPC_IsValid(gNpcCoreBot[index])) return false;
    new route = gNpcCoreRoute[index];
    if (!NPCRoute_Has(route)) return false;
    new point = gNpcCoreNextPoint[index];
    if (point < 0 || point >= gNPCRouteCount[route]) return false;
    new Float:x, Float:y, Float:z;
    if (!NPCRoute_GetPoint(route, point, x, y, z)) return false;
    if (!NPC_Move(gNpcCoreBot[index], x, y, z,
        NPC_MOVE_TYPE_WALK, NPC_MOVE_SPEED_WALK, 0.35)) return false;
    NPC_ApplyAnimation(gNpcCoreBot[index], "PED", "WALK_player", 4.1,
        true, false, false, false, 0);
    // Patrol back and forth along adjacent points: never jump from the last
    // recorded position directly to the first over an unrecorded shortcut.
    if (point >= gNPCRouteCount[route] - 1)
    {
        gNpcCoreReverse[index] = true;
        gNpcCoreNextPoint[index] = point - 1;
    }
    else if (point <= 0)
    {
        gNpcCoreReverse[index] = false;
        gNpcCoreNextPoint[index] = 1;
    }
    else
        gNpcCoreNextPoint[index] = point + (gNpcCoreReverse[index] ? -1 : 1);
    return true;
}

stock NpcCore_OnFinishMove(npcid)
{
    for (new i = 0; i < gNpcCoreCount; i++)
    {
        if (gNpcCoreBot[i] != npcid || !NPC_IsValid(npcid)) continue;
        if (!NpcCore_MoveNext(i))
        {
            NPC_Destroy(npcid);
            gNpcCoreBot[i] = INVALID_NPC_ID;
            printf("[NPC-CORE] Route playback stopped for npc_id=%d.", gNpcCoreId[i]);
        }
        return 1;
    }
    return 0;
}

// Called on startup and after successful editor writes, never on a per-player timer.
stock bool:NpcCore_Reload()
{
    if (!gNpcCoreReady) return false;
    new Cache:c, rows;
    if (!NpcCore_Query(
        "SELECT npc_id,display_name,skin_id,behavior,route_slot,pos_x,pos_y,pos_z,facing,world_id,interior_id FROM npcs WHERE is_enabled=1 ORDER BY npc_id LIMIT 65", c)) return false;
    if (!cache_get_row_count(rows)) { cache_delete(c); return false; }

    // Validate the full result before modifying existing entities.
    for (new i = 0; i < rows; i++)
    {
        new id, skin, mode, route, world, interior;
        new Float:x, Float:y, Float:z, Float:angle;
        if (i >= NPC_CORE_LIMIT ||
            !cache_get_value_name_int(i, "npc_id", id) ||
            !cache_get_value_name_int(i, "skin_id", skin) ||
            !cache_get_value_name_int(i, "behavior", mode) ||
            !cache_get_value_name_int(i, "world_id", world) ||
            !cache_get_value_name_int(i, "interior_id", interior) ||
            !cache_get_value_name_float(i, "pos_x", x) ||
            !cache_get_value_name_float(i, "pos_y", y) ||
            !cache_get_value_name_float(i, "pos_z", z) ||
            !cache_get_value_name_float(i, "facing", angle) ||
            (mode != NPC_CORE_IDLE && mode != NPC_CORE_ROUTE) ||
            (mode == NPC_CORE_ROUTE && (!cache_get_value_name_int(i, "route_slot", route) ||
                !NPCRoute_Has(route))) ||
            !NPCRoute_PointOK(x, y, z) || angle < 0.0 || angle > 360.0 ||
            skin < 0 || skin > 311 || world < 0 || world > 100000 ||
            interior < 0 || interior > 255)
        {
            cache_delete(c);
            printf("[NPC-CORE] Invalid NPC data at row %d; runtime reload rejected.", i);
            return false;
        }
    }
    NpcCore_ClearWorld();
    for (new i = 0; i < rows; i++)
    {
        new index = gNpcCoreCount++;
        cache_get_value_name_int(i, "npc_id", gNpcCoreId[index]);
        cache_get_value_name_int(i, "skin_id", gNpcCoreSkin[index]);
        cache_get_value_name_int(i, "behavior", gNpcCoreBehavior[index]);
        if (gNpcCoreBehavior[index] == NPC_CORE_ROUTE)
            cache_get_value_name_int(i, "route_slot", gNpcCoreRoute[index]);
        else gNpcCoreRoute[index] = -1;
        cache_get_value_name_float(i, "pos_x", gNpcCoreX[index]);
        cache_get_value_name_float(i, "pos_y", gNpcCoreY[index]);
        cache_get_value_name_float(i, "pos_z", gNpcCoreZ[index]);
        cache_get_value_name_float(i, "facing", gNpcCoreAngle[index]);
        cache_get_value_name_int(i, "world_id", gNpcCoreWorld[index]);
        cache_get_value_name_int(i, "interior_id", gNpcCoreInterior[index]);
        cache_get_value_name(i, "display_name", gNpcCoreName[index], sizeof(gNpcCoreName[]));
        gNpcCoreActor[index] = INVALID_ACTOR_ID;
        gNpcCoreBot[index] = INVALID_NPC_ID;
        gNpcCoreNextPoint[index] = 1;

        if (gNpcCoreBehavior[index] == NPC_CORE_IDLE)
        {
            // Actors do not provide interior isolation on all open.mp builds:
            // skip nonzero interiors rather than show them in the wrong location.
            if (gNpcCoreInterior[index] != 0) continue;
            new actor = CreateActor(gNpcCoreSkin[index], gNpcCoreX[index],
                gNpcCoreY[index], gNpcCoreZ[index], gNpcCoreAngle[index]);
            if (actor == INVALID_ACTOR_ID) continue;
            SetActorVirtualWorld(actor, gNpcCoreWorld[index]);
            gNpcCoreActor[index] = actor;
        }

    }
    cache_delete(c);
    printf("[NPC-CORE] Loaded %d enabled records (max %d); walkers stream near players.",
        gNpcCoreCount, NPC_CORE_LIMIT);
    return true;
}

// Spawn moving bots only when an authenticated non-NPC player comes nearby.
// No database queries are issued from the stream timer.
stock bool:NpcCore_StartWalker(index)
{
    if (index < 0 || index >= gNpcCoreCount ||
        gNpcCoreBehavior[index] != NPC_CORE_ROUTE ||
        NPC_IsValid(gNpcCoreBot[index]) || !NPCRoute_Has(gNpcCoreRoute[index]))
        return false;
    new nickname[24];
    format(nickname, sizeof(nickname), "TarkNPC_%d", gNpcCoreId[index]);
    new npcid = NPC_Create(nickname);
    if (!NPC_IsValid(npcid)) return false;
    if (!NPC_Spawn(npcid))
    {
        NPC_Destroy(npcid);
        return false;
    }
    new Float:x,Float:y,Float:z;
    if (!NPCRoute_GetPoint(gNpcCoreRoute[index], 0, x, y, z))
    {
        NPC_Destroy(npcid);
        return false;
    }
    NPC_SetSkin(npcid, gNpcCoreSkin[index]);
    NPC_SetVirtualWorld(npcid, 0);
    NPC_SetPos(npcid, x, y, z);
    gNpcCoreBot[index] = npcid;
    gNpcCoreNextPoint[index] = 1;
    gNpcCoreReverse[index] = false;
    if (!NpcCore_MoveNext(index))
    {
        NPC_Destroy(npcid);
        gNpcCoreBot[index] = INVALID_NPC_ID;
        return false;
    }
    return true;
}

public NpcCore_StreamTick()
{
    if (!gNpcCoreReady) return 1;
    new active;
    for (new i = 0; i < gNpcCoreCount; i++)
        if (NPC_IsValid(gNpcCoreBot[i])) active++;
    for (new i = 0; i < gNpcCoreCount; i++)
    {
        if (gNpcCoreBehavior[i] != NPC_CORE_ROUTE ||
            gNpcCoreWorld[i] != 0 || gNpcCoreInterior[i] != 0) continue;

        new bool:spawned = NPC_IsValid(gNpcCoreBot[i]);
        new Float:x = gNpcCoreX[i], Float:y = gNpcCoreY[i], Float:z = gNpcCoreZ[i];
        if (spawned) NPC_GetPos(gNpcCoreBot[i], x, y, z);
        new bool:near;
        for (new playerid = 0; playerid < MAX_PLAYERS; playerid++)
        {
            if (!IsPlayerConnected(playerid) || IsPlayerNPC(playerid) ||
                gAuthState[playerid] != AUTH_READY ||
                GetPlayerVirtualWorld(playerid) != 0 ||
                GetPlayerInterior(playerid) != 0) continue;
            new Float:px,Float:py,Float:pz;
            GetPlayerPos(playerid,px,py,pz);
            new Float:dx = px-x, Float:dy = py-y, Float:dz = pz-z;
            new Float:d = dx*dx+dy*dy+dz*dz;
            if (d <= (spawned ? 65025.0 : 32400.0))
            {
                near = true;
                break;
            }
        }
        if (spawned && !near)
        {
            NPC_Destroy(gNpcCoreBot[i]);
            gNpcCoreBot[i] = INVALID_NPC_ID;
            active--;
        }
        else if (!spawned && near && active < NPC_CORE_MAX_WALKERS &&
            NpcCore_StartWalker(i)) active++;
    }
    return 1;
}

stock NpcCore_Init()
{
    gNpcCoreReady = false;
    gNpcCoreCount = 0;
    gNpcCoreStreamTimer = 0;
    if (!DBPrimary_IsReady()) return 1;
    new Cache:c, rows;
    if (!NpcCore_Query(
        "SELECT version FROM schema_migrations WHERE version=5 LIMIT 1", c))
    {
        print("[NPC-CORE] Schema v5 not available. NPC editor disabled, game stays online.");
        return 1;
    }
    new bool:ok = cache_get_row_count(rows) && rows == 1;
    cache_delete(c);
    if (!ok)
    {
        print("[NPC-CORE] Apply migration 005_npc_core.sql in phpMyAdmin, then restart.");
        return 1;
    }
    gNpcCoreReady = true;
    if (!NpcCore_Reload())
        print("[NPC-CORE] Bad database NPC data; check npc table. No default citizens created.");
    gNpcCoreStreamTimer = SetTimer("NpcCore_StreamTick", 2000, true);
    return 1;
}
stock NpcCore_Shutdown()
{
    if (gNpcCoreStreamTimer) KillTimer(gNpcCoreStreamTimer);
    gNpcCoreStreamTimer = 0;
    NpcCore_ClearWorld();
    gNpcCoreReady = false;
    return 1;
}
#else
stock bool:NpcCore_Require(playerid)
{
    SendClientMessage(playerid, ADMIN_ERROR, "NPC Studio доступна только в SQL-first сборке.");
    return false;
}
stock NpcCore_Init() { return 1; }
stock NpcCore_Shutdown() { return 1; }
stock NpcCore_OnFinishMove(npcid) { if (npcid < 0) return 0; return 0; }
#endif
