#if defined _tarkenso_gov_debt_included
    #endinput
#endif
#define _tarkenso_gov_debt_included

// GOV-04: state debt-crisis and overdue bond service. Never credits offline cash.
// Uses existing SQL escrow & GOV-03 claim collection, preserving investor rights.
// RP issuer permissions and public administration UI will be added at GOV-05.
#define GOV_DEBT_TIMER_MS (60000)
new bool:gGovDebtReady;
new bool:gGovDebtScanBusy;
new bool:gGovDebtRecoveryBusy;
new bool:gGovDebtCrisisBusy;
new gGovDebtTimer;
new gGovDebtCrisisSerial;
new bool:gGovDebtAdminBusy[MAX_PLAYERS];
new gGovDebtAdminSerial[MAX_PLAYERS];
new gGovDebtAdminAccount[MAX_PLAYERS];
new gGovDebtAdminEvent[MAX_PLAYERS][65];

forward GovDebt_Tick();
forward GovDebt_OnScan();
forward GovDebt_OnRecovered(issueId);
forward GovDebt_OnCrisis();
forward GovDebt_OnAdminAction(playerid, authToken, accountId, requestSerial);

stock bool:GovDebt_IsReady()
{
#if defined TARKENSO_DB_PRIMARY
    return gGovDebtReady && BondSettlement_IsReady() && BondFund_IsReady();
#else
    return false;
#endif
}

stock GovDebt_Reset(playerid)
{
    gGovDebtAdminBusy[playerid]=false;
    gGovDebtAdminSerial[playerid]++;
    gGovDebtAdminAccount[playerid]=0;
    gGovDebtAdminEvent[playerid][0]=EOS;
    return 1;
}

stock GovDebt_Shutdown()
{
    gGovDebtReady=false;
    if (gGovDebtTimer) KillTimer(gGovDebtTimer);
    gGovDebtTimer=0;
    gGovDebtScanBusy=false;
    gGovDebtRecoveryBusy=false;
    gGovDebtCrisisBusy=false;
    return 1;
}

stock GovDebt_Init()
{
    GovDebt_Shutdown();
#if defined TARKENSO_DB_PRIMARY
    if (!BondSettlement_IsReady()) return 1;
    new Cache:c=mysql_query(gDBPrimary,
        "SELECT version FROM schema_migrations WHERE version=22 LIMIT 1");
    if (c==Cache:0) return 1;
    new rows=0;
    new bool:valid=mysql_errno(gDBPrimary)==0 &&
        cache_get_row_count(rows) && rows==1;
    cache_delete(c);
    if (!valid)
    {
        print("[GOV-04] Migration 022 missing: recovery engine disabled.");
        return 1;
    }
    gGovDebtReady=true;
    gGovDebtTimer=SetTimer("GovDebt_Tick",GOV_DEBT_TIMER_MS,true);
    if (!gGovDebtTimer)
    {
        gGovDebtReady=false;
        print("[GOV-04] Recovery scheduler timer failed.");
        return 1;
    }
    print("[GOV-04] Recovery/crisis scheduler ready; unpaid claims preserved.");
    GovDebt_Tick();
#else
    print("[GOV-04] Recovery disabled: MySQL-primary is required.");
#endif
    return 1;
}

// The account cannot propose and approve the same restructuring.
// This senior-admin adapter is INTERNAL until GOV-05 supplies official roles.
stock bool:GovDebt_AdminAllowed(playerid,const eventKey[],const issueCode[])
{
#if defined TARKENSO_DB_PRIMARY
    return GovDebt_IsReady() && playerid>=0 && playerid<MAX_PLAYERS &&
        IsPlayerConnected(playerid) && !IsPlayerNPC(playerid) &&
        gAuthState[playerid]==AUTH_READY && gDBPrimaryLoaded[playerid] &&
        gDBPrimaryAccountId[playerid]>0 && !gGovDebtAdminBusy[playerid] &&
        Admin_Allowed(playerid,PERM_MANAGE_STAFF) && gAdminDuty[playerid] &&
        strlen(eventKey)>=7 && strlen(eventKey)<=64 &&
        !strcmp(eventKey,"bond:",false,5) &&
        Treasury_ValidToken(eventKey,64) &&
        Treasury_ValidToken(issueCode,64);
#else
    #pragma unused playerid,eventKey,issueCode
    return false;
#endif
}

stock bool:GovDebt_QueueAdmin(playerid,const query[],const eventKey[])
{
#if defined TARKENSO_DB_PRIMARY
    if (!GovDebt_IsReady() || gGovDebtAdminBusy[playerid]) return false;
    gGovDebtAdminBusy[playerid]=true;
    gGovDebtAdminAccount[playerid]=gDBPrimaryAccountId[playerid];
    format(gGovDebtAdminEvent[playerid],65,"%s",eventKey);
    gGovDebtAdminSerial[playerid]++;
    if (!mysql_tquery(gDBPrimary,query,"GovDebt_OnAdminAction","iiii",
        playerid,gAuthToken[playerid],gDBPrimaryAccountId[playerid],
        gGovDebtAdminSerial[playerid]))
    {
        gGovDebtAdminBusy[playerid]=false;
        gGovDebtAdminAccount[playerid]=0;
        gGovDebtAdminEvent[playerid][0]=EOS;
        return false;
    }
    return true;
#else
    #pragma unused playerid,query
    return false;
#endif
}

stock bool:GovDebt_Propose(playerid,const eventKey[],const issueCode[],
    const targetUTC[],const rationale[])
{
#if defined TARKENSO_DB_PRIMARY
    if (!GovDebt_AdminAllowed(playerid,eventKey,issueCode) ||
        strlen(targetUTC)!=19 || strlen(rationale)<10 ||
        strlen(rationale)>512) return false;
    new query[1150];
    mysql_format(gDBPrimary,query,sizeof(query),
        "CALL gov_bond_restructure_propose('%e','%e','%e','%e',%d)",
        eventKey,issueCode,targetUTC,rationale,gDBPrimaryAccountId[playerid]);
    return GovDebt_QueueAdmin(playerid,query,eventKey);
#else
    #pragma unused playerid,eventKey,issueCode,targetUTC,rationale
    return false;
#endif
}

stock bool:GovDebt_Approve(playerid,const eventKey[],const issueCode[],planId)
{
#if defined TARKENSO_DB_PRIMARY
    if (!GovDebt_AdminAllowed(playerid,eventKey,issueCode) || planId<=0)
        return false;
    new query[350];
    mysql_format(gDBPrimary,query,sizeof(query),
        "CALL gov_bond_restructure_approve('%e','%e',%d,%d)",
        eventKey,issueCode,planId,gDBPrimaryAccountId[playerid]);
    return GovDebt_QueueAdmin(playerid,query,eventKey);
#else
    #pragma unused playerid,eventKey,issueCode,planId
    return false;
#endif
}

public GovDebt_OnAdminAction(playerid,authToken,accountId,requestSerial)
{
#if defined TARKENSO_DB_PRIMARY
    if (playerid<0 || playerid>=MAX_PLAYERS) return 1;
    if (!IsPlayerConnected(playerid) || IsPlayerNPC(playerid) ||
        gAuthToken[playerid]!=authToken || gAuthState[playerid]!=AUTH_READY ||
        !gDBPrimaryLoaded[playerid] || gDBPrimaryAccountId[playerid]!=accountId ||
        !gGovDebtAdminBusy[playerid] ||
        gGovDebtAdminAccount[playerid]!=accountId ||
        gGovDebtAdminSerial[playerid]!=requestSerial) return 1;
    gGovDebtAdminBusy[playerid]=false;
    gGovDebtAdminAccount[playerid]=0;
    gGovDebtAdminEvent[playerid][0]=EOS;
    new rows=0;
    if (mysql_errno(gDBPrimary)!=0 || !cache_get_row_count(rows) || rows!=1)
    {
        print("[GOV-04] Unconfirmed governance action; re-read SQL audit.");
        return 1;
    }
    SendClientMessage(playerid,COLOR_AMBER,
        "Казначейство: решение о реструктуризации зафиксировано в журнале.");
#else
    #pragma unused playerid,authToken,accountId,requestSerial
#endif
    return 1;
}

public GovDebt_Tick()
{
#if defined TARKENSO_DB_PRIMARY
    if (!GovDebt_IsReady()) return 1;
    // One pending recovery round at a time, ordered by earliest maturity.
    if (!gGovDebtScanBusy && !gGovDebtRecoveryBusy)
    {
        gGovDebtScanBusy=true;
        if (!mysql_tquery(gDBPrimary,
            "SELECT i.issue_id,i.issue_code,i.recovery_round,t.max_transaction AS max_tx FROM government_bond_issues i CROSS JOIN treasury_accounts t WHERE t.code='state' AND t.enabled=1 AND t.expense_enabled=1 AND t.balance>t.minimum_reserve+t.bond_fund_reserved+t.bond_payout_reserved AND i.insurance_event_key IS NOT NULL AND i.debt_status IN ('DEFAULTED','RESTRUCTURED') AND EXISTS (SELECT 1 FROM government_bond_claims c WHERE c.issue_id=i.issue_id AND c.principal_due+c.interest_due>c.principal_budget_awarded+c.interest_budget_awarded+c.insurance_awarded) ORDER BY i.maturity_at,i.issue_id LIMIT 1",
            "GovDebt_OnScan",""))
        {
            gGovDebtScanBusy=false;
            print("[GOV-04] Recovery scanner failed to queue.");
        }
    }
    if (!gGovDebtCrisisBusy)
    {
        new eventKey[65],query[200];
        gGovDebtCrisisSerial++;
        format(eventKey,sizeof(eventKey),"bond:crisis:%d:%d",
            gettime(),gGovDebtCrisisSerial);
        mysql_format(gDBPrimary,query,sizeof(query),
            "CALL gov_bond_crisis_reconcile('%e')",eventKey);
        gGovDebtCrisisBusy=true;
        if (!mysql_tquery(gDBPrimary,query,"GovDebt_OnCrisis",""))
        {
            gGovDebtCrisisBusy=false;
            print("[GOV-04] Fiscal crisis reconciliation did not queue.");
        }
    }
#endif
    return 1;
}

public GovDebt_OnScan()
{
#if defined TARKENSO_DB_PRIMARY
    gGovDebtScanBusy=false;
    if (!GovDebt_IsReady()) return 1;
    new rows=0;
    if (mysql_errno(gDBPrimary)!=0 || !cache_get_row_count(rows) || rows<1)
        return 1;
    new issueId,round,maxTx,code[65],eventKey[65],query[400];
    if (!cache_get_value_name_int(0,"issue_id",issueId) ||
        !cache_get_value_name_int(0,"recovery_round",round) ||
        !cache_get_value_name_int(0,"max_tx",maxTx) ||
        !cache_get_value_name(0,"issue_code",code,sizeof(code)) ||
        issueId<=0 || round<0 || round>=2147483646 || maxTx<=0 ||
        !Treasury_ValidToken(code,64)) return 1;
    format(eventKey,sizeof(eventKey),"bond:recovery:%d:%d",issueId,round+1);
    mysql_format(gDBPrimary,query,sizeof(query),
        "CALL gov_bond_recovery_allocate('%e','%e',%d)",
        eventKey,code,maxTx);
    gGovDebtRecoveryBusy=true;
    if (!mysql_tquery(gDBPrimary,query,"GovDebt_OnRecovered","i",issueId))
    {
        gGovDebtRecoveryBusy=false;
        print("[GOV-04] Recovery allocation did not queue.");
    }
#else
    return 1;
#endif
    return 1;
}

public GovDebt_OnRecovered(issueId)
{
#if defined TARKENSO_DB_PRIMARY
    gGovDebtRecoveryBusy=false;
    new rows=0;
    if (mysql_errno(gDBPrimary)!=0 || !cache_get_row_count(rows) || rows!=1)
    {
        printf("[GOV-04] Recovery not confirmed for issue %d.",issueId);
        return 1;
    }
    new principal=0,interest=0;
    if (!cache_get_value_name_int(0,"principal_allocated",principal) ||
        !cache_get_value_name_int(0,"interest_allocated",interest)) return 1;
    printf("[GOV-04] Recovery escrow: issue=%d, principal=%d, interest=%d.",
           issueId,principal,interest);
    if (!BondFund_Reload())
        print("[GOV-04] Fund snapshot refresh failed after recovery.");
#else
    #pragma unused issueId
#endif
    return 1;
}

public GovDebt_OnCrisis()
{
#if defined TARKENSO_DB_PRIMARY
    gGovDebtCrisisBusy=false;
    new rows=0;
    if (mysql_errno(gDBPrimary)!=0 || !cache_get_row_count(rows) || rows!=1)
        print("[GOV-04] Crisis status reconciliation failed.");
    else if (!BondFund_Reload())
        print("[GOV-04] Fund snapshot refresh failed after crisis reconciliation.");
#endif
    return 1;
}

// Only known, deterministic business-rule SQL rollback should be absorbed;
// connectivity and non-business SQL errors remain handled by the DB fail-safe.
stock bool:GovDebt_OnError(errorid,const callback[],const query[])
{
#if defined TARKENSO_DB_PRIMARY
    if (errorid!=1644 && errorid!=1062) return false;
    if (!strcmp(callback,"GovDebt_OnRecovered",true))
    {
        gGovDebtRecoveryBusy=false;
        print("[GOV-04] Recovery rolled back (concurrent budget/status change).");
        return true;
    }
    if (!strcmp(callback,"GovDebt_OnCrisis",true))
    {
        gGovDebtCrisisBusy=false;
        return true;
    }
    if (!strcmp(callback,"GovDebt_OnAdminAction",true))
    {
        for(new playerid=0;playerid<MAX_PLAYERS;playerid++)
        {
            if (!gGovDebtAdminBusy[playerid] ||
                !gGovDebtAdminEvent[playerid][0]) continue;
            new token[68];
            format(token,sizeof(token),"'%s'",gGovDebtAdminEvent[playerid]);
            if (strfind(query,token,true)<0) continue;
            new accountId=gGovDebtAdminAccount[playerid];
            gGovDebtAdminBusy[playerid]=false;
            gGovDebtAdminAccount[playerid]=0;
            gGovDebtAdminEvent[playerid][0]=EOS;
            if (IsPlayerConnected(playerid) && !IsPlayerNPC(playerid) &&
                gAuthState[playerid]==AUTH_READY &&
                gDBPrimaryAccountId[playerid]==accountId)
                SendClientMessage(playerid,COLOR_RED,
                    "Казначейство отклонило реструктуризацию: перепроверьте полномочия и условия.");
            break;
        }
        return true; // an old disconnected actor cannot disconnect other users
    }
#else
    #pragma unused errorid,callback,query
#endif
    return false;
}
