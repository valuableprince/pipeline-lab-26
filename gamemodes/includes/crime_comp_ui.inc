#if defined _tarkenso_crime_comp_ui_included
    #endinput
#endif
#define _tarkenso_crime_comp_ui_included

// CRIME-03.1 uses the same authenticated dialog session as /crime.
#define COMP_DLG_BOARD  (7968)
#define COMP_DLG_DETAIL (7969)
new gCompUICount[MAX_PLAYERS];
new gCompUIRounds[MAX_PLAYERS][3];
new gCompUISelected[MAX_PLAYERS];

stock CrimeCompUI_Board(playerid)
{
#if defined TARKENSO_DB_PRIMARY
    if (!CrimeComp_Refresh(playerid))
        return SendClientMessage(playerid,CRIME_ERROR,"Не удалось обновить общий пул заказов.");
    new Cache:c=mysql_query(gDBPrimary,
        "SELECT r.round_id,r.contact_id,r.state,r.expires_at,COALESCE(r.winner_faction_id,0) winner_faction_id,(SELECT COUNT(*) FROM crime_contracts e WHERE e.competition_id=r.round_id) entries FROM crime_competition_rounds r WHERE r.hour_bucket=FLOOR(UNIX_TIMESTAMP()/3600) ORDER BY r.contact_id LIMIT 3");
    new rows=0;
    if(c==Cache:0 || mysql_errno(gDBPrimary) ||
        !cache_get_row_count(rows) || rows>3)
    {
        if(c!=Cache:0)cache_delete(c);
        return SendClientMessage(playerid,CRIME_ERROR,"Доска конкурсов недоступна.");
    }
    new ids[3],contacts[3],states[3],deadlines[3],winners[3],entries[3];
    for(new i;i<rows;i++)
    {
        if(!cache_get_value_name_int(i,"round_id",ids[i]) ||
           !cache_get_value_name_int(i,"contact_id",contacts[i]) ||
           !cache_get_value_name_int(i,"state",states[i]) ||
           !cache_get_value_name_int(i,"expires_at",deadlines[i]) ||
           !cache_get_value_name_int(i,"winner_faction_id",winners[i]) ||
           !cache_get_value_name_int(i,"entries",entries[i]))
        {
            cache_delete(c);
            return SendClientMessage(playerid,CRIME_ERROR,"Ошибка чтения раунда.");
        }
    }
    cache_delete(c);
    new body[630],line[180],status[60];
    format(body,sizeof(body),
        "{FFD15C}Посредник\t{FFD15C}Состояние\t{FFD15C}Заявки{FFFFFF}\n");
    gCompUICount[playerid]=0;
    for(new i;i<rows;i++)
    {
        if(contacts[i]<1 || contacts[i]>CRIME_CONTACT_COUNT)continue;
        if(states[i]==2)format(status,sizeof(status),"Победил синдикат %d",winners[i]);
        else if(states[i]==3)format(status,sizeof(status),"Истёк");
        else if(deadlines[i]-gettime()<=CRIME_COMP_MIN_LEFT)
            format(status,sizeof(status),"Приём закрывается");
        else format(status,sizeof(status),"%d мин",(deadlines[i]-gettime()+59)/60);
        gCompUIRounds[playerid][gCompUICount[playerid]++]=ids[i];
        format(line,sizeof(line),"%s\t%s\t%d/3\n",
            gCrimeContactName[contacts[i]-1],status,entries[i]);
        strcat(body,line,sizeof(body));
    }
    if(!gCompUICount[playerid])
        strcat(body,"Нет активных раундов\t-\t-\n",sizeof(body));
    CrimeUI_Open(playerid,COMP_DLG_BOARD);
    return ShowPlayerDialog(playerid,COMP_DLG_BOARD,DIALOG_STYLE_TABLIST_HEADERS,
        "{FFD15C}Конкурсы трёх синдикатов",body,"Подробнее","Назад");
#else
    return CrimeUI_Main(playerid);
#endif
}

stock CrimeCompUI_Detail(playerid, roundId)
{
#if defined TARKENSO_DB_PRIMARY
    if(roundId<=0 || !CrimeComp_Refresh(playerid))return CrimeCompUI_Board(playerid);
    new q[610],rows=0,contact=0,roundState=0,deadline=0,winner=0,reward=0,entries=0,title[68];
    mysql_format(gDBPrimary,q,sizeof(q),
        "SELECT r.contact_id,r.state,r.expires_at,COALESCE(r.winner_faction_id,0) winner_faction_id,t.title,t.reward_amount,(SELECT COUNT(*) FROM crime_contracts c WHERE c.competition_id=r.round_id) entries FROM crime_competition_rounds r JOIN crime_contract_templates t ON t.template_id=r.template_id WHERE r.round_id=%d LIMIT 1",
        roundId);
    new Cache:c=mysql_query(gDBPrimary,q);
    if(c==Cache:0 || mysql_errno(gDBPrimary) ||
       !cache_get_row_count(rows) || rows!=1 ||
       !cache_get_value_name_int(0,"contact_id",contact) ||
       !cache_get_value_name_int(0,"state",roundState) ||
       !cache_get_value_name_int(0,"expires_at",deadline) ||
       !cache_get_value_name_int(0,"winner_faction_id",winner) ||
       !cache_get_value_name_int(0,"reward_amount",reward) ||
       !cache_get_value_name_int(0,"entries",entries) ||
       !cache_get_value_name(0,"title",title,sizeof(title)))
    {
        if(c!=Cache:0)cache_delete(c);
        return CrimeCompUI_Board(playerid);
    }
    cache_delete(c);
    if(contact<1 || contact>CRIME_CONTACT_COUNT)return CrimeCompUI_Board(playerid);
    new body[750],status[100];
    if(roundState==2)format(status,sizeof(status),"Победил синдикат %d.",winner);
    else if(roundState==3 || deadline<=gettime()+CRIME_COMP_MIN_LEFT)
        format(status,sizeof(status),"Приём заявок закрыт.");
    else format(status,sizeof(status),"Осталось %d минут.",(deadline-gettime()+59)/60);
    format(body,sizeof(body),
        "{FFD15C}ЗАКАЗ:{FFFFFF} %s\n{FFD15C}ПОСРЕДНИК:{FFFFFF} %s\n{FFD15C}НАГРАДА:{FFFFFF} $%d\n{FFD15C}СИНДИКАТУ:{FFFFFF} +8 влияние, +5 отношения\n{FFD15C}УЧАСТНИКИ:{FFFFFF} %d/3\n{FFD15C}СОСТОЯНИЕ:{FFFFFF} %s\n\nТолько первый доставивший груз получает награду. Каждый синдикат — один исполнитель. Конкурентов нет? Выиграть можно одному. Для участия лично подойдите к NPC (доверие 5, связи 5, внимание ниже 70).",
        title,gCrimeContactName[contact-1],reward,entries,status);
    gCompUISelected[playerid]=(roundState==1 &&
        deadline>gettime()+CRIME_COMP_MIN_LEFT) ? roundId : 0;
    CrimeUI_Open(playerid,COMP_DLG_DETAIL);
    return ShowPlayerDialog(playerid,COMP_DLG_DETAIL,DIALOG_STYLE_MSGBOX,
        "{FFD15C}Общий контракт",body,
        gCompUISelected[playerid] ? ("Участвовать") : ("К списку"),"Назад");
#else
    #pragma unused roundId
    return CrimeUI_Main(playerid);
#endif
}

stock CrimeCompUI_OnDialog(playerid,dialogid,response,listitem)
{
    if(dialogid!=COMP_DLG_BOARD && dialogid!=COMP_DLG_DETAIL)return 0;
#if defined TARKENSO_DB_PRIMARY
    if(gCrimeUIDialog[playerid]!=dialogid ||
       gCrimeUIAuthToken[playerid]!=gAuthToken[playerid] ||
       gCrimeUIAccountId[playerid]!=gDBPrimaryAccountId[playerid] ||
       !Crime_Available(playerid))
    {
        CrimeUI_Reset(playerid);
        return 1;
    }
    // Prevent replays of stale windows and old menu selections.
    gCrimeUIDialog[playerid]=0;
    if(!response)
    {
        if(dialogid==COMP_DLG_DETAIL)return CrimeCompUI_Board(playerid);
        return CrimeUI_Main(playerid);
    }
    if(dialogid==COMP_DLG_BOARD)
    {
        if(listitem<0 || listitem>=gCompUICount[playerid])
            return CrimeCompUI_Board(playerid);
        return CrimeCompUI_Detail(playerid,gCompUIRounds[playerid][listitem]);
    }
    if(gCompUISelected[playerid]<=0)return CrimeCompUI_Board(playerid);
    new roundId=gCompUISelected[playerid];
    gCompUISelected[playerid]=0;
    CrimeComp_Start(playerid,roundId);
    return CrimeUI_Route(playerid);
#else
    #pragma unused playerid,response,listitem
    return 1;
#endif
}
