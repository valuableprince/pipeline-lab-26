#if defined _tarkenso_bond_issues_included
    #endinput
#endif
#define _tarkenso_bond_issues_included

// GOV-02: state bond issue lifecycle; NO purchases, payoffs, or public commands.
// Internal founder-only staging adapter. GOV-05 will replace this with
// auditable, role-based government approvals and a user interface.
#define BOND_ISSUES_QUERY_MAX (1500)
new bool:gBondIssuesReady;
new bool:gBondIssuesBusy[MAX_PLAYERS];
new gBondIssuesRequestSerial[MAX_PLAYERS];
new gBondIssuesPendingAccount[MAX_PLAYERS];
new gBondIssuesPendingAction[MAX_PLAYERS];

forward BondIssues_OnComplete(playerid, authToken, accountId, requestSerial);

stock bool:BondIssues_IsReady()
{
#if defined TARKENSO_DB_PRIMARY
    return gBondIssuesReady && DBPrimary_IsReady() && BondFund_IsReady();
#else
    return false;
#endif
}

stock BondIssues_Reset(playerid)
{
    gBondIssuesBusy[playerid] = false;
    gBondIssuesRequestSerial[playerid]++;
    gBondIssuesPendingAccount[playerid] = 0;
    gBondIssuesPendingAction[playerid] = 0;
    return 1;
}

stock BondIssues_Shutdown()
{
    gBondIssuesReady = false;
    return 1;
}

stock BondIssues_Init()
{
    gBondIssuesReady = false;
#if defined TARKENSO_DB_PRIMARY
    if (!BondFund_IsReady()) return 1;
    new Cache:c = mysql_query(gDBPrimary,
        "SELECT version FROM schema_migrations WHERE version=20 LIMIT 1");
    if (c == Cache:0) return 1;
    new rows = 0;
    new bool:valid = mysql_errno(gDBPrimary) == 0 &&
        cache_get_row_count(rows) && rows == 1;
    cache_delete(c);
    if (!valid)
    {
        print("[GOV-02] Migration 020 absent; bond issue lifecycle disabled.");
        return 1;
    }
    gBondIssuesReady = true;
    print("[GOV-02] Issue lifecycle ready; public issuance and trading disabled.");
#else
    print("[GOV-02] Issue lifecycle disabled: MySQL-primary required.");
#endif
    return 1;
}

stock bool:BondIssues_CheckActor(playerid, const eventKey[], const issueCode[])
{
#if defined TARKENSO_DB_PRIMARY
    return BondIssues_IsReady() &&
        playerid >= 0 && playerid < MAX_PLAYERS &&
        IsPlayerConnected(playerid) && !IsPlayerNPC(playerid) &&
        gAuthState[playerid] == AUTH_READY && gDBPrimaryLoaded[playerid] &&
        gDBPrimaryAccountId[playerid] > 0 &&
        Admin_IsFounder(playerid) && !gBondIssuesBusy[playerid] &&
        strlen(eventKey) >= 6 && strlen(eventKey) <= 64 &&
        !strcmp(eventKey,"bond:",false,5) &&
        Treasury_ValidToken(eventKey,64) &&
        Treasury_ValidToken(issueCode,64);
#else
    #pragma unused playerid,eventKey,issueCode
    return false;
#endif
}

// Return value means QUEUED, not committed. No in-game command invokes these
// operations until GOV-05 establishes the financial authority workflow.
stock bool:BondIssues_Queue(playerid, action, const query[])
{
#if defined TARKENSO_DB_PRIMARY
    if (!BondIssues_IsReady() || gBondIssuesBusy[playerid] ||
        action < 1 || action > 4) return false;
    gBondIssuesBusy[playerid] = true;
    gBondIssuesPendingAccount[playerid] = gDBPrimaryAccountId[playerid];
    gBondIssuesPendingAction[playerid] = action;
    gBondIssuesRequestSerial[playerid]++;
    if (!mysql_tquery(gDBPrimary, query, "BondIssues_OnComplete", "iiii",
        playerid, gAuthToken[playerid],gDBPrimaryAccountId[playerid],
        gBondIssuesRequestSerial[playerid]))
    {
        gBondIssuesBusy[playerid] = false;
        gBondIssuesPendingAccount[playerid] = 0;
        return false;
    }
    return true;
#else
    #pragma unused playerid,action,query
    return false;
#endif
}

stock bool:BondIssues_CreateDraft(playerid, const eventKey[],
    const issueCode[], const title[], const purpose[], const issueType[],
    faceValue, maxQuantity, yieldBps, const offerStart[],
    const offerEnd[], const maturity[])
{
#if defined TARKENSO_DB_PRIMARY
    if (!BondIssues_CheckActor(playerid,eventKey,issueCode) ||
        strlen(title) < 3 || strlen(title) > 128 ||
        strlen(purpose) < 3 || strlen(purpose) > 512 ||
        (strcmp(issueType,"GENERAL") && strcmp(issueType,"TARGETED") &&
         strcmp(issueType,"CRISIS")) ||
        faceValue <= 0 || maxQuantity <= 0 || yieldBps < 0 || yieldBps > 10000 ||
        faceValue > TREASURY_MAX_CHANGE || maxQuantity > 1000000 ||
        strlen(offerStart) != 19 || strlen(offerEnd) != 19 ||
        strlen(maturity) != 19) return false;

    new query[BOND_ISSUES_QUERY_MAX];
    mysql_format(gDBPrimary,query,sizeof(query),
        "CALL gov_bond_issue_create('%e','%e','%e','%e','%e',%d,%d,%d,'%e','%e','%e',%d)",
        eventKey,issueCode,title,purpose,issueType,faceValue,maxQuantity,yieldBps,
        offerStart,offerEnd,maturity,gDBPrimaryAccountId[playerid]);
    return BondIssues_Queue(playerid,1,query);
#else
    #pragma unused playerid,eventKey,issueCode,title,purpose,issueType
    #pragma unused faceValue,maxQuantity,yieldBps,offerStart,offerEnd,maturity
    return false;
#endif
}

stock bool:BondIssues_Approve(playerid, const eventKey[], const issueCode[])
{
#if defined TARKENSO_DB_PRIMARY
    if (!BondIssues_CheckActor(playerid,eventKey,issueCode)) return false;
    new query[300];
    mysql_format(gDBPrimary,query,sizeof(query),
        "CALL gov_bond_issue_approve('%e','%e',%d)",
        eventKey,issueCode,gDBPrimaryAccountId[playerid]);
    return BondIssues_Queue(playerid,2,query);
#else
    #pragma unused playerid,eventKey,issueCode
    return false;
#endif
}

stock bool:BondIssues_Open(playerid, const eventKey[], const issueCode[])
{
#if defined TARKENSO_DB_PRIMARY
    if (!BondIssues_CheckActor(playerid,eventKey,issueCode)) return false;
    new query[300];
    mysql_format(gDBPrimary,query,sizeof(query),
        "CALL gov_bond_issue_open('%e','%e',%d)",
        eventKey,issueCode,gDBPrimaryAccountId[playerid]);
    return BondIssues_Queue(playerid,3,query);
#else
    #pragma unused playerid,eventKey,issueCode
    return false;
#endif
}

stock bool:BondIssues_Cancel(playerid, const eventKey[], const issueCode[])
{
#if defined TARKENSO_DB_PRIMARY
    if (!BondIssues_CheckActor(playerid,eventKey,issueCode)) return false;
    new query[300];
    mysql_format(gDBPrimary,query,sizeof(query),
        "CALL gov_bond_issue_cancel('%e','%e',%d)",
        eventKey,issueCode,gDBPrimaryAccountId[playerid]);
    return BondIssues_Queue(playerid,4,query);
#else
    #pragma unused playerid,eventKey,issueCode
    return false;
#endif
}

public BondIssues_OnComplete(playerid, authToken, accountId, requestSerial)
{
#if defined TARKENSO_DB_PRIMARY
    if (playerid < 0 || playerid >= MAX_PLAYERS ||
        !IsPlayerConnected(playerid) || IsPlayerNPC(playerid) ||
        gAuthToken[playerid] != authToken || gAuthState[playerid] != AUTH_READY ||
        !gDBPrimaryLoaded[playerid] || gDBPrimaryAccountId[playerid] != accountId ||
        !gBondIssuesBusy[playerid] ||
        gBondIssuesPendingAccount[playerid] != accountId ||
        gBondIssuesRequestSerial[playerid] != requestSerial)
        return 1;

    new action = gBondIssuesPendingAction[playerid];
    gBondIssuesBusy[playerid] = false;
    gBondIssuesPendingAccount[playerid] = 0;
    gBondIssuesPendingAction[playerid] = 0;

    new rows = 0, issueId, replayed;
    new bool:valid = mysql_errno(gDBPrimary) == 0 &&
        cache_get_row_count(rows) && rows == 1 &&
        cache_get_value_name_int(0,"issue_id",issueId) &&
        cache_get_value_name_int(0,"replayed",replayed);
    if (!valid || issueId <= 0 || (replayed != 0 && replayed != 1))
    {
        // No wallet mutation in GOV-02, but approval might have committed.
        // Never retry with a fresh event key after uncertain acknowledgement.
        print("[GOV-02] Issue command unconfirmed; reload official issue from DB.");
        return 1;
    }
    if (action == 2 || action == 4)
    {
        if (!BondFund_Reload())
            print("[GOV-02] Failed to reload authoritative fund snapshot.");
    }
    printf("[GOV-02] Issue action=%d issue=%d replay=%d.",
        action,issueId,replayed);
#else
    #pragma unused playerid,authToken,accountId,requestSerial
#endif
    return 1;
}
