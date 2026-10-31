#if defined _tarkenso_crime_recovery_included
    #endinput
#endif
#define _tarkenso_crime_recovery_included

// CRIME-03.6: solo risk-management jobs, independent of ordinary deliveries.
// DB owns active-state, faction ownership, 20m deadline and 60m cooldown.
#define CRIME_RECOVERY_COUNT (6)
#define CRIME_RECOVERY_RADIUS (3.5)
#define CRIME_RECOVERY_DLG_BOARD (7978)
#define CRIME_RECOVERY_DLG_METHOD (7979)
#define CRIME_RECOVERY_DLG_FINISH (7980)
#define CRIME_RECOVERY_DLG_FIELD (7981)

new gCrimeRecoveryActors[CRIME_RECOVERY_COUNT];
new bool:gCrimeRecoveryReady[CRIME_RECOVERY_COUNT];
new gCrimeRecoveryContact[MAX_PLAYERS];
new gCrimeRecoverySelected[MAX_PLAYERS];
new Float:gCrimeRecoveryPos[CRIME_RECOVERY_COUNT][3] =
{
    {2325.0,-2320.0,13.5}, {2705.0,-2410.0,13.5},
    {-1985.0,1130.0,45.0}, {-1900.0,1200.0,45.0},
    {2035.0,1650.0,12.0}, {2205.0,1890.0,12.0}
};
new gCrimeRecoveryModels[CRIME_RECOVERY_COUNT] = {57,73,59,112,71,134};

stock CrimeRecovery_Reset(playerid)
{
    gCrimeRecoveryContact[playerid]=0;
    gCrimeRecoverySelected[playerid]=0;
    return 1;
}

stock CrimeRecovery_Shutdown()
{
    for(new i;i<CRIME_RECOVERY_COUNT;i++)
    {
        if(!gCrimeRecoveryReady[i]) continue;
        DestroyActor(gCrimeRecoveryActors[i]);
        gCrimeRecoveryReady[i]=false;
    }
    return 1;
}

stock bool:CrimeRecovery_Init()
{
    for(new i;i<CRIME_RECOVERY_COUNT;i++)
    {
        gCrimeRecoveryActors[i]=CreateActor(gCrimeRecoveryModels[i],
            gCrimeRecoveryPos[i][0],gCrimeRecoveryPos[i][1],
            gCrimeRecoveryPos[i][2],0.0);
        if(gCrimeRecoveryActors[i]==INVALID_ACTOR_ID)
        {
            CrimeRecovery_Shutdown();
            print("[CRIME-03.6] Recovery contact spawn failed.");
            return false;
        }
        SetActorInvulnerable(gCrimeRecoveryActors[i],true);
        gCrimeRecoveryReady[i]=true;
    }
    print("[CRIME-03.6] Six risk-management contacts loaded.");
    return true;
}

// Successful empty query returns id=0, a database error returns false.
stock bool:CrimeRecovery_Info(playerid,&id,&contact,&method,&stage,&deadline)
{
    id=0;contact=0;method=0;stage=0;deadline=0;
#if defined TARKENSO_DB_PRIMARY
    if(!Crime_Available(playerid)) return false;
    new q[420],rows=0;
    mysql_format(gDBPrimary,q,sizeof(q),
        "SELECT r.recovery_id,r.contact_id,r.method,r.state,r.expires_at FROM crime_contact_recoveries r JOIN faction_members m ON m.account_id=r.account_id AND m.faction_id=r.faction_id WHERE r.account_id=%d AND r.faction_id=%d AND r.state IN(1,2) AND r.expires_at>UNIX_TIMESTAMP() LIMIT 1",
        gDBPrimaryAccountId[playerid],Leader_PlayerOrg(playerid));
    new Cache:c=mysql_query(gDBPrimary,q);
    if(c==Cache:0 || mysql_errno(gDBPrimary) ||
       !cache_get_row_count(rows) || rows>1)
    {
        if(c!=Cache:0) cache_delete(c);
        return false;
    }
    if(rows==1 &&
       (!cache_get_value_name_int(0,"recovery_id",id) ||
        !cache_get_value_name_int(0,"contact_id",contact) ||
        !cache_get_value_name_int(0,"method",method) ||
        !cache_get_value_name_int(0,"state",stage) ||
        !cache_get_value_name_int(0,"expires_at",deadline)))
    {
        cache_delete(c);
        return false;
    }
    cache_delete(c);
    return !id || (contact>=1 && contact<=3 &&
        method>=1 && method<=2 && stage>=1 && stage<=2);
#else
    #pragma unused playerid
    return false;
#endif
}

stock bool:CrimeRecovery_Start(playerid,contact,method)
{
#if defined TARKENSO_DB_PRIMARY
    if(!Crime_Available(playerid) || gGovTransferBusy[playerid] ||
       !Leader_IsCriminal(Leader_PlayerOrg(playerid)) ||
       IsPlayerInAnyVehicle(playerid) ||
       contact<1 || contact>3 || method<1 || method>2 ||
       !CrimeContacts_Near(playerid,contact) ||
       !Crime_Expire(playerid)) return false;
    new q[1200],affected;
    mysql_format(gDBPrimary,q,sizeof(q),
        "UPDATE crime_contact_recoveries SET state=4 WHERE account_id=%d AND state IN(1,2) AND expires_at<=UNIX_TIMESTAMP()",
        gDBPrimaryAccountId[playerid]);
    // Expiry cleanup is durable even when the new mission is rejected
    // for cooldown, heat or faction changes.
    if(!Crime_Exec(q) || !Crime_Exec("START TRANSACTION")) return false;
    mysql_format(gDBPrimary,q,sizeof(q),
        "INSERT INTO crime_contact_recoveries(account_id,faction_id,contact_id,method,started_at,expires_at) SELECT m.account_id,m.faction_id,p.contact_id,%d,UNIX_TIMESTAMP(),UNIX_TIMESTAMP()+1200 FROM faction_members m JOIN crime_contact_progress p ON p.account_id=m.account_id AND p.contact_id=%d AND p.heat>=60 JOIN crime_syndicate_contacts sc ON sc.faction_id=m.faction_id AND sc.contact_id=p.contact_id WHERE m.account_id=%d AND m.faction_id=%d AND NOT EXISTS(SELECT 1 FROM crime_contact_recoveries r WHERE r.account_id=m.account_id AND r.state IN(1,2)) AND NOT EXISTS(SELECT 1 FROM crime_contact_recoveries old WHERE old.account_id=m.account_id AND old.contact_id=p.contact_id AND old.started_at>UNIX_TIMESTAMP()-3600) AND NOT EXISTS(SELECT 1 FROM crime_contracts c WHERE c.account_id=m.account_id AND c.state IN(1,2))",
        method,contact,gDBPrimaryAccountId[playerid],Leader_PlayerOrg(playerid));
    if(!DBPrimary_Execute(q,affected) || affected!=1)
    {
        Crime_Rollback();
        return false;
    }
    return Crime_Exec("COMMIT");
#else
    #pragma unused playerid,contact,method
    return false;
#endif
}

// One physical action at the NPC station, persisted without rewards.
stock bool:CrimeRecovery_FieldComplete(playerid,recoveryId)
{
#if defined TARKENSO_DB_PRIMARY
    if(!Crime_Available(playerid) || gGovTransferBusy[playerid] ||
       IsPlayerInAnyVehicle(playerid) || recoveryId<=0 ||
       GetPlayerInterior(playerid)!=0 || GetPlayerVirtualWorld(playerid)!=0)
        return false;
    new id,contact,method,stage,deadline;
    if(!CrimeRecovery_Info(playerid,id,contact,method,stage,deadline) ||
       id!=recoveryId || stage!=1) return false;
    new slot=(contact-1)*2+method-1;
    if(!gCrimeRecoveryReady[slot] ||
       !IsPlayerInRangeOfPoint(playerid,CRIME_RECOVERY_RADIUS,
           gCrimeRecoveryPos[slot][0],gCrimeRecoveryPos[slot][1],
           gCrimeRecoveryPos[slot][2])) return false;
    new q[560],affected;
    mysql_format(gDBPrimary,q,sizeof(q),
        "UPDATE crime_contact_recoveries r JOIN faction_members m ON m.account_id=r.account_id AND m.faction_id=r.faction_id SET r.state=2,r.field_at=UNIX_TIMESTAMP() WHERE r.recovery_id=%d AND r.account_id=%d AND r.faction_id=%d AND r.state=1 AND r.field_at=0 AND r.expires_at>UNIX_TIMESTAMP()",
        id,gDBPrimaryAccountId[playerid],Leader_PlayerOrg(playerid));
    return DBPrimary_Execute(q,affected) && affected==1;
#else
    #pragma unused playerid,recoveryId
    return false;
#endif
}

// One SQL transaction: change state once and store before/after values;
// no accounts.balance, no direct influence minting, no dependency on rivals.
stock bool:CrimeRecovery_Finish(playerid,recoveryId)
{
#if defined TARKENSO_DB_PRIMARY
    if(!Crime_Available(playerid) || gGovTransferBusy[playerid] ||
       IsPlayerInAnyVehicle(playerid) || recoveryId<=0) return false;
    new id,contact,method,stage,deadline;
    if(!CrimeRecovery_Info(playerid,id,contact,method,stage,deadline) ||
       id!=recoveryId || stage!=2 || !CrimeContacts_Near(playerid,contact) ||
       !Crime_Exec("START TRANSACTION")) return false;
    new q[900],affected;
    mysql_format(gDBPrimary,q,sizeof(q),
        "UPDATE crime_contact_recoveries r JOIN faction_members m ON m.account_id=r.account_id AND m.faction_id=r.faction_id JOIN crime_contact_progress p ON p.account_id=r.account_id AND p.contact_id=r.contact_id JOIN crime_syndicate_contacts sc ON sc.faction_id=r.faction_id AND sc.contact_id=r.contact_id SET r.state=3,r.finished_at=UNIX_TIMESTAMP(),r.heat_before=p.heat,r.heat_after=GREATEST(0,CAST(p.heat AS SIGNED)-(CASE WHEN r.method=1 THEN 25 ELSE 35 END)),r.standing_before=sc.standing,r.standing_after=GREATEST(0,CAST(sc.standing AS SIGNED)-(CASE WHEN r.method=2 THEN 3 ELSE 0 END)) WHERE r.recovery_id=%d AND r.account_id=%d AND r.faction_id=%d AND r.state=2 AND r.field_at>0 AND r.finished_at=0 AND r.expires_at>UNIX_TIMESTAMP()",
        id,gDBPrimaryAccountId[playerid],Leader_PlayerOrg(playerid));
    if(!DBPrimary_Execute(q,affected) || affected!=1)
    {
        Crime_Rollback();
        return false;
    }
    mysql_format(gDBPrimary,q,sizeof(q),
        "UPDATE crime_contact_progress p JOIN crime_contact_recoveries r ON r.account_id=p.account_id AND r.contact_id=p.contact_id SET p.heat=r.heat_after,p.trust=LEAST(100,p.trust+(CASE WHEN r.method=1 THEN 1 ELSE 0 END)) WHERE r.recovery_id=%d AND r.account_id=%d AND r.faction_id=%d AND r.state=3 AND r.finished_at>0",
        id,gDBPrimaryAccountId[playerid],Leader_PlayerOrg(playerid));
    if(!DBPrimary_Execute(q,affected) || affected!=1)
    {
        Crime_Rollback();
        return false;
    }
    mysql_format(gDBPrimary,q,sizeof(q),
        "UPDATE crime_syndicate_contacts sc JOIN crime_contact_recoveries r ON r.faction_id=sc.faction_id AND r.contact_id=sc.contact_id SET sc.standing=r.standing_after WHERE r.recovery_id=%d AND r.account_id=%d AND r.faction_id=%d AND r.state=3 AND r.finished_at>0",
        id,gDBPrimaryAccountId[playerid],Leader_PlayerOrg(playerid));
    // A quiet route does not change standing; a risky route can hit the
    // zero clamp. Both are valid updates with 0 changed rows in MySQL.
    if(!Crime_Exec(q))
    {
        Crime_Rollback();
        return false;
    }
    return Crime_Exec("COMMIT");
#else
    #pragma unused playerid,recoveryId
    return false;
#endif
}

stock CrimeRecovery_OnKeys(playerid,KEY:newkeys,KEY:oldkeys)
{
    if(!(newkeys & KEY_YES) || (oldkeys & KEY_YES) ||
       !Crime_Available(playerid) || gCrimeUIDialog[playerid]!=0 ||
       IsPlayerInAnyVehicle(playerid) ||
       GetPlayerInterior(playerid)!=0 || GetPlayerVirtualWorld(playerid)!=0)
        return 0;
    new id,contact,method,stage,deadline;
    if(!CrimeRecovery_Info(playerid,id,contact,method,stage,deadline) ||
       !id || stage!=1) return 0;
    new slot=(contact-1)*2+method-1;
    if(!gCrimeRecoveryReady[slot] ||
       !IsPlayerInRangeOfPoint(playerid,CRIME_RECOVERY_RADIUS,
            gCrimeRecoveryPos[slot][0],gCrimeRecoveryPos[slot][1],
            gCrimeRecoveryPos[slot][2])) return 0;
    gCrimeRecoverySelected[playerid]=id;
    CrimeUI_Open(playerid,CRIME_RECOVERY_DLG_FIELD);
    return ShowPlayerDialog(playerid,CRIME_RECOVERY_DLG_FIELD,
        DIALOG_STYLE_MSGBOX,
        "{FFD15C}Теневой рынок | Полевое задание",
        method==1 ?
        ("Вы передали новую накладную связному. Он готов подтвердить прикрытие. Записать результат?") :
        ("Вы запутали наблюдателей и оставили ложный след. Записать результат?"),
        "Выполнить","Позже");
}
