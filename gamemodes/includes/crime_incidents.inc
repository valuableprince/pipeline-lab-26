#if defined _tarkenso_crime_incidents_included
    #endinput
#endif
#define _tarkenso_crime_incidents_included

// CRIME-03.3: physical NPC encounter between pickup and delivery on a
// won tender. The DB owns the persisted choice, payout bonus and risk.
#define CRIME_INCIDENT_COUNT (3)
#define CRIME_INCIDENT_RADIUS (3.5)
#define CRIME_INCIDENT_DLG (7973)

new gCrimeIncidentActors[CRIME_INCIDENT_COUNT];
new bool:gCrimeIncidentReady[CRIME_INCIDENT_COUNT];
new gCrimeIncidentSelected[MAX_PLAYERS];
new Float:gCrimeIncidentPos[CRIME_INCIDENT_COUNT][3] =
{
    {2490.0,-2365.0,13.5},
    {-1850.0,1220.0,10.0},
    {2115.0,1690.0,11.0}
};
new gCrimeIncidentModel[CRIME_INCIDENT_COUNT] = {57, 59, 71};

stock CrimeIncident_Reset(playerid)
{
    gCrimeIncidentSelected[playerid]=0;
    return 1;
}

stock CrimeIncident_Shutdown()
{
    for(new i;i<CRIME_INCIDENT_COUNT;i++)
    {
        if(!gCrimeIncidentReady[i]) continue;
        DestroyActor(gCrimeIncidentActors[i]);
        gCrimeIncidentReady[i]=false;
    }
    return 1;
}

stock bool:CrimeIncident_Init()
{
    for(new i;i<CRIME_INCIDENT_COUNT;i++)
    {
        gCrimeIncidentActors[i]=CreateActor(gCrimeIncidentModel[i],
            gCrimeIncidentPos[i][0],gCrimeIncidentPos[i][1],
            gCrimeIncidentPos[i][2],0.0);
        if(gCrimeIncidentActors[i]==INVALID_ACTOR_ID)
        {
            CrimeIncident_Shutdown();
            print("[CRIME-03.3] Encounter NPC spawn failed.");
            return false;
        }
        SetActorInvulnerable(gCrimeIncidentActors[i],true);
        gCrimeIncidentReady[i]=true;
    }
    print("[CRIME-03.3] Three tender encounter NPCs loaded.");
    return true;
}

// Read account-owned contract and durable incident. A missing row is an
// error, never permission to bypass the intermediate objective.
stock bool:CrimeIncident_Info(playerid,contractId,&contact,&code,&choice)
{
    contact=0;code=0;choice=0;
#if defined TARKENSO_DB_PRIMARY
    if(!Crime_Available(playerid) || contractId<=0) return false;
    new q[420],rows=0;
    mysql_format(gDBPrimary,q,sizeof(q),
        "SELECT c.contact_id,i.incident_code,i.choice FROM crime_contracts c JOIN crime_tender_incidents i ON i.contract_id=c.contract_id WHERE c.contract_id=%d AND c.account_id=%d AND c.mode_id=3 AND c.state=2 AND c.expires_at>UNIX_TIMESTAMP() LIMIT 1",
        contractId,gDBPrimaryAccountId[playerid]);
    new Cache:c=mysql_query(gDBPrimary,q);
    if(c==Cache:0 || mysql_errno(gDBPrimary) ||
       !cache_get_row_count(rows) || rows!=1 ||
       !cache_get_value_name_int(0,"contact_id",contact) ||
       !cache_get_value_name_int(0,"incident_code",code) ||
       !cache_get_value_name_int(0,"choice",choice))
    {
        if(c!=Cache:0) cache_delete(c);
        return false;
    }
    cache_delete(c);
    return contact>=1 && contact<=CRIME_INCIDENT_COUNT &&
        code>=1 && code<=3 && choice>=0 && choice<=2;
#else
    #pragma unused playerid,contractId
    return false;
#endif
}

// Returns 1 if an encounter route was displayed, 0 for a regular route,
// and -1 when its persisted state is inconsistent/unavailable.
stock CrimeIncident_Route(playerid,contractId)
{
#if defined TARKENSO_DB_PRIMARY
    new q[230],rows=0,mode=0;
    mysql_format(gDBPrimary,q,sizeof(q),
        "SELECT mode_id FROM crime_contracts WHERE contract_id=%d AND account_id=%d AND state=2 LIMIT 1",
        contractId,gDBPrimaryAccountId[playerid]);
    new Cache:c=mysql_query(gDBPrimary,q);
    if(c==Cache:0 || mysql_errno(gDBPrimary) ||
       !cache_get_row_count(rows) || rows!=1 ||
       !cache_get_value_name_int(0,"mode_id",mode))
    {
        if(c!=Cache:0) cache_delete(c);
        return -1;
    }
    cache_delete(c);
    if(mode!=3) return 0;
    new contact,code,choice;
    if(!CrimeIncident_Info(playerid,contractId,contact,code,choice)) return -1;
    if(choice>0) return 0;
    new description[105];
    switch(code)
    {
        case 1: format(description,sizeof(description),"Проверка документов на пути.");
        case 2: format(description,sizeof(description),"Неожиданная встреча у маршрута.");
        case 3: format(description,sizeof(description),"Возникли вопросы к перевозимому грузу.");
    }
    new body[590];
    format(body,sizeof(body),
        "{FFD15C}НЕОЖИДАННОЕ СОБЫТИЕ{FFFFFF}\n%s\n\nNPC ожидает вас по координатам:\n{FFD15C}X:{FFFFFF} %.1f\n{FFD15C}Y:{FFFFFF} %.1f\n{FFD15C}Z:{FFFFFF} %.1f\n\nПодойдите пешком на 3.5 м и нажмите Y, чтобы решить ситуацию. После этого можно продолжить маршрут. Если игнорировать событие, доставка не завершится.",
        description,gCrimeIncidentPos[contact-1][0],
        gCrimeIncidentPos[contact-1][1],gCrimeIncidentPos[contact-1][2]);
    CrimeUI_Open(playerid,CRIME_DLG_ROUTE);
    ShowPlayerDialog(playerid,CRIME_DLG_ROUTE,DIALOG_STYLE_MSGBOX,
        "{FFD15C}Теневой рынок | Событие в пути",body,"Понятно","Назад");
    return 1;
#else
    #pragma unused playerid,contractId
    return -1;
#endif
}

stock CrimeIncident_OnKeys(playerid,KEY:newkeys,KEY:oldkeys)
{
    if(!(newkeys & KEY_YES) || (oldkeys & KEY_YES) ||
       !Crime_Available(playerid) || gCrimeUIDialog[playerid]!=0 ||
       IsPlayerInAnyVehicle(playerid) ||
       GetPlayerVirtualWorld(playerid)!=0 || GetPlayerInterior(playerid)!=0)
        return 0;
    new id,phase,faction,reward,deadline,title[68];
    new Float:pickup[3],Float:delivery[3];
    if(!Crime_Expire(playerid) ||
       !Crime_LoadActive(playerid,id,phase,faction,reward,deadline,
            pickup,delivery,title,sizeof(title)) ||
       !id || phase!=2 || faction!=Leader_PlayerOrg(playerid)) return 0;
    new contact,code,choice;
    if(!CrimeIncident_Info(playerid,id,contact,code,choice) || choice!=0)
        return 0;
    if(!gCrimeIncidentReady[contact-1] ||
       !IsPlayerInRangeOfPoint(playerid,CRIME_INCIDENT_RADIUS,
           gCrimeIncidentPos[contact-1][0],
           gCrimeIncidentPos[contact-1][1],
           gCrimeIncidentPos[contact-1][2])) return 0;
    new header[92],body[530];
    switch(code)
    {
        case 1: format(header,sizeof(header),"Проверка документов");
        case 2: format(header,sizeof(header),"Неожиданная встреча");
        case 3: format(header,sizeof(header),"Спор о грузе");
    }
    format(body,sizeof(body),
        "{FFD15C}Выбор\t{FFD15C}Последствия{FFFFFF}\nОсторожно разобраться\tБез доплаты, внимание -12\nПройти напролом\tДоплата $300, внимание +12\n\nРешение сохраняется в контракте. Изменить его нельзя.");
    gCrimeIncidentSelected[playerid]=id;
    CrimeUI_Open(playerid,CRIME_INCIDENT_DLG);
    return ShowPlayerDialog(playerid,CRIME_INCIDENT_DLG,
        DIALOG_STYLE_TABLIST_HEADERS,header,body,"Выбрать","Позже");
}

// Re-check player, position, faction, route state and source-of-truth SQL.
// This transaction resolves exactly once and can add only one $300 bonus.
stock bool:CrimeIncident_Resolve(playerid,contractId,choice)
{
#if defined TARKENSO_DB_PRIMARY
    if(choice<1 || choice>2 || contractId<=0 ||
       !Crime_Available(playerid) || gGovTransferBusy[playerid] ||
       IsPlayerInAnyVehicle(playerid) ||
       GetPlayerVirtualWorld(playerid)!=0 || GetPlayerInterior(playerid)!=0)
        return false;
    new contact,code,oldChoice;
    if(!CrimeIncident_Info(playerid,contractId,contact,code,oldChoice) ||
       oldChoice!=0 || !gCrimeIncidentReady[contact-1] ||
       !IsPlayerInRangeOfPoint(playerid,CRIME_INCIDENT_RADIUS,
          gCrimeIncidentPos[contact-1][0],
          gCrimeIncidentPos[contact-1][1],
          gCrimeIncidentPos[contact-1][2])) return false;
    if(!Crime_Exec("START TRANSACTION")) return false;
    new q[660],affected;
    mysql_format(gDBPrimary,q,sizeof(q),
        "UPDATE crime_tender_incidents i JOIN crime_contracts c ON c.contract_id=i.contract_id JOIN faction_members m ON m.account_id=c.account_id AND m.faction_id=c.faction_id SET i.choice=%d,i.resolved_at=UNIX_TIMESTAMP() WHERE i.contract_id=%d AND i.choice=0 AND c.account_id=%d AND c.faction_id=%d AND c.mode_id=3 AND c.state=2 AND c.expires_at>UNIX_TIMESTAMP() AND c.reward_amount<=49700",
        choice,contractId,gDBPrimaryAccountId[playerid],Leader_PlayerOrg(playerid));
    if(!DBPrimary_Execute(q,affected) || affected!=1)
    {
        Crime_Rollback();
        return false;
    }
    if(choice==2)
    {
        mysql_format(gDBPrimary,q,sizeof(q),
            "UPDATE crime_contracts SET reward_amount=reward_amount+300 WHERE contract_id=%d AND account_id=%d AND mode_id=3 AND state=2 AND reward_amount<=49700 AND EXISTS(SELECT 1 FROM crime_tender_incidents i WHERE i.contract_id=%d AND i.choice=2 AND i.resolved_at>0)",
            contractId,gDBPrimaryAccountId[playerid],contractId);
        if(!DBPrimary_Execute(q,affected) || affected!=1)
        {
            Crime_Rollback();
            return false;
        }
    }
    mysql_format(gDBPrimary,q,sizeof(q),
        "INSERT INTO crime_contract_events(contract_id,actor_account_id,event_type) VALUES (%d,%d,'INCIDENT_RESOLVED')",
        contractId,gDBPrimaryAccountId[playerid]);
    if(!Crime_Exec(q))
    {
        Crime_Rollback();
        return false;
    }
    return Crime_Exec("COMMIT");
#else
    #pragma unused playerid,contractId,choice
    return false;
#endif
}

stock CrimeIncident_OnDialog(playerid,dialogid,response,listitem)
{
    if(dialogid!=CRIME_INCIDENT_DLG) return 0;
#if defined TARKENSO_DB_PRIMARY
    if(gCrimeUIDialog[playerid]!=dialogid ||
       gCrimeUIAuthToken[playerid]!=gAuthToken[playerid] ||
       gCrimeUIAccountId[playerid]!=gDBPrimaryAccountId[playerid] ||
       !Crime_Available(playerid))
    {
        CrimeUI_Reset(playerid);
        CrimeIncident_Reset(playerid);
        return 1;
    }
    gCrimeUIDialog[playerid]=0;
    new id=gCrimeIncidentSelected[playerid];
    CrimeIncident_Reset(playerid);
    if(!response || listitem<0 || listitem>1 || id<=0)
        return CrimeUI_Route(playerid);
    if(!CrimeIncident_Resolve(playerid,id,listitem+1))
        SendClientMessage(playerid,CRIME_ERROR,
            "Событие не подтверждено: проверьте контракт и позицию NPC.");
    else SendClientMessage(playerid,CRIME_COLOR,
            "Ситуация разрешена. Продолжайте доставку по маршруту.");
    return CrimeUI_Route(playerid);
#else
    #pragma unused playerid,response,listitem
    return 1;
#endif
}
