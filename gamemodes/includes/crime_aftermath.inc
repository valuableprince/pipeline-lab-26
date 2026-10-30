#if defined _tarkenso_crime_aftermath_included
    #endinput
#endif
#define _tarkenso_crime_aftermath_included

// CRIME-03.4: different physical follow-up for safe/risky choices.
// 3 cities x 2 possible paths, exactly one path active per contract.
#define CRIME_AFTERMATH_COUNT (6)
#define CRIME_AFTERMATH_RADIUS (3.5)
#define CRIME_AFTERMATH_DLG (7974)

new bool:gCrimeAftermathReady[CRIME_AFTERMATH_COUNT];
new gCrimeAftermathActors[CRIME_AFTERMATH_COUNT];
new gCrimeAftermathSelected[MAX_PLAYERS];
new Float:gCrimeAftermathPos[CRIME_AFTERMATH_COUNT][3]=
{
    {2410.0,-2320.0,13.5}, {2640.0,-2300.0,13.5},
    {-1930.0,1170.0,45.0}, {-1780.0,1120.0,45.0},
    {2040.0,1780.0,12.0}, {2200.0,1760.0,12.0}
};
new gCrimeAftermathModels[CRIME_AFTERMATH_COUNT]={60,73,98,112,134,170};

stock CrimeAftermath_Reset(playerid)
{
    gCrimeAftermathSelected[playerid]=0;
    return 1;
}

stock CrimeAftermath_Shutdown()
{
    for(new i;i<CRIME_AFTERMATH_COUNT;i++)
    {
        if(!gCrimeAftermathReady[i]) continue;
        DestroyActor(gCrimeAftermathActors[i]);
        gCrimeAftermathReady[i]=false;
    }
    return 1;
}

stock bool:CrimeAftermath_Init()
{
    for(new i;i<CRIME_AFTERMATH_COUNT;i++)
    {
        gCrimeAftermathActors[i]=CreateActor(gCrimeAftermathModels[i],
            gCrimeAftermathPos[i][0],gCrimeAftermathPos[i][1],
            gCrimeAftermathPos[i][2],0.0);
        if(gCrimeAftermathActors[i]==INVALID_ACTOR_ID)
        {
            CrimeAftermath_Shutdown();
            print("[CRIME-03.4] Follow-up NPC spawn failed.");
            return false;
        }
        SetActorInvulnerable(gCrimeAftermathActors[i],true);
        gCrimeAftermathReady[i]=true;
    }
    print("[CRIME-03.4] Six branch-specific NPCs loaded.");
    return true;
}

// Returns 0 for no tender follow-up, -1 on SQL error, and 1 for a valid
// follow-up state (including already completed). No missing-state bypass.
stock CrimeAftermath_Info(playerid,contractId,&contact,&choice,&stateCode)
{
    contact=0;choice=0;stateCode=0;
#if defined TARKENSO_DB_PRIMARY
    if(!Crime_Available(playerid) || contractId<=0) return -1;
    new q[300],rows=0,mode=0;
    mysql_format(gDBPrimary,q,sizeof(q),
        "SELECT c.mode_id,c.contact_id,a.choice,a.state FROM crime_contracts c LEFT JOIN crime_tender_aftermath a ON a.contract_id=c.contract_id WHERE c.contract_id=%d AND c.account_id=%d AND c.state=2 AND c.expires_at>UNIX_TIMESTAMP() LIMIT 1",
        contractId,gDBPrimaryAccountId[playerid]);
    new Cache:c=mysql_query(gDBPrimary,q);
    if(c==Cache:0 || mysql_errno(gDBPrimary) ||
       !cache_get_row_count(rows) || rows!=1 ||
       !cache_get_value_name_int(0,"mode_id",mode))
    {
        if(c!=Cache:0) cache_delete(c);
        return -1;
    }
    if(mode!=3)
    {
        cache_delete(c);
        return 0;
    }
    new bool:ok=cache_get_value_name_int(0,"contact_id",contact) &&
        cache_get_value_name_int(0,"choice",choice) &&
        cache_get_value_name_int(0,"state",stateCode);
    cache_delete(c);
    if(!ok || contact<1 || contact>3 || choice<1 || choice>2 ||
       stateCode<1 || stateCode>2) return -1;
    return 1;
#else
    #pragma unused playerid,contractId
    return -1;
#endif
}

stock CrimeAftermath_Route(playerid,contractId)
{
    new contact,choice,stateCode;
    new result=CrimeAftermath_Info(playerid,contractId,contact,choice,stateCode);
    if(result<=0 || stateCode==2) return result;
    new slot=(contact-1)*2+choice-1;
    if(!gCrimeAftermathReady[slot]) return -1;
    new body[530];
    format(body,sizeof(body),
        "{FFD15C}%s{FFFFFF}\n%s\n\n{FFD15C}ВТОРАЯ ТОЧКА:{FFFFFF}\nX: %.1f\nY: %.1f\nZ: %.1f\n\nДоберитесь до контакта пешком и нажмите Y в пределах 3,5 м. Без этого окончательная передача груза невозможна.",
        choice==1 ? ("Подтвердить прикрытие") : ("Избавиться от наблюдения"),
        choice==1 ? ("Передайте документы доверенному связному, чтобы сохранить репутацию.") :
        ("Найдите наблюдателя и договоритесь о снятии контроля за грузом."),
        gCrimeAftermathPos[slot][0],gCrimeAftermathPos[slot][1],
        gCrimeAftermathPos[slot][2]);
    CrimeUI_Open(playerid,CRIME_DLG_ROUTE);
    ShowPlayerDialog(playerid,CRIME_DLG_ROUTE,DIALOG_STYLE_MSGBOX,
        "{FFD15C}Теневой рынок | Последствия выбора",body,"Понятно","Назад");
    return 1;
}

stock CrimeAftermath_OnKeys(playerid,KEY:newkeys,KEY:oldkeys)
{
    if(!(newkeys & KEY_YES) || (oldkeys & KEY_YES) ||
       !Crime_Available(playerid) || gCrimeUIDialog[playerid]!=0 ||
       IsPlayerInAnyVehicle(playerid) ||
       GetPlayerInterior(playerid)!=0 || GetPlayerVirtualWorld(playerid)!=0)
        return 0;
    new id,phase,faction,reward,deadline,title[68];
    new Float:pickup[3],Float:delivery[3];
    if(!Crime_Expire(playerid) ||
       !Crime_LoadActive(playerid,id,phase,faction,reward,deadline,
            pickup,delivery,title,sizeof(title)) ||
       !id || phase!=2 || faction!=Leader_PlayerOrg(playerid)) return 0;
    new contact,choice,stateCode;
    if(CrimeAftermath_Info(playerid,id,contact,choice,stateCode)!=1 ||
       stateCode!=1) return 0;
    new slot=(contact-1)*2+choice-1;
    if(!gCrimeAftermathReady[slot] ||
       !IsPlayerInRangeOfPoint(playerid,CRIME_AFTERMATH_RADIUS,
           gCrimeAftermathPos[slot][0],gCrimeAftermathPos[slot][1],
           gCrimeAftermathPos[slot][2])) return 0;
    gCrimeAftermathSelected[playerid]=id;
    CrimeUI_Open(playerid,CRIME_AFTERMATH_DLG);
    return ShowPlayerDialog(playerid,CRIME_AFTERMATH_DLG,DIALOG_STYLE_MSGBOX,
        "{FFD15C}Завершить промежуточное задание",
        choice==1 ?
        ("Вы встретились со связным и проверили прикрытие. После доставки посредник повысит доверие на 7, связи синдиката — на 4 (без бонусов активов). Зафиксировать выполнение?") :
        ("Вы нашли наблюдателя. Доплата за риск сохраняется, но посредник повысит доверие лишь на 2, связи синдиката — на 1 (без бонусов активов). Зафиксировать выполнение?"),
        "Завершить","Позже");
}

stock bool:CrimeAftermath_Complete(playerid,contractId)
{
#if defined TARKENSO_DB_PRIMARY
    if(!Crime_Available(playerid) || contractId<=0 ||
       gGovTransferBusy[playerid] || IsPlayerInAnyVehicle(playerid) ||
       GetPlayerInterior(playerid)!=0 || GetPlayerVirtualWorld(playerid)!=0)
        return false;
    new contact,choice,stateCode;
    if(CrimeAftermath_Info(playerid,contractId,contact,choice,stateCode)!=1 ||
       stateCode!=1) return false;
    new slot=(contact-1)*2+choice-1;
    if(!gCrimeAftermathReady[slot] ||
       !IsPlayerInRangeOfPoint(playerid,CRIME_AFTERMATH_RADIUS,
          gCrimeAftermathPos[slot][0],gCrimeAftermathPos[slot][1],
          gCrimeAftermathPos[slot][2]) ||
       !Crime_Exec("START TRANSACTION")) return false;
    new q[690],affected;
    mysql_format(gDBPrimary,q,sizeof(q),
        "UPDATE crime_tender_aftermath a JOIN crime_contracts c ON c.contract_id=a.contract_id JOIN crime_tender_incidents i ON i.contract_id=c.contract_id AND i.choice=a.choice AND i.resolved_at>0 JOIN faction_members m ON m.account_id=c.account_id AND m.faction_id=c.faction_id SET a.state=2,a.finished_at=UNIX_TIMESTAMP() WHERE a.contract_id=%d AND a.state=1 AND a.finished_at=0 AND c.account_id=%d AND c.faction_id=%d AND c.mode_id=3 AND c.state=2 AND c.expires_at>UNIX_TIMESTAMP()",
        contractId,gDBPrimaryAccountId[playerid],Leader_PlayerOrg(playerid));
    if(!DBPrimary_Execute(q,affected) || affected!=1)
    {
        Crime_Rollback();
        return false;
    }
    mysql_format(gDBPrimary,q,sizeof(q),
        "INSERT INTO crime_contract_events(contract_id,actor_account_id,event_type) VALUES (%d,%d,'AFTERMATH_DONE')",
        contractId,gDBPrimaryAccountId[playerid]);
    if(!Crime_Exec(q))
    {
        Crime_Rollback();
        return false;
    }
    return Crime_Exec("COMMIT");
#else
    #pragma unused playerid,contractId
    return false;
#endif
}

stock CrimeAftermath_OnDialog(playerid,dialogid,response,listitem)
{
    if(dialogid!=CRIME_AFTERMATH_DLG) return 0;
#if defined TARKENSO_DB_PRIMARY
    if(gCrimeUIDialog[playerid]!=dialogid ||
       gCrimeUIAuthToken[playerid]!=gAuthToken[playerid] ||
       gCrimeUIAccountId[playerid]!=gDBPrimaryAccountId[playerid] ||
       !Crime_Available(playerid))
    {
        CrimeUI_Reset(playerid);
        CrimeAftermath_Reset(playerid);
        return 1;
    }
    gCrimeUIDialog[playerid]=0;
    new id=gCrimeAftermathSelected[playerid];
    CrimeAftermath_Reset(playerid);
    if(!response || id<=0) return CrimeUI_Route(playerid);
    if(!CrimeAftermath_Complete(playerid,id))
        SendClientMessage(playerid,CRIME_ERROR,
            "Задание не зачтено: проверьте контракт, место и состояние NPC.");
    else SendClientMessage(playerid,CRIME_COLOR,
            "Связной подтвердил результат. Теперь доставьте груз получателю.");
    return CrimeUI_Route(playerid);
#else
    #pragma unused playerid,response,listitem
    return 1;
#endif
}
