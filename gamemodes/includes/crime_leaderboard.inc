#if defined _tarkenso_crime_leaderboard_included
    #endinput
#endif
#define _tarkenso_crime_leaderboard_included

// CRIME-03.7: read-only public results. No denormalized counters, payouts
// or timers. All three factions are scored from authoritative SQL ledgers.
#define CRIME_RANK_DLG_BOARD (7983)
#define CRIME_RANK_DLG_HISTORY (7984)
#define CRIME_RANK_COUNT (3)
new gCrimeRankFaction[MAX_PLAYERS][CRIME_RANK_COUNT];
new gCrimeRankSelected[MAX_PLAYERS];

stock CrimeRank_Reset(playerid)
{
    gCrimeRankSelected[playerid]=0;
    for(new i;i<CRIME_RANK_COUNT;i++) gCrimeRankFaction[playerid][i]=0;
    return 1;
}

stock CrimeRank_Name(faction,title[],size)
{
    switch(faction)
    {
        case 6: format(title,size,"Гавань");
        case 7: format(title,size,"Вектор");
        case 8: format(title,size,"Орион");
        default: format(title,size,"Синдикат %d",faction);
    }
    return 1;
}

stock CrimeRank_Board(playerid)
{
#if defined TARKENSO_DB_PRIMARY
    if(!Crime_Available(playerid) ||
       !Leader_IsCriminal(Leader_PlayerOrg(playerid))) return 1;
    // Three indexed aggregate probes per organization. Deliberately NO
    // writes and NO competition refresh: viewing results cannot create a round.
    new Cache:c=mysql_query(gDBPrimary,
        "SELECT p.faction_id,p.influence,"
        "(SELECT COUNT(*) FROM crime_competition_rounds r JOIN crime_contract_rewards rr ON rr.contract_id=r.winner_contract_id WHERE r.winner_faction_id=p.faction_id AND r.state=2) race_wins,"
        "(SELECT COUNT(*) FROM crime_contracts x JOIN crime_contract_events e ON e.contract_id=x.contract_id AND e.event_type='RACE_LOST' WHERE x.faction_id=p.faction_id AND x.mode_id=2) race_losses,"
        "(SELECT COUNT(*) FROM crime_contracts x JOIN crime_contract_rewards rr ON rr.contract_id=x.contract_id WHERE x.faction_id=p.faction_id AND x.mode_id=3 AND x.state=3) tender_done,"
        "(SELECT COUNT(*) FROM crime_tender_bids b JOIN crime_tender_rounds r ON r.tender_id=b.tender_id WHERE b.faction_id=p.faction_id AND b.state=3 AND r.state IN(2,3)) tender_lost,"
        "(SELECT COUNT(*) FROM crime_contracts x JOIN crime_contract_rewards rr ON rr.contract_id=x.contract_id WHERE x.faction_id=p.faction_id AND x.mode_id=1 AND x.state=3) solo_done,"
        "(SELECT COALESCE(SUM(rr.amount),0) FROM crime_contract_rewards rr JOIN crime_contracts x ON x.contract_id=rr.contract_id WHERE x.faction_id=p.faction_id) total_paid "
        "FROM crime_syndicate_progress p WHERE p.faction_id IN(6,7,8) ORDER BY p.faction_id");
    new rows;
    if(c==Cache:0 || mysql_errno(gDBPrimary) ||
       !cache_get_row_count(rows) || rows!=CRIME_RANK_COUNT)
    {
        if(c!=Cache:0) cache_delete(c);
        return SendClientMessage(playerid,CRIME_ERROR,
            "Результаты синдикатов временно недоступны.");
    }
    new faction[3],race[3],lost[3],tender[3],rejected[3],solo[3];
    new influence[3],points[3],paid[3];
    for(new i;i<rows;i++)
    {
        if(!cache_get_value_name_int(i,"faction_id",faction[i]) ||
           !cache_get_value_name_int(i,"race_wins",race[i]) ||
           !cache_get_value_name_int(i,"race_losses",lost[i]) ||
           !cache_get_value_name_int(i,"tender_done",tender[i]) ||
           !cache_get_value_name_int(i,"tender_lost",rejected[i]) ||
           !cache_get_value_name_int(i,"solo_done",solo[i]) ||
           !cache_get_value_name_int(i,"influence",influence[i]) ||
           !cache_get_value_name_int(i,"total_paid",paid[i]) ||
           faction[i]<6 || faction[i]>8 || race[i]<0 || tender[i]<0 ||
           race[i]>200000000 || tender[i]>200000000)
        {
            cache_delete(c);
            return SendClientMessage(playerid,CRIME_ERROR,
                "Не удалось сверить результаты синдикатов.");
        }
        points[i]=race[i]*5+tender[i]*3;
    }
    cache_delete(c);
    // Stable ordering: rating points, then races won, then deliveries,
    // then the original faction ID. Reading this board never rewards anyone.
    for(new i=0;i<CRIME_RANK_COUNT;i++)
        for(new j=i+1;j<CRIME_RANK_COUNT;j++)
            if(points[j]>points[i] ||
               (points[j]==points[i] && race[j]>race[i]) ||
               (points[j]==points[i] && race[j]==race[i] && tender[j]>tender[i]) ||
               (points[j]==points[i] && race[j]==race[i] &&
                tender[j]==tender[i] && faction[j]<faction[i]))
            {
                new swap;
                swap=faction[i]; faction[i]=faction[j]; faction[j]=swap;
                swap=race[i]; race[i]=race[j]; race[j]=swap;
                swap=lost[i]; lost[i]=lost[j]; lost[j]=swap;
                swap=tender[i]; tender[i]=tender[j]; tender[j]=swap;
                swap=rejected[i]; rejected[i]=rejected[j]; rejected[j]=swap;
                swap=solo[i]; solo[i]=solo[j]; solo[j]=swap;
                swap=influence[i]; influence[i]=influence[j]; influence[j]=swap;
                swap=paid[i]; paid[i]=paid[j]; paid[j]=swap;
                swap=points[i]; points[i]=points[j]; points[j]=swap;
            }
    new body[1050],line[220],name[40];
    format(body,sizeof(body),
        "{FFD15C}Место / синдикат\t{FFD15C}Очки\t{FFD15C}Гонки\t{FFD15C}Тендеры{FFFFFF}\n");
    for(new i=0;i<CRIME_RANK_COUNT;i++)
    {
        gCrimeRankFaction[playerid][i]=faction[i];
        CrimeRank_Name(faction[i],name,sizeof(name));
        format(line,sizeof(line),"%d. %s\t%d\t%d побед / %d пораж.\t%d сдано / %d откл.\n",
            i+1,name,points[i],race[i],lost[i],tender[i],rejected[i]);
        strcat(body,line,sizeof(body));
    }
    strcat(body,
        "\nОчки: 5 за победу в гонке, 3 за оплаченный тендер.\n"
        "Обычные заказы, влияние и проигранные заявки очков не дают.\n"
        "Выберите синдикат, чтобы изучить историю результатов.",sizeof(body));
    CrimeUI_Open(playerid,CRIME_RANK_DLG_BOARD);
    return ShowPlayerDialog(playerid,CRIME_RANK_DLG_BOARD,
        DIALOG_STYLE_TABLIST_HEADERS,
        "{FFD15C}TARKENSO | Рейтинг синдикатов",
        body,"История","Назад");
#else
    return CrimeUI_Main(playerid);
#endif
}

stock CrimeRank_History(playerid,faction)
{
#if defined TARKENSO_DB_PRIMARY
    if(!Crime_Available(playerid) ||
       !Leader_IsCriminal(Leader_PlayerOrg(playerid)) ||
       faction<6 || faction>8) return CrimeUI_Main(playerid);
    new query[2350],rows;
    // 1 race victory, 2 race defeat, 3 delivered tender,
    // 4 rejected tender bid, 5 expired/aborted awarded tender.
    // Only committed payout ledger entries qualify as a success.
    mysql_format(gDBPrimary,query,sizeof(query),
        "SELECT h.refid,h.kind FROM ("
        "SELECT x.contract_id refid,CASE WHEN e.event_type='RACE_WON' THEN 1 ELSE 2 END kind,UNIX_TIMESTAMP(e.created_at) stamp FROM crime_contract_events e JOIN crime_contracts x ON x.contract_id=e.contract_id WHERE x.faction_id=%d AND x.mode_id=2 AND (e.event_type='RACE_LOST' OR (e.event_type='RACE_WON' AND EXISTS(SELECT 1 FROM crime_contract_rewards rr WHERE rr.contract_id=x.contract_id))) "
        "UNION ALL SELECT x.contract_id refid,3 kind,x.finished_at stamp FROM crime_contracts x JOIN crime_contract_rewards rr ON rr.contract_id=x.contract_id WHERE x.faction_id=%d AND x.mode_id=3 AND x.state=3 "
        "UNION ALL SELECT b.bid_id refid,4 kind,r.decided_at stamp FROM crime_tender_bids b JOIN crime_tender_rounds r ON r.tender_id=b.tender_id WHERE b.faction_id=%d AND b.state=3 AND r.state IN(2,3) "
        "UNION ALL SELECT x.contract_id refid,5 kind,x.finished_at stamp FROM crime_contracts x JOIN crime_tender_penalties p ON p.contract_id=x.contract_id WHERE x.faction_id=%d AND x.mode_id=3 AND x.state IN(4,5)"
        ") h ORDER BY h.stamp DESC,h.refid DESC LIMIT 12",
        faction,faction,faction,faction);
    new Cache:c=mysql_query(gDBPrimary,query);
    if(c==Cache:0 || mysql_errno(gDBPrimary) ||
       !cache_get_row_count(rows) || rows>12)
    {
        if(c!=Cache:0) cache_delete(c);
        return SendClientMessage(playerid,CRIME_ERROR,
            "История синдиката временно недоступна.");
    }
    new body[1000],line[90],refid,kind,name[40];
    format(body,sizeof(body),
        "{FFD15C}Запись\t{FFD15C}Результат{FFFFFF}\n");
    for(new i=0;i<rows;i++)
    {
        if(!cache_get_value_name_int(i,"refid",refid) ||
           !cache_get_value_name_int(i,"kind",kind) ||
           refid<=0 || kind<1 || kind>5)
        {
            cache_delete(c);
            return SendClientMessage(playerid,CRIME_ERROR,
                "Не удалось сверить события синдиката.");
        }
        switch(kind)
        {
            case 1: format(line,sizeof(line),"Контракт #%d\tПобеда в гонке\n",refid);
            case 2: format(line,sizeof(line),"Контракт #%d\tПроиграна гонка\n",refid);
            case 3: format(line,sizeof(line),"Контракт #%d\tТендер выполнен\n",refid);
            case 4: format(line,sizeof(line),"Заявка #%d\tТендер отклонён\n",refid);
            case 5: format(line,sizeof(line),"Контракт #%d\tТендер сорван\n",refid);
        }
        strcat(body,line,sizeof(body));
    }
    cache_delete(c);
    if(!rows) strcat(body,"Пока нет результатов\t-\n",sizeof(body));
    CrimeRank_Name(faction,name,sizeof(name));
    new title[100];
    format(title,sizeof(title),"TARKENSO | История: %s",name);
    gCrimeRankSelected[playerid]=faction;
    CrimeUI_Open(playerid,CRIME_RANK_DLG_HISTORY);
    return ShowPlayerDialog(playerid,CRIME_RANK_DLG_HISTORY,
        DIALOG_STYLE_TABLIST_HEADERS,title,body,"К рейтингу","Закрыть");
#else
    #pragma unused faction
    return CrimeUI_Main(playerid);
#endif
}

stock CrimeRank_OnDialog(playerid,dialogid,response,listitem)
{
    if(dialogid!=CRIME_RANK_DLG_BOARD && dialogid!=CRIME_RANK_DLG_HISTORY) return 0;
#if defined TARKENSO_DB_PRIMARY
    if(gCrimeUIDialog[playerid]!=dialogid ||
       gCrimeUIAuthToken[playerid]!=gAuthToken[playerid] ||
       gCrimeUIAccountId[playerid]!=gDBPrimaryAccountId[playerid] ||
       !Crime_Available(playerid) ||
       !Leader_IsCriminal(Leader_PlayerOrg(playerid)))
    {
        CrimeUI_Reset(playerid);
        CrimeRank_Reset(playerid);
        return 1;
    }
    gCrimeUIDialog[playerid]=0;
    if(!response) return CrimeUI_Main(playerid);
    if(dialogid==CRIME_RANK_DLG_HISTORY) return CrimeRank_Board(playerid);
    if(listitem<0 || listitem>=CRIME_RANK_COUNT)
        return CrimeRank_Board(playerid);
    new faction=gCrimeRankFaction[playerid][listitem];
    if(faction<6 || faction>8) return CrimeRank_Board(playerid);
    return CrimeRank_History(playerid,faction);
#else
    #pragma unused response,listitem
    return 1;
#endif
}
