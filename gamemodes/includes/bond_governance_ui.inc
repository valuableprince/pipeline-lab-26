#if defined _tarkenso_bond_governance_ui_included
    #endinput
#endif
#define _tarkenso_bond_governance_ui_included

// GOV-05 | State Treasury issue management: two signatures, DB roles, audited IDs.
// No direct money edits. /bondgovrole is founder-only appointment bootstrap.
#define GOVUI_MAIN (6800)
#define GOVUI_TYPE (6801)
#define GOVUI_INPUT (6802)
#define GOVUI_CONFIRM (6803)
#define GOVUI_LIST (6804)
#define GOVUI_ISSUE (6805)
#define GOVUI_ACTION (6806)
#define GOVUI_REPORT (6807)
#define GOVUI_ROWS (10)

new bool:gGovUIReady;
new gGovUITaxTimer;
new bool:gGovUITaxScanBusy;
new bool:gGovUITaxPostBusy;
new bool:gGovUILoading[MAX_PLAYERS];
new bool:gGovUIBusy[MAX_PLAYERS];
new gGovUIReadSerial[MAX_PLAYERS];
new gGovUIWriteSerial[MAX_PLAYERS];
new gGovUIPendingAccount[MAX_PLAYERS];
new gGovUIPendingAction[MAX_PLAYERS];
new gGovUIStage[MAX_PLAYERS];
new gGovUIWizardStep[MAX_PLAYERS];
new gGovUIRole[MAX_PLAYERS];
new gGovUIEventSerial;
new gGovUITitle[MAX_PLAYERS][129];
new gGovUIPurpose[MAX_PLAYERS][193];
new gGovUIType[MAX_PLAYERS][16];
new gGovUIFace[MAX_PLAYERS];
new gGovUIQuantity[MAX_PLAYERS];
new gGovUIYield[MAX_PLAYERS];
new gGovUIOfferDays[MAX_PLAYERS];
new gGovUIMaturityDays[MAX_PLAYERS];
new gGovUIPage[MAX_PLAYERS];
new gGovUICount[MAX_PLAYERS];
new bool:gGovUIHasMore[MAX_PLAYERS];
new gGovUIRows[MAX_PLAYERS][GOVUI_ROWS];
new gGovUISelectedIssue[MAX_PLAYERS];
new gGovUISelectedCode[MAX_PLAYERS][65];
new gGovUISelectedStatus[MAX_PLAYERS][20];
new gGovUIAction[MAX_PLAYERS];
new gGovUIPendingEvent[MAX_PLAYERS][65];
new bool:gGovUIPublicReport[MAX_PLAYERS];

forward GovBond_UI_OnRead(playerid,authToken,accountId,serial,kind);
forward GovBond_UI_OnWrite(playerid,authToken,accountId,serial);
forward GovBond_UI_TaxTick();
forward GovBond_UI_OnTaxScan();
forward GovBond_UI_OnTaxPost();
forward bool:GovBond_UI_OnError(errorid,const callback[],const query[]);

stock GovBond_UI_Reset(playerid)
{
    gGovUILoading[playerid]=false;
    gGovUIBusy[playerid]=false;
    gGovUIReadSerial[playerid]++;
    gGovUIWriteSerial[playerid]++;
    gGovUIPendingAccount[playerid]=0;
    gGovUIPendingAction[playerid]=0;
    gGovUIStage[playerid]=0;
    gGovUIWizardStep[playerid]=0;
    gGovUIRole[playerid]=0;
    gGovUIPage[playerid]=0;
    gGovUICount[playerid]=0;
    gGovUIHasMore[playerid]=false;
    gGovUISelectedIssue[playerid]=0;
    gGovUISelectedCode[playerid][0]=EOS;
    gGovUISelectedStatus[playerid][0]=EOS;
    gGovUIAction[playerid]=0;
    gGovUIPendingEvent[playerid][0]=EOS;
    gGovUIPublicReport[playerid]=false;
    return 1;
}

stock GovBond_UI_Shutdown()
{
    gGovUIReady=false;
    if(gGovUITaxTimer) KillTimer(gGovUITaxTimer);
    gGovUITaxTimer=0;
    gGovUITaxScanBusy=false;
    gGovUITaxPostBusy=false;
    return 1;
}

stock GovBond_UI_Init()
{
    gGovUIReady=false;
#if defined TARKENSO_DB_PRIMARY
    if (!GovDebt_IsReady()) return 1;
    new Cache:c=mysql_query(gDBPrimary,
        "SELECT version FROM schema_migrations WHERE version=23 LIMIT 1");
    if (c==Cache:0) return 1;
    new rows=0;
    new bool:valid=mysql_errno(gDBPrimary)==0 &&
        cache_get_row_count(rows) && rows==1;
    cache_delete(c);
    if (!valid)
    {
        print("[GOV-05] Migration 023 missing; government issue UI disabled.");
        return 1;
    }
    gGovUIReady=true;
    gGovUITaxScanBusy=false;
    gGovUITaxPostBusy=false;
    gGovUITaxTimer=SetTimer("GovBond_UI_TaxTick",60000,true);
    if (!gGovUITaxTimer)
    {
        gGovUIReady=false;
        print("[GOV-05] ERROR: insurance tax sweep timer unavailable.");
        return 1;
    }
    print("[GOV-05] Bond governance UI ready with SQL roles and auto codes.");
    GovBond_UI_TaxTick();
#else
    print("[GOV-05] Government issue UI requires MySQL-primary.");
#endif
    return 1;
}

stock bool:GovBond_UI_Authorized(playerid)
{
#if defined TARKENSO_DB_PRIMARY
    return gGovUIReady && playerid>=0 && playerid<MAX_PLAYERS &&
       IsPlayerConnected(playerid) && !IsPlayerNPC(playerid) &&
       gAuthState[playerid]==AUTH_READY && gDBPrimaryLoaded[playerid] &&
       gDBPrimaryAccountId[playerid]>0 && !gGovTransferBusy[playerid] &&
       !gBondSettlementBusy[playerid] && BondSettlement_IsReady();
#else
    #pragma unused playerid
    return false;
#endif
}

stock GovBond_UI_Event(playerid,output[],size)
{
    gGovUIEventSerial++;
    format(output,size,"bond:gov5:%d:%d:%d:%d",
       gDBPrimaryAccountId[playerid],gettime(),gAuthToken[playerid],
       gGovUIEventSerial);
    return 1;
}

stock bool:GovBond_UI_Query(playerid,const sql[],kind)
{
#if defined TARKENSO_DB_PRIMARY
    if (!GovBond_UI_Authorized(playerid) || gGovUILoading[playerid] ||
        gGovUIBusy[playerid]) return false;
    gGovUILoading[playerid]=true;
    gGovUIReadSerial[playerid]++;
    if (!mysql_tquery(gDBPrimary,sql,"GovBond_UI_OnRead","iiiii",
        playerid,gAuthToken[playerid],gDBPrimaryAccountId[playerid],
        gGovUIReadSerial[playerid],kind))
    {
        gGovUILoading[playerid]=false;
        return false;
    }
    return true;
#else
    #pragma unused playerid,sql,kind
    return false;
#endif
}

stock bool:GovBond_UI_Write(playerid,const sql[],action,const eventKey[])
{
#if defined TARKENSO_DB_PRIMARY
    if (!GovBond_UI_Authorized(playerid) || gGovUILoading[playerid] ||
        gGovUIBusy[playerid]) return false;
    gGovUIBusy[playerid]=true;
    gGovUIPendingAction[playerid]=action;
    gGovUIPendingAccount[playerid]=gDBPrimaryAccountId[playerid];
    format(gGovUIPendingEvent[playerid],65,"%s",eventKey);
    gGovUIWriteSerial[playerid]++;
    if (!mysql_tquery(gDBPrimary,sql,"GovBond_UI_OnWrite","iiii",
        playerid,gAuthToken[playerid],gDBPrimaryAccountId[playerid],
        gGovUIWriteSerial[playerid]))
    {
        gGovUIBusy[playerid]=false;
        gGovUIPendingAction[playerid]=0;
        gGovUIPendingEvent[playerid][0]=EOS;
        return false;
    }
    return true;
#else
    #pragma unused playerid,sql,action,eventKey
    return false;
#endif
}

stock GovBond_UI_Main(playerid)
{
    if (!GovBond_UI_Authorized(playerid))
    {
        SendClientMessage(playerid,COLOR_RED,
           "Казначейство недоступно: необходим MySQL и авторизация.");
        return 1;
    }
#if defined TARKENSO_DB_PRIMARY
    new query[440];
    mysql_format(gDBPrimary,query,sizeof(query),
        "SELECT COALESCE((SELECT role_code FROM government_bond_officials WHERE account_id=%d AND enabled=1),'NONE') AS role_code",
        gDBPrimaryAccountId[playerid]);
    if (!GovBond_UI_Query(playerid,query,1))
        SendClientMessage(playerid,COLOR_RED,"Реестр полномочий недоступен.");
#endif
    return 1;
}

stock bool:GovBond_UI_CanCreate(playerid)
{
    return gGovUIRole[playerid]==1 || gGovUIRole[playerid]==3;
}
stock bool:GovBond_UI_CanApprove(playerid)
{
    return gGovUIRole[playerid]==2 || gGovUIRole[playerid]==3;
}

stock GovBond_UI_DrawMain(playerid)
{
    new body[360];
    format(body,sizeof(body),
        "{E9C67E}Новый государственный заём\n{F1F4F8}Реестр выпусков и решения\n{78D8AA}Финансовая отчётность\n{B7C4D3}Назначение финансовых должностей");
    gGovUIStage[playerid]=1;
    ShowPlayerDialog(playerid,GOVUI_MAIN,DIALOG_STYLE_LIST,
        "{D9B872}TARKENSO {A6B5C6}| {F1F4F8}Государственное казначейство",
        body,"Выбрать","Закрыть");
    return 1;
}

stock GovBond_UI_LoadIssues(playerid,page=0)
{
#if defined TARKENSO_DB_PRIMARY
    if (!GovBond_UI_Authorized(playerid) || page<0 || page>500) return 1;
    new query[430];
    mysql_format(gDBPrimary,query,sizeof(query),
        "SELECT issue_id,issue_code,status,face_value,sold_quantity,max_quantity FROM government_bond_issues ORDER BY issue_id DESC LIMIT 11 OFFSET %d",
        page*GOVUI_ROWS);
    gGovUIPage[playerid]=page;
    if (!GovBond_UI_Query(playerid,query,2))
        SendClientMessage(playerid,COLOR_RED,"Реестр выпусков не отвечает.");
#else
    #pragma unused playerid,page
#endif
    return 1;
}

stock GovBond_UI_LoadIssue(playerid,issueId)
{
#if defined TARKENSO_DB_PRIMARY
    if (!GovBond_UI_Authorized(playerid) || issueId<=0) return 1;
    new query[550];
    mysql_format(gDBPrimary,query,sizeof(query),
        "SELECT issue_id,issue_code,title,purpose,status,face_value,max_quantity,sold_quantity,yield_basis_points,insured_max_exposure,offer_end_at,maturity_at,created_by_account_id,approved_by_account_id FROM government_bond_issues WHERE issue_id=%d LIMIT 1",
        issueId);
    gGovUISelectedIssue[playerid]=issueId;
    if (!GovBond_UI_Query(playerid,query,3))
        SendClientMessage(playerid,COLOR_RED,"Выпуск не найден.");
#else
    #pragma unused playerid,issueId
#endif
    return 1;
}

stock GovBond_UI_Report(playerid)
{
#if defined TARKENSO_DB_PRIMARY
    new query[800];
    mysql_format(gDBPrimary,query,sizeof(query),
        "SELECT balance,bond_fund_reserved,bond_fund_committed,bond_payout_reserved,bond_crisis_state,(SELECT COUNT(*) FROM government_bond_issues) AS issues,(SELECT COUNT(*) FROM government_bond_issues WHERE status='OFFERING') AS offering,(SELECT COALESCE(SUM(sold_quantity),0) FROM government_bond_issues) AS bonds_sold,(SELECT COALESCE(SUM(principal_due+interest_due-principal_budget_awarded-interest_budget_awarded-insurance_awarded),0) FROM government_bond_claims) AS overdue FROM treasury_accounts WHERE code='state'");
    if (!GovBond_UI_Query(playerid,query,4))
        SendClientMessage(playerid,COLOR_RED,"Отчёт казны временно недоступен.");
#else
    #pragma unused playerid
#endif
    return 1;
}

stock GovBond_UI_PublicReport(playerid)
{
    if (!GovBond_UI_Authorized(playerid)) return 1;
    gGovUIPublicReport[playerid]=true;
    return GovBond_UI_Report(playerid);
}

public GovBond_UI_OnRead(playerid,authToken,accountId,serial,kind)
{
#if defined TARKENSO_DB_PRIMARY
    if (playerid<0 || playerid>=MAX_PLAYERS ||
        !IsPlayerConnected(playerid) || IsPlayerNPC(playerid) ||
        !gGovUILoading[playerid] || !GovBond_UI_Authorized(playerid) ||
        gAuthToken[playerid]!=authToken ||
        gDBPrimaryAccountId[playerid]!=accountId ||
        gGovUIReadSerial[playerid]!=serial) return 1;
    gGovUILoading[playerid]=false;
    new rows=0;
    if (mysql_errno(gDBPrimary)!=0 || !cache_get_row_count(rows))
    {
        SendClientMessage(playerid,COLOR_RED,"Ошибка чтения данных казначейства.");
        return 1;
    }
    if (kind==1)
    {
        new role[24];
        if (rows!=1 || !cache_get_value_name(0,"role_code",role,sizeof(role)))
            return 1;
        gGovUIRole[playerid]=0;
        if (!strcmp(role,"TREASURER",true)) gGovUIRole[playerid]=1;
        else if (!strcmp(role,"CONTROLLER",true)) gGovUIRole[playerid]=2;
        else if (!strcmp(role,"DIRECTOR",true)) gGovUIRole[playerid]=3;
        if (!gGovUIRole[playerid] && !Admin_IsFounder(playerid))
        {
            SendClientMessage(playerid,COLOR_RED,
                "У вас нет должности государственного казначейства.");
            return 1;
        }
        return GovBond_UI_DrawMain(playerid);
    }
    if (kind==2)
    {
        new shown=rows>GOVUI_ROWS?GOVUI_ROWS:rows;
        gGovUICount[playerid]=shown;
        gGovUIHasMore[playerid]=rows>GOVUI_ROWS;
        new body[1900],line[165],code[65],status[20],id,face,sold,total;
        format(body,sizeof(body),"{DDBB77}Код\tНоминал\tПродано\tСтатус\n");
        for(new i=0;i<shown;i++)
        {
            if (!cache_get_value_name_int(i,"issue_id",id) ||
                !cache_get_value_name(i,"issue_code",code,sizeof(code)) ||
                !cache_get_value_name(i,"status",status,sizeof(status)) ||
                !cache_get_value_name_int(i,"face_value",face) ||
                !cache_get_value_name_int(i,"sold_quantity",sold) ||
                !cache_get_value_name_int(i,"max_quantity",total)) return 1;
            gGovUIRows[playerid][i]=id;
            format(line,sizeof(line),
                "{F1F4F8}%s\t{DDBB77}$%d\t{78D8AA}%d/%d\t{AAB6C6}%s\n",
                code,face,sold,total,status);
            strcat(body,line,sizeof(body));
        }
        if (!shown)
            strcat(body,"{AAB6C6}Выпусков пока нет\t—\t—\t—\n",sizeof(body));
        if (gGovUIHasMore[playerid])
            strcat(body,"{E9C67E}Следующая страница\t\t\t\n",sizeof(body));
        if (gGovUIPage[playerid]>0)
            strcat(body,"{E9C67E}Предыдущая страница\t\t\t\n",sizeof(body));
        strcat(body,"{BAC6D5}Назад\t\t\t\n",sizeof(body));
        gGovUIStage[playerid]=2;
        ShowPlayerDialog(playerid,GOVUI_LIST,DIALOG_STYLE_TABLIST_HEADERS,
            "{D9B872}TARKENSO | Реестр выпусков",
            body,"Открыть","Назад");
        return 1;
    }
    if (kind==3)
    {
        new id,code[65],title[129],purpose[193],status[20];
        new face,total,sold,yield,exposure,creator,offerEnd[32],maturity[32];
        if(rows!=1 || !cache_get_value_name_int(0,"issue_id",id) ||
            id!=gGovUISelectedIssue[playerid] ||
            !cache_get_value_name(0,"issue_code",code,sizeof(code)) ||
            !cache_get_value_name(0,"title",title,sizeof(title)) ||
            !cache_get_value_name(0,"purpose",purpose,sizeof(purpose)) ||
            !cache_get_value_name(0,"status",status,sizeof(status)) ||
            !cache_get_value_name(0,"offer_end_at",offerEnd,sizeof(offerEnd)) ||
            !cache_get_value_name(0,"maturity_at",maturity,sizeof(maturity)) ||
            !cache_get_value_name_int(0,"face_value",face) ||
            !cache_get_value_name_int(0,"max_quantity",total) ||
            !cache_get_value_name_int(0,"sold_quantity",sold) ||
            !cache_get_value_name_int(0,"yield_basis_points",yield) ||
            !cache_get_value_name_int(0,"insured_max_exposure",exposure) ||
            !cache_get_value_name_int(0,"created_by_account_id",creator))
            return 1;
        gGovUISelectedIssue[playerid]=id;
        format(gGovUISelectedCode[playerid],65,"%s",code);
        format(gGovUISelectedStatus[playerid],20,"%s",status);
        new body[1150];
        format(body,sizeof(body),
            "{DDBB77}ГОСУДАРСТВЕННЫЙ ВЫПУСК\n{F1F4F8}%s\n{B6C5D4}%s\n{BAC6D4}%s\n\n{BAC6D4}Состояние: {F1F4F8}%s\n{BAC6D4}Номинал: {E9C67E}$%d\n{BAC6D4}Продано: {78D8AA}%d / %d\n{BAC6D4}Доход за весь срок: {78D8AA}%d.%02d%%\n{BAC6D4}Зарезервирована страховка: {DDBB77}$%d\n{BAC6D4}Окончание продажи (UTC): {F1F4F8}%s\n{BAC6D4}Погашение (UTC): {F1F4F8}%s\n{BAC6D4}Автор (account ID): %d",
            code,title,purpose,status,face,sold,total,yield/100,yield%100,
            exposure,offerEnd,maturity,creator);
        gGovUIStage[playerid]=3;
        ShowPlayerDialog(playerid,GOVUI_ISSUE,DIALOG_STYLE_MSGBOX,
            "{D9B872}TARKENSO | Управление выпуском",body,
            "Действия","Назад");
        return 1;
    }
    if (kind==4)
    {
        new cash,fund,committed,payout,issues,offering,sold,overdue,crisis[20],body[900];
        if(rows!=1 || !cache_get_value_name_int(0,"balance",cash) ||
            !cache_get_value_name_int(0,"bond_fund_reserved",fund) ||
            !cache_get_value_name_int(0,"bond_fund_committed",committed) ||
            !cache_get_value_name_int(0,"bond_payout_reserved",payout) ||
            !cache_get_value_name_int(0,"issues",issues) ||
            !cache_get_value_name_int(0,"offering",offering) ||
            !cache_get_value_name_int(0,"bonds_sold",sold) ||
            !cache_get_value_name_int(0,"overdue",overdue) ||
            !cache_get_value_name(0,"bond_crisis_state",crisis,sizeof(crisis)))
            return 1;
        format(body,sizeof(body),
            "{DDBB77}ФИНАНСОВАЯ ОТЧЁТНОСТЬ\n\n{BAC6D4}Баланс казны: {E9C67E}$%d\n{BAC6D4}Страховой резерв: {78D8AA}$%d\n{BAC6D4}Принятые гарантии: {E9C67E}$%d\n{BAC6D4}Резерв выплат: {78D8AA}$%d\n\n{BAC6D4}Кризисный статус: {F1F4F8}%s\n{BAC6D4}Всего выпусков: %d\n{BAC6D4}На размещении: %d\n{BAC6D4}Продано облигаций: %d\n{BAC6D4}Необеспеченный долг: {E48A8A}$%d\n\n{92A0AF}Суммы получены из MySQL; отчёт без права списания.",
            cash,fund,committed,payout,crisis,issues,offering,sold,overdue);
        gGovUIStage[playerid]=4;
        ShowPlayerDialog(playerid,GOVUI_REPORT,DIALOG_STYLE_MSGBOX,
            "{D9B872}TARKENSO | Отчёт казначейства",body,"Назад","");
    }
#else
    #pragma unused playerid,authToken,accountId,serial,kind
#endif
    return 1;
}

stock bool:GovBond_UI_ParsePositive(const input[],&number,maximum)
{
    new len=strlen(input);
    if(len<1 || len>10) return false;
    for(new i=0;i<len;i++)
        if(input[i]<'0' || input[i]>'9') return false;
    number=strval(input);
    return number>=1 && number<=maximum;
}

stock GovBond_UI_Wizard(playerid,step)
{
    if (!GovBond_UI_Authorized(playerid) || !GovBond_UI_CanCreate(playerid))
        return 1;
    gGovUIWizardStep[playerid]=step;
    gGovUIStage[playerid]=5;
    new prompt[330],title[125];
    format(title,sizeof(title),"{D9B872}Новый выпуск | Этап %d/7",step);
    switch(step)
    {
        case 1: format(prompt,sizeof(prompt),
            "{E9C67E}Название государственного займа\n{AAB6C6}От 3 до 128 символов.");
        case 2: format(prompt,sizeof(prompt),
            "{E9C67E}Назначение займа\n{AAB6C6}От 3 до 192 символов.");
        case 3: format(prompt,sizeof(prompt),
            "{E9C67E}Номинал одной облигации ($)\n{AAB6C6}Целое число 1—1000000000.");
        case 4: format(prompt,sizeof(prompt),
            "{E9C67E}Количество облигаций в выпуске\n{AAB6C6}Целое число 1—1000000. Общий номинал не более 1 млрд.");
        case 5: format(prompt,sizeof(prompt),
            "{E9C67E}Доходность за весь срок (базисные пункты)\n{AAB6C6}0—10000; например, 150 = 1.50%%.");
        case 6: format(prompt,sizeof(prompt),
            "{E9C67E}Продолжительность размещения, дней\n{AAB6C6}От 1 до 30; отсчитывается от UTC-времени регистрации.");
        case 7: format(prompt,sizeof(prompt),
            "{E9C67E}Срок до погашения после завершения продажи, дней\n{AAB6C6}От 1 до 365.");
        default: return 1;
    }
    ShowPlayerDialog(playerid,GOVUI_INPUT,DIALOG_STYLE_INPUT,
        title,prompt,"Далее","Отмена");
    return 1;
}

stock GovBond_UI_ConfirmDraft(playerid)
{
    new body[900];
    format(body,sizeof(body),
        "{DDBB77}ПРОЕКТ ГОСУДАРСТВЕННОГО ЗАЙМА\n\n{BAC6D4}Название: {F1F4F8}%s\n{BAC6D4}Цель: {F1F4F8}%s\n{BAC6D4}Тип: {F1F4F8}%s\n{BAC6D4}Номинал: {E9C67E}$%d\n{BAC6D4}Количество: {F1F4F8}%d\n{BAC6D4}Доходность: {78D8AA}%d.%02d%%\n{BAC6D4}Размещение: %d дней\n{BAC6D4}Погашение через: %d дней после размещения\n\n{78D8AA}Код GOV-000001 и далее назначит MySQL.\n{E48A8A}Создание НЕ открывает продажу и НЕ расходует бюджет.",
        gGovUITitle[playerid],gGovUIPurpose[playerid],gGovUIType[playerid],
        gGovUIFace[playerid],gGovUIQuantity[playerid],
        gGovUIYield[playerid]/100,gGovUIYield[playerid]%100,
        gGovUIOfferDays[playerid],gGovUIMaturityDays[playerid]);
    gGovUIStage[playerid]=6;
    ShowPlayerDialog(playerid,GOVUI_CONFIRM,DIALOG_STYLE_MSGBOX,
        "{D9B872}TARKENSO | Подтверждение проекта",body,
        "Создать","Отмена");
    return 1;
}

stock GovBond_UI_Create(playerid)
{
#if defined TARKENSO_DB_PRIMARY
    if (!GovBond_UI_Authorized(playerid) || !GovBond_UI_CanCreate(playerid))
        return 1;
    new eventKey[65],query[1500];
    GovBond_UI_Event(playerid,eventKey,sizeof(eventKey));
    mysql_format(gDBPrimary,query,sizeof(query),
        "CALL gov_bond_issue_create_auto('%e',%d,'%e','%e','%e',%d,%d,%d,%d,%d)",
        eventKey,gDBPrimaryAccountId[playerid],gGovUITitle[playerid],
        gGovUIPurpose[playerid],gGovUIType[playerid],
        gGovUIFace[playerid],gGovUIQuantity[playerid],gGovUIYield[playerid],
        gGovUIOfferDays[playerid],gGovUIMaturityDays[playerid]);
    gGovUIStage[playerid]=0;
    if (!GovBond_UI_Write(playerid,query,1,eventKey))
        SendClientMessage(playerid,COLOR_RED,"Проект не принят на создание.");
#else
    #pragma unused playerid
#endif
    return 1;
}

stock GovBond_UI_Actions(playerid)
{
    if (!GovBond_UI_Authorized(playerid) || gGovUISelectedIssue[playerid]<=0)
        return 1;
    gGovUIStage[playerid]=7;
    ShowPlayerDialog(playerid,GOVUI_ACTION,DIALOG_STYLE_LIST,
        "{D9B872}Казначейство | Действия по выпуску",
        "{78D8AA}Утвердить выпуск (контролёр)\n{E9C67E}Открыть размещение (казначей)\n{E48A8A}Отменить непроданный выпуск (контролёр)\n{AAB6C6}Вернуться к реестру",
        "Выбрать","Закрыть");
    return 1;
}

stock GovBond_UI_ConfirmAction(playerid,action)
{
    if (!GovBond_UI_Authorized(playerid) || !gGovUISelectedCode[playerid][0])
        return 1;
    if ((action==2 || action==4) && !GovBond_UI_CanApprove(playerid))
        return SendClientMessage(playerid,COLOR_RED,"Требуется должность контролёра.");
    if (action==3 && !GovBond_UI_CanCreate(playerid))
        return SendClientMessage(playerid,COLOR_RED,"Требуется должность казначея.");
    gGovUIAction[playerid]=action;
    gGovUIStage[playerid]=8;
    new body[480],verb[60];
    switch(action)
    {
        case 2: format(verb,sizeof(verb),"утверждение и резервирование страховки");
        case 3: format(verb,sizeof(verb),"открытие размещения");
        case 4: format(verb,sizeof(verb),"отмену выпуска без инвесторов");
        default: return 1;
    }
    format(body,sizeof(body),
        "{DDBB77}ГОСУДАРСТВЕННОЕ РЕШЕНИЕ\n\n{BAC6D4}Выпуск: {F1F4F8}%s\n{BAC6D4}Операция: {E9C67E}%s\n\n{E48A8A}Проверки прав, фонда, сроков и держателей повторит MySQL.\n{BAC6D4}Подтвердить?",
        gGovUISelectedCode[playerid],verb);
    ShowPlayerDialog(playerid,GOVUI_CONFIRM,DIALOG_STYLE_MSGBOX,
        "{D9B872}TARKENSO | Утверждение решения",body,
        "Подтвердить","Отмена");
    return 1;
}

stock GovBond_UI_ExecuteAction(playerid)
{
#if defined TARKENSO_DB_PRIMARY
    new action=gGovUIAction[playerid];
    if (!GovBond_UI_Authorized(playerid) || gGovUISelectedIssue[playerid]<=0 ||
        (action!=2 && action!=3 && action!=4)) return 1;
    if ((action==2 || action==4) && !GovBond_UI_CanApprove(playerid))
        return 1;
    if (action==3 && !GovBond_UI_CanCreate(playerid)) return 1;
    new query[440],eventKey[65];
    GovBond_UI_Event(playerid,eventKey,sizeof(eventKey));
    switch(action)
    {
        case 2: mysql_format(gDBPrimary,query,sizeof(query),
            "CALL gov_bond_official_approve('%e','%e',%d)",
            eventKey,gGovUISelectedCode[playerid],gDBPrimaryAccountId[playerid]);
        case 3: mysql_format(gDBPrimary,query,sizeof(query),
            "CALL gov_bond_official_open('%e','%e',%d)",
            eventKey,gGovUISelectedCode[playerid],gDBPrimaryAccountId[playerid]);
        case 4: mysql_format(gDBPrimary,query,sizeof(query),
            "CALL gov_bond_official_cancel('%e','%e',%d)",
            eventKey,gGovUISelectedCode[playerid],gDBPrimaryAccountId[playerid]);
    }
    gGovUIStage[playerid]=0;
    if (!GovBond_UI_Write(playerid,query,action,eventKey))
        SendClientMessage(playerid,COLOR_RED,"Решение не поставлено в очередь.");
#else
    #pragma unused playerid
#endif
    return 1;
}

// Founder bootstraps roles on a CONNECTED SQL account; appointment is audited.
// Syntax: /bondgovrole <player-id> <role=1..3> <enabled=0|1>
stock GovBond_UI_Grant(playerid,targetId,role,enabled)
{
#if defined TARKENSO_DB_PRIMARY
    if (!GovBond_UI_Authorized(playerid) ||
        !Admin_IsFounder(playerid) || !gAdminDuty[playerid])
        return SendClientMessage(playerid,COLOR_RED,
            "Назначения доступны только основателю в /aduty.");
    if (targetId<0 || targetId>=MAX_PLAYERS ||
        !IsPlayerConnected(targetId) || IsPlayerNPC(targetId) ||
        !gDBPrimaryLoaded[targetId] ||
        gAuthState[targetId]!=AUTH_READY || role<1 || role>3 ||
        enabled<0 || enabled>1)
        return SendClientMessage(playerid,COLOR_RED,
            "Нужен онлайн-игрок с SQL-аккаунтом, роль 1..3 и доступ 0/1.");
    new eventKey[65],query[410],roleCode[17];
    switch(role)
    {
        case 1: format(roleCode,sizeof(roleCode),"TREASURER");
        case 2: format(roleCode,sizeof(roleCode),"CONTROLLER");
        case 3: format(roleCode,sizeof(roleCode),"DIRECTOR");
    }
    GovBond_UI_Event(playerid,eventKey,sizeof(eventKey));
    mysql_format(gDBPrimary,query,sizeof(query),
       "CALL gov_bond_role_set('%e',%d,%d,'%e',%d)",
       eventKey,gDBPrimaryAccountId[playerid],gDBPrimaryAccountId[targetId],
       roleCode,enabled);
    if (!GovBond_UI_Write(playerid,query,5,eventKey))
        SendClientMessage(playerid,COLOR_RED,"Роль не поставлена на согласование.");
#else
    #pragma unused playerid,targetId,role,enabled
#endif
    return 1;
}

public GovBond_UI_OnWrite(playerid,authToken,accountId,serial)
{
#if defined TARKENSO_DB_PRIMARY
    if (playerid<0 || playerid>=MAX_PLAYERS || !IsPlayerConnected(playerid) ||
        IsPlayerNPC(playerid) || !gGovUIBusy[playerid] ||
        gAuthState[playerid]!=AUTH_READY || gAuthToken[playerid]!=authToken ||
        !gDBPrimaryLoaded[playerid] || gDBPrimaryAccountId[playerid]!=accountId ||
        gGovUIPendingAccount[playerid]!=accountId ||
        gGovUIWriteSerial[playerid]!=serial) return 1;
    new rows=0,replayed,issueId=0,code[65];
    new bool:valid=mysql_errno(gDBPrimary)==0 &&
        cache_get_row_count(rows) && rows==1 &&
        cache_get_value_name_int(0,"replayed",replayed);
    new action=gGovUIPendingAction[playerid];
    if (valid && action==1)
        valid=cache_get_value_name_int(0,"issue_id",issueId) &&
              cache_get_value_name(0,"issue_code",code,sizeof(code));
    // Unknown result may have COMMITTED. Disable retry for this session.
    if (!valid || (replayed!=0 && replayed!=1))
    {
        print("[GOV-05] Unconfirmed government operation; reload SQL audit.");
        gGovUIBusy[playerid]=false;
        gGovUIPendingEvent[playerid][0]=EOS;
        gGovUIPendingAction[playerid]=0;
        SendClientMessage(playerid,COLOR_RED,
            "Неизвестный результат операции. Проверьте реестр и журнал перед повтором.");
        return 1;
    }
    gGovUIBusy[playerid]=false;
    gGovUIPendingEvent[playerid][0]=EOS;
    gGovUIPendingAction[playerid]=0;
    if (action==1)
    {
        new message[150];
        format(message,sizeof(message),
            "Казначейство: проект %s зарегистрирован (ID %d).",code,issueId);
        SendClientMessage(playerid,COLOR_AMBER,message);
    }
    else if (action==5)
        SendClientMessage(playerid,COLOR_AMBER,
           "Должность казначейства изменена. Решение записано в журнал БД.");
    else
        SendClientMessage(playerid,COLOR_AMBER,
           "Решение по выпуску зафиксировано в казначейском реестре.");
    if (action==2 || action==4)
        if (!BondFund_Reload())
            print("[GOV-05] Fund snapshot refresh failed after issuer decision.");
#else
    #pragma unused playerid,authToken,accountId,serial
#endif
    return 1;
}

// Every successful tax credit is a real positive treasury_ledger row.
// The contribution is a reclassification, not a credit to the treasury.
public GovBond_UI_TaxTick()
{
#if defined TARKENSO_DB_PRIMARY
    if (!gGovUIReady || gGovUITaxScanBusy || gGovUITaxPostBusy) return 1;
    gGovUITaxScanBusy=true;
    if (!mysql_tquery(gDBPrimary,
       "SELECT t.ledger_id,t.event_key FROM treasury_ledger t LEFT JOIN government_bond_tax_earmarks e ON e.tax_ledger_id=t.ledger_id WHERE t.source='tax' AND t.amount_delta>0 AND e.tax_ledger_id IS NULL ORDER BY t.ledger_id LIMIT 1",
       "GovBond_UI_OnTaxScan",""))
    {
        gGovUITaxScanBusy=false;
        print("[GOV-05] Tax sweep scan was not queued.");
    }
#endif
    return 1;
}

public GovBond_UI_OnTaxScan()
{
#if defined TARKENSO_DB_PRIMARY
    gGovUITaxScanBusy=false;
    if (!gGovUIReady) return 1;
    new rows=0,ledgerId,event[65],fundEvent[65],query[310];
    if (mysql_errno(gDBPrimary)!=0 || !cache_get_row_count(rows) || rows<1)
        return 1;
    if (!cache_get_value_name_int(0,"ledger_id",ledgerId) ||
        !cache_get_value_name(0,"event_key",event,sizeof(event)) ||
        ledgerId<=0 || !Treasury_ValidToken(event,64)) return 1;
    format(fundEvent,sizeof(fundEvent),"fund:tax:%d",ledgerId);
    mysql_format(gDBPrimary,query,sizeof(query),
        "CALL gov_bond_tax_earmark('%e','%e')",fundEvent,event);
    gGovUITaxPostBusy=true;
    if (!mysql_tquery(gDBPrimary,query,"GovBond_UI_OnTaxPost",""))
    {
        gGovUITaxPostBusy=false;
        print("[GOV-05] Tax earmark did not queue.");
    }
#endif
    return 1;
}

public GovBond_UI_OnTaxPost()
{
#if defined TARKENSO_DB_PRIMARY
    gGovUITaxPostBusy=false;
    new rows=0;
    if (mysql_errno(gDBPrimary)!=0 || !cache_get_row_count(rows) || rows!=1)
    {
        print("[GOV-05] Tax earmark unconfirmed. Retrying idempotently later.");
        return 1;
    }
    if (!BondFund_Reload())
        print("[GOV-05] Fund snapshot not refreshed after tax allocation.");
#endif
    return 1;
}

public bool:GovBond_UI_OnError(errorid,const callback[],const query[])
{
#if defined TARKENSO_DB_PRIMARY
    if (errorid!=1644 && errorid!=1062) return false;
    if (!strcmp(callback,"GovBond_UI_OnTaxPost",true))
    {
        gGovUITaxPostBusy=false;
        print("[GOV-05] Tax earmark rejected; source or free balance must be checked.");
        return true;
    }
    if (strcmp(callback,"GovBond_UI_OnWrite",true))return false;
    for(new playerid=0;playerid<MAX_PLAYERS;playerid++)
    {
        if(!gGovUIBusy[playerid] || !gGovUIPendingEvent[playerid][0]) continue;
        new needle[68];
        format(needle,sizeof(needle),"'%s'",gGovUIPendingEvent[playerid]);
        if(strfind(query,needle,true)<0)continue;
        gGovUIBusy[playerid]=false;
        gGovUIPendingEvent[playerid][0]=EOS;
        gGovUIPendingAction[playerid]=0;
        if(IsPlayerConnected(playerid) && gAuthState[playerid]==AUTH_READY)
            SendClientMessage(playerid,COLOR_RED,
                "Казначейство отклонило решение: проверяйте роль, статус, фонд и подписи.");
        break;
    }
#else
    #pragma unused errorid,callback,query
#endif
    return true;
}

stock GovBond_UI_OnDialog(playerid,dialogid,response,listitem,const inputtext[])
{
    if (dialogid<GOVUI_MAIN || dialogid>GOVUI_REPORT)return 0;
    if(!GovBond_UI_Authorized(playerid) || gGovUILoading[playerid] ||
       gGovUIBusy[playerid]) return 1;
    if(dialogid==GOVUI_MAIN)
    {
        if(gGovUIStage[playerid]!=1 || !response || listitem<0 || listitem>3)
            return 1;
        switch(listitem)
        {
            case 0:
            {
                if (!GovBond_UI_CanCreate(playerid))
                    return SendClientMessage(playerid,COLOR_RED,
                        "Для создания выпуска нужна должность казначея.");
                gGovUIStage[playerid]=9;
                ShowPlayerDialog(playerid,GOVUI_TYPE,DIALOG_STYLE_LIST,
                    "{D9B872}Казначейство | Тип государственного займа",
                    "Общий бюджетный заём\nЦелевое финансирование\nАнтикризисный выпуск",
                    "Выбрать","Назад");
                return 1;
            }
            case 1:return GovBond_UI_LoadIssues(playerid,0);
            case 2:return GovBond_UI_Report(playerid);
            case 3:
            {
                if(!Admin_IsFounder(playerid) || !gAdminDuty[playerid])
                    return SendClientMessage(playerid,COLOR_RED,
                        "Доступно только основателю в режиме администратора.");
                return SendClientMessage(playerid,COLOR_AMBER,
                    "Назначение: /bondgovrole [ID онлайн-игрока] [1=казначей/2=контролёр/3=руководитель] [1=вкл/0=выкл].");
            }
        }
        return 1;
    }
    if(dialogid==GOVUI_TYPE)
    {
        if(gGovUIStage[playerid]!=9 || !response || listitem<0 || listitem>2)
            return 1;
        switch(listitem)
        {
            case 0: format(gGovUIType[playerid],16,"GENERAL");
            case 1: format(gGovUIType[playerid],16,"TARGETED");
            case 2: format(gGovUIType[playerid],16,"CRISIS");
        }
        return GovBond_UI_Wizard(playerid,1);
    }
    if(dialogid==GOVUI_INPUT)
    {
        if(gGovUIStage[playerid]!=5 || gGovUIWizardStep[playerid]<1 ||
            gGovUIWizardStep[playerid]>7)return 1;
        if(!response)return GovBond_UI_Main(playerid);
        new num,step=gGovUIWizardStep[playerid],len=strlen(inputtext);
        if(step==1)
        {
            if(len<3 || len>128)return GovBond_UI_Wizard(playerid,step);
            format(gGovUITitle[playerid],129,"%s",inputtext);
        }
        else if(step==2)
        {
            if(len<3 || len>192)return GovBond_UI_Wizard(playerid,step);
            format(gGovUIPurpose[playerid],193,"%s",inputtext);
        }
        else
        {
            if(step==5 && !strcmp(inputtext,"0")) num=0;
            else if(!GovBond_UI_ParsePositive(inputtext,num,1000000000))
                return GovBond_UI_Wizard(playerid,step);
            switch(step)
            {
                case 3: gGovUIFace[playerid]=num;
                case 4:
                {
                    if(num>1000000)return GovBond_UI_Wizard(playerid,step);
                    gGovUIQuantity[playerid]=num;
                }
                case 5:
                {
                    if(num>10000)return GovBond_UI_Wizard(playerid,step);
                    gGovUIYield[playerid]=num;
                }
                case 6:
                {
                    if(num>30)return GovBond_UI_Wizard(playerid,step);
                    gGovUIOfferDays[playerid]=num;
                }
                case 7:
                {
                    if(num>365)return GovBond_UI_Wizard(playerid,step);
                    gGovUIMaturityDays[playerid]=num;
                }
            }
        }
        if(step<7)return GovBond_UI_Wizard(playerid,step+1);
        return GovBond_UI_ConfirmDraft(playerid);
    }
    if(dialogid==GOVUI_CONFIRM)
    {
        if(gGovUIStage[playerid]!=6 && gGovUIStage[playerid]!=8)return 1;
        new mode=gGovUIStage[playerid];
        gGovUIStage[playerid]=0; // consume confirmation before any SQL enqueue
        if(!response)return GovBond_UI_Main(playerid);
        if(mode==6)return GovBond_UI_Create(playerid);
        return GovBond_UI_ExecuteAction(playerid);
    }
    if(dialogid==GOVUI_LIST)
    {
        if(gGovUIStage[playerid]!=2)return 1;
        if(!response)return GovBond_UI_Main(playerid);
        if(listitem>=0 && listitem<gGovUICount[playerid])
            return GovBond_UI_LoadIssue(playerid,gGovUIRows[playerid][listitem]);
        new index=gGovUICount[playerid];
        if(index==0)
        {
            if(listitem==0)return 1;
            index=1;
        }
        if(gGovUIHasMore[playerid])
        {
            if(listitem==index)return GovBond_UI_LoadIssues(playerid,gGovUIPage[playerid]+1);
            index++;
        }
        if(gGovUIPage[playerid]>0)
        {
            if(listitem==index)return GovBond_UI_LoadIssues(playerid,gGovUIPage[playerid]-1);
            index++;
        }
        if(listitem==index)return GovBond_UI_Main(playerid);
        return 1;
    }
    if(dialogid==GOVUI_ISSUE)
    {
        if(gGovUIStage[playerid]!=3)return 1;
        if(!response)return GovBond_UI_LoadIssues(playerid,gGovUIPage[playerid]);
        return GovBond_UI_Actions(playerid);
    }
    if(dialogid==GOVUI_ACTION)
    {
        if(gGovUIStage[playerid]!=7)return 1;
        if(!response)return GovBond_UI_LoadIssues(playerid,gGovUIPage[playerid]);
        if(listitem>=0 && listitem<=2)
            return GovBond_UI_ConfirmAction(playerid,listitem+2);
        return GovBond_UI_LoadIssues(playerid,gGovUIPage[playerid]);
    }
    if(dialogid==GOVUI_REPORT)
    {
        if(gGovUIStage[playerid]==4)
        {
            if(gGovUIPublicReport[playerid])
            {
                gGovUIPublicReport[playerid]=false;
                return BondsUI_Main(playerid);
            }
            return GovBond_UI_Main(playerid);
        }
        return 1;
    }
    return 1;
}
