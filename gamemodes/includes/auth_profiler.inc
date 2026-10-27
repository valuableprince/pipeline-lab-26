#if defined _tarkenso_auth_profiler_included
    #endinput
#endif
#define _tarkenso_auth_profiler_included

// AUTH-PROFILE-01: low-overhead timing diagnostics, no credentials/usernames.
// Timings are wall-clock callback latencies or synchronous Pawn section times,
// never CPU utilization. CSV is grouped by server-local calendar day.
#define AUTHPROF_DIR "auth_profiles"
#define AUTHPROF_HEADER "unix_time,player_id,session_token,step,duration_ms,ok,tickrate\n"
#define AUTHPROF_WARN_MS (100)

new gAuthProfDbStart[MAX_PLAYERS];
new gAuthProfHashStart[MAX_PLAYERS];
new gAuthProfWorldStart[MAX_PLAYERS];
new gAuthProfSpawnStart[MAX_PLAYERS];
new bool:gAuthProfDbPending[MAX_PLAYERS];
new bool:gAuthProfHashPending[MAX_PLAYERS];
new bool:gAuthProfWorldPending[MAX_PLAYERS];
new bool:gAuthProfSpawnPending[MAX_PLAYERS];
new gAuthProfLogFile[90];
new gAuthProfWarnedFile[90];

stock AuthProfiler_Reset(playerid)
{
    gAuthProfDbPending[playerid] = false;
    gAuthProfHashPending[playerid] = false;
    gAuthProfWorldPending[playerid] = false;
    gAuthProfSpawnPending[playerid] = false;
    return 1;
}

stock AuthProfiler_EnsureFile()
{
    new year, month, day, path[90];
    getdate(year, month, day);
    format(path, sizeof(path), AUTHPROF_DIR "/%04d-%02d-%02d.csv", year, month, day);
    if (!strcmp(path, gAuthProfLogFile)) return 1;
    fcreatedir(AUTHPROF_DIR); // Existing parent is fine.
    if (!fexist(path))
    {
        new File:header = fopen(path, io_write);
        if (header)
        {
            new written = fwrite(header, AUTHPROF_HEADER);
            fclose(header);
            if (written > 0)
            {
                format(gAuthProfLogFile, sizeof(gAuthProfLogFile), "%s", path);
                return 1;
            }
        }
    }
    else
    {
        format(gAuthProfLogFile, sizeof(gAuthProfLogFile), "%s", path);
        return 1;
    }
    if (strcmp(path, gAuthProfWarnedFile))
    {
        printf("[AUTH-PROFILE] WARNING: cannot create profile CSV %s", path);
        format(gAuthProfWarnedFile, sizeof(gAuthProfWarnedFile), "%s", path);
    }
    return 0;
}

stock AuthProfiler_Record(playerid, const step[], duration, bool:ok = true)
{
    if (!IsPlayerConnected(playerid) || IsPlayerNPC(playerid)) return 0;
    if (duration < 0) duration = 0; // Tick counter wrap / invalid sample.
    if (!AuthProfiler_EnsureFile()) return 0;
    new File:out = fopen(gAuthProfLogFile, io_append);
    if (!out) return 0;
    new row[180];
    format(row, sizeof(row), "%d,%d,%d,%s,%d,%d,%d\n",
        gettime(), playerid, gAuthToken[playerid], step,
        duration, _:ok, GetServerTickRate());
    new written = fwrite(out, row);
    fclose(out);
    if (duration >= AUTHPROF_WARN_MS)
    {
        new message[190];
        format(message, sizeof(message),
            "[AUTH-PROFILE] Медленный этап %s: %d мс, ID=%d (время ожидания, не CPU)",
            step, duration, playerid);
        ConsoleRU_Log(message);
    }
    return written > 0;
}

stock AuthProfiler_Step(playerid, const step[], start, bool:ok = true)
{
    return AuthProfiler_Record(playerid, step, GetTickCount() - start, ok);
}

stock AuthProfiler_DBStart(playerid)
{
    gAuthProfDbPending[playerid] = true;
    gAuthProfDbStart[playerid] = GetTickCount();
    return 1;
}

stock AuthProfiler_DBEnd(playerid, bool:ok = true)
{
    if (!gAuthProfDbPending[playerid]) return 0;
    gAuthProfDbPending[playerid] = false;
    return AuthProfiler_Step(playerid, "account_sql_callback", gAuthProfDbStart[playerid], ok);
}

stock AuthProfiler_BcryptStart(playerid)
{
    gAuthProfHashPending[playerid] = true;
    gAuthProfHashStart[playerid] = GetTickCount();
    return 1;
}

stock AuthProfiler_BcryptEnd(playerid, bool:registered, bool:ok)
{
    if (!gAuthProfHashPending[playerid]) return 0;
    gAuthProfHashPending[playerid] = false;
    if (registered)
        return AuthProfiler_Step(playerid, "bcrypt_hash_callback", gAuthProfHashStart[playerid], ok);
    return AuthProfiler_Step(playerid, "bcrypt_verify_callback", gAuthProfHashStart[playerid], ok);
}

stock AuthProfiler_WorldStart(playerid)
{
    gAuthProfWorldPending[playerid] = true;
    gAuthProfWorldStart[playerid] = GetTickCount();
    return 1;
}

stock AuthProfiler_WorldEnd(playerid, bool:ok = true)
{
    if (!gAuthProfWorldPending[playerid]) return 0;
    gAuthProfWorldPending[playerid] = false;
    return AuthProfiler_Step(playerid, "world_enter_sync_total", gAuthProfWorldStart[playerid], ok);
}

stock AuthProfiler_FirstSpawnBegin(playerid)
{
    if (gAuthState[playerid] != AUTH_READY || !gAuthProfSpawnPending[playerid]) return 0;
    gAuthProfSpawnStart[playerid] = GetTickCount();
    return 1;
}

stock AuthProfiler_FirstSpawnEnd(playerid)
{
    if (!gAuthProfSpawnPending[playerid]) return 0;
    gAuthProfSpawnPending[playerid] = false;
    return AuthProfiler_Step(playerid, "first_spawn_handler", gAuthProfSpawnStart[playerid]);
}
