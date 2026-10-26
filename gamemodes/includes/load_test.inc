#if defined _tarkenso_loadtest_included
    #endinput
#endif
#define _tarkenso_loadtest_included

// LOADTEST-02: bounded open.mp NPC probe, NOT real network clients.
// Founder + /aduty only. Each run creates its own revisioned CSV report.
#define LOADTEST_MAX_NPCS        (100)
#define LOADTEST_BATCH_NPCS      (4)
#define LOADTEST_MOVE_BATCH     (8)
#define LOADTEST_MAX_SECONDS     (300)
#define LOADTEST_MIN_SECONDS     (10)
#define LOADTEST_TICK_MS         (1000)
#define LOADTEST_MIN_TICKRATE    (20)
#define LOADTEST_BAD_SAMPLES     (3)
#define LOADTEST_MAX_REVISIONS   (999)
#define LOADTEST_DEFAULT_NPCS    (25)
#define LOADTEST_DEFAULT_SECONDS (60)
#define LOADTEST_CSV_HEADER      "run_id,elapsed_sec,active_npcs,target_npcs,tickrate,min_tickrate,worst_timer_interval_ms,guard_heartbeat_ms,guard_delayed_count,state\n"

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
new bool:gLoadFullScenario;
new Float:gLoadX, Float:gLoadY, Float:gLoadZ;
new gLoadWorld;
new gLoadReportFile[96];

forward LoadTest_Tick();
forward bool:Bench_Begin(playerid);

stock LoadTest_Init()
{
    gLoadRunning = false;
    gLoadFullScenario = false;
    gLoadTimer = 0;
    gLoadNPCCount = 0;
    gLoadOwner = INVALID_PLAYER_ID;
    gLoadReportFile[0] = EOS;
    for (new i = 0; i < LOADTEST_MAX_NPCS; i++)
        gLoadNPCIds[i] = INVALID_NPC_ID;
    return 1;
}

// Reserve a new file per run. Revisions are daily sequence numbers, not code versions.
stock bool:LoadTest_CreateReport()
{
    new year, month, day;
    getdate(year, month, day); // Date from server's configured timezone.
    gLoadReportFile[0] = EOS;
    for (new revision = 1; revision <= LOADTEST_MAX_REVISIONS; revision++)
    {
        format(gLoadReportFile, sizeof(gLoadReportFile),
            "load_test_%02d_%02d.%02d.%04d.csv", revision, day, month, year);
        if (fexist(gLoadReportFile)) continue;

        new File:reportHandle = fopen(gLoadReportFile, io_write);
        if (!reportHandle)
        {
            gLoadReportFile[0] = EOS;
            return false;
        }
        new headerBytes = fwrite(reportHandle, LOADTEST_CSV_HEADER);
        fclose(reportHandle);
        if (headerBytes <= 0)
        {
            fremove(gLoadReportFile);
            gLoadReportFile[0] = EOS;
            return false;
        }
        return true;
    }
    gLoadReportFile[0] = EOS;
    return false;
}

stock LoadTest_Append(const phase[])
{
    // One file write per five seconds. File is diagnostic, not authoritative.
    if (gLoadReportFile[0] == EOS) return 0;
    new File:reportHandle = fopen(gLoadReportFile, io_append);
    if (!reportHandle) return 0;
    new reportRow[208];
    format(reportRow, sizeof(reportRow), "%d,%d,%d,%d,%d,%d,%d,%d,%d,%s\n",
        gLoadRunId, gettime() - gLoadStartedAt, gLoadNPCCount,
        gLoadNPCTarget, GetServerTickRate(), gLoadMinTickrate,
        gLoadWorstTickMs, gGuardLastHeartbeatMs, gGuardDelayedHeartbeatCount, phase);
    new recordBytes = fwrite(reportHandle, reportRow);
    fclose(reportHandle);
    return recordBytes > 0;
}

stock LoadTest_Stop(const reason[])
{
    if (!gLoadRunning) return 0;
    new owner = gLoadOwner;
    LoadTest_Append(reason);
    if (gLoadFullScenario) Bench_Finish(reason);
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
        format(msg, sizeof(msg), "[LoadTest] Stopped (%s). Report: %s", reason, gLoadReportFile);
        SendClientMessage(owner, ADMIN_COLOR, msg);
    }
    gLoadReportFile[0] = EOS;
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
            "[LoadTest] Idle. /load start [count] [seconds] (default 25/60); max=%d",
            LOADTEST_MAX_NPCS);
    }
    else
    {
        format(msg, sizeof(msg),
            "[LoadTest] NPC %d/%d; elapsed %d/%d sec; tickrate %d; min %d; worst timer %dms",
            gLoadNPCCount, gLoadNPCTarget, gettime() - gLoadStartedAt,
            gLoadDuration, GetServerTickRate(), gLoadMinTickrate, gLoadWorstTickMs);
    }
    SendClientMessage(playerid, ADMIN_COLOR, msg);
    if (gLoadRunning)
    {
        format(msg, sizeof(msg), "[LoadTest] Report: %s", gLoadReportFile);
        SendClientMessage(playerid, ADMIN_COLOR, msg);
        if (gLoadFullScenario) Bench_Progress(playerid);
    }
    return 1;
}

stock LoadTest_Start(playerid, count, duration, bool:fullScenario = false)
{
    if (gLoadRunning)
        return SendClientMessage(playerid, ADMIN_ERROR, "[LoadTest] Already running; /load stop.");
    if (count < 1 || count > LOADTEST_MAX_NPCS ||
        duration < LOADTEST_MIN_SECONDS || duration > LOADTEST_MAX_SECONDS)
        return SendClientMessage(playerid, ADMIN_ERROR, "[LoadTest] Count: 1..100; seconds: 10..300.");
    if (!gAccountSpawned[playerid] || GetPlayerInterior(playerid) != 0)
        return SendClientMessage(playerid, ADMIN_ERROR, "[LoadTest] Spawn outdoors before starting.");
    if (!LoadTest_CreateReport())
        return SendClientMessage(playerid, ADMIN_ERROR,
            "[LoadTest] Report creation failed (I/O error or 999 daily revisions). Test not started.");
    gLoadOwner = playerid;
    gLoadFullScenario = fullScenario;
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
    // Full-mode pseudo-players stand at the Fort Carson start area in world 0.
    if (gLoadFullScenario)
    {
        gLoadWorld = 0;
        gLoadX = START_X;
        gLoadY = START_Y;
        gLoadZ = START_Z;
    }
    if (gLoadFullScenario && !Bench_Begin(playerid))
    {
        fremove(gLoadReportFile);
        gLoadReportFile[0] = EOS;
        gLoadOwner = INVALID_PLAYER_ID;
        return SendClientMessage(playerid, ADMIN_ERROR,
            "[LoadTest] Full scenario unavailable: install MySQL migration 013 and verify DB.");
    }
    gLoadRunning = true;
    if (!LoadTest_Append("start"))
    {
        if (gLoadFullScenario) Bench_Finish("report_failure");
        fremove(gLoadReportFile);
        gLoadReportFile[0] = EOS;
        gLoadRunning = false;
        gLoadOwner = INVALID_PLAYER_ID;
        return SendClientMessage(playerid, ADMIN_ERROR, "[LoadTest] Report write failed. Test not started.");
    }
    gLoadTimer = SetTimer("LoadTest_Tick", LOADTEST_TICK_MS, true);
    if (!gLoadTimer)
    {
        LoadTest_Append("timer_failed");
        if (gLoadFullScenario) Bench_Finish("timer_failed");
        gLoadRunning = false;
        gLoadOwner = INVALID_PLAYER_ID;
        gLoadReportFile[0] = EOS;
        return SendClientMessage(playerid, ADMIN_ERROR, "[LoadTest] Timer creation failed.");
    }
    printf("[LOADTEST] start: run=%d, target=%d NPC, duration=%ds, world=%d, report=%s",
        gLoadRunId, count, duration, gLoadWorld, gLoadReportFile);
    new startMessage[144];
    format(startMessage, sizeof(startMessage),
        "[LoadTest] Running %d NPC for %d sec. Report: %s", count, duration, gLoadReportFile);
    return SendClientMessage(playerid, ADMIN_COLOR, startMessage);
}

public LoadTest_Tick()
{
    if (!gLoadRunning) return 1;
    if (!IsPlayerConnected(gLoadOwner) || !Admin_IsFounder(gLoadOwner) ||
        !gAdminDuty[gLoadOwner])
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
    if (gLoadNPCCount > 0 && !gLoadFullScenario)
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
    if (gLoadFullScenario) Bench_Tick();
    if ((gettime() - gLoadStartedAt) % 5 == 0) LoadTest_Append("running");
    return 1;
}

stock LoadTest_Command(playerid, const params[])
{
    if (!Admin_IsFounder(playerid))
        return SendClientMessage(playerid, ADMIN_ERROR, "[LoadTest] Founder only.");
    new action[16];
    if (isnull(params) || sscanf(params, "s[16]", action))
        return SendClientMessage(playerid, ADMIN_COLOR,
            "[LoadTest] /load start [count] [sec] [npc|full] | status | stop (default npc 25/60).");
    if (!strcmp(action, "status", true)) return LoadTest_Status(playerid);
    if (!strcmp(action, "stop", true))
    {
        if (!gLoadRunning) return LoadTest_Status(playerid);
        LoadTest_Stop("manual");
        return 1;
    }
    if (!strcmp(action, "start", true))
    {
        if (!gAdminDuty[playerid])
            return SendClientMessage(playerid, ADMIN_ERROR, "[LoadTest] Enable /aduty first.");
        if (!strcmp(params, "start", true))
            return LoadTest_Start(playerid, LOADTEST_DEFAULT_NPCS, LOADTEST_DEFAULT_SECONDS);
        new count, seconds, scenario[12];
        if (!sscanf(params, "s[16]iis[12]", action, count, seconds, scenario))
        {
            if (!strcmp(scenario, "full", true)) return LoadTest_Start(playerid, count, seconds, true);
            if (!strcmp(scenario, "npc", true)) return LoadTest_Start(playerid, count, seconds);
            return SendClientMessage(playerid, ADMIN_ERROR, "[LoadTest] Scenario: npc or full.");
        }
        if (sscanf(params, "s[16]ii", action, count, seconds))
            return SendClientMessage(playerid, ADMIN_ERROR,
                "[LoadTest] Usage: /load start 50 120 full | /load start 50 120 npc");
        return LoadTest_Start(playerid, count, seconds);
    }
    return SendClientMessage(playerid, ADMIN_ERROR,
        "[LoadTest] /load start [count] [seconds] | status | stop.");
}
