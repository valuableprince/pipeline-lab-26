#if defined _tarkenso_loadtest_included
    #endinput
#endif
#define _tarkenso_loadtest_included

// LOADTEST-01: bounded open.mp NPC world-load probe, NOT real network clients.
// Disabled until scriptfiles/load_test.enabled exists. Founder + /aduty only.
#define LOADTEST_ENABLE_FLAG     "load_test.enabled"
#define LOADTEST_REPORT_FILE     "loadtest.csv"
#define LOADTEST_MAX_NPCS        (100)
#define LOADTEST_BATCH_NPCS      (4)
#define LOADTEST_MOVE_BATCH     (8)
#define LOADTEST_MAX_SECONDS     (300)
#define LOADTEST_MIN_SECONDS     (10)
#define LOADTEST_TICK_MS         (1000)
#define LOADTEST_MIN_TICKRATE    (20)
#define LOADTEST_BAD_SAMPLES     (3)

new gLoadNPCIds[LOADTEST_MAX_NPCS];
new gLoadNPCCount;
new gLoadNPCTarget;
new gLoadOwner = INVALID_PLAYER_ID;
new gLoadTimer;
new gLoadStartedAt;
new gLoadDuration;
new gLoadRunId;
new gLoadMoveIndex;
new gLoadMovePhase;
new gLoadLowTickCount;
new gLoadMinTickrate;
new gLoadWorstTickMs;
new gLoadLastTick;
new bool:gLoadRunning;
new Float:gLoadX, Float:gLoadY, Float:gLoadZ;
new gLoadWorld;

forward LoadTest_Tick();

stock LoadTest_Init()
{
    gLoadRunning = false;
    gLoadTimer = 0;
    gLoadNPCCount = 0;
    gLoadOwner = INVALID_PLAYER_ID;
    for (new i = 0; i < LOADTEST_MAX_NPCS; i++)
        gLoadNPCIds[i] = INVALID_NPC_ID;
    return 1;
}

stock LoadTest_Append(const phase[])
{
    // One file write per five seconds. File is diagnostic, not authoritative.
    new File:log = fopen(LOADTEST_REPORT_FILE, io_append);
    if (!log) return 0;
    new line[208];
    format(line, sizeof(line), "%d,%d,%d,%d,%d,%d,%d,%d,%d,%s\n",
        gLoadRunId, gettime() - gLoadStartedAt, gLoadNPCCount,
        gLoadNPCTarget, GetServerTickRate(), gLoadMinTickrate,
        gLoadWorstTickMs, gGuardLastHeartbeatMs, gGuardDelayedHeartbeatCount, phase);
    new written = fwrite(log, line);
    fclose(log);
    return written > 0;
}

stock LoadTest_Stop(const reason[])
{
    if (!gLoadRunning) return 0;
    new owner = gLoadOwner;
    LoadTest_Append(reason);
    printf("[LOADTEST] stop: reason=%s, run=%d, spawned=%d/%d, min_tickrate=%d, worst_timer_delay=%dms",
        reason, gLoadRunId, gLoadNPCCount, gLoadNPCTarget,
        gLoadMinTickrate, gLoadWorstTickMs);
    gLoadRunning = false;
    if (gLoadTimer)
    {
        KillTimer(gLoadTimer);
        gLoadTimer = 0;
    }
    for (new i = 0; i < gLoadNPCCount; i++)
    {
        if (gLoadNPCIds[i] != INVALID_NPC_ID && NPC_IsValid(gLoadNPCIds[i]))
            NPC_Destroy(gLoadNPCIds[i]);
        gLoadNPCIds[i] = INVALID_NPC_ID;
    }
    gLoadNPCCount = 0;
    gLoadNPCTarget = 0;
    gLoadOwner = INVALID_PLAYER_ID;
    if (owner != INVALID_PLAYER_ID && IsPlayerConnected(owner))
    {
        new msg[130];
        format(msg, sizeof(msg), "[LoadTest] Stopped (%s). CSV: scriptfiles/loadtest.csv", reason);
        SendClientMessage(owner, ADMIN_COLOR, msg);
    }
    return 1;
}

stock LoadTest_Shutdown()
{
    if (gLoadRunning) LoadTest_Stop("shutdown");
    return 1;
}

stock LoadTest_OnDisconnect(playerid)
{
    if (gLoadRunning && playerid == gLoadOwner)
        LoadTest_Stop("owner_disconnect");
    return 1;
}

stock LoadTest_Status(playerid)
{
    new msg[144];
    if (!gLoadRunning)
    {
        format(msg, sizeof(msg),
            "[LoadTest] Idle. Enabled: %d. Max NPC: %d. /loadtest start [count] [seconds]",
            fexist(LOADTEST_ENABLE_FLAG), LOADTEST_MAX_NPCS);
    }
    else
    {
        format(msg, sizeof(msg),
            "[LoadTest] NPC %d/%d; elapsed %d/%d sec; tickrate %d; min %d; worst timer %dms",
            gLoadNPCCount, gLoadNPCTarget, gettime() - gLoadStartedAt,
            gLoadDuration, GetServerTickRate(), gLoadMinTickrate, gLoadWorstTickMs);
    }
    return SendClientMessage(playerid, ADMIN_COLOR, msg);
}

stock LoadTest_Start(playerid, count, duration)
{
    if (gLoadRunning)
        return SendClientMessage(playerid, ADMIN_ERROR, "[LoadTest] Already running; /loadtest stop.");
    if (!fexist(LOADTEST_ENABLE_FLAG))
        return SendClientMessage(playerid, ADMIN_ERROR, "[LoadTest] Disabled. Create scriptfiles/load_test.enabled on a test server.");
    if (count < 1 || count > LOADTEST_MAX_NPCS ||
        duration < LOADTEST_MIN_SECONDS || duration > LOADTEST_MAX_SECONDS)
        return SendClientMessage(playerid, ADMIN_ERROR, "[LoadTest] Count: 1..100; seconds: 10..300.");
    if (!gAccountSpawned[playerid] || GetPlayerInterior(playerid) != 0)
        return SendClientMessage(playerid, ADMIN_ERROR, "[LoadTest] Spawn outdoors before starting.");
    gLoadOwner = playerid;
    gLoadNPCTarget = count;
    gLoadNPCCount = 0;
    gLoadDuration = duration;
    gLoadStartedAt = gettime();
    gLoadRunId = gLoadStartedAt;
    gLoadMoveIndex = 0;
    gLoadMovePhase = 0;
    gLoadLowTickCount = 0;
    gLoadMinTickrate = GetServerTickRate();
    gLoadWorstTickMs = 0;
    gLoadLastTick = GetTickCount();
    gLoadWorld = GetPlayerVirtualWorld(playerid);
    GetPlayerPos(playerid, gLoadX, gLoadY, gLoadZ);
    gLoadRunning = true;
    gLoadTimer = SetTimer("LoadTest_Tick", LOADTEST_TICK_MS, true);
    if (!gLoadTimer)
    {
        gLoadRunning = false;
        gLoadOwner = INVALID_PLAYER_ID;
        return SendClientMessage(playerid, ADMIN_ERROR, "[LoadTest] Timer creation failed.");
    }
    printf("[LOADTEST] start: run=%d, target=%d NPC, duration=%ds, world=%d",
        gLoadRunId, count, duration, gLoadWorld);
    LoadTest_Append("start");
    return SendClientMessage(playerid, ADMIN_COLOR,
        "[LoadTest] NPC stress started; auto-stop enabled. Use /loadtest status or /loadtest stop.");
}

public LoadTest_Tick()
{
    if (!gLoadRunning) return 1;
    if (!fexist(LOADTEST_ENABLE_FLAG))
    {
        LoadTest_Stop("flag_removed");
        return 1;
    }
    if (!IsPlayerConnected(gLoadOwner) || !Admin_IsFounder(gLoadOwner))
    {
        LoadTest_Stop("owner_unavailable");
        return 1;
    }
    if (gettime() - gLoadStartedAt >= gLoadDuration)
    {
        LoadTest_Stop("complete");
        return 1;
    }

    new nowTick = GetTickCount();
    new elapsed = nowTick - gLoadLastTick;
    gLoadLastTick = nowTick;
    if (elapsed > gLoadWorstTickMs && elapsed >= 0)
        gLoadWorstTickMs = elapsed;
    new rate = GetServerTickRate();
    if (rate < gLoadMinTickrate) gLoadMinTickrate = rate;
    if (rate < LOADTEST_MIN_TICKRATE)
        gLoadLowTickCount++;
    else
        gLoadLowTickCount = 0;
    if (gLoadLowTickCount >= LOADTEST_BAD_SAMPLES)
    {
        LoadTest_Stop("low_tickrate");
        return 1;
    }

    for (new batch = 0; batch < LOADTEST_BATCH_NPCS &&
         gLoadNPCCount < gLoadNPCTarget; batch++)
    {
        new slot = gLoadNPCCount;
        new name[24];
        format(name, sizeof(name), "LT_%d", slot);
        new npcid = NPC_Create(name);
        if (npcid == INVALID_NPC_ID || !NPC_IsValid(npcid))
        {
            LoadTest_Stop("npc_create_failed");
            return 1;
        }
        if (!NPC_Spawn(npcid))
        {
            NPC_Destroy(npcid);
            LoadTest_Stop("npc_spawn_failed");
            return 1;
        }
        NPC_SetVirtualWorld(npcid, gLoadWorld);
        new Float:x = gLoadX + float((slot % 12) * 3 - 18);
        new Float:y = gLoadY + float((slot / 12) * 3 + 5);
        NPC_SetPos(npcid, x, y, gLoadZ);
        gLoadNPCIds[slot] = npcid;
        gLoadNPCCount++;
    }

    // Move a bounded group each tick. NPCs outside this test are never touched.
    if (gLoadNPCCount > 0)
    {
        gLoadMovePhase = !gLoadMovePhase;
        for (new i = 0; i < LOADTEST_MOVE_BATCH; i++)
        {
            new slot = gLoadMoveIndex++ % gLoadNPCCount;
            new id = gLoadNPCIds[slot];
            if (id == INVALID_NPC_ID || !NPC_IsValid(id) || NPC_IsMoving(id)) continue;
            new Float:x = gLoadX + float((slot % 12) * 3 - 18);
            new Float:y = gLoadY + float((slot / 12) * 3 + 5);
            NPC_Move(id, x + (gLoadMovePhase ? 1.5 : -1.5),
                y + (gLoadMovePhase ? 1.5 : -1.5), gLoadZ, NPC_MOVE_TYPE_WALK);
        }
    }
    if ((gettime() - gLoadStartedAt) % 5 == 0) LoadTest_Append("running");
    return 1;
}

stock LoadTest_Command(playerid, const params[])
{
    if (!Admin_IsFounder(playerid) || !gAdminDuty[playerid])
        return SendClientMessage(playerid, ADMIN_ERROR, "[LoadTest] Founder only; enable /aduty.");
    new action[16];
    if (isnull(params) || sscanf(params, "s[16]", action))
        return SendClientMessage(playerid, ADMIN_COLOR,
            "[LoadTest] /loadtest start [1..100] [10..300] | status | stop.");
    if (!strcmp(action, "status", true)) return LoadTest_Status(playerid);
    if (!strcmp(action, "stop", true))
    {
        if (!gLoadRunning) return LoadTest_Status(playerid);
        LoadTest_Stop("manual");
        return 1;
    }
    if (!strcmp(action, "start", true))
    {
        new count, seconds;
        if (sscanf(params, "s[16]ii", action, count, seconds))
            return SendClientMessage(playerid, ADMIN_ERROR,
                "[LoadTest] Usage: /loadtest start 50 120");
        return LoadTest_Start(playerid, count, seconds);
    }
    return SendClientMessage(playerid, ADMIN_ERROR,
        "[LoadTest] /loadtest start [count] [seconds] | status | stop.");
}
