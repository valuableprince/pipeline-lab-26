#if defined _tarkenso_loadnet_included
    #endinput
#endif
#define _tarkenso_loadnet_included

// LOADNET-01: game-issued jobs, external Windows worker. NO shell execution in Pawn.
// Only the authenticated founder with /aduty may control a job.
#define LOADNET_MAX_CLIENTS (50)
#define LOADNET_MAX_SECONDS (600)
#define LOADNET_MIN_SECONDS (10)

forward LoadNet_OnQueued(playerid, authToken);
forward LoadNet_OnStopped(playerid, authToken);
forward LoadNet_OnStatus(playerid, authToken);

stock LoadNet_IsAuthorized(playerid)
{
    return Admin_IsFounder(playerid) && gAdminDuty[playerid] &&
        gDBPrimaryLoaded[playerid] && gDBPrimaryAccountId[playerid] > 0;
}

stock LoadNet_Start(playerid, count, duration, const mode[])
{
#if defined TARKENSO_DB_PRIMARY
    if (!DBPrimary_IsReady())
        return SendClientMessage(playerid, ADMIN_ERROR, "[LoadNet] MySQL is unavailable.");
    if (count < 1 || count > LOADNET_MAX_CLIENTS ||
        duration < LOADNET_MIN_SECONDS || duration > LOADNET_MAX_SECONDS)
        return SendClientMessage(playerid, ADMIN_ERROR,
            "[LoadNet] Clients 1..50, seconds 10..600.");
    if (strcmp(mode, "register", true) && strcmp(mode, "login", true))
        return SendClientMessage(playerid, ADMIN_ERROR, "[LoadNet] Mode: register or login.");
    // Unique active_slot allows only one active test; INSERT IGNORE avoids
    // reporting duplicate-key failures as general SQL errors.
    new q[380];
    mysql_format(gDBPrimary, q, sizeof(q),
        "INSERT IGNORE INTO loadtest_jobs (account_id,target_clients,duration_seconds,auth_mode,scenario,state) VALUES (%d,%d,%d,'%e','idle','PENDING')",
        gDBPrimaryAccountId[playerid], count, duration, mode);
    if (!mysql_tquery(gDBPrimary, q, "LoadNet_OnQueued", "ii",
        playerid, gAuthToken[playerid]))
        return SendClientMessage(playerid, ADMIN_ERROR, "[LoadNet] Could not queue request.");
    return SendClientMessage(playerid, ADMIN_COLOR, "[LoadNet] Creating test job...");
#else
    return SendClientMessage(playerid, ADMIN_ERROR, "[LoadNet] MySQL build is required.");
#endif
}

stock LoadNet_Stop(playerid)
{
#if defined TARKENSO_DB_PRIMARY
    if (!DBPrimary_IsReady()) return SendClientMessage(playerid, ADMIN_ERROR, "[LoadNet] MySQL is unavailable.");
    if (!mysql_tquery(gDBPrimary,
        "UPDATE loadtest_jobs SET state='STOP_REQUESTED',status_message='Stop requested from game' WHERE state IN ('PENDING','RUNNING')",
        "LoadNet_OnStopped", "ii", playerid, gAuthToken[playerid]))
        return SendClientMessage(playerid, ADMIN_ERROR, "[LoadNet] Could not request stop.");
    return 1;
#else
    return SendClientMessage(playerid, ADMIN_ERROR, "[LoadNet] MySQL build is required.");
#endif
}

stock LoadNet_Status(playerid)
{
#if defined TARKENSO_DB_PRIMARY
    if (!DBPrimary_IsReady()) return SendClientMessage(playerid, ADMIN_ERROR, "[LoadNet] MySQL is unavailable.");
    if (!mysql_tquery(gDBPrimary,
        "SELECT job_id,state,target_clients,duration_seconds,auth_mode,launched_clients,joined_clients,authenticated_clients,spawned_clients,status_message FROM loadtest_jobs ORDER BY job_id DESC LIMIT 1",
        "LoadNet_OnStatus", "ii", playerid, gAuthToken[playerid]))
        return SendClientMessage(playerid, ADMIN_ERROR, "[LoadNet] Could not read status.");
    return 1;
#else
    return SendClientMessage(playerid, ADMIN_ERROR, "[LoadNet] MySQL build is required.");
#endif
}

public LoadNet_OnQueued(playerid, authToken)
{
#if defined TARKENSO_DB_PRIMARY
    if (!IsPlayerConnected(playerid) ||
        gAuthToken[playerid] != authToken || !LoadNet_IsAuthorized(playerid)) return 1;
    if (cache_affected_rows() == 1)
        SendClientMessage(playerid, ADMIN_COLOR,
            "[LoadNet] Test queued in MySQL. PC Worker will pick it up.");
    else
        SendClientMessage(playerid, ADMIN_ERROR,
            "[LoadNet] Another test is active, or migration 012 is missing.");
#else
    #pragma unused playerid, authToken
#endif
    return 1;
}

public LoadNet_OnStopped(playerid, authToken)
{
#if defined TARKENSO_DB_PRIMARY
    if (!IsPlayerConnected(playerid) || gAuthToken[playerid] != authToken ||
        !LoadNet_IsAuthorized(playerid)) return 1;
    if (cache_affected_rows() == 1)
        SendClientMessage(playerid, ADMIN_COLOR, "[LoadNet] Stop requested. Waiting for PC Worker.");
    else
        SendClientMessage(playerid, ADMIN_ERROR, "[LoadNet] No running or pending test.");
#else
    #pragma unused playerid, authToken
#endif
    return 1;
}

public LoadNet_OnStatus(playerid, authToken)
{
#if defined TARKENSO_DB_PRIMARY
    if (!IsPlayerConnected(playerid) || gAuthToken[playerid] != authToken ||
        !LoadNet_IsAuthorized(playerid)) return 1;
    new rows;
    if (!cache_get_row_count(rows) || rows == 0)
        return SendClientMessage(playerid, ADMIN_COLOR, "[LoadNet] No tests in queue.");
    new job, clients, duration, launched, joined, authed, spawned;
    new state[24], mode[16], note[121], msg[190];
    if (!cache_get_value_name_int(0, "job_id", job) ||
        !cache_get_value_name_int(0, "target_clients", clients) ||
        !cache_get_value_name_int(0, "duration_seconds", duration) ||
        !cache_get_value_name_int(0, "launched_clients", launched) ||
        !cache_get_value_name_int(0, "joined_clients", joined) ||
        !cache_get_value_name_int(0, "authenticated_clients", authed) ||
        !cache_get_value_name_int(0, "spawned_clients", spawned) ||
        !cache_get_value_name(0, "state", state, sizeof(state)) ||
        !cache_get_value_name(0, "auth_mode", mode, sizeof(mode)) ||
        !cache_get_value_name(0, "status_message", note, sizeof(note)))
        return SendClientMessage(playerid, ADMIN_ERROR, "[LoadNet] Malformed job record.");
    format(msg, sizeof(msg),
        "[LoadNet] Job #%d %s (%s); %d/%d launched, %d joined, %d auth, %d spawn; %ds",
        job, state, mode, launched, clients, joined, authed, spawned, duration);
    SendClientMessage(playerid, ADMIN_COLOR, msg);
    if (note[0] != EOS)
    {
        format(msg, sizeof(msg), "[LoadNet] %s", note);
        SendClientMessage(playerid, ADMIN_COLOR, msg);
    }
#else
    #pragma unused playerid, authToken
#endif
    return 1;
}

stock LoadNet_Command(playerid, const params[])
{
    if (!LoadNet_IsAuthorized(playerid))
        return SendClientMessage(playerid, ADMIN_ERROR,
            "[LoadNet] Founder only; authenticate and enable /aduty.");
    new action[16];
    if (isnull(params) || sscanf(params, "s[16]", action))
        return SendClientMessage(playerid, ADMIN_COLOR,
            "[LoadNet] /loadnet start 5 90 [register|login]; /loadnet status; /loadnet stop");
    if (!strcmp(action, "status", true)) return LoadNet_Status(playerid);
    if (!strcmp(action, "stop", true)) return LoadNet_Stop(playerid);
    if (!strcmp(action, "start", true))
    {
        new count, seconds, mode[12] = "register";
        // Optional fourth word is auth mode; reject any other trailing syntax.
        if (!sscanf(params, "s[16]iis[12]", action, count, seconds, mode))
            return LoadNet_Start(playerid, count, seconds, mode);
        if (!sscanf(params, "s[16]ii", action, count, seconds))
            return LoadNet_Start(playerid, count, seconds, "register");
        return SendClientMessage(playerid, ADMIN_ERROR,
            "[LoadNet] Usage: /loadnet start 5 90 [register|login]");
    }
    return SendClientMessage(playerid, ADMIN_ERROR,
        "[LoadNet] /loadnet start 5 90 | status | stop");
}
