#if defined _tarkenso_crime_tenders_included
    #endinput
#endif
#define _tarkenso_crime_tenders_included

// CRIME-03.2: 60-second offers, deterministic NPC selection at closing.
// Rules: the lower fee / shorter execution wins; ties go to earlier bid.
// The resulting contract uses the ordinary SQL wallet and route lifecycle.
#define CRIME_TENDER_WINDOW (60)
#define CRIME_TENDER_BUCKET (600)

stock bool:CrimeTender_Finalize(tenderId)
{
#if defined TARKENSO_DB_PRIMARY
    if(tenderId<=0 || !Crime_Exec("START TRANSACTION")) return false;
    new q[1150],rows=0,state=0,closes=0;
    mysql_format(gDBPrimary,q,sizeof(q),
        "SELECT state,closes_at FROM crime_tender_rounds WHERE tender_id=%d FOR UPDATE",tenderId);
    new Cache:c=mysql_query(gDBPrimary,q);
    if(c==Cache:0 || mysql_errno(gDBPrimary) ||
       !cache_get_row_count(rows) || rows!=1 ||
       !cache_get_value_name_int(0,"state",state) ||
       !cache_get_value_name_int(0,"closes_at",closes))
    {
        if(c!=Cache:0) cache_delete(c);
        Crime_Rollback();
        return false;
    }
    cache_delete(c);
    if(state!=1 || closes>gettime()) return Crime_Exec("COMMIT");

    new bidId=0,account=0,faction=0,code=0;
    mysql_format(gDBPrimary,q,sizeof(q),
        "SELECT b.bid_id,b.account_id,b.faction_id,b.offer_code FROM crime_tender_bids b JOIN crime_tender_rounds r ON r.tender_id=b.tender_id JOIN crime_contract_templates t ON t.template_id=r.template_id AND t.mode_id=3 AND t.is_enabled=1 JOIN faction_members m ON m.account_id=b.account_id AND m.faction_id=b.faction_id JOIN crime_contact_progress p ON p.account_id=b.account_id AND p.contact_id=r.contact_id AND p.trust>=t.required_trust AND p.heat<70 JOIN crime_syndicate_contacts sc ON sc.faction_id=b.faction_id AND sc.contact_id=r.contact_id AND sc.standing>=5 WHERE b.tender_id=%d AND b.state=1 AND NOT EXISTS(SELECT 1 FROM crime_contracts a WHERE a.account_id=b.account_id AND a.state IN(1,2)) ORDER BY b.offer_code,b.bid_id LIMIT 1",tenderId);
    c=mysql_query(gDBPrimary,q);
    if(c==Cache:0 || mysql_errno(gDBPrimary) || !cache_get_row_count(rows) || rows>1)
    {
        if(c!=Cache:0) cache_delete(c);
        Crime_Rollback();
        return false;
    }
    if(rows==1 && (!cache_get_value_name_int(0,"bid_id",bidId) ||
                   !cache_get_value_name_int(0,"account_id",account) ||
                   !cache_get_value_name_int(0,"faction_id",faction) ||
                   !cache_get_value_name_int(0,"offer_code",code)))
    {
        cache_delete(c);
        Crime_Rollback();
        return false;
    }
    cache_delete(c);
    new changed;
    if(bidId>0)
    {
        // Recheck eligibility in INSERT; the unique tender_id also prevents
        // duplicate contract creation if a stale client retries resolution.
        mysql_format(gDBPrimary,q,sizeof(q),
            "INSERT INTO crime_contracts(template_id,account_id,faction_id,mode_id,reward_amount,started_at,expires_at,contact_id,scenario_kind,scenario_event,tender_id) SELECT t.template_id,b.account_id,b.faction_id,3,FLOOR(t.reward_amount*(CASE b.offer_code WHEN 1 THEN 70 WHEN 2 THEN 85 ELSE 100 END)/100),UNIX_TIMESTAMP(),UNIX_TIMESTAMP()+(CASE b.offer_code WHEN 1 THEN 900 WHEN 2 THEN 1500 ELSE 2400 END),r.contact_id,0,0,r.tender_id FROM crime_tender_bids b JOIN crime_tender_rounds r ON r.tender_id=b.tender_id AND r.state=1 JOIN crime_contract_templates t ON t.template_id=r.template_id AND t.mode_id=3 AND t.is_enabled=1 JOIN faction_members m ON m.account_id=b.account_id AND m.faction_id=b.faction_id JOIN crime_contact_progress p ON p.account_id=b.account_id AND p.contact_id=r.contact_id AND p.trust>=t.required_trust AND p.heat<70 JOIN crime_syndicate_contacts sc ON sc.faction_id=b.faction_id AND sc.contact_id=r.contact_id AND sc.standing>=5 WHERE b.bid_id=%d AND b.tender_id=%d AND b.account_id=%d AND b.faction_id=%d AND b.offer_code=%d AND b.state=1 AND NOT EXISTS(SELECT 1 FROM crime_contracts h WHERE h.account_id=b.account_id AND h.state IN(1,2))",
            bidId,tenderId,account,faction,code);
        if(!DBPrimary_Execute(q,changed) || changed!=1)
        {
            Crime_Rollback();
            return false;
        }
        new contractId=0;
        c=mysql_query(gDBPrimary,"SELECT LAST_INSERT_ID() AS id");
        if(c==Cache:0 || mysql_errno(gDBPrimary) ||
           !cache_get_row_count(rows) || rows!=1 ||
           !cache_get_value_name_int(0,"id",contractId) || contractId<=0)
        {
            if(c!=Cache:0) cache_delete(c);
            Crime_Rollback();
            return false;
        }
        cache_delete(c);
        mysql_format(gDBPrimary,q,sizeof(q),
            "UPDATE crime_tender_rounds SET state=2,winner_bid_id=%d,winner_contract_id=%d,decided_at=UNIX_TIMESTAMP() WHERE tender_id=%d AND state=1",
            bidId,contractId,tenderId);
        if(!DBPrimary_Execute(q,changed) || changed!=1)
        {
            Crime_Rollback();
            return false;
        }
        mysql_format(gDBPrimary,q,sizeof(q),
            "INSERT INTO crime_contract_events(contract_id,actor_account_id,event_type) VALUES (%d,%d,'ACCEPTED')",
            contractId,account);
        if(!Crime_Exec(q))
        {
            Crime_Rollback();
            return false;
        }
    }
    else
    {
        mysql_format(gDBPrimary,q,sizeof(q),
            "UPDATE crime_tender_rounds SET state=3,decided_at=UNIX_TIMESTAMP() WHERE tender_id=%d AND state=1",
            tenderId);
        if(!DBPrimary_Execute(q,changed) || changed!=1)
        {
            Crime_Rollback();
            return false;
        }
    }
    mysql_format(gDBPrimary,q,sizeof(q),
        "UPDATE crime_tender_bids SET state=CASE WHEN bid_id=%d THEN 2 ELSE 3 END WHERE tender_id=%d AND state=1",
        bidId,tenderId);
    if(!Crime_Exec(q))
    {
        Crime_Rollback();
        return false;
    }
    return Crime_Exec("COMMIT");
#else
    #pragma unused tenderId
    return false;
#endif
}

stock bool:CrimeTender_Refresh(playerid)
{
#if defined TARKENSO_DB_PRIMARY
    if(!Crime_Available(playerid)) return false;
    // Finalize older rounds before creating new ones: releases pending slots.
    new ids[24],count=0,rows=0;
    new Cache:c=mysql_query(gDBPrimary,
        "SELECT tender_id FROM crime_tender_rounds WHERE state=1 AND closes_at<=UNIX_TIMESTAMP() ORDER BY tender_id LIMIT 24");
    if(c==Cache:0 || mysql_errno(gDBPrimary) ||
       !cache_get_row_count(rows) || rows>24)
    {
        if(c!=Cache:0) cache_delete(c);
        return false;
    }
    for(new i;i<rows;i++)
    {
        if(!cache_get_value_name_int(i,"tender_id",ids[i]))
        {
            cache_delete(c);
            return false;
        }
        count++;
    }
    cache_delete(c);
    for(new i;i<count;i++)
        if(!CrimeTender_Finalize(ids[i])) return false;
    return Crime_Exec(
        "INSERT IGNORE INTO crime_tender_rounds(contact_id,template_id,bucket,opens_at,closes_at) SELECT t.contact_id,t.template_id,FLOOR(UNIX_TIMESTAMP()/600),UNIX_TIMESTAMP(),UNIX_TIMESTAMP()+60 FROM crime_contract_templates t JOIN crime_contacts n ON n.contact_id=t.contact_id AND n.is_enabled=1 WHERE t.template_id IN(10,11,12) AND t.mode_id=3 AND t.is_enabled=1");
#else
    #pragma unused playerid
    return false;
#endif
}

stock CrimeTender_Submit(playerid,tenderId,offerCode)
{
    if(!Crime_Available(playerid) || gGovTransferBusy[playerid] ||
       !Leader_IsCriminal(Leader_PlayerOrg(playerid)) ||
       tenderId<=0 || offerCode<1 || offerCode>3)
        return SendClientMessage(playerid,CRIME_ERROR,"Предложение недоступно.");
#if defined TARKENSO_DB_PRIMARY
    if(!CrimeTender_Refresh(playerid) || !Crime_Expire(playerid))
        return SendClientMessage(playerid,CRIME_ERROR,"Не удалось обновить торги.");
    new q[1650],contact=0,rows=0;
    mysql_format(gDBPrimary,q,sizeof(q),
        "SELECT contact_id FROM crime_tender_rounds WHERE tender_id=%d AND state=1 AND closes_at>UNIX_TIMESTAMP() LIMIT 1",
        tenderId);
    new Cache:c=mysql_query(gDBPrimary,q);
    if(c==Cache:0 || mysql_errno(gDBPrimary) ||
       !cache_get_row_count(rows) || rows!=1 ||
       !cache_get_value_name_int(0,"contact_id",contact))
    {
        if(c!=Cache:0) cache_delete(c);
        return SendClientMessage(playerid,CRIME_ERROR,"Приём предложений закрыт.");
    }
    cache_delete(c);
    if(!CrimeContacts_Near(playerid,contact))
        return SendClientMessage(playerid,CRIME_ERROR,
            "Предложение можно передать только лично посреднику (до 5 м).");
    if(!Crime_Exec("START TRANSACTION"))
        return SendClientMessage(playerid,CRIME_ERROR,"Нет соединения с базой.");
    new changed;
    mysql_format(gDBPrimary,q,sizeof(q),
        "INSERT INTO crime_tender_bids(tender_id,account_id,faction_id,offer_code,submitted_at) SELECT r.tender_id,m.account_id,m.faction_id,%d,UNIX_TIMESTAMP() FROM crime_tender_rounds r JOIN crime_contract_templates t ON t.template_id=r.template_id AND t.mode_id=3 AND t.is_enabled=1 JOIN faction_members m ON m.account_id=%d AND m.faction_id=%d JOIN crime_contact_progress p ON p.account_id=m.account_id AND p.contact_id=r.contact_id AND p.trust>=t.required_trust AND p.heat<70 JOIN crime_syndicate_contacts sc ON sc.faction_id=m.faction_id AND sc.contact_id=r.contact_id AND sc.standing>=5 WHERE r.tender_id=%d AND r.state=1 AND r.closes_at>UNIX_TIMESTAMP() AND NOT EXISTS(SELECT 1 FROM crime_contracts h WHERE h.account_id=m.account_id AND h.state IN(1,2))",
        offerCode,gDBPrimaryAccountId[playerid],Leader_PlayerOrg(playerid),tenderId);
    if(!DBPrimary_Execute(q,changed) || changed!=1)
    {
        Crime_Rollback();
        return SendClientMessage(playerid,CRIME_ERROR,
            "Заявка отклонена: доверие 5, связи 5, внимание ниже 70; один участник от синдиката, один ожидающий конкурс и нет активного заказа.");
    }
    if(!Crime_Exec("COMMIT"))
        return SendClientMessage(playerid,CRIME_ERROR,
            "Итог заявки неизвестен. Проверьте доску предложений.");
    return SendClientMessage(playerid,CRIME_COLOR,
        "Предложение принято. Через 60 секунд после открытия конкурса проверьте результат у NPC.");
#else
    #pragma unused tenderId,offerCode
    return 1;
#endif
}

// Called inside the existing expire/abort transaction. The INSERT ledger
// and both reputation changes must roll back together on any DB error.
stock bool:CrimeTender_Penalize(playerid,contractId)
{
#if defined TARKENSO_DB_PRIMARY
    if(contractId<=0) return true;
    new q[580],changed;
    mysql_format(gDBPrimary,q,sizeof(q),
        "INSERT IGNORE INTO crime_tender_penalties(contract_id,account_id,faction_id,contact_id,reason) SELECT contract_id,account_id,faction_id,contact_id,state FROM crime_contracts WHERE contract_id=%d AND account_id=%d AND mode_id=3 AND state IN(4,5)",
        contractId,gDBPrimaryAccountId[playerid]);
    if(!DBPrimary_Execute(q,changed)) return false;
    if(changed==0) return true;
    mysql_format(gDBPrimary,q,sizeof(q),
        "UPDATE crime_contact_progress p JOIN crime_tender_penalties e ON e.account_id=p.account_id AND e.contact_id=p.contact_id SET p.trust=GREATEST(0,CAST(p.trust AS SIGNED)-3),p.heat=LEAST(100,p.heat+5) WHERE e.contract_id=%d",
        contractId);
    if(!Crime_Exec(q)) return false;
    mysql_format(gDBPrimary,q,sizeof(q),
        "UPDATE crime_syndicate_contacts s JOIN crime_tender_penalties e ON e.faction_id=s.faction_id AND e.contact_id=s.contact_id SET s.standing=GREATEST(0,CAST(s.standing AS SIGNED)-2) WHERE e.contract_id=%d",
        contractId);
    return Crime_Exec(q);
#else
    #pragma unused playerid,contractId
    return false;
#endif
}

stock bool:CrimeTender_PenalizeExpired(playerid)
{
#if defined TARKENSO_DB_PRIMARY
    new q[340],rows=0,contractId=0;
    mysql_format(gDBPrimary,q,sizeof(q),
        "SELECT c.contract_id FROM crime_contracts c WHERE c.account_id=%d AND c.mode_id=3 AND c.state=4 AND NOT EXISTS(SELECT 1 FROM crime_tender_penalties e WHERE e.contract_id=c.contract_id) ORDER BY c.contract_id LIMIT 1",
        gDBPrimaryAccountId[playerid]);
    new Cache:c=mysql_query(gDBPrimary,q);
    if(c==Cache:0 || mysql_errno(gDBPrimary) ||
       !cache_get_row_count(rows) || rows>1 ||
       (rows==1 && !cache_get_value_name_int(0,"contract_id",contractId)))
    {
        if(c!=Cache:0) cache_delete(c);
        return false;
    }
    cache_delete(c);
    return CrimeTender_Penalize(playerid,contractId);
#else
    #pragma unused playerid
    return false;
#endif
}
