#if defined _tarkenso_crime_intel_ui_included
    #endinput
#endif
#define _tarkenso_crime_intel_ui_included

stock CrimeIntelUI_Board(playerid)
{
#if defined TARKENSO_DB_PRIMARY
    if(!Crime_Available(playerid) ||
       !Leader_IsCriminal(Leader_PlayerOrg(playerid))) return 1;
    new q[550],rows=0;
    mysql_format(gDBPrimary,q,sizeof(q),
        "SELECT l.contract_id,c.contact_id,l.solved_at,l.expires_at FROM crime_intelligence_cases l JOIN crime_contracts c ON c.contract_id=l.contract_id JOIN faction_members m ON m.account_id=c.account_id AND m.faction_id=c.faction_id WHERE c.account_id=%d AND c.faction_id=%d AND c.mode_id=3 AND c.state=3 AND l.decision=0 AND l.expires_at>UNIX_TIMESTAMP() ORDER BY l.expires_at ASC LIMIT 8",
        gDBPrimaryAccountId[playerid],Leader_PlayerOrg(playerid));
    new Cache:c=mysql_query(gDBPrimary,q);
    if(c==Cache:0 || mysql_errno(gDBPrimary) ||
       !cache_get_row_count(rows) || rows>CRIME_INTEL_LIMIT)
    {
        if(c!=Cache:0) cache_delete(c);
        return SendClientMessage(playerid,CRIME_ERROR,"Сводки временно недоступны.");
    }
    gCrimeIntelCount[playerid]=0;
    gCrimeIntelSelected[playerid]=0;
    new body[1100],line[150],id,contact,solved,deadline;
    format(body,sizeof(body),
        "{FFD15C}Досье\t{FFD15C}Посредник\t{FFD15C}Состояние\t{FFD15C}Срок{FFFFFF}\n");
    for(new i;i<rows;i++)
    {
        if(!cache_get_value_name_int(i,"contract_id",id) ||
           !cache_get_value_name_int(i,"contact_id",contact) ||
           !cache_get_value_name_int(i,"solved_at",solved) ||
           !cache_get_value_name_int(i,"expires_at",deadline) ||
           contact<1 || contact>CRIME_CONTACT_COUNT || id<=0)
        {
            cache_delete(c);
            return SendClientMessage(playerid,CRIME_ERROR,"Не удалось прочитать досье.");
        }
        gCrimeIntelIds[playerid][gCrimeIntelCount[playerid]++]=id;
        format(line,sizeof(line),"#%d\t%s\t%s\t%d мин\n",
            id,gCrimeContactName[contact-1],
            solved>0 ? ("Расшифровано") : ("Проверить факты"),
            (deadline-gettime()+59)/60);
        strcat(body,line,sizeof(body));
    }
    cache_delete(c);
    if(!gCrimeIntelCount[playerid])
        strcat(body,"Сводок нет\t-\t-\t-\n",sizeof(body));
    CrimeUI_Open(playerid,CRIME_INTEL_DLG_BOARD);
    return ShowPlayerDialog(playerid,CRIME_INTEL_DLG_BOARD,
        DIALOG_STYLE_TABLIST_HEADERS,
        "{FFD15C}Теневой рынок | Оперативные сводки",body,
        "Открыть","Назад");
#else
    return CrimeUI_Main(playerid);
#endif
}

stock CrimeIntelUI_Decision(playerid,contractId)
{
    new contact,puzzle,solved,retryAt;
    if(!CrimeIntel_Info(playerid,contractId,contact,puzzle,solved,retryAt) ||
       solved<=0 || !CrimeContacts_Near(playerid,contact) ||
       IsPlayerInAnyVehicle(playerid))
    {
        SendClientMessage(playerid,CRIME_ERROR,
            "Для передачи сводки подойдите пешком к её посреднику.");
        return CrimeIntelUI_Board(playerid);
    }
    gCrimeIntelSelected[playerid]=contractId;
    CrimeUI_Open(playerid,CRIME_INTEL_DLG_DECISION);
    return ShowPlayerDialog(playerid,CRIME_INTEL_DLG_DECISION,
        DIALOG_STYLE_TABLIST_HEADERS,
        "{FFD15C}Оперативная сводка | Решение",
        "{FFD15C}Действие\t{FFD15C}Результат{FFFFFF}\nПередать в архив синдиката\tВлияние +4, связи +1\nСтереть следы\tВнимание -10, доверие +1\n",
        "Подтвердить","Назад");
}

stock CrimeIntelUI_Puzzle(playerid,contractId)
{
    new contact,puzzle,solved,retryAt;
    if(!CrimeIntel_Info(playerid,contractId,contact,puzzle,solved,retryAt))
    {
        SendClientMessage(playerid,CRIME_ERROR,"Досье больше не доступно.");
        return CrimeIntelUI_Board(playerid);
    }
    if(!CrimeContacts_Near(playerid,contact) ||
       IsPlayerInAnyVehicle(playerid))
    {
        new message[160];
        format(message,sizeof(message),
            "Чтобы проверить сводку, подойдите пешком к %s (радиус 5 м).",
            gCrimeContactName[contact-1]);
        SendClientMessage(playerid,CRIME_COLOR,message);
        return CrimeIntelUI_Board(playerid);
    }
    if(solved>0) return CrimeIntelUI_Decision(playerid,contractId);
    if(retryAt>gettime())
    {
        new message[110];
        format(message,sizeof(message),
            "Посредник повторно проверит материалы через %d сек.",retryAt-gettime());
        SendClientMessage(playerid,CRIME_ERROR,message);
        return CrimeIntelUI_Board(playerid);
    }
    new title[90],body[230];
    switch(puzzle)
    {
        case 1:
        {
            format(title,sizeof(title),"В накладной 18 ящиков, фактически 17. Расхождение?");
            format(body,sizeof(body),
                "Количество груза\nДата отправки\nМарка автомобиля\n");
        }
        case 2:
        {
            format(title,sizeof(title),"Пломба 041, в журнале 014. Что неверно?");
            format(body,sizeof(body),
                "Данные водителя\nНомер пломбы\nСтоимость топлива\n");
        }
        case 3:
        {
            format(title,sizeof(title),"Судно вышло в 06:00, накладная: 08:00. Ошибка?");
            format(body,sizeof(body),
                "Название порта\nТип груза\nВремя отправления\n");
        }
    }
    gCrimeIntelSelected[playerid]=contractId;
    CrimeUI_Open(playerid,CRIME_INTEL_DLG_PUZZLE);
    return ShowPlayerDialog(playerid,CRIME_INTEL_DLG_PUZZLE,
        DIALOG_STYLE_LIST,title,body,"Ответить","Позже");
}

stock CrimeIntelUI_OnDialog(playerid,dialogid,response,listitem)
{
    if(dialogid!=CRIME_INTEL_DLG_BOARD &&
       dialogid!=CRIME_INTEL_DLG_PUZZLE &&
       dialogid!=CRIME_INTEL_DLG_DECISION) return 0;
#if defined TARKENSO_DB_PRIMARY
    if(gCrimeUIDialog[playerid]!=dialogid ||
       gCrimeUIAuthToken[playerid]!=gAuthToken[playerid] ||
       gCrimeUIAccountId[playerid]!=gDBPrimaryAccountId[playerid] ||
       !Crime_Available(playerid) ||
       !Leader_IsCriminal(Leader_PlayerOrg(playerid)))
    {
        CrimeUI_Reset(playerid);
        CrimeIntel_Reset(playerid);
        return 1;
    }
    gCrimeUIDialog[playerid]=0; // Client response is single use.
    if(!response)
    {
        if(dialogid==CRIME_INTEL_DLG_BOARD) return CrimeUI_Main(playerid);
        return CrimeIntelUI_Board(playerid);
    }
    if(dialogid==CRIME_INTEL_DLG_BOARD)
    {
        if(listitem<0 || listitem>=gCrimeIntelCount[playerid])
            return CrimeIntelUI_Board(playerid);
        new id=gCrimeIntelIds[playerid][listitem];
        if(id<=0) return CrimeIntelUI_Board(playerid);
        return CrimeIntelUI_Puzzle(playerid,id);
    }
    new id=gCrimeIntelSelected[playerid];
    gCrimeIntelSelected[playerid]=0;
    if(id<=0 || listitem<0 || listitem>2)
        return CrimeIntelUI_Board(playerid);
    if(dialogid==CRIME_INTEL_DLG_PUZZLE)
    {
        new outcome=CrimeIntel_Answer(playerid,id,listitem+1);
        if(outcome==1)
        {
            SendClientMessage(playerid,CRIME_COLOR,
                "Расхождение установлено. Выберите дальнейшую судьбу материалов.");
            return CrimeIntelUI_Decision(playerid,id);
        }
        if(outcome==2) SendClientMessage(playerid,CRIME_ERROR,
            "Ошибка в разборе документов. Повторная попытка через 45 секунд.");
        else SendClientMessage(playerid,CRIME_ERROR,
            "Досье недоступно, время истекло или вы не у посредника.");
        return CrimeIntelUI_Board(playerid);
    }
    if(listitem>1) return CrimeIntelUI_Board(playerid);
    if(!CrimeIntel_Redeem(playerid,id,listitem+1))
        SendClientMessage(playerid,CRIME_ERROR,
            "Результат не принят. Проверьте положение, синдикат и срок.");
    else SendClientMessage(playerid,CRIME_COLOR,
            listitem==0 ? ("Сведения переданы. Синдикат получил влияние и связи.") :
            ("Следы скрыты. Внимание снижено, доверие посредника увеличено."));
    return CrimeIntelUI_Board(playerid);
#else
    #pragma unused playerid,response,listitem
    return 1;
#endif
}
