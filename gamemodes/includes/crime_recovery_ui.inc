#if defined _tarkenso_crime_recovery_ui_included
    #endinput
#endif
#define _tarkenso_crime_recovery_ui_included

#define CRIME_RECOVERY_DLG_ROUTE (7982)

stock CrimeRecoveryUI_Board(playerid)
{
    if(!Crime_Available(playerid) ||
       !Leader_IsCriminal(Leader_PlayerOrg(playerid))) return 1;
    new id,contact,method,stage,deadline;
    if(!CrimeRecovery_Info(playerid,id,contact,method,stage,deadline))
        return SendClientMessage(playerid,CRIME_ERROR,
            "Не удалось проверить операции по восстановлению связи.");
    new body[660],line[135],heat;
    if(id>0)
    {
        format(body,sizeof(body),
            "{FFD15C}Операция\t{FFD15C}Состояние{FFFFFF}\nПродолжить #%d\t%s, %d мин\n",
            id,stage==1 ? ("Найти полевого связного") : ("Вернуться к посреднику"),
            (deadline-gettime()+59)/60);
    }
    else
    {
        format(body,sizeof(body),
            "{FFD15C}Посредник\t{FFD15C}Внимание / доступ{FFFFFF}\n");
        for(new i;i<CRIME_CONTACT_COUNT;i++)
        {
            if(!CrimeContacts_Heat(playerid,i+1,heat))
                return SendClientMessage(playerid,CRIME_ERROR,
                    "Не удалось проверить внимание посредника.");
            format(line,sizeof(line),"%s\t%d/100, %s\n",
                gCrimeContactName[i],heat,
                heat>=60 ? ("Восстановление доступно") : ("Доступно от 60"));
            strcat(body,line,sizeof(body));
        }
    }
    CrimeUI_Open(playerid,CRIME_RECOVERY_DLG_BOARD);
    return ShowPlayerDialog(playerid,CRIME_RECOVERY_DLG_BOARD,
        DIALOG_STYLE_TABLIST_HEADERS,
        "{FFD15C}Теневой рынок | Восстановление связи",body,
        "Выбрать","Назад");
}

stock CrimeRecoveryUI_Method(playerid,contact)
{
    if(contact<1 || contact>CRIME_CONTACT_COUNT ||
       !CrimeContacts_Near(playerid,contact) || IsPlayerInAnyVehicle(playerid))
    {
        SendClientMessage(playerid,CRIME_ERROR,
            "Операцию можно начать только пешком рядом с нужным посредником.");
        return CrimeRecoveryUI_Board(playerid);
    }
    new heat;
    if(!CrimeContacts_Heat(playerid,contact,heat) || heat<60)
    {
        SendClientMessage(playerid,CRIME_ERROR,
            "Внимание недостаточно высокое: восстановление доступно от 60.");
        return CrimeRecoveryUI_Board(playerid);
    }
    gCrimeRecoveryContact[playerid]=contact;
    CrimeUI_Open(playerid,CRIME_RECOVERY_DLG_METHOD);
    return ShowPlayerDialog(playerid,CRIME_RECOVERY_DLG_METHOD,
        DIALOG_STYLE_TABLIST_HEADERS,
        "{FFD15C}Теневой рынок | Способ восстановления",
        "{FFD15C}Метод\t{FFD15C}После выполнения{FFFFFF}\nТихая легенда\tВнимание -25, доверие +1\nОтвлекающий манёвр\tВнимание -35, связи -3\n",
        "Начать","Назад");
}

stock CrimeRecoveryUI_Route(playerid)
{
    new id,contact,method,stage,deadline;
    if(!CrimeRecovery_Info(playerid,id,contact,method,stage,deadline) ||
       id<=0) return CrimeRecoveryUI_Board(playerid);
    if(stage==2)
    {
        if(!CrimeContacts_Near(playerid,contact) ||
           IsPlayerInAnyVehicle(playerid))
        {
            new info[180];
            format(info,sizeof(info),
                "Задание выполнено. Вернитесь пешком к %s и завершите операцию в /crime.",
                gCrimeContactName[contact-1]);
            SendClientMessage(playerid,CRIME_COLOR,info);
            return CrimeRecoveryUI_Board(playerid);
        }
        gCrimeRecoverySelected[playerid]=id;
        CrimeUI_Open(playerid,CRIME_RECOVERY_DLG_FINISH);
        return ShowPlayerDialog(playerid,CRIME_RECOVERY_DLG_FINISH,
            DIALOG_STYLE_MSGBOX,
            "{FFD15C}Теневой рынок | Операция завершена",
            method==1 ?
            ("Посредник подтверждает прикрытие. Внимание снизится на 25, доверие повысится на 1. Подтвердить результат?") :
            ("Наблюдение отвлечено. Внимание снизится на 35, но отношения синдиката с посредником уменьшатся на 3. Подтвердить?"),
            "Завершить","Назад");
    }
    new slot=(contact-1)*2+method-1,body[540];
    format(body,sizeof(body),
        "{FFD15C}%s{FFFFFF}\n%s\n\n{FFD15C}ПОЛЕВАЯ ТОЧКА:{FFFFFF}\nX: %.1f\nY: %.1f\nZ: %.1f\n\nОсталось %d мин. Подойдите пешком к NPC (3,5 м), нажмите Y, затем вернитесь к посреднику. Время ограничено; повторная операция у этого NPC доступна не раньше чем через 60 минут от начала.",
        method==1 ? ("Тихая легенда") : ("Отвлекающий манёвр"),
        method==1 ? ("Нужно передать связному новые документы.") :
        ("Оставьте ложный маршрут наблюдателям."),
        gCrimeRecoveryPos[slot][0],gCrimeRecoveryPos[slot][1],
        gCrimeRecoveryPos[slot][2],(deadline-gettime()+59)/60);
    CrimeUI_Open(playerid,CRIME_RECOVERY_DLG_ROUTE);
    return ShowPlayerDialog(playerid,CRIME_RECOVERY_DLG_ROUTE,
        DIALOG_STYLE_MSGBOX,"{FFD15C}Теневой рынок | Маршрут восстановления",
        body,"Понятно","Назад");
}

stock CrimeRecoveryUI_OnDialog(playerid,dialogid,response,listitem)
{
    if(dialogid!=CRIME_RECOVERY_DLG_BOARD &&
       dialogid!=CRIME_RECOVERY_DLG_METHOD &&
       dialogid!=CRIME_RECOVERY_DLG_ROUTE &&
       dialogid!=CRIME_RECOVERY_DLG_FINISH &&
       dialogid!=CRIME_RECOVERY_DLG_FIELD) return 0;
#if defined TARKENSO_DB_PRIMARY
    if(gCrimeUIDialog[playerid]!=dialogid ||
       gCrimeUIAuthToken[playerid]!=gAuthToken[playerid] ||
       gCrimeUIAccountId[playerid]!=gDBPrimaryAccountId[playerid] ||
       !Crime_Available(playerid) ||
       !Leader_IsCriminal(Leader_PlayerOrg(playerid)))
    {
        CrimeUI_Reset(playerid);
        CrimeRecovery_Reset(playerid);
        return 1;
    }
    gCrimeUIDialog[playerid]=0;
    if(!response)
    {
        if(dialogid==CRIME_RECOVERY_DLG_BOARD) return CrimeUI_Main(playerid);
        return CrimeRecoveryUI_Board(playerid);
    }
    if(dialogid==CRIME_RECOVERY_DLG_BOARD)
    {
        new id,contact,method,stage,deadline;
        if(!CrimeRecovery_Info(playerid,id,contact,method,stage,deadline))
            return CrimeRecoveryUI_Board(playerid);
        if(id>0)
        {
            if(listitem!=0) return CrimeRecoveryUI_Board(playerid);
            return CrimeRecoveryUI_Route(playerid);
        }
        if(listitem<0 || listitem>=CRIME_CONTACT_COUNT)
            return CrimeRecoveryUI_Board(playerid);
        return CrimeRecoveryUI_Method(playerid,listitem+1);
    }
    if(dialogid==CRIME_RECOVERY_DLG_METHOD)
    {
        new contact=gCrimeRecoveryContact[playerid];
        gCrimeRecoveryContact[playerid]=0;
        if(contact<1 || contact>3 || listitem<0 || listitem>1)
            return CrimeRecoveryUI_Board(playerid);
        if(!CrimeRecovery_Start(playerid,contact,listitem+1))
            SendClientMessage(playerid,CRIME_ERROR,
                "Начать нельзя: внимание от 60, одна операция, нет действующего заказа, перерыв 60 мин. Нужна близость NPC.");
        else SendClientMessage(playerid,CRIME_COLOR,
                "Операция началась. У вас 20 минут; маршрут в разделе восстановления связи.");
        return CrimeRecoveryUI_Route(playerid);
    }
    if(dialogid==CRIME_RECOVERY_DLG_ROUTE)
        return CrimeRecoveryUI_Board(playerid);
    new id=gCrimeRecoverySelected[playerid];
    gCrimeRecoverySelected[playerid]=0;
    if(id<=0) return CrimeRecoveryUI_Board(playerid);
    if(dialogid==CRIME_RECOVERY_DLG_FIELD)
    {
        if(!CrimeRecovery_FieldComplete(playerid,id))
            SendClientMessage(playerid,CRIME_ERROR,
                "Полевое задание не принято: проверьте срок, позицию и статус.");
        else SendClientMessage(playerid,CRIME_COLOR,
                "След подготовлен. Вернитесь к посреднику для завершения операции.");
    }
    if(dialogid==CRIME_RECOVERY_DLG_FINISH)
    {
        if(!CrimeRecovery_Finish(playerid,id))
            SendClientMessage(playerid,CRIME_ERROR,
                "Результат не зачтён: проверьте контракт, позицию NPC и срок.");
        else SendClientMessage(playerid,CRIME_COLOR,
                "Канал восстановлен: внимание снижено, изменения сохранены в базе.");
    }
    return CrimeRecoveryUI_Board(playerid);
#else
    #pragma unused playerid,response,listitem
    return 1;
#endif
}
