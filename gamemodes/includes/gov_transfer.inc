#if defined _tarkenso_gov_transfer_included
    #endinput
#endif
#define _tarkenso_gov_transfer_included

// GOV-00: one SQL transaction changes player cash, state treasury and both ledgers.
// No commands, automatic payments or bonds are enabled in this stage.
// All calls are asynchronous; unacknowledged writes are treated as uncertain.
#define GOV_TRANSFER_MAX_PENDING  (16)
#define GOV_TRANSFER_EVENT_MAX   (64)

new bool:gGovTransferReady;
new gGovTransferPendingCount;
new gGovTransferPendingAccount[MAX_PLAYERS];
new gGovTransferExpectedVersion[MAX_PLAYERS];

forward GovTransfer_OnComplete(playerid, authToken, accountId);

stock bool:GovTransfer_IsReady()
{
#if defined TARKENSO_DB_PRIMARY
    return gGovTransferReady && DBPrimary_IsReady() && Treasury_IsReady();
#else
    return false;
#endif
}

stock GovTransfer_OnConnect(playerid)
{
#if defined TARKENSO_DB_PRIMARY
    gGovTransferBusy[playerid] = false;
    gGovTransferPendingAccount[playerid] = 0;
    gGovTransferExpectedVersion[playerid] = 0;
#else
    #pragma unused playerid
#endif
    return 1;
}

stock GovTransfer_OnDisconnect(playerid)
{
#if defined TARKENSO_DB_PRIMARY
    // Only after attempting disconnect save; pending SQL still owns its event key.
    // A delayed callback checks authToken/accountId before touching another session.
    gGovTransferBusy[playerid] = false;
    gGovTransferPendingAccount[playerid] = 0;
    gGovTransferExpectedVersion[playerid] = 0;
#else
    #pragma unused playerid
#endif
    return 1;
}

stock GovTransfer_Shutdown()
{
    gGovTransferReady = false;
    // Pending MySQL transactions can complete independently: next login reloads DB.
    return 1;
}

stock GovTransfer_Init()
{
    gGovTransferReady = false;
    gGovTransferPendingCount = 0;
#if defined TARKENSO_DB_PRIMARY
    if (!Treasury_IsReady()) return 1;
    new Cache:c = mysql_query(gDBPrimary,
        "SELECT version FROM schema_migrations WHERE version=18 LIMIT 1");
    if (c == Cache:0) return 1;
    new rows = 0;
    new bool:valid = mysql_errno(gDBPrimary) == 0 &&
        cache_get_row_count(rows) && rows == 1;
    cache_delete(c);
    if (!valid)
    {
        print("[GOV-00] Migration 018 not applied; transfers disabled.");
        return 1;
    }
    gGovTransferReady = true;
    print("[GOV-00] Async player/treasury settlement ready; no automatic transfers.");
#else
    print("[GOV-00] Transfers disabled: MySQL-primary required.");
#endif
    return 1;
}

// Positive amount; toTreasury=true means player pays, false means state pays.
// eventKey must be server-generated and stable across retries: "gov00:<id>".
// No direct user-controlled event keys, account IDs, source or amounts.
stock bool:GovTransfer_Request(playerid, amount, bool:toTreasury,
    const eventKey[], const source[])
{
#if defined TARKENSO_DB_PRIMARY
    if (!GovTransfer_IsReady() || !IsPlayerConnected(playerid) || IsPlayerNPC(playerid) ||
        gAuthState[playerid] != AUTH_READY || !gDBPrimaryLoaded[playerid] ||
        gDBPrimaryAccountId[playerid] <= 0 || gGovTransferBusy[playerid] ||
        gGovTransferPendingCount >= GOV_TRANSFER_MAX_PENDING ||
        amount <= 0 || amount > TREASURY_MAX_CHANGE ||
        gDBPrimaryVersion[playerid] < 0 || gDBPrimaryVersion[playerid] >= 2147483646 ||
        gAccountTxnSerial[playerid] >= ACCOUNT_MAX_MONEY ||
        strlen(eventKey) < 7 || strlen(eventKey) > GOV_TRANSFER_EVENT_MAX ||
        strcmp(eventKey, "gov00:", false, 6) != 0 ||
        !Treasury_ValidToken(eventKey, GOV_TRANSFER_EVENT_MAX) ||
        !Treasury_ValidToken(source, 32))
        return false;

    // The only way this event may affect the wallet is after successful SQL commit.
    new delta = toTreasury ? amount : -amount;
    new query[400];
    mysql_format(gDBPrimary, query, sizeof(query),
        "CALL gov_transfer_post('%e',%d,%d,'%e',%d)",
        eventKey, gDBPrimaryAccountId[playerid], delta, source,
        gDBPrimaryVersion[playerid]);

    gGovTransferBusy[playerid] = true;
    gGovTransferPendingAccount[playerid] = gDBPrimaryAccountId[playerid];
    gGovTransferExpectedVersion[playerid] = gDBPrimaryVersion[playerid];
    gGovTransferPendingCount++;
    if (!mysql_tquery(gDBPrimary, query, "GovTransfer_OnComplete", "iii",
        playerid, gAuthToken[playerid], gDBPrimaryAccountId[playerid]))
    {
        gGovTransferPendingCount--;
        gGovTransferBusy[playerid] = false;
        gGovTransferPendingAccount[playerid] = 0;
        return false;
    }
    return true;
#else
    #pragma unused playerid, amount, toTreasury, eventKey, source
    return false;
#endif
}

// All SQL result parsing and player-state updates happen back on the game thread.
public GovTransfer_OnComplete(playerid, authToken, accountId)
{
#if defined TARKENSO_DB_PRIMARY
    if (gGovTransferPendingCount > 0) gGovTransferPendingCount--;
    // Player ID may have been reused after reconnect. Never credit that session.
    if (playerid < 0 || playerid >= MAX_PLAYERS ||
        !IsPlayerConnected(playerid) || IsPlayerNPC(playerid) ||
        gAuthToken[playerid] != authToken || gAuthState[playerid] != AUTH_READY ||
        !gDBPrimaryLoaded[playerid] || gDBPrimaryAccountId[playerid] != accountId ||
        !gGovTransferBusy[playerid] || gGovTransferPendingAccount[playerid] != accountId)
        return 1;

    new rows = 0, balance, treasury, accountVersion, treasuryVersion, serial, replayed;
    new bool:valid = mysql_errno(gDBPrimary) == 0 &&
        cache_get_row_count(rows) && rows == 1 &&
        cache_get_value_name_int(0, "account_balance", balance) &&
        cache_get_value_name_int(0, "treasury_balance", treasury) &&
        cache_get_value_name_int(0, "account_row_version", accountVersion) &&
        cache_get_value_name_int(0, "treasury_row_version", treasuryVersion) &&
        cache_get_value_name_int(0, "account_txn_serial", serial) &&
        cache_get_value_name_int(0, "replayed", replayed);
    if (!valid || balance < 0 || balance > ACCOUNT_MAX_MONEY ||
        treasury < 0 || treasury > TREASURY_MAX_BALANCE ||
        accountVersion < gGovTransferExpectedVersion[playerid] ||
        accountVersion >= 2147483647 || treasuryVersion < 0 ||
        treasuryVersion >= 2147483647 || serial < gAccountTxnSerial[playerid] ||
        serial > ACCOUNT_MAX_MONEY || (replayed != 0 && replayed != 1) ||
        gDBPrimaryVersion[playerid] != gGovTransferExpectedVersion[playerid])
    {
        // Connection loss may have occurred after COMMIT: never save stale cash.
        print("[GOV-00] Unconfirmed settlement: blocking account saves until next login.");
        gGovTransferReady = false;
        Kick(playerid); // busy flag stays set until disconnect save attempt
        return 1;
    }

    gAccountMoney[playerid] = balance;
    gAccountTxnSerial[playerid] = serial;
    gDBPrimaryVersion[playerid] = accountVersion;
    if (treasuryVersion >= gTreasuryVersion)
    {
        gTreasuryBalance = treasury;
        gTreasuryVersion = treasuryVersion;
    }
    gGovTransferBusy[playerid] = false;
    gGovTransferPendingAccount[playerid] = 0;
    Economy_SyncHUD(playerid);
    if (replayed) print("[GOV-00] Idempotent transfer replay acknowledged.");
#else
    #pragma unused playerid, authToken, accountId
#endif
    return 1;
}
