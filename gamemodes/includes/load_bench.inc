#if defined _tarkenso_load_bench_included
    #endinput
#endif
#define _tarkenso_load_bench_included

// LOADTEST-04. Optional, isolated synthetic account benchmark. Not real clients.
// All SQL targets loadtest_virtual_accounts, NEVER accounts/economy tables.
#define BENCH_METRICS_HEADER "elapsed_sec,npcs,registered,authenticated,saved,sql_completed,sql_errors,sql_avg_ms,sql_max_ms,hash_completed,hash_avg_ms,verify_completed,verify_avg_ms,bcrypt_max_ms,tickrate,heartbeat_ms,heartbeat_delays_delta,phase\n"
#define BENCH_TEST_PASSWORD "LoadBenchTest!2026"
#define BENCH_HASH_LIMIT (2)
#define BENCH_SQL_LIMIT (8)

#if defined TARKENSO_DB_PRIMARY

new bool:gBenchActive;
new gBenchToken;
new gBenchOwner;
new gBenchStage[LOADTEST_MAX_NPCS]; // 0=new,1=hash,2=INSERT,3=SELECT,4=verify,5=UPDATE,6=done,7=failed.
new gBenchStartedMs[LOADTEST_MAX_NPCS];
new gBenchPendingHash, gBenchPendingSQL;
new gBenchRegistered, gBenchAuthenticated, gBenchSaved;
new gBenchSQLCount, gBenchSQLFailures, gBenchSQLSum, gBenchSQLMax;
new gBenchHashCount, gBenchHashSum, gBenchVerifyCount, gBenchVerifySum, gBenchBcryptMax;
new gBenchWorstRate, gBenchBaselineRate, gBenchStartGuardDelays;
new gBenchSQLBuckets[7], gBenchBcryptBuckets[7];
new gBenchCursor;
new gBenchRunKey[96], gBenchMetricsPath[125], gBenchSummaryPath[125];
new gBenchLogFailures;
new gBenchFailureCount;
// Continuous workload: one outstanding task per virtual account, <=4 per tick.
// 6 ready, 8 read in flight, 9 write in flight; no real player data touched.
new gBenchNextAction[LOADTEST_MAX_NPCS];
new gBenchActions, gBenchReads, gBenchWrites, gBenchPaydayWrites;
new gBenchActionErrors, gBenchLastPaydayMinute;
new gBenchContinuousPath[125];
new bool:gBenchLastReady;
new gBenchLastVerdict[16], gBenchLastReason[32];
new gBenchLastTarget, gBenchLastSpawned;

forward Telemetry_Open();
forward Telemetry_RecordSQL(ms);
forward Telemetry_RecordBcrypt(ms, bool:verify);
forward Telemetry_StartCycle(slot);
forward Telemetry_FinishCycle(slot);
forward Telemetry_CountSecond(bool:registration);
forward Telemetry_Sample(const phase[]);
forward Telemetry_WriteSummary();
forward Telemetry_Progress(playerid);

forward Bench_OnHash(playerid, token, slot);
forward Bench_OnInserted(playerid, token, slot);
forward Bench_OnLoaded(playerid, token, slot);
forward Bench_OnVerified(playerid, bool:success, token, slot);
forward Bench_OnSaved(playerid, token, slot);
forward Bench_OnActivityRead(playerid, token, slot);
forward Bench_OnActivityWrite(playerid, token, slot, payday);

stock Bench_Bucket(ms, bool:bcryptOperation)
{
    if (bcryptOperation)
    {
        if (ms <= 25) return 0;
        if (ms <= 50) return 1;
        if (ms <= 100) return 2;
        if (ms <= 250) return 3;
        if (ms <= 500) return 4;
        if (ms <= 1000) return 5;
        return 6;
    }
    if (ms <= 10) return 0;
    if (ms <= 25) return 1;
    if (ms <= 50) return 2;
    if (ms <= 100) return 3;
    if (ms <= 250) return 4;
    if (ms <= 500) return 5;
    return 6;
}

stock Bench_P95Bucket(bool:bcryptOperation)
{
    new total = bcryptOperation ? gBenchHashCount + gBenchVerifyCount : gBenchSQLCount;
    if (!total) return -1;
    new wanted = (total * 95 + 99) / 100;
    new running = 0;
    for (new i = 0; i < 7; i++)
    {
        running += bcryptOperation ? gBenchBcryptBuckets[i] : gBenchSQLBuckets[i];
        if (running >= wanted)
        {
            if (bcryptOperation)
            {
                switch (i)
                {
                    case 0: return 25;
                    case 1: return 50;
                    case 2: return 100;
                    case 3: return 250;
                    case 4: return 500;
                    case 5: return 1000;
                    default: return 1001; // p95 > 1000ms
                }
            }
            switch (i)
            {
                case 0: return 10;
                case 1: return 25;
                case 2: return 50;
                case 3: return 100;
                case 4: return 250;
                case 5: return 500;
                default: return 501; // p95 > 500ms
            }
        }
    }
    return -1;
}

stock Bench_RecordSQL(ms, bool:ok)
{
    if (ms < 0) ms = 0;
    if (ms > gBenchSQLMax) gBenchSQLMax = ms;
    gBenchSQLCount++;
    gBenchSQLSum += ms;
    gBenchSQLBuckets[Bench_Bucket(ms, false)]++;
    if (!ok) gBenchSQLFailures++;
    Telemetry_RecordSQL(ms);
    return 1;
}

stock Bench_RecordBcrypt(ms, bool:verify)
{
    if (ms < 0) ms = 0;
    if (ms > gBenchBcryptMax) gBenchBcryptMax = ms;
    if (verify)
    {
        gBenchVerifyCount++;
        gBenchVerifySum += ms;
    }
    else
    {
        gBenchHashCount++;
        gBenchHashSum += ms;
    }
    gBenchBcryptBuckets[Bench_Bucket(ms, true)]++;
    Telemetry_RecordBcrypt(ms, verify);
    return 1;
}

stock Bench_Valid(playerid, token, slot, stage)
{
    return gBenchActive && gLoadRunning && token == gBenchToken &&
        playerid == gBenchOwner && playerid == gLoadOwner &&
        IsPlayerConnected(playerid) && Admin_IsFounder(playerid) &&
        slot >= 0 && slot < gLoadNPCCount && gBenchStage[slot] == stage;
}

stock Bench_MarkFailed(slot)
{
    gBenchStage[slot] = 7;
    gBenchFailureCount++;
    // Stop on the first benchmark error; do not leave malformed test load running.
    LoadTest_Stop("benchmark_error");
    return 1;
}

stock bool:Bench_Begin(playerid)
{
    if (!DBPrimary_IsReady()) return false;
    // One preflight schema query, before any bots/hashes. Migration 013 is mandatory.
    new Cache:check = mysql_query(gDBPrimary,
        "SELECT version FROM schema_migrations WHERE version=13 LIMIT 1");
    if (check == Cache:0) return false;
    new rows;
    new bool:ok = mysql_errno(gDBPrimary) == 0 &&
        cache_get_row_count(rows) && rows == 1;
    cache_delete(check);
    if (!ok) return false;

    gBenchActive = false;
    gBenchLastReady = false;
    gBenchRunKey[0] = EOS;
    // The directory identifies a run in MySQL, independent of report filenames.
    format(gBenchRunKey, sizeof(gBenchRunKey), "%s", gLoadReportDir);
    format(gBenchMetricsPath, sizeof(gBenchMetricsPath), "%s/metrics.csv", gLoadReportDir);
    format(gBenchSummaryPath, sizeof(gBenchSummaryPath), "%s/summary.txt", gLoadReportDir);
    format(gBenchContinuousPath, sizeof(gBenchContinuousPath), "%s/activity.csv", gLoadReportDir);
    new File:activity = fopen(gBenchContinuousPath, io_write);
    if (!activity) return false;
    new activityHeader = fwrite(activity, "elapsed_sec,reads,writes,payday_writes,action_errors,pending_sql,phase\n");
    fclose(activity);
    if (activityHeader <= 0) return false;
    new File:metricsHandle = fopen(gBenchMetricsPath, io_write);
    if (!metricsHandle) return false;
    new written = fwrite(metricsHandle, BENCH_METRICS_HEADER);
    fclose(metricsHandle);
    if (written <= 0)
    {
        fremove(gBenchMetricsPath);
        return false;
    }
    if (!Telemetry_Open())
    {
        fremove(gBenchMetricsPath);
        return false;
    }

    gBenchToken++;
    if (gBenchToken <= 0) gBenchToken = 1;
    gBenchOwner = playerid;
    gBenchPendingHash = 0;
    gBenchPendingSQL = 0;
    gBenchRegistered = 0;
    gBenchAuthenticated = 0;
    gBenchSaved = 0;
    gBenchSQLCount = 0;
    gBenchSQLFailures = 0;
    gBenchSQLSum = 0;
    gBenchSQLMax = 0;
    gBenchHashCount = 0;
    gBenchHashSum = 0;
    gBenchVerifyCount = 0;
    gBenchVerifySum = 0;
    gBenchBcryptMax = 0;
    gBenchFailureCount = 0;
    gBenchActions = 0;
    gBenchReads = 0;
    gBenchWrites = 0;
    gBenchPaydayWrites = 0;
    gBenchActionErrors = 0;
    gBenchLastPaydayMinute = -1;
    for (new a = 0; a < LOADTEST_MAX_NPCS; a++) gBenchNextAction[a] = 0;
    gBenchLogFailures = 0;
    gBenchCursor = 0;
    gBenchBaselineRate = GetServerTickRate();
    gBenchWorstRate = gBenchBaselineRate;
    gBenchStartGuardDelays = gGuardDelayedHeartbeatCount;
    for (new i = 0; i < LOADTEST_MAX_NPCS; i++) gBenchStage[i] = 0;
    for (new j = 0; j < 7; j++)
    {
        gBenchSQLBuckets[j] = 0;
        gBenchBcryptBuckets[j] = 0;
    }
    gBenchActive = true;
    return true;
}

stock Bench_Sample(const phase[])
{
    if (!gBenchActive) return 0;
    new tickrate = GetServerTickRate();
    if (tickrate < gBenchWorstRate) gBenchWorstRate = tickrate;
    // LOADTEST core measures tickrate every second, not only on CSV samples.
    if (gLoadMinTickrate < gBenchWorstRate) gBenchWorstRate = gLoadMinTickrate;
    new File:metricsHandle = fopen(gBenchMetricsPath, io_append);
    if (!metricsHandle)
    {
        gBenchLogFailures++;
        return 0;
    }
    new line[310];
    format(line, sizeof(line),
        "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%s\n",
        gettime() - gLoadStartedAt, gLoadNPCCount,
        gBenchRegistered, gBenchAuthenticated, gBenchSaved,
        gBenchSQLCount, gBenchSQLFailures,
        gBenchSQLCount ? gBenchSQLSum / gBenchSQLCount : 0, gBenchSQLMax,
        gBenchHashCount, gBenchHashCount ? gBenchHashSum / gBenchHashCount : 0,
        gBenchVerifyCount, gBenchVerifyCount ? gBenchVerifySum / gBenchVerifyCount : 0,
        gBenchBcryptMax, tickrate, gGuardLastHeartbeatMs,
        gGuardDelayedHeartbeatCount - gBenchStartGuardDelays, phase);
    new bytesWritten = fwrite(metricsHandle, line);
    fclose(metricsHandle);
    if (bytesWritten <= 0) gBenchLogFailures++;
    if (!Telemetry_Sample(phase)) gBenchLogFailures++;
    new File:activityFile = fopen(gBenchContinuousPath, io_append);
    if (activityFile)
    {
        new activityLine[150];
        format(activityLine, sizeof(activityLine), "%d,%d,%d,%d,%d,%d,%s\n",
            gettime() - gLoadStartedAt, gBenchReads, gBenchWrites,
            gBenchPaydayWrites, gBenchActionErrors, gBenchPendingSQL, phase);
        if (fwrite(activityFile, activityLine) <= 0) gBenchLogFailures++;
        fclose(activityFile);
    }
    else gBenchLogFailures++;
    return bytesWritten > 0;
}

stock Bench_Finish(const reason[])
{
    if (!gBenchActive) return 1;
    Bench_Sample(reason);
    new bool:complete = !strcmp(reason, "complete") &&
        gBenchSaved == gLoadNPCTarget && gLoadNPCCount == gLoadNPCTarget;
    new bool:stable = gBenchWorstRate >= LOADTEST_MIN_TICKRATE &&
        gBenchSQLFailures == 0 && gBenchFailureCount == 0 &&
        gBenchActionErrors == 0 && !gBenchLogFailures;
    new verdict[16];
    if (!stable) format(verdict, sizeof(verdict), "FAIL");
    else if (!complete) format(verdict, sizeof(verdict), "INCOMPLETE");
    else format(verdict, sizeof(verdict), "PASS");
    format(gBenchLastVerdict, sizeof(gBenchLastVerdict), "%s", verdict);
    format(gBenchLastReason, sizeof(gBenchLastReason), "%s", reason);
    gBenchLastTarget = gLoadNPCTarget;
    gBenchLastSpawned = gLoadNPCCount;
    gBenchLastReady = true;
    new File:summary = fopen(gBenchSummaryPath, io_write);
    if (summary)
    {
        new line[240];
        format(line, sizeof(line),
            "TARKENSO LOADTEST-04 | %s | run=%s | reason=%s\n",
            verdict, gBenchRunKey, reason);
        fwrite(summary, line);
        format(line, sizeof(line),
            "NPC=%d/%d | registered=%d | verified=%d | saved=%d | failed=%d\n",
            gLoadNPCCount, gLoadNPCTarget, gBenchRegistered,
            gBenchAuthenticated, gBenchSaved, gBenchFailureCount);
        fwrite(summary, line);
        format(line, sizeof(line),
            "SQL completed=%d | errors=%d | mean=%dms | max=%dms | p95_bucket_upper=%dms\n",
            gBenchSQLCount, gBenchSQLFailures,
            gBenchSQLCount ? gBenchSQLSum / gBenchSQLCount : 0,
            gBenchSQLMax, Bench_P95Bucket(false));
        fwrite(summary, line);
        format(line, sizeof(line),
            "bcrypt hash=%d mean=%dms | verify=%d mean=%dms | max=%dms | p95_bucket_upper=%dms\n",
            gBenchHashCount, gBenchHashCount ? gBenchHashSum / gBenchHashCount : 0,
            gBenchVerifyCount, gBenchVerifyCount ? gBenchVerifySum / gBenchVerifyCount : 0,
            gBenchBcryptMax, Bench_P95Bucket(true));
        fwrite(summary, line);
        format(line, sizeof(line),
            "Tickrate baseline=%d min=%d threshold=%d | guard_delays_delta=%d | pending_sql=%d pending_bcrypt=%d | log_errors=%d\n",
            gBenchBaselineRate, gBenchWorstRate, LOADTEST_MIN_TICKRATE,
            gGuardDelayedHeartbeatCount - gBenchStartGuardDelays,
            gBenchPendingSQL, gBenchPendingHash, gBenchLogFailures);
        fwrite(summary, line);
        fwrite(summary, "p95_bucket_upper is a histogram bucket bound, not an exact percentile. 501=SQL>500ms; 1001=bcrypt>1000ms.\n");
        format(line, sizeof(line),
            "CONTINUOUS: reads=%d writes=%d payday_like_writes=%d failures=%d pending_sql=%d\n",
            gBenchReads, gBenchWrites, gBenchPaydayWrites,
            gBenchActionErrors, gBenchPendingSQL);
        fwrite(summary, line);
        fwrite(summary, "PayDay-like is a bounded update on isolated test rows, NOT the real PayDay handler.\n");
        fwrite(summary, "CPU/RAM, true player RTT and network throughput are NOT measured in this mode.\n");
        fwrite(summary, "Synthetic rows persist in loadtest_virtual_accounts until explicitly deleted by run_key; no real accounts are modified.\n");
        fclose(summary);
        if (!Telemetry_WriteSummary()) gBenchLogFailures++;
    }
    else
    {
        printf("[LOADBENCH] ERROR writing summary: %s", gBenchSummaryPath);
    }
    printf("[LOADBENCH] %s: sql=%d failures=%d hash=%d verify=%d saved=%d/%d report=%s",
        verdict, gBenchSQLCount, gBenchSQLFailures,
        gBenchHashCount, gBenchVerifyCount, gBenchSaved, gLoadNPCTarget, gBenchSummaryPath);
    gBenchActive = false;
    return 1;
}

stock Bench_Report(playerid)
{
    if (!gBenchLastReady)
        return SendClientMessage(playerid, ADMIN_ERROR,
            "[Load] No completed FULL-mode report in this server session.");
    new msg[150];
    format(msg, sizeof(msg),
        "[Load] Last FULL: %s (%s), NPC %d/%d; reg=%d auth=%d saved=%d",
        gBenchLastVerdict, gBenchLastReason, gBenchLastSpawned,
        gBenchLastTarget, gBenchRegistered, gBenchAuthenticated, gBenchSaved);
    SendClientMessage(playerid, ADMIN_COLOR, msg);
    format(msg, sizeof(msg),
        "[Load] SQL n=%d errors=%d avg=%dms max=%dms; tick min=%d",
        gBenchSQLCount, gBenchSQLFailures,
        gBenchSQLCount ? gBenchSQLSum / gBenchSQLCount : -1,
        gBenchSQLMax, gBenchWorstRate);
    SendClientMessage(playerid, ADMIN_COLOR, msg);
    Telemetry_Progress(playerid);
    format(msg, sizeof(msg), "[Load] Full report: %s", gBenchSummaryPath);
    SendClientMessage(playerid, ADMIN_COLOR, msg);
    return 1;
}

stock Bench_Progress(playerid)
{
    if (!gBenchActive) return 1;
    new msg[145];
    format(msg, sizeof(msg),
        "[LoadBench] Reg=%d Auth=%d Save=%d/%d SQL=%d errors=%d avg=%dms max=%dms",
        gBenchRegistered, gBenchAuthenticated, gBenchSaved, gLoadNPCTarget,
        gBenchSQLCount, gBenchSQLFailures,
        gBenchSQLCount ? gBenchSQLSum / gBenchSQLCount : 0, gBenchSQLMax);
    SendClientMessage(playerid, ADMIN_COLOR, msg);
    format(msg, sizeof(msg),
        "[LoadBench] bcrypt hashes=%d verify=%d max=%dms | min tickrate=%d | pending SQL=%d",
        gBenchHashCount, gBenchVerifyCount, gBenchBcryptMax,
        gBenchWorstRate, gBenchPendingSQL);
    SendClientMessage(playerid, ADMIN_COLOR, msg);
    Telemetry_Progress(playerid);
    format(msg, sizeof(msg),
        "[LoadBench] Activity read=%d write=%d payday=%d errors=%d pending=%d",
        gBenchReads, gBenchWrites, gBenchPaydayWrites,
        gBenchActionErrors, gBenchPendingSQL);
    SendClientMessage(playerid, ADMIN_COLOR, msg);
    return 1;
}

stock Bench_OnNPC()
{
    // Called by no extra hooks: newly created NPCs are discovered by Bench_Tick.
    return 1;
}

stock Bench_Tick()
{
    if (!gBenchActive || !gLoadRunning) return 1;
    if (gBenchSQLFailures || gBenchFailureCount || gBenchLogFailures)
    {
        LoadTest_Stop("benchmark_error");
        return 1;
    }
    // Hash only a small, bounded set at a time, through the same bcrypt plugin
    // used by real accounts. The player ID identifies the authenticated founder,
    // not an NPC; NPCs do not go through OnDialogResponse.
    for (new i = 0; i < gLoadNPCCount && gBenchPendingHash < BENCH_HASH_LIMIT &&
         gBenchPendingSQL < BENCH_SQL_LIMIT; i++)
    {
        new slot = (gBenchCursor + i) % gLoadNPCCount;
        if (gBenchStage[slot] != 0) continue;
        gBenchStage[slot] = 1;
        Telemetry_StartCycle(slot);
        gBenchStartedMs[slot] = GetTickCount();
        gBenchPendingHash++;
        bcrypt_hash(gBenchOwner, "Bench_OnHash", BENCH_TEST_PASSWORD,
            AUTH_COST, "ii", gBenchToken, slot);
    }
    gBenchCursor++;
    if (gLoadNPCCount) gBenchCursor %= gLoadNPCCount;
    // Controlled sustained workload: every completed account performs a
    // SELECT then UPDATE ~once every 10s, staggered; cap dispatch 4/tick.
    new nowSec = gettime() - gLoadStartedAt;
    new dispatched = 0;
    new paydayWindow = nowSec > 0 && (nowSec % 60) < 5;
    for (new slot = 0; slot < gLoadNPCCount && dispatched < 4 &&
         gBenchPendingSQL < BENCH_SQL_LIMIT; slot++)
    {
        if (gBenchStage[slot] != 6 || nowSec < gBenchNextAction[slot]) continue;
        new query[265];
        mysql_format(gDBPrimary, query, sizeof(query),
            "SELECT balance FROM loadtest_virtual_accounts WHERE run_key='%e' AND virtual_id=%d LIMIT 1",
            gBenchRunKey, slot);
        gBenchStage[slot] = 8;
        gBenchStartedMs[slot] = GetTickCount();
        gBenchPendingSQL++;
        if (!mysql_tquery(gDBPrimary, query, "Bench_OnActivityRead", "iii",
            gBenchOwner, gBenchToken, slot))
        {
            gBenchPendingSQL--;
            Bench_MarkFailed(slot);
            return 1;
        }
        // A minute-boundary PayDay-like burst is throttled by the same global
        // SQL in-flight budget; never fires the live economy handler.
        gBenchNextAction[slot] = nowSec + (paydayWindow ? 1 : 10);
        dispatched++;
    }
    if ((gettime() - gLoadStartedAt) % 5 == 0) Bench_Sample("running");
    return 1;
}

public Bench_OnHash(playerid, token, slot)
{
    if (token != gBenchToken) return 1;
    if (gBenchPendingHash > 0) gBenchPendingHash--;
    if (!Bench_Valid(playerid, token, slot, 1)) return 1;
    new hash[61];
    bcrypt_get_hash(hash, sizeof(hash));
    if (strlen(hash) != 60) return Bench_MarkFailed(slot);
    new nick[24], q[390];
    format(nick, sizeof(nick), "LT_%03d", slot);
    mysql_format(gDBPrimary, q, sizeof(q),
        "INSERT INTO loadtest_virtual_accounts (run_key,virtual_id,nickname,password_hash) VALUES ('%e',%d,'%e','%e')",
        gBenchRunKey, slot, nick, hash);
    Bench_RecordBcrypt(GetTickCount() - gBenchStartedMs[slot], false);
    gBenchStage[slot] = 2;
    gBenchStartedMs[slot] = GetTickCount();
    gBenchPendingSQL++;
    if (!mysql_tquery(gDBPrimary, q, "Bench_OnInserted", "iii",
        playerid, token, slot))
    {
        gBenchPendingSQL--;
        return Bench_MarkFailed(slot);
    }
    return 1;
}

public Bench_OnInserted(playerid, token, slot)
{
    if (token != gBenchToken) return 1;
    if (gBenchPendingSQL > 0) gBenchPendingSQL--;
    if (!Bench_Valid(playerid, token, slot, 2)) return 1;
    new bool:ok = cache_affected_rows() == 1;
    Bench_RecordSQL(GetTickCount() - gBenchStartedMs[slot], ok);
    if (!ok) return Bench_MarkFailed(slot);
    gBenchRegistered++;
    Telemetry_CountSecond(true);
    new q[240];
    mysql_format(gDBPrimary, q, sizeof(q),
        "SELECT password_hash FROM loadtest_virtual_accounts WHERE run_key='%e' AND virtual_id=%d LIMIT 1",
        gBenchRunKey, slot);
    gBenchStage[slot] = 3;
    gBenchStartedMs[slot] = GetTickCount();
    gBenchPendingSQL++;
    if (!mysql_tquery(gDBPrimary, q, "Bench_OnLoaded", "iii", playerid, token, slot))
    {
        gBenchPendingSQL--;
        return Bench_MarkFailed(slot);
    }
    return 1;
}

public Bench_OnLoaded(playerid, token, slot)
{
    if (token != gBenchToken) return 1;
    if (gBenchPendingSQL > 0) gBenchPendingSQL--;
    if (!Bench_Valid(playerid, token, slot, 3)) return 1;
    new rows, hash[61];
    new bool:ok = cache_get_row_count(rows) && rows == 1 &&
        cache_get_value_name(0, "password_hash", hash, sizeof(hash)) &&
        strlen(hash) == 60;
    Bench_RecordSQL(GetTickCount() - gBenchStartedMs[slot], ok);
    if (!ok) return Bench_MarkFailed(slot);
    gBenchStage[slot] = 4;
    gBenchStartedMs[slot] = GetTickCount();
    gBenchPendingHash++;
    bcrypt_verify(playerid, "Bench_OnVerified", BENCH_TEST_PASSWORD, hash,
        "ii", token, slot);
    return 1;
}

public Bench_OnVerified(playerid, bool:success, token, slot)
{
    if (token != gBenchToken) return 1;
    if (gBenchPendingHash > 0) gBenchPendingHash--;
    if (!Bench_Valid(playerid, token, slot, 4)) return 1;
    Bench_RecordBcrypt(GetTickCount() - gBenchStartedMs[slot], true);
    if (!success) return Bench_MarkFailed(slot);
    gBenchAuthenticated++;
    Telemetry_CountSecond(false);
    new q[250];
    mysql_format(gDBPrimary, q, sizeof(q),
        "UPDATE loadtest_virtual_accounts SET balance=balance+1 WHERE run_key='%e' AND virtual_id=%d",
        gBenchRunKey, slot);
    gBenchStage[slot] = 5;
    gBenchStartedMs[slot] = GetTickCount();
    gBenchPendingSQL++;
    if (!mysql_tquery(gDBPrimary, q, "Bench_OnSaved", "iii", playerid, token, slot))
    {
        gBenchPendingSQL--;
        return Bench_MarkFailed(slot);
    }
    return 1;
}

public Bench_OnSaved(playerid, token, slot)
{
    if (token != gBenchToken) return 1;
    if (gBenchPendingSQL > 0) gBenchPendingSQL--;
    if (!Bench_Valid(playerid, token, slot, 5)) return 1;
    new bool:ok = cache_affected_rows() == 1;
    Bench_RecordSQL(GetTickCount() - gBenchStartedMs[slot], ok);
    if (!ok) return Bench_MarkFailed(slot);
    gBenchStage[slot] = 6;
    gBenchSaved++;
    gBenchNextAction[slot] = gettime() - gLoadStartedAt + 2 + slot % 8;
    Telemetry_FinishCycle(slot);
    return 1;
}

public Bench_OnActivityRead(playerid, token, slot)
{
    if (token != gBenchToken) return 1;
    if (gBenchPendingSQL > 0) gBenchPendingSQL--;
    if (!Bench_Valid(playerid, token, slot, 8)) return 1;
    new rows, balance;
    new bool:ok = cache_get_row_count(rows) && rows == 1 &&
        cache_get_value_name_int(0, "balance", balance);
    Bench_RecordSQL(GetTickCount() - gBenchStartedMs[slot], ok);
    if (!ok)
    {
        gBenchActionErrors++;
        return Bench_MarkFailed(slot);
    }
    gBenchReads++;
    // The virtual action is a synthetic progression update only.
    new nowSec = gettime() - gLoadStartedAt;
    new bool:payday = nowSec > 0 && (nowSec % 60) < 5;
    new query[255];
    mysql_format(gDBPrimary, query, sizeof(query),
        "UPDATE loadtest_virtual_accounts SET balance=balance+%d WHERE run_key='%e' AND virtual_id=%d",
        payday ? 10 : 1, gBenchRunKey, slot);
    gBenchStage[slot] = 9;
    gBenchStartedMs[slot] = GetTickCount();
    gBenchPendingSQL++;
    if (!mysql_tquery(gDBPrimary, query, "Bench_OnActivityWrite", "iiii",
        playerid, token, slot, payday))
    {
        gBenchPendingSQL--;
        gBenchActionErrors++;
        return Bench_MarkFailed(slot);
    }
    return 1;
}

public Bench_OnActivityWrite(playerid, token, slot, payday)
{
    if (token != gBenchToken) return 1;
    if (gBenchPendingSQL > 0) gBenchPendingSQL--;
    if (!Bench_Valid(playerid, token, slot, 9)) return 1;
    new bool:ok = cache_affected_rows() == 1;
    Bench_RecordSQL(GetTickCount() - gBenchStartedMs[slot], ok);
    if (!ok)
    {
        gBenchActionErrors++;
        return Bench_MarkFailed(slot);
    }
    gBenchWrites++;
    if (payday) gBenchPaydayWrites++;
    gBenchStage[slot] = 6;
    return 1;
}

#else

stock bool:Bench_Begin(playerid)
{
    #pragma unused playerid
    return false;
}
stock Bench_Tick() { return 1; }
stock Bench_Sample(const phase[])
{
    #pragma unused phase
    return 1;
}
stock Bench_Finish(const reason[])
{
    #pragma unused reason
    return 1;
}
stock Bench_Progress(playerid)
{
    #pragma unused playerid
    return 1;
}
stock Bench_Report(playerid)
{
    return SendClientMessage(playerid, ADMIN_ERROR,
        "[Load] FULL benchmark requires MySQL build.");
}

#endif
