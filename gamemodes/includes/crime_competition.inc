#if defined _tarkenso_crime_competition_included
    #endinput
#endif
#define _tarkenso_crime_competition_included

// CRIME-03.1: 3 shared hourly delivery rounds, exactly one winner worldwide.
// All competitors can finish solo; the first committed completion wins.
// MySQL is the single source of truth for offers, claims, expiry and results.
#define CRIME_COMP_MIN_LEFT (300)

stock bool:CrimeComp_Refresh(playerid)
{
#if defined TARKENSO_DB_PRIMARY
    if (!Crime_Available(playerid)) return false;
    // Lazy generation: a visiting player is enough to create the hourly
    // boards. Unique(contact_id,hour_bucket) prevents duplicate rounds.
    if (!Crime_Exec(
        "INSERT IGNORE INTO crime_competition_rounds(contact_id,template_id,hour_bucket,opens_at,expires_at) SELECT t.contact_id,t.template_id,FLOOR(UNIX_TIMESTAMP()/3600),FLOOR(UNIX_TIMESTAMP()/3600)*3600,(FLOOR(UNIX_TIMESTAMP()/3600)+1)*3600 FROM crime_contract_templates t JOIN crime_contacts n ON n.contact_id=t.contact_id AND n.is_enabled=1 WHERE t.template_id IN(7,8,9) AND t.mode_id=2 AND t.is_enabled=1"))
        return false;
    if (!Crime_Exec(
        "UPDATE crime_competition_rounds SET state=3,closed_at=UNIX_TIMESTAMP() WHERE state=1 AND expires_at<=UNIX_TIMESTAMP()"))
        return false;
    // Release any expired account slot on a closed shared board; accounts
    // can also be expired through existing Crime_Expire on login.
    if (!Crime_Exec(
        "UPDATE crime_contracts c JOIN crime_competition_rounds r ON r.round_id=c.competition_id SET c.state=4,c.finished_at=UNIX_TIMESTAMP() WHERE r.state=3 AND c.state IN(1,2)"))
        return false;
    return true;
#else
    #pragma unused playerid
    return false;
#endif
}

stock bool:CrimeComp_RoundForContract(playerid, contractId, faction, &roundId)
{
    roundId = 0;
#if defined TARKENSO_DB_PRIMARY
    new q[260], rows = 0;
    mysql_format(gDBPrimary,q,sizeof(q),
        "SELECT COALESCE(competition_id,0) round_id FROM crime_contracts WHERE contract_id=%d AND account_id=%d AND faction_id=%d AND state=2 LIMIT 1",
        contractId,gDBPrimaryAccountId[playerid],faction);
    new Cache:c=mysql_query(gDBPrimary,q);
    if (c==Cache:0 || mysql_errno(gDBPrimary) ||
        !cache_get_row_count(rows) || rows!=1 ||
        !cache_get_value_name_int(0,"round_id",roundId))
    {
        if(c!=Cache:0) cache_delete(c);
        return false;
    }
    cache_delete(c);
    return roundId >= 0;
#else
    #pragma unused playerid, contractId, faction
    return false;
#endif
}

stock bool:CrimeComp_Claim(playerid, roundId, contractId, faction)
{
#if defined TARKENSO_DB_PRIMARY
    if (roundId <= 0 || !Crime_Available(playerid)) return false;
    new q[600], changed;
    // This row update is the serialization point: two factions cannot
    // commit first place, even if they deliver on the same server tick.
    mysql_format(gDBPrimary,q,sizeof(q),
        "UPDATE crime_competition_rounds r JOIN crime_contracts c ON c.competition_id=r.round_id SET r.state=2,r.winner_contract_id=c.contract_id,r.winner_faction_id=c.faction_id,r.winner_account_id=c.account_id,r.closed_at=UNIX_TIMESTAMP() WHERE r.round_id=%d AND r.state=1 AND r.expires_at>UNIX_TIMESTAMP() AND c.contract_id=%d AND c.account_id=%d AND c.faction_id=%d AND c.mode_id=2 AND c.state=2 AND c.expires_at>UNIX_TIMESTAMP()",
        roundId,contractId,gDBPrimaryAccountId[playerid],faction);
    if (!DBPrimary_Execute(q,changed) || changed!=1) return false;
    mysql_format(gDBPrimary,q,sizeof(q),
        "INSERT INTO crime_contract_events(contract_id,actor_account_id,event_type) SELECT contract_id,account_id,'RACE_WON' FROM crime_contracts WHERE contract_id=%d AND account_id=%d AND state=2",
        contractId,gDBPrimaryAccountId[playerid]);
    if (!Crime_Exec(q)) return false;
    mysql_format(gDBPrimary,q,sizeof(q),
        "INSERT INTO crime_contract_events(contract_id,actor_account_id,event_type) SELECT contract_id,account_id,'RACE_LOST' FROM crime_contracts WHERE competition_id=%d AND contract_id<>%d AND state IN(1,2)",
        roundId,contractId);
    if (!Crime_Exec(q)) return false;
    // Rival contracts are closed in the same transaction, releasing their
    // unique active-account slot; no loser wallet or trust is credited.
    mysql_format(gDBPrimary,q,sizeof(q),
        "UPDATE crime_contracts SET state=5,finished_at=UNIX_TIMESTAMP() WHERE competition_id=%d AND contract_id<>%d AND state IN(1,2)",
        roundId,contractId);
    return Crime_Exec(q);
#else
    #pragma unused playerid, roundId, contractId, faction
    return false;
#endif
}

stock CrimeComp_Start(playerid, roundId)
{
    if (!Crime_Available(playerid) || gGovTransferBusy[playerid] ||
        !Leader_IsCriminal(Leader_PlayerOrg(playerid)) || roundId<=0)
        return SendClientMessage(playerid,CRIME_ERROR,
            "Конкурс недоступен. Проверьте авторизацию и синдикат.");
#if defined TARKENSO_DB_PRIMARY
    if (!CrimeComp_Refresh(playerid) || !Crime_Expire(playerid))
        return SendClientMessage(playerid,CRIME_ERROR,"Не удалось обновить конкурс.");
    new contact=0,rows=0,q[1850];
    mysql_format(gDBPrimary,q,sizeof(q),
        "SELECT contact_id FROM crime_competition_rounds WHERE round_id=%d AND state=1 AND expires_at>UNIX_TIMESTAMP()+%d LIMIT 1",
        roundId,CRIME_COMP_MIN_LEFT);
    new Cache:c=mysql_query(gDBPrimary,q);
    if (c==Cache:0 || mysql_errno(gDBPrimary) ||
        !cache_get_row_count(rows) || rows!=1 ||
        !cache_get_value_name_int(0,"contact_id",contact))
    {
        if(c!=Cache:0) cache_delete(c);
        return SendClientMessage(playerid,CRIME_ERROR,"Раунд закрыт или скоро закончится.");
    }
    cache_delete(c);
    if (!CrimeContacts_Near(playerid,contact))
        return SendClientMessage(playerid,CRIME_ERROR,
            "Для участия необходимо встретиться с посредником лично.");
    if (!Crime_Exec("START TRANSACTION"))
        return SendClientMessage(playerid,CRIME_ERROR,"Нет соединения с базой.");
    new affected;
    mysql_format(gDBPrimary,q,sizeof(q),
        "INSERT INTO crime_contracts(template_id,account_id,faction_id,mode_id,reward_amount,started_at,expires_at,contact_id,scenario_kind,scenario_event,competition_id) SELECT t.template_id,m.account_id,m.faction_id,t.mode_id,t.reward_amount,UNIX_TIMESTAMP(),r.expires_at,t.contact_id,0,0,r.round_id FROM crime_competition_rounds r JOIN crime_contract_templates t ON t.template_id=r.template_id AND t.mode_id=2 AND t.is_enabled=1 JOIN faction_members m ON m.account_id=%d AND m.faction_id=%d JOIN crime_contact_progress p ON p.account_id=m.account_id AND p.contact_id=r.contact_id AND p.trust>=t.required_trust AND p.heat<70 JOIN crime_syndicate_contacts sc ON sc.faction_id=m.faction_id AND sc.contact_id=r.contact_id AND sc.standing>=5 WHERE r.round_id=%d AND r.state=1 AND r.expires_at>UNIX_TIMESTAMP()+%d AND NOT EXISTS(SELECT 1 FROM crime_contracts h WHERE h.competition_id=r.round_id AND h.faction_id=m.faction_id)",
        gDBPrimaryAccountId[playerid],Leader_PlayerOrg(playerid),
        roundId,CRIME_COMP_MIN_LEFT);
    if (!DBPrimary_Execute(q,affected) || affected!=1)
    {
        Crime_Rollback();
        return SendClientMessage(playerid,CRIME_ERROR,
            "Заявка отклонена: нужен контакт, доверие 5, связи синдиката 5, низкое внимание и свободный контракт. Одна заявка на синдикат.");
    }
    if (!Crime_Exec("INSERT INTO crime_contract_events(contract_id,actor_account_id,event_type) SELECT contract_id,account_id,'ACCEPTED' FROM crime_contracts WHERE contract_id=LAST_INSERT_ID()"))
    {
        Crime_Rollback();
        return SendClientMessage(playerid,CRIME_ERROR,"Не удалось записать участие.");
    }
    if (!Crime_Exec("COMMIT"))
        return SendClientMessage(playerid,CRIME_ERROR,
            "Статус заявки неизвестен. Проверьте свой контракт.");
    return SendClientMessage(playerid,CRIME_COLOR,
        "Вы участвуете в конкурсе. Первая доставившая организация выиграет.");
#else
    #pragma unused roundId
    return 1;
#endif
}
