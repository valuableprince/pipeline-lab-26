#if defined _tarkenso_crime_intel_included
    #endinput
#endif
#define _tarkenso_crime_intel_included

// CRIME-03.5: optional after-delivery intelligence. No direct wallet writes.
// Every successful tender produces one dossier within the payout transaction.
#define CRIME_INTEL_DLG_BOARD (7975)
#define CRIME_INTEL_DLG_PUZZLE (7976)
#define CRIME_INTEL_DLG_DECISION (7977)
#define CRIME_INTEL_LIMIT (8)

new gCrimeIntelCount[MAX_PLAYERS];
new gCrimeIntelIds[MAX_PLAYERS][CRIME_INTEL_LIMIT];
new gCrimeIntelSelected[MAX_PLAYERS];

stock CrimeIntel_Reset(playerid)
{
    gCrimeIntelCount[playerid]=0;
    gCrimeIntelSelected[playerid]=0;
    return 1;
}

// Account, faction and NPC ownership are always read from SQL, not from UI.
// The available window is 30 minutes from actual tender completion.
stock bool:CrimeIntel_Info(playerid,contractId,&contact,&puzzle,&solved,&retryAt)
{
    contact=0;puzzle=0;solved=0;retryAt=0;
#if defined TARKENSO_DB_PRIMARY
    if(!Crime_Available(playerid) || contractId<=0) return false;
    new q[650],rows=0;
    mysql_format(gDBPrimary,q,sizeof(q),
        "SELECT c.contact_id,l.puzzle_code,l.solved_at,l.next_attempt_at FROM crime_intelligence_cases l JOIN crime_contracts c ON c.contract_id=l.contract_id JOIN faction_members m ON m.account_id=c.account_id AND m.faction_id=c.faction_id WHERE l.contract_id=%d AND c.account_id=%d AND c.faction_id=%d AND c.mode_id=3 AND c.state=3 AND l.decision=0 AND l.closed_at=0 AND l.expires_at>UNIX_TIMESTAMP() LIMIT 1",
        contractId,gDBPrimaryAccountId[playerid],Leader_PlayerOrg(playerid));
    new Cache:c=mysql_query(gDBPrimary,q);
    if(c==Cache:0 || mysql_errno(gDBPrimary) ||
       !cache_get_row_count(rows) || rows!=1 ||
       !cache_get_value_name_int(0,"contact_id",contact) ||
       !cache_get_value_name_int(0,"puzzle_code",puzzle) ||
       !cache_get_value_name_int(0,"solved_at",solved) ||
       !cache_get_value_name_int(0,"next_attempt_at",retryAt))
    {
        if(c!=Cache:0) cache_delete(c);
        return false;
    }
    cache_delete(c);
    return contact>=1 && contact<=CRIME_CONTACT_COUNT && puzzle>=1 && puzzle<=3;
#else
    #pragma unused playerid,contractId
    return false;
#endif
}

// 0 = database failure/rejected; 1 = solved; 2 = wrong and cooldown set.
// An incorrect answer does not forfeit the dossier.
stock CrimeIntel_Answer(playerid,contractId,answer)
{
#if defined TARKENSO_DB_PRIMARY
    if(answer<1 || answer>3 || !Crime_Available(playerid) ||
       gGovTransferBusy[playerid] || IsPlayerInAnyVehicle(playerid))
        return 0;
    new contact,puzzle,solved,retryAt;
    if(!CrimeIntel_Info(playerid,contractId,contact,puzzle,solved,retryAt) ||
       solved>0 || retryAt>gettime() ||
       !CrimeContacts_Near(playerid,contact)) return 0;
    new q[780],changed;
    if(answer==puzzle)
    {
        mysql_format(gDBPrimary,q,sizeof(q),
            "UPDATE crime_intelligence_cases l JOIN crime_contracts c ON c.contract_id=l.contract_id JOIN faction_members m ON m.account_id=c.account_id AND m.faction_id=c.faction_id SET l.solved_at=UNIX_TIMESTAMP() WHERE l.contract_id=%d AND l.solved_at=0 AND l.decision=0 AND l.next_attempt_at<=UNIX_TIMESTAMP() AND l.expires_at>UNIX_TIMESTAMP() AND c.account_id=%d AND c.faction_id=%d AND c.mode_id=3 AND c.state=3 AND l.puzzle_code=%d",
            contractId,gDBPrimaryAccountId[playerid],Leader_PlayerOrg(playerid),answer);
        return (DBPrimary_Execute(q,changed) && changed==1) ? 1 : 0;
    }
    mysql_format(gDBPrimary,q,sizeof(q),
        "UPDATE crime_intelligence_cases l JOIN crime_contracts c ON c.contract_id=l.contract_id JOIN faction_members m ON m.account_id=c.account_id AND m.faction_id=c.faction_id SET l.attempts=l.attempts+1,l.next_attempt_at=UNIX_TIMESTAMP()+45 WHERE l.contract_id=%d AND l.solved_at=0 AND l.decision=0 AND l.attempts<65535 AND l.next_attempt_at<=UNIX_TIMESTAMP() AND l.expires_at>UNIX_TIMESTAMP() AND c.account_id=%d AND c.faction_id=%d AND c.mode_id=3 AND c.state=3 AND l.puzzle_code<>%d",
        contractId,gDBPrimaryAccountId[playerid],Leader_PlayerOrg(playerid),answer);
    return (DBPrimary_Execute(q,changed) && changed==1) ? 2 : 0;
#else
    #pragma unused playerid,contractId,answer
    return 0;
#endif
}

// Decision 1: archive intelligence (+4 influence, +1 syndicate standing).
// Decision 2: erase traces (-10 contact heat, +1 personal NPC trust).
// The dossier state, faction effects and event ledger commit atomically.
stock bool:CrimeIntel_Redeem(playerid,contractId,decision)
{
#if defined TARKENSO_DB_PRIMARY
    if(decision<1 || decision>2 || !Crime_Available(playerid) ||
       gGovTransferBusy[playerid] || IsPlayerInAnyVehicle(playerid))
        return false;
    new contact,puzzle,solved,retryAt;
    if(!CrimeIntel_Info(playerid,contractId,contact,puzzle,solved,retryAt) ||
       solved<=0 || !CrimeContacts_Near(playerid,contact) ||
       !Crime_Exec("START TRANSACTION")) return false;
    new q[1050],changed;
    mysql_format(gDBPrimary,q,sizeof(q),
        "UPDATE crime_intelligence_cases l JOIN crime_contracts c ON c.contract_id=l.contract_id JOIN faction_members m ON m.account_id=c.account_id AND m.faction_id=c.faction_id JOIN crime_contact_progress p ON p.account_id=c.account_id AND p.contact_id=c.contact_id JOIN crime_syndicate_progress s ON s.faction_id=c.faction_id JOIN crime_syndicate_contacts sc ON sc.faction_id=c.faction_id AND sc.contact_id=c.contact_id SET l.decision=%d,l.closed_at=UNIX_TIMESTAMP() WHERE l.contract_id=%d AND l.decision=0 AND l.closed_at=0 AND l.solved_at>0 AND l.expires_at>UNIX_TIMESTAMP() AND c.account_id=%d AND c.faction_id=%d AND c.contact_id=%d AND c.mode_id=3 AND c.state=3",
        decision,contractId,gDBPrimaryAccountId[playerid],
        Leader_PlayerOrg(playerid),contact);
    if(!DBPrimary_Execute(q,changed) || changed!=1)
    {
        Crime_Rollback();
        return false;
    }
    if(decision==1)
    {
        mysql_format(gDBPrimary,q,sizeof(q),
            "UPDATE crime_syndicate_progress p JOIN crime_contracts c ON c.faction_id=p.faction_id JOIN crime_intelligence_cases l ON l.contract_id=c.contract_id AND l.decision=1 AND l.closed_at>0 SET p.influence=LEAST(1000000,p.influence+4) WHERE c.contract_id=%d AND c.account_id=%d AND c.faction_id=%d AND c.state=3",
            contractId,gDBPrimaryAccountId[playerid],Leader_PlayerOrg(playerid));
        if(!Crime_Exec(q)) { Crime_Rollback(); return false; }
        mysql_format(gDBPrimary,q,sizeof(q),
            "UPDATE crime_syndicate_contacts p JOIN crime_contracts c ON c.faction_id=p.faction_id AND c.contact_id=p.contact_id JOIN crime_intelligence_cases l ON l.contract_id=c.contract_id AND l.decision=1 AND l.closed_at>0 SET p.standing=LEAST(100,p.standing+1) WHERE c.contract_id=%d AND c.account_id=%d AND c.faction_id=%d AND c.state=3",
            contractId,gDBPrimaryAccountId[playerid],Leader_PlayerOrg(playerid));
        if(!Crime_Exec(q)) { Crime_Rollback(); return false; }
    }
    else
    {
        mysql_format(gDBPrimary,q,sizeof(q),
            "UPDATE crime_contact_progress p JOIN crime_contracts c ON c.account_id=p.account_id AND c.contact_id=p.contact_id JOIN crime_intelligence_cases l ON l.contract_id=c.contract_id AND l.decision=2 AND l.closed_at>0 SET p.heat=GREATEST(0,CAST(p.heat AS SIGNED)-10),p.trust=LEAST(100,p.trust+1) WHERE c.contract_id=%d AND c.account_id=%d AND c.faction_id=%d AND c.state=3",
            contractId,gDBPrimaryAccountId[playerid],Leader_PlayerOrg(playerid));
        if(!Crime_Exec(q)) { Crime_Rollback(); return false; }
    }
    mysql_format(gDBPrimary,q,sizeof(q),
        "INSERT INTO crime_contract_events(contract_id,actor_account_id,event_type) VALUES (%d,%d,'%s')",
        contractId,gDBPrimaryAccountId[playerid],
        decision==1 ? ("INTEL_ARCHIVED") : ("INTEL_ERASED"));
    if(!Crime_Exec(q))
    {
        Crime_Rollback();
        return false;
    }
    return Crime_Exec("COMMIT");
#else
    #pragma unused playerid,contractId,decision
    return false;
#endif
}
