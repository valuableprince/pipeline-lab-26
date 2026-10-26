#if defined _tarkenso_loadtelemetry_included
    #endinput
#endif
#define _tarkenso_loadtelemetry_included

// LOADTEST-05: exact nearest-rank latency quantiles and bounded throughput counters.
// No operating-system CPU/RAM information is available from Pawn in this module.
#if defined TARKENSO_DB_PRIMARY
#define TELEMETRY_MAX_SQL (LOADTEST_MAX_NPCS * 3)
#define TELEMETRY_HEADER "elapsed_sec,sql_p50_ms,sql_p95_ms,sql_p99_ms,hash_p95_ms,hash_p99_ms,verify_p95_ms,verify_p99_ms,cycle_completed,cycle_avg_ms,cycle_p95_ms,cycle_p99_ms,registration_rate_x100,authentication_rate_x100,peak_registrations_1s,peak_authentications_1s,phase\n"

new gTelemetrySQL[TELEMETRY_MAX_SQL];
new gTelemetryHash[LOADTEST_MAX_NPCS];
new gTelemetryVerify[LOADTEST_MAX_NPCS];
new gTelemetryCycle[LOADTEST_MAX_NPCS];
new gTelemetryCycleStarted[LOADTEST_MAX_NPCS];
new gTelemetrySQLCount, gTelemetryHashCount, gTelemetryVerifyCount;
new gTelemetryCycleCount, gTelemetryCycleSum;
new gTelemetryRegPerSecond[LOADTEST_MAX_SECONDS + 1];
new gTelemetryAuthPerSecond[LOADTEST_MAX_SECONDS + 1];
new gTelemetryPeakReg, gTelemetryPeakAuth, gTelemetryIOErrors;
new gTelemetryPath[125];

stock Telemetry_Quantile(const values[], count, percentile)
{
    if (count < 1 || count > TELEMETRY_MAX_SQL ||
        percentile < 1 || percentile > 100) return -1;
    // 300 integer samples max: bounded insertion-sort of a local copy.
    new sorted[TELEMETRY_MAX_SQL];
    for (new i = 0; i < count; i++)
    {
        new value = values[i], j = i - 1;
        while (j >= 0 && sorted[j] > value)
        {
            sorted[j + 1] = sorted[j];
            j--;
        }
        sorted[j + 1] = value;
    }
    return sorted[(count * percentile + 99) / 100 - 1];
}

stock Telemetry_Rate100(count, elapsed)
{
    return elapsed > 0 ? (count * 100) / elapsed : 0;
}

stock Telemetry_CountSecond(bool:registration)
{
    new second = gettime() - gLoadStartedAt;
    if (second < 0) second = 0;
    if (second > LOADTEST_MAX_SECONDS) second = LOADTEST_MAX_SECONDS;
    if (registration)
    {
        gTelemetryRegPerSecond[second]++;
        if (gTelemetryRegPerSecond[second] > gTelemetryPeakReg)
            gTelemetryPeakReg = gTelemetryRegPerSecond[second];
    }
    else
    {
        gTelemetryAuthPerSecond[second]++;
        if (gTelemetryAuthPerSecond[second] > gTelemetryPeakAuth)
            gTelemetryPeakAuth = gTelemetryAuthPerSecond[second];
    }
    return 1;
}

stock Telemetry_Open()
{
    // The legacy report is already reserved; derive a second, uniquely named file.
    new stem[96];
    strmid(stem, gLoadReportFile, 0, strlen(gLoadReportFile) - 4, sizeof(stem));
    format(gTelemetryPath, sizeof(gTelemetryPath), "%s_advanced.csv", stem);
    new File:out = fopen(gTelemetryPath, io_write);
    if (!out) return 0;
    new written = fwrite(out, TELEMETRY_HEADER);
    fclose(out);
    if (written <= 0)
    {
        fremove(gTelemetryPath);
        return 0;
    }
    gTelemetrySQLCount = 0;
    gTelemetryHashCount = 0;
    gTelemetryVerifyCount = 0;
    gTelemetryCycleCount = 0;
    gTelemetryCycleSum = 0;
    gTelemetryPeakReg = 0;
    gTelemetryPeakAuth = 0;
    gTelemetryIOErrors = 0;
    for (new i = 0; i < LOADTEST_MAX_NPCS; i++) gTelemetryCycleStarted[i] = 0;
    for (new j = 0; j <= LOADTEST_MAX_SECONDS; j++)
    {
        gTelemetryRegPerSecond[j] = 0;
        gTelemetryAuthPerSecond[j] = 0;
    }
    return 1;
}

stock Telemetry_RecordSQL(ms)
{
    if (gTelemetrySQLCount < TELEMETRY_MAX_SQL)
        gTelemetrySQL[gTelemetrySQLCount++] = ms;
    return 1;
}

stock Telemetry_RecordBcrypt(ms, bool:verify)
{
    if (verify)
    {
        if (gTelemetryVerifyCount < LOADTEST_MAX_NPCS)
            gTelemetryVerify[gTelemetryVerifyCount++] = ms;
    }
    else
    {
        if (gTelemetryHashCount < LOADTEST_MAX_NPCS)
            gTelemetryHash[gTelemetryHashCount++] = ms;
    }
    return 1;
}

stock Telemetry_StartCycle(slot)
{
    gTelemetryCycleStarted[slot] = GetTickCount();
    return 1;
}

stock Telemetry_FinishCycle(slot)
{
    new ms = GetTickCount() - gTelemetryCycleStarted[slot];
    if (ms < 0) ms = 0;
    if (gTelemetryCycleCount < LOADTEST_MAX_NPCS)
        gTelemetryCycle[gTelemetryCycleCount++] = ms;
    gTelemetryCycleSum += ms;
    return 1;
}

stock Telemetry_Sample(const phase[])
{
    new File:out = fopen(gTelemetryPath, io_append);
    if (!out)
    {
        gTelemetryIOErrors++;
        return 0;
    }
    new elapsed = gettime() - gLoadStartedAt;
    if (elapsed < 0) elapsed = 0;
    new row[300];
    format(row, sizeof(row),
        "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%s\n",
        elapsed,
        Telemetry_Quantile(gTelemetrySQL, gTelemetrySQLCount, 50),
        Telemetry_Quantile(gTelemetrySQL, gTelemetrySQLCount, 95),
        Telemetry_Quantile(gTelemetrySQL, gTelemetrySQLCount, 99),
        Telemetry_Quantile(gTelemetryHash, gTelemetryHashCount, 95),
        Telemetry_Quantile(gTelemetryHash, gTelemetryHashCount, 99),
        Telemetry_Quantile(gTelemetryVerify, gTelemetryVerifyCount, 95),
        Telemetry_Quantile(gTelemetryVerify, gTelemetryVerifyCount, 99),
        gTelemetryCycleCount,
        gTelemetryCycleCount ? gTelemetryCycleSum / gTelemetryCycleCount : -1,
        Telemetry_Quantile(gTelemetryCycle, gTelemetryCycleCount, 95),
        Telemetry_Quantile(gTelemetryCycle, gTelemetryCycleCount, 99),
        Telemetry_Rate100(gBenchRegistered, elapsed),
        Telemetry_Rate100(gBenchAuthenticated, elapsed),
        gTelemetryPeakReg, gTelemetryPeakAuth, phase);
    new written = fwrite(out, row);
    fclose(out);
    if (written <= 0) gTelemetryIOErrors++;
    return written > 0;
}

stock Telemetry_WriteSummary()
{
    new File:out = fopen(gBenchSummaryPath, io_append);
    if (!out)
    {
        printf("[LOADTELEMETRY] ERROR: cannot append advanced summary %s", gBenchSummaryPath);
        return 0;
    }
    new line[240];
    format(line, sizeof(line),
        "EXACT SQL n=%d p50=%dms p95=%dms p99=%dms (callback round-trip)\n",
        gTelemetrySQLCount,
        Telemetry_Quantile(gTelemetrySQL, gTelemetrySQLCount, 50),
        Telemetry_Quantile(gTelemetrySQL, gTelemetrySQLCount, 95),
        Telemetry_Quantile(gTelemetrySQL, gTelemetrySQLCount, 99));
    fwrite(out, line);
    format(line, sizeof(line),
        "EXACT bcrypt hash n=%d p95=%dms p99=%dms; verify n=%d p95=%dms p99=%dms\n",
        gTelemetryHashCount,
        Telemetry_Quantile(gTelemetryHash, gTelemetryHashCount, 95),
        Telemetry_Quantile(gTelemetryHash, gTelemetryHashCount, 99),
        gTelemetryVerifyCount,
        Telemetry_Quantile(gTelemetryVerify, gTelemetryVerifyCount, 95),
        Telemetry_Quantile(gTelemetryVerify, gTelemetryVerifyCount, 99));
    fwrite(out, line);
    format(line, sizeof(line),
        "Full account cycle n=%d avg=%dms p95=%dms p99=%dms\n",
        gTelemetryCycleCount,
        gTelemetryCycleCount ? gTelemetryCycleSum / gTelemetryCycleCount : -1,
        Telemetry_Quantile(gTelemetryCycle, gTelemetryCycleCount, 95),
        Telemetry_Quantile(gTelemetryCycle, gTelemetryCycleCount, 99));
    fwrite(out, line);
    new elapsed = gettime() - gLoadStartedAt;
    new regRate = Telemetry_Rate100(gBenchRegistered, elapsed);
    new authRate = Telemetry_Rate100(gBenchAuthenticated, elapsed);
    format(line, sizeof(line),
        "Throughput cumulative: registrations=%d.%02d/s authorizations=%d.%02d/s; peak one-second reg=%d auth=%d\n",
        regRate / 100, regRate % 100, authRate / 100, authRate % 100,
        gTelemetryPeakReg, gTelemetryPeakAuth);
    fwrite(out, line);
    fwrite(out, "Quantiles: exact nearest-rank (bounded observations); -1 means no samples. Peak is measured per server-clock second.\n");
    fwrite(out, "CPU_PERCENT=UNAVAILABLE; RAM_MB=UNAVAILABLE; NETWORK_RTT=UNAVAILABLE (requires host monitoring/client).\n");
    fclose(out);
    return 1;
}

stock Telemetry_Progress(playerid)
{
    new msg[150];
    format(msg, sizeof(msg),
        "[Load] SQL p95=%dms p99=%dms; full cycle p95=%dms; peaks reg=%d/s auth=%d/s",
        Telemetry_Quantile(gTelemetrySQL, gTelemetrySQLCount, 95),
        Telemetry_Quantile(gTelemetrySQL, gTelemetrySQLCount, 99),
        Telemetry_Quantile(gTelemetryCycle, gTelemetryCycleCount, 95),
        gTelemetryPeakReg, gTelemetryPeakAuth);
    return SendClientMessage(playerid, ADMIN_COLOR, msg);
}

#else
stock Telemetry_Open() { return 0; }
stock Telemetry_RecordSQL(ms) { #pragma unused ms return 1; }
stock Telemetry_RecordBcrypt(ms, bool:verify) { #pragma unused ms, verify return 1; }
stock Telemetry_StartCycle(slot) { #pragma unused slot return 1; }
stock Telemetry_FinishCycle(slot) { #pragma unused slot return 1; }
stock Telemetry_CountSecond(bool:registration) { #pragma unused registration return 1; }
stock Telemetry_Sample(const phase[]) { #pragma unused phase return 1; }
stock Telemetry_WriteSummary() { return 1; }
stock Telemetry_Progress(playerid) { #pragma unused playerid return 1; }
#endif
