#if defined _tarkenso_bond_settlement_included
    #endinput
#endif
#define _tarkenso_bond_settlement_included

// GOV-03: player purchases, online escrow collection and automatic debt assessment.
// Repayment entitlements are durable for offline investors; the actual wallet
// credit happens ONLY on an authorized online session with row_version fencing.
#define BOND_SETTLEMENT_TIMER_MS (30000)
#define BOND_SETTLEMENT_MAX_PENDING (8)
new bool:gBondSettlementReady;
new bool:gBondSettlementBusy[MAX_PLAYERS];
new gBondSettlementExpectedVersion[MAX_PLAYERS];
new gBondSettlementPendingAccount[MAX_PLAYERS];
new gBondSettlementPendingSerial[MAX_PLAYERS];
new gBondSettlementPendingGlobal;
new gBondSettlementTimer;
new bool:gBondSettlementScanBusy;

forward BondSettlement_OnPlayerDone(playerid, authToken, accountId, serial);
forward BondSettlement_OnScan();
forward BondSettlement_OnAutomated(issueId, kind);
forward BondSettlement_Tick();

stock bool:BondSettlement_IsReady()
{
#if defined TARKENSO_DB_PRIMARY
    return gBondSettlementReady && BondIssues_IsReady() &&
        BondFund_IsReady() && DBPrimary_IsReady();
#else
    return false;
#endif
}

stock BondSettlement_OnConnect(playerid)
{
    gBondSettlementBusy[playerid] = false;
    gBondSettlementExpectedVersion[playerid] = 0;
    gBondSettlementPendingAccount[playerid] = 0;
    gBondSettlementPendingSerial[playerid]++;
    return 1;
}

stock BondSettlement_OnDisconnect(playerid)
{
    // Called AFTER disconnect save attempt, preserving cash fencing until then.
    return BondSettlement_OnConnect(playerid);
}

stock BondSettlement_Shutdown()
{
    gBondSettlementReady = false;
    if (gBondSettlementTimer) KillTimer(gBondSettlementTimer);
    gBondSettlementTimer = 0;
    gBondSettlementScanBusy = false;
    return 1;
}

stock BondSettlement_Init()
{
    gBondSettlementReady = false;
#if defined TARKENSO_DB_PRIMARY
    if (!BondIssues_IsReady()) return 1;
    new Cache:c = mysql_query(gDBPrimary,
        "SELECT version FROM schema_migrations WHERE version=21 LIMIT 1");
    if (c == Cache:0) return 1;
    new rows = 0;
    new bool:valid = mysql_errno(gDBPrimary) == 0 &&
        cache_get_row_count(rows) && rows == 1;
    cache_delete(c);
    if (!valid)
    {
        print("[GOV-03] Migration 021 absent. Settlement is disabled.");
        return 1;
    }
    gBondSettlementReady = true;
    gBondSettlementPendingGlobal = 0;
    gBondSettlementScanBusy = false;
    gBondSettlementTimer = SetTimer("BondSettlement_Tick",
        BOND_SETTLEMENT_TIMER_MS, true);
    if (!gBondSettlementTimer)
    {
        gBondSettlementReady = false;
        print("[GOV-03] ERROR: assessment timer failed.");
    }
    else
    {
        print("[GOV-03] Placement/escrow engine ready; automated maturity enabled.");
        BondSettlement_Tick(); // catch up after server downtime
    }
#else
    print("[GOV-03] Settlement disabled: MySQL-primary required.");
#endif
    return 1;
}

// All dynamic amounts and event keys must be generated and validated by the
// server's gameplay layer. Do not trust a client-supplied SQL amount/owner.
stock bool:BondSettlement_Queue(playerid, const query[])
{
#if defined TARKENSO_DB_PRIMARY
    if (!BondSettlement_IsReady() || playerid<0 || playerid>=MAX_PLAYERS ||
        !IsPlayerConnected(playerid) || IsPlayerNPC(playerid) ||
        gAuthState[playerid]!=AUTH_READY || !gDBPrimaryLoaded[playerid] ||
        gDBPrimaryAccountId[playerid]<=0 || gGovTransferBusy[playerid] ||
        gBondSettlementBusy[playerid] ||
        gDBPrimaryVersion[playerid]<0 ||
        gDBPrimaryVersion[playerid]>=2147483646 ||
        gAccountTxnSerial[playerid]>=ACCOUNT_MAX_MONEY)
        return false;

    gGovTransferBusy[playerid] = true; // shared account-save and economy lock
    gBondSettlementBusy[playerid] = true;
    gBondSettlementExpectedVersion[playerid] = gDBPrimaryVersion[playerid];
    gBondSettlementPendingAccount[playerid] = gDBPrimaryAccountId[playerid];
    gBondSettlementPendingSerial[playerid]++;
    if (!mysql_tquery(gDBPrimary, query, "BondSettlement_OnPlayerDone", "iiii",
        playerid,gAuthToken[playerid],gDBPrimaryAccountId[playerid],
        gBondSettlementPendingSerial[playerid]))
    {
        gGovTransferBusy[playerid] = false;
        gBondSettlementBusy[playerid] = false;
        gBondSettlementPendingAccount[playerid] = 0;
        return false;
    }
    return true;
#else
    #pragma unused playerid, query
    return false;
#endif
}

// The user may buy only within the announced OFFERING window; SQL is authority.
stock bool:BondSettlement_Buy(playerid, const eventKey[],
    const issueCode[], quantity)
{
#if defined TARKENSO_DB_PRIMARY
    if (!BondSettlement_IsReady() || !Treasury_ValidToken(eventKey,64) ||
        strlen(eventKey)<7 || strcmp(eventKey,"bond:",false,5) != 0 ||
        !Treasury_ValidToken(issueCode,64) ||
        quantity<=0 || quantity>1000000 || !IsPlayerConnected(playerid) ||
        gAuthState[playerid]!=AUTH_READY || !gDBPrimaryLoaded[playerid]) return false;

    new query[512];
    mysql_format(gDBPrimary,query,sizeof(query),
        "CALL gov_bond_buy('%e','%e',%d,%d,%d)",
        eventKey,issueCode,gDBPrimaryAccountId[playerid],quantity,
        gDBPrimaryVersion[playerid]);
    return BondSettlement_Queue(playerid,query);
#else
    #pragma unused playerid,eventKey,issueCode,quantity
    return false;
#endif
}

// Withdraw a confirmed claim escrow to an authenticated player's wallet.
// The same event key must be retained across retries.
stock bool:BondSettlement_Collect(playerid, const eventKey[], claimId, amount)
{
#if defined TARKENSO_DB_PRIMARY
    if (!BondSettlement_IsReady() || !Treasury_ValidToken(eventKey,64) ||
        strlen(eventKey)<7 || strcmp(eventKey,"bond:",false,5) != 0 ||
        claimId<=0 || amount<=0 || amount>TREASURY_MAX_CHANGE ||
        !IsPlayerConnected(playerid) || gAuthState[playerid]!=AUTH_READY ||
        !gDBPrimaryLoaded[playerid]) return false;

    new query[512];
    mysql_format(gDBPrimary,query,sizeof(query),
        "CALL gov_bond_claim_collect('%e',%d,%d,%d,%d)",
        eventKey,claimId,gDBPrimaryAccountId[playerid],amount,
        gDBPrimaryVersion[playerid]);
    return BondSettlement_Queue(playerid,query);
#else
    #pragma unused playerid,eventKey,claimId,amount
    return false;
#endif
}

public BondSettlement_OnPlayerDone(playerid,authToken,accountId,serial)
{
#if defined TARKENSO_DB_PRIMARY
    if (playerid<0 || playerid>=MAX_PLAYERS) return 1;
    if (IsPlayerConnected(playerid) && !IsPlayerNPC(playerid) &&
        gAuthToken[playerid]!=authToken &&
        gAuthState[playerid]==AUTH_READY && gDBPrimaryLoaded[playerid] &&
        gDBPrimaryAccountId[playerid]==accountId)
    {
        // A reconnect could have loaded cash before the previous SQL COMMIT.
        gGovTransferBusy[playerid] = true;
        print("[GOV-03] Late settlement of same account; re-login required.");
        Kick(playerid);
        return 1;
    }
    if (!IsPlayerConnected(playerid) || IsPlayerNPC(playerid) ||
        gAuthToken[playerid]!=authToken || gAuthState[playerid]!=AUTH_READY ||
        !gDBPrimaryLoaded[playerid] || gDBPrimaryAccountId[playerid]!=accountId ||
        !gBondSettlementBusy[playerid] ||
        gBondSettlementPendingAccount[playerid]!=accountId ||
        gBondSettlementPendingSerial[playerid]!=serial)
        return 1;

    new rows = 0, cash, version, txnSerial, treasury, treasuryVersion, replayed;
    new bool:valid = mysql_errno(gDBPrimary)==0 &&
        cache_get_row_count(rows) && rows==1 &&
        cache_get_value_name_int(0,"account_balance",cash) &&
        cache_get_value_name_int(0,"account_row_version",version) &&
        cache_get_value_name_int(0,"account_txn_serial",txnSerial) &&
        cache_get_value_name_int(0,"treasury_balance",treasury) &&
        cache_get_value_name_int(0,"treasury_row_version",treasuryVersion) &&
        cache_get_value_name_int(0,"replayed",replayed);

    if (!valid || cash<0 || cash>ACCOUNT_MAX_MONEY ||
        version<gBondSettlementExpectedVersion[playerid] ||
        version>=2147483647 || txnSerial<gAccountTxnSerial[playerid] ||
        txnSerial>ACCOUNT_MAX_MONEY || treasury<0 ||
        treasury>TREASURY_MAX_BALANCE || treasuryVersion<0 ||
        (replayed!=0 && replayed!=1) ||
        gDBPrimaryVersion[playerid]!=gBondSettlementExpectedVersion[playerid])
    {
        // The transaction may have committed. Do not save stale account cash.
        print("[GOV-03] Unconfirmed cash settlement; forcing account reload.");
        gBondSettlementReady = false; // fail closed, no additional purchases
        Kick(playerid); // keep gGovTransferBusy set until disconnect save attempt
        return 1;
    }

    gAccountMoney[playerid]=cash;
    gAccountTxnSerial[playerid]=txnSerial;
    gDBPrimaryVersion[playerid]=version;
    if (treasuryVersion>=gTreasuryVersion)
    {
        gTreasuryBalance=treasury;
        gTreasuryVersion=treasuryVersion;
    }
    gGovTransferBusy[playerid]=false;
    gBondSettlementBusy[playerid]=false;
    gBondSettlementPendingAccount[playerid]=0;
    Economy_SyncHUD(playerid);
    if (!BondFund_Reload())
        print("[GOV-03] ERROR: fund snapshot reload failed after player cash settlement.");
    if (replayed) print("[GOV-03] Settlement event replay acknowledged.");
    BondsUI_OnSettlement(playerid, replayed != 0);
#else
    #pragma unused playerid,authToken,accountId,serial
#endif
    return 1;
}

// Scan due issues. Server UTC + DB real time determine maturity/grace.
// Result sets contain only issue IDs/codes from our own SQL registry.
public BondSettlement_Tick()
{
#if defined TARKENSO_DB_PRIMARY
    if (!BondSettlement_IsReady() || gBondSettlementScanBusy ||
        gBondSettlementPendingGlobal>0) return 1;
    gBondSettlementScanBusy=true;
    if (!mysql_tquery(gDBPrimary,
        "SELECT issue_id,issue_code,IF(maturity_event_key IS NULL,1,2) AS step_kind FROM government_bond_issues WHERE (maturity_event_key IS NULL AND maturity_at<=UTC_TIMESTAMP() AND status IN ('APPROVED','OFFERING','CLOSED')) OR (maturity_event_key IS NOT NULL AND insurance_event_key IS NULL AND UTC_TIMESTAMP()>=DATE_ADD(maturity_at,INTERVAL grace_seconds SECOND)) ORDER BY maturity_at LIMIT 8",
        "BondSettlement_OnScan",""))
    {
        gBondSettlementScanBusy=false;
        print("[GOV-03] Due-issue scan not queued.");
    }
#endif
    return 1;
}

public BondSettlement_OnScan()
{
#if defined TARKENSO_DB_PRIMARY
    gBondSettlementScanBusy=false;
    if (!BondSettlement_IsReady()) return 1;
    new rows=0;
    if (mysql_errno(gDBPrimary)!=0 || !cache_get_row_count(rows))
    {
        print("[GOV-03] Due-issue scan failed.");
        return 1;
    }
    for(new i=0; i<rows && i<BOND_SETTLEMENT_MAX_PENDING; i++)
    {
        new issueId, kind, issueCode[65],eventKey[65],query[350];
        if (!cache_get_value_name_int(i,"issue_id",issueId) ||
            !cache_get_value_name_int(i,"step_kind",kind) ||
            !cache_get_value_name(i,"issue_code",issueCode,sizeof(issueCode)) ||
            issueId<=0 || (kind!=1 && kind!=2) ||
            !Treasury_ValidToken(issueCode,64))
            continue;
        if (kind==1)
        {
            format(eventKey,sizeof(eventKey),"bond:maturity:%d",issueId);
            mysql_format(gDBPrimary,query,sizeof(query),
                "CALL gov_bond_maturity_settle('%e','%e')",eventKey,issueCode);
        }
        else
        {
            format(eventKey,sizeof(eventKey),"bond:insurance:%d",issueId);
            mysql_format(gDBPrimary,query,sizeof(query),
                "CALL gov_bond_insurance_assess('%e','%e')",eventKey,issueCode);
        }
        gBondSettlementPendingGlobal++;
        if (!mysql_tquery(gDBPrimary,query,"BondSettlement_OnAutomated","ii",
            issueId,kind))
        {
            gBondSettlementPendingGlobal--;
            print("[GOV-03] Failed to enqueue one automatic bond assessment.");
        }
    }
#endif
    return 1;
}

public BondSettlement_OnAutomated(issueId,kind)
{
#if defined TARKENSO_DB_PRIMARY
    if (gBondSettlementPendingGlobal>0) gBondSettlementPendingGlobal--;
    new rows=0;
    if (mysql_errno(gDBPrimary)!=0 || !cache_get_row_count(rows) || rows!=1)
    {
        printf("[GOV-03] ERROR: automatic assessment failed, issue=%d step=%d.",issueId,kind);
        return 1;
    }
    printf("[GOV-03] Bond assessment committed: issue=%d, step=%d.",issueId,kind);
    if (!BondFund_Reload())
        print("[GOV-03] Fund snapshot unavailable after automatic assessment.");
#else
    #pragma unused issueId,kind
#endif
    return 1;
}
