#if defined _tarkenso_crime_reconnect_included
    #endinput
#endif
#define _tarkenso_crime_reconnect_included

// CRIME-UAT-01: one-shot chat reminder on successful world entry.
// The database owns contract stages and deadlines; this only reads them
// and invokes the existing expiration / tender resolution transactions.
new bool:gCrimeReconnectNotified[MAX_PLAYERS];

stock CrimeReconnect_Reset(playerid)
{
    gCrimeReconnectNotified[playerid] = false;
    return 1;
}

// Inline open.mp/SAMP chat colours match the /crime dialog palette:
// gold #FFD15C: prefix and emphasis, white #FFFFFF: body,
// grey #C7C7C7: hints, muted red #E27E82: warnings.
stock CrimeReconnect_Chat(playerid, bool:warning, const body[])
{
    new line[165];
    format(line,sizeof(line),
        "{FFD15C}[Криминальная система]{FFFFFF} %s",body);
    return SendClientMessage(playerid,
        warning ? CRIME_ERROR : CRIME_COLOR,line);
}

#if defined TARKENSO_DB_PRIMARY
stock bool:CrimeReconnect_ReadPending(playerid, &bidId, &tenderId, &closesAt)
{
    bidId = 0;
    tenderId = 0;
    closesAt = 0;
    new query[520],rows=0;
    mysql_format(gDBPrimary,query,sizeof(query),
        "SELECT b.bid_id,b.tender_id,r.closes_at FROM crime_tender_bids b JOIN crime_tender_rounds r ON r.tender_id=b.tender_id JOIN faction_members m ON m.account_id=b.account_id AND m.faction_id=b.faction_id WHERE b.account_id=%d AND b.faction_id=%d AND b.state=1 AND r.state=1 ORDER BY b.bid_id DESC LIMIT 2",
        gDBPrimaryAccountId[playerid],Leader_PlayerOrg(playerid));
    new Cache:c=mysql_query(gDBPrimary,query);
    if(c==Cache:0 || mysql_errno(gDBPrimary) ||
       !cache_get_row_count(rows) || rows>1)
    {
        if(c!=Cache:0) cache_delete(c);
        return false;
    }
    if(rows==1 &&
       (!cache_get_value_name_int(0,"bid_id",bidId) ||
        !cache_get_value_name_int(0,"tender_id",tenderId) ||
        !cache_get_value_name_int(0,"closes_at",closesAt) ||
        bidId<=0 || tenderId<=0 || closesAt<=0))
    {
        cache_delete(c);
        return false;
    }
    cache_delete(c);
    return true;
}

stock bool:CrimeReconnect_ReadContract(playerid, &contractId, &phase,
    &faction, &deadline, &kind, title[], titleSize)
{
    contractId=0;
    phase=0;
    faction=0;
    deadline=0;
    kind=0;
    new query[440],rows=0;
    mysql_format(gDBPrimary,query,sizeof(query),
        "SELECT c.contract_id,c.state,c.faction_id,c.expires_at,c.scenario_kind,t.title FROM crime_contracts c JOIN crime_contract_templates t ON t.template_id=c.template_id WHERE c.account_id=%d AND c.state IN(1,2) LIMIT 2",
        gDBPrimaryAccountId[playerid]);
    new Cache:c=mysql_query(gDBPrimary,query);
    if(c==Cache:0 || mysql_errno(gDBPrimary) ||
       !cache_get_row_count(rows) || rows>1)
    {
        if(c!=Cache:0) cache_delete(c);
        return false;
    }
    if(rows==1 &&
       (!cache_get_value_name_int(0,"contract_id",contractId) ||
        !cache_get_value_name_int(0,"state",phase) ||
        !cache_get_value_name_int(0,"faction_id",faction) ||
        !cache_get_value_name_int(0,"expires_at",deadline) ||
        !cache_get_value_name_int(0,"scenario_kind",kind) ||
        !cache_get_value_name(0,"title",title,titleSize) ||
        contractId<=0 || (phase!=1 && phase!=2) ||
        faction<6 || faction>8 || deadline<=0 || kind<0 || kind>3))
    {
        cache_delete(c);
        return false;
    }
    cache_delete(c);
    return true;
}

stock CrimeReconnect_Notify(playerid)
{
    if(!Crime_Available(playerid) ||
       !Leader_IsCriminal(Leader_PlayerOrg(playerid)) ||
       gCrimeReconnectNotified[playerid]) return 1;
    // Never repeat from OnPlayerSpawn or a dialog; one attempt per login.
    gCrimeReconnectNotified[playerid]=true;
    new bidId,tenderId,closesAt;
    new bool:pendingOK=CrimeReconnect_ReadPending(playerid,bidId,tenderId,closesAt);
    new bool:justDecided=false;
    if(pendingOK && bidId>0 && closesAt<=gettime())
    {
        // Resolve only this now-due tender, not unrelated rounds or new boards.
        // The existing SQL transaction handles solo NPC awards exactly once.
        justDecided=CrimeTender_Finalize(tenderId);
        if(!justDecided)
        {
            CrimeReconnect_Chat(playerid,true,
                "{E27E82}Не удалось обновить итоги тендера.{FFFFFF}");
            SendClientMessage(playerid,CRIME_COLOR,
                "{C7C7C7}Проверьте результаты в {FFD15C}/crime{C7C7C7}.");
        }
    }

    new contractId,phase,faction,deadline,kind,title[68];
    new bool:contractOK=CrimeReconnect_ReadContract(playerid,contractId,
        phase,faction,deadline,kind,title,sizeof(title));
    if(!contractOK)
        return CrimeReconnect_Chat(playerid,true,
            "{E27E82}Не удалось проверить незавершённые задания.{FFFFFF}");
    if(contractId>0)
    {
        new text[175];
        if(deadline<=gettime())
        {
            // Only inform about an expiration after COMMIT succeeds. This
            // also applies the existing tender penalty exactly once.
            if(!Crime_Expire(playerid))
                return CrimeReconnect_Chat(playerid,true,
                    "{E27E82}Не удалось обновить срок контракта.{FFFFFF}");
            CrimeReconnect_Chat(playerid,true,
                "{E27E82}Контракт просрочен.{FFFFFF}");
            format(text,sizeof(text),
                "{C7C7C7}Контракт {FFD15C}#%d{C7C7C7}: {FFFFFF}%s",
                contractId,title);
            SendClientMessage(playerid,CRIME_COLOR,text);
            SendClientMessage(playerid,CRIME_COLOR,
                "{C7C7C7}Время истекло. История: {FFD15C}/crime{C7C7C7}.");
        }
        else
        {
            CrimeReconnect_Chat(playerid,false,
                "{FFFFFF}Остался {FFD15C}незавершённый контракт{FFFFFF}.");
            format(text,sizeof(text),
                "{FFD15C}Контракт #%d{FFFFFF}: %s",contractId,title);
            SendClientMessage(playerid,CRIME_COLOR,text);
            new stage[64];
            CrimeScenario_StageLabel(kind,phase,stage,sizeof(stage));
            // Tender delivery has mandatory physical NPC stages after pickup.
            // Read the persisted choices so the notice never suggests that
            // final delivery is already available before both encounters.
            if(kind==0 && phase==2)
            {
                new contact,code,choice;
                if(CrimeIncident_Info(playerid,contractId,contact,code,choice))
                {
                    if(choice==0)
                        format(stage,sizeof(stage),"Встреча с первым связным");
                    else
                    {
                        new secondContact,secondChoice,secondState;
                        if(CrimeAftermath_Info(playerid,contractId,
                           secondContact,secondChoice,secondState)==1 &&
                           secondState==1)
                            format(stage,sizeof(stage),"Встреча со вторым связным");
                    }
                }
            }
            new mins=(deadline-gettime()+59)/60;
            if(mins<1) mins=1;
            format(text,sizeof(text),
                "{FFD15C}Этап:{FFFFFF} %s. {FFD15C}Осталось:{FFFFFF} %d мин.",
                stage,mins);
            SendClientMessage(playerid,CRIME_COLOR,text);
            if(faction==Leader_PlayerOrg(playerid))
                SendClientMessage(playerid,CRIME_COLOR,
                    "{C7C7C7}Продолжить задание: {FFD15C}/crime{C7C7C7} -> Мой контракт / Маршрут.");
            else
                SendClientMessage(playerid,CRIME_ERROR,
                    "{E27E82}Членство в синдикате изменилось.{C7C7C7} Проверьте {FFD15C}/crime{C7C7C7}.");
        }
    }

    if(!pendingOK || bidId<=0) return 1;
    if(closesAt>gettime())
    {
        new msg[156],remaining=(closesAt-gettime()+59)/60;
        if(remaining<1) remaining=1;
        format(msg,sizeof(msg),
            "{FFFFFF}Тендер {FFD15C}#%d{FFFFFF}: ожидает решения NPC.",
            tenderId);
        CrimeReconnect_Chat(playerid,false,msg);
        format(msg,sizeof(msg),
            "{C7C7C7}Осталось: {FFD15C}%d мин.{C7C7C7} Проверить: {FFD15C}/crime{C7C7C7}.",
            remaining);
        SendClientMessage(playerid,CRIME_COLOR,msg);
        return 1;
    }
    if(!justDecided) return 1;

    // Show a fresh loss only if the bid was actually decided on this login.
    // Successful tenders already appear above as a persisted active contract.
    new query[225],rows=0,bidState=0;
    mysql_format(gDBPrimary,query,sizeof(query),
        "SELECT state FROM crime_tender_bids WHERE bid_id=%d AND account_id=%d AND faction_id=%d LIMIT 1",
        bidId,gDBPrimaryAccountId[playerid],Leader_PlayerOrg(playerid));
    new Cache:c=mysql_query(gDBPrimary,query);
    if(c==Cache:0 || mysql_errno(gDBPrimary) ||
       !cache_get_row_count(rows) || rows!=1 ||
       !cache_get_value_name_int(0,"state",bidState))
    {
        if(c!=Cache:0) cache_delete(c);
        return CrimeReconnect_Chat(playerid,true,
            "{E27E82}Итог заявки недоступен.{C7C7C7} Проверьте {FFD15C}/crime{C7C7C7}.");
    }
    cache_delete(c);
    if(bidState==3)
    {
        CrimeReconnect_Chat(playerid,false,
            "{FFFFFF}Заявка на тендер {E27E82}не выбрана NPC{FFFFFF}.");
        SendClientMessage(playerid,CRIME_COLOR,
            "{C7C7C7}Новые заказы: {FFD15C}/crime{C7C7C7}.");
    }
    return 1;
}
#else
stock CrimeReconnect_Notify(playerid)
{
    #pragma unused playerid
    return 1;
}
#endif
