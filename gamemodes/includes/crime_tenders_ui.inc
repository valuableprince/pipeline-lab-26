#if defined _tarkenso_crime_tenders_ui_included
    #endinput
#endif
#define _tarkenso_crime_tenders_ui_included

#define TENDER_DLG_BOARD   (7970)
#define TENDER_DLG_DETAIL  (7971)
#define TENDER_DLG_OFFER   (7972)

new gTenderUICount[MAX_PLAYERS];
new gTenderUIIds[MAX_PLAYERS][3];
new gTenderUISelected[MAX_PLAYERS];

stock CrimeTenderUI_Board(playerid)
{
#if defined TARKENSO_DB_PRIMARY
    if(!CrimeTender_Refresh(playerid))
        return SendClientMessage(playerid,CRIME_ERROR,"Доска предложений временно недоступна.");
    new Cache:c=mysql_query(gDBPrimary,
        "SELECT r.tender_id,r.contact_id,r.state,r.closes_at,COALESCE(b.faction_id,0) winner, (SELECT COUNT(*) FROM crime_tender_bids e WHERE e.tender_id=r.tender_id) bids FROM crime_tender_rounds r LEFT JOIN crime_tender_bids b ON b.bid_id=r.winner_bid_id WHERE r.bucket=FLOOR(UNIX_TIMESTAMP()/600) ORDER BY r.contact_id LIMIT 3");
    new rows=0;
    if(c==Cache:0 || mysql_errno(gDBPrimary) ||
       !cache_get_row_count(rows) || rows>3)
    {
        if(c!=Cache:0) cache_delete(c);
        return SendClientMessage(playerid,CRIME_ERROR,"Ошибка чтения предложений.");
    }
    new ids[3],contacts[3],states[3],closes[3],winners[3],bids[3];
    for(new i;i<rows;i++)
    {
        if(!cache_get_value_name_int(i,"tender_id",ids[i]) ||
           !cache_get_value_name_int(i,"contact_id",contacts[i]) ||
           !cache_get_value_name_int(i,"state",states[i]) ||
           !cache_get_value_name_int(i,"closes_at",closes[i]) ||
           !cache_get_value_name_int(i,"winner",winners[i]) ||
           !cache_get_value_name_int(i,"bids",bids[i]))
        {
            cache_delete(c);
            return SendClientMessage(playerid,CRIME_ERROR,"Ошибка состояния конкурса.");
        }
    }
    cache_delete(c);
    new body[660],line[170],status[60];
    format(body,sizeof(body),"{FFD15C}Посредник\t{FFD15C}Состояние\t{FFD15C}Заявки{FFFFFF}\n");
    gTenderUICount[playerid]=0;
    for(new i;i<rows;i++)
    {
        if(contacts[i]<1 || contacts[i]>CRIME_CONTACT_COUNT) continue;
        gTenderUIIds[playerid][gTenderUICount[playerid]++]=ids[i];
        if(states[i]==2) format(status,sizeof(status),"Выбран синдикат %d",winners[i]);
        else if(states[i]==3) format(status,sizeof(status),"Без исполнителя");
        else format(status,sizeof(status),"Приём %d сек",closes[i]-gettime()>0 ? closes[i]-gettime() : 0);
        format(line,sizeof(line),"%s\t%s\t%d/3\n",
            gCrimeContactName[contacts[i]-1],status,bids[i]);
        strcat(body,line,sizeof(body));
    }
    if(!gTenderUICount[playerid]) strcat(body,"Нет предложений\t-\t-\n",sizeof(body));
    CrimeUI_Open(playerid,TENDER_DLG_BOARD);
    return ShowPlayerDialog(playerid,TENDER_DLG_BOARD,DIALOG_STYLE_TABLIST_HEADERS,
        "{FFD15C}Теневой рынок | Конкурсы предложений",body,"Подробнее","Назад");
#else
    return CrimeUI_Main(playerid);
#endif
}

stock CrimeTenderUI_Detail(playerid,tenderId)
{
#if defined TARKENSO_DB_PRIMARY
    if(tenderId<=0 || !CrimeTender_Refresh(playerid)) return CrimeTenderUI_Board(playerid);
    new q[670],rows=0,contact=0,state=0,closes=0,winner=0,bids=0,reward=0,myCode=0,myState=0,title[68];
    mysql_format(gDBPrimary,q,sizeof(q),
        "SELECT r.contact_id,r.state,r.closes_at,t.title,t.reward_amount,COALESCE(w.faction_id,0) winner,(SELECT COUNT(*) FROM crime_tender_bids z WHERE z.tender_id=r.tender_id) bids,COALESCE(b.offer_code,0) my_code,COALESCE(b.state,0) my_state FROM crime_tender_rounds r JOIN crime_contract_templates t ON t.template_id=r.template_id LEFT JOIN crime_tender_bids w ON w.bid_id=r.winner_bid_id LEFT JOIN crime_tender_bids b ON b.tender_id=r.tender_id AND b.account_id=%d WHERE r.tender_id=%d LIMIT 1",
        gDBPrimaryAccountId[playerid],tenderId);
    new Cache:c=mysql_query(gDBPrimary,q);
    if(c==Cache:0 || mysql_errno(gDBPrimary) ||
       !cache_get_row_count(rows) || rows!=1 ||
       !cache_get_value_name_int(0,"contact_id",contact) ||
       !cache_get_value_name_int(0,"state",state) ||
       !cache_get_value_name_int(0,"closes_at",closes) ||
       !cache_get_value_name_int(0,"reward_amount",reward) ||
       !cache_get_value_name_int(0,"winner",winner) ||
       !cache_get_value_name_int(0,"bids",bids) ||
       !cache_get_value_name_int(0,"my_code",myCode) ||
       !cache_get_value_name_int(0,"my_state",myState) ||
       !cache_get_value_name(0,"title",title,sizeof(title)))
    {
        if(c!=Cache:0) cache_delete(c);
        return CrimeTenderUI_Board(playerid);
    }
    cache_delete(c);
    if(contact<1 || contact>CRIME_CONTACT_COUNT) return CrimeTenderUI_Board(playerid);
    new body[860],status[100],mine[65];
    if(state==2) format(status,sizeof(status),"Победил синдикат %d.",winner);
    else if(state==3) format(status,sizeof(status),"NPC не нашёл исполнителя.");
    else format(status,sizeof(status),"Приём ещё %d секунд.",closes>gettime()?closes-gettime():0);
    if(!myCode) format(mine,sizeof(mine),"Вы пока не подали заявку.");
    else if(myState==1) format(mine,sizeof(mine),"Заявка %d ожидает решения.",myCode);
    else if(myState==2) format(mine,sizeof(mine),"Вы выиграли. Откройте «Мой контракт».");
    else format(mine,sizeof(mine),"Заявка отклонена или уступила другой.");
    format(body,sizeof(body),
        "{FFD15C}ЗАКАЗ:{FFFFFF} %s\n{FFD15C}ПОСРЕДНИК:{FFFFFF} %s\n{FFD15C}МАКС. ВОЗНАГРАЖДЕНИЕ:{FFFFFF} $%d\n{FFD15C}ЗАЯВОК:{FFFFFF} %d/3\n{FFD15C}РЕЗУЛЬТАТ:{FFFFFF} %s\n{FFD15C}ВАША ЗАЯВКА:{FFFFFF} %s\n\nПредложите цену и срок. NPC отдаёт заказ более выгодному и быстрому исполнителю; при равных условиях выигрывает первая заявка. После победы нужно забрать и доставить груз. Даже один игрок может выиграть. Заявку подают только лично посреднику при доверии и связях от 5, внимании ниже 70 и без активного задания.",
        title,gCrimeContactName[contact-1],reward,bids,status,mine);
    gTenderUISelected[playerid]=(state==1 && closes>gettime() && myCode==0)?tenderId:0;
    CrimeUI_Open(playerid,TENDER_DLG_DETAIL);
    return ShowPlayerDialog(playerid,TENDER_DLG_DETAIL,DIALOG_STYLE_MSGBOX,
        "{FFD15C}Конкурс предложений",body,
        gTenderUISelected[playerid] ? ("Предложить") : ("К списку"),"Назад");
#else
    #pragma unused tenderId
    return CrimeUI_Main(playerid);
#endif
}

stock CrimeTenderUI_Offers(playerid)
{
#if defined TARKENSO_DB_PRIMARY
    if(gTenderUISelected[playerid]<=0) return CrimeTenderUI_Board(playerid);
    new q[320],rows=0,reward=0;
    mysql_format(gDBPrimary,q,sizeof(q),
        "SELECT t.reward_amount FROM crime_tender_rounds r JOIN crime_contract_templates t ON t.template_id=r.template_id WHERE r.tender_id=%d AND r.state=1 AND r.closes_at>UNIX_TIMESTAMP() LIMIT 1",
        gTenderUISelected[playerid]);
    new Cache:c=mysql_query(gDBPrimary,q);
    if(c==Cache:0 || mysql_errno(gDBPrimary) ||
       !cache_get_row_count(rows) || rows!=1 ||
       !cache_get_value_name_int(0,"reward_amount",reward))
    {
        if(c!=Cache:0) cache_delete(c);
        return CrimeTenderUI_Board(playerid);
    }
    cache_delete(c);
    new body[450];
    format(body,sizeof(body),
        "{FFD15C}Вариант\t{FFD15C}Гонорар\t{FFD15C}Срок{FFFFFF}\nСрочный — лучший приоритет\t$%d\t15 мин\nСбалансированный\t$%d\t25 мин\nОсторожный — больше денег\t$%d\t40 мин",
        reward*70/100,reward*85/100,reward);
    CrimeUI_Open(playerid,TENDER_DLG_OFFER);
    return ShowPlayerDialog(playerid,TENDER_DLG_OFFER,DIALOG_STYLE_TABLIST_HEADERS,
        "{FFD15C}Ваше предложение посреднику",body,"Отправить","Назад");
#else
    return CrimeUI_Main(playerid);
#endif
}

stock CrimeTenderUI_OnDialog(playerid,dialogid,response,listitem)
{
    if(dialogid!=TENDER_DLG_BOARD && dialogid!=TENDER_DLG_DETAIL &&
       dialogid!=TENDER_DLG_OFFER) return 0;
#if defined TARKENSO_DB_PRIMARY
    if(gCrimeUIDialog[playerid]!=dialogid ||
       gCrimeUIAuthToken[playerid]!=gAuthToken[playerid] ||
       gCrimeUIAccountId[playerid]!=gDBPrimaryAccountId[playerid] ||
       !Crime_Available(playerid))
    {
        CrimeUI_Reset(playerid);
        gTenderUISelected[playerid]=0;
        return 1;
    }
    gCrimeUIDialog[playerid]=0;
    if(!response)
    {
        if(dialogid==TENDER_DLG_BOARD) return CrimeUI_Main(playerid);
        if(dialogid==TENDER_DLG_OFFER) return CrimeTenderUI_Detail(playerid,gTenderUISelected[playerid]);
        return CrimeTenderUI_Board(playerid);
    }
    if(dialogid==TENDER_DLG_BOARD)
    {
        if(listitem<0 || listitem>=gTenderUICount[playerid])
            return CrimeTenderUI_Board(playerid);
        return CrimeTenderUI_Detail(playerid,gTenderUIIds[playerid][listitem]);
    }
    if(dialogid==TENDER_DLG_DETAIL)
    {
        if(gTenderUISelected[playerid]<=0) return CrimeTenderUI_Board(playerid);
        return CrimeTenderUI_Offers(playerid);
    }
    if(listitem<0 || listitem>2 || gTenderUISelected[playerid]<=0)
        return CrimeTenderUI_Board(playerid);
    new tenderId=gTenderUISelected[playerid];
    gTenderUISelected[playerid]=0;
    CrimeTender_Submit(playerid,tenderId,listitem+1);
    return CrimeTenderUI_Board(playerid);
#else
    #pragma unused playerid,response,listitem
    return 1;
#endif
}
