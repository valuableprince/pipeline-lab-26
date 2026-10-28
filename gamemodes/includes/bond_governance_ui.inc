#if defined _tarkenso_bond_governance_ui_included
    #endinput
#endif
#define _tarkenso_bond_governance_ui_included

// GOV-05 | Ministry of Finance: rank-based signing/rejection via SQL issue register.
// Rejections require an auditable reason; no direct money edits.
#define GOVUI_MAIN (6800)
#define GOVUI_TYPE (6801)
#define GOVUI_INPUT (6802)
#define GOVUI_CONFIRM (6803)
#define GOVUI_LIST (6804)
#define GOVUI_ISSUE (6805)
#define GOVUI_ACTION (6806)
#define GOVUI_REPORT (6807)
#define GOVUI_APPROVAL_REASON (6808)
#define GOVUI_FUND_AMOUNT (6809)
#define GOVUI_FUND_CONFIRM (6810)
#define GOVUI_ROWS (10)

// UI-only mapping. SQL statuses remain ASCII for financial procedures and checks.
stock GovBond_UI_IssueStatusLabel(const status[],output[],size)
{
    if (!strcmp(status,"DRAFT")) format(output,size,"На подпись");
    else if (!strcmp(status,"APPROVED")) format(output,size,"Утверждён");
    else if (!strcmp(status,"RETURNED")) format(output,size,"На доработке");
    else if (!strcmp(status,"OFFERING")) format(output,size,"Размещается");
    else if (!strcmp(status,"CLOSED")) format(output,size,"Размещение закрыто");
    else if (!strcmp(status,"CANCELLED")) format(output,size,"Отменён");
    else if (!strcmp(status,"REJECTED")) format(output,size,"Отклонён");
    else format(output,size,"Неизвестный статус");
    return 1;
}

stock GovBond_UI_TypeLabel(const kind[],output[],size)
{
    if (!strcmp(kind,"GENERAL")) format(output,size,"Общий бюджетный заём");
    else if (!strcmp(kind,"TARGETED")) format(output,size,"Целевое финансирование");
    else if (!strcmp(kind,"CRISIS")) format(output,size,"Антикризисный выпуск");
    else format(output,size,"Неизвестный тип");
    return 1;
}

stock GovBond_UI_CrisisStatusLabel(const status[],output[],size)
{
    if (!strcmp(status,"NORMAL")) format(output,size,"Норма");
    else if (!strcmp(status,"STRESSED")) format(output,size,"Напряжённый");
    else if (!strcmp(status,"DEFAULT")) format(output,size,"Дефолт");
    else if (!strcmp(status,"RECOVERY")) format(output,size,"Восстановление");
    else format(output,size,"Неизвестный статус");
    return 1;
}

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
// Snapshot the issue being opened; asynchronous dialog clicks cannot rename it.
new gGovUIPendingIssue[MAX_PLAYERS];
new gGovUIPendingCode[MAX_PLAYERS][65];
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
new bool:gGovUIEditing[MAX_PLAYERS];
new gGovUIReviewCreator[MAX_PLAYERS];
new gGovUIPendingEvent[MAX_PLAYERS][65];
new bool:gGovUIPublicReport[MAX_PLAYERS];
new gGovUIFundMax[MAX_PLAYERS];
new gGovUIFundAmount[MAX_PLAYERS];
new gGovUIPendingFundAmount[MAX_PLAYERS];

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
    gGovUIPendingIssue[playerid]=0;
    gGovUIPendingCode[playerid][0]=EOS;
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
    gGovUIEditing[playerid]=false;
    gGovUIReviewCreator[playerid]=0;
    gGovUIPendingEvent[playerid][0]=EOS;
    gGovUIPublicReport[playerid]=false;
    gGovUIFundMax[playerid]=0;
    gGovUIFundAmount[playerid]=0;
    gGovUIPendingFundAmount[playerid]=0;
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
        "SELECT version FROM schema_migrations WHERE version=29 LIMIT 1");
    if (c==Cache:0) return 1;
    new rows=0;
    new bool:valid=mysql_errno(gDBPrimary)==0 &&
        cache_get_row_count(rows) && rows==1;
    cache_delete(c);
    if (!valid)
    {
        print("[GOV-05] Migration 029 missing; government issue UI disabled.");
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
    gGovUIPendingIssue[playerid]=0;
    gGovUIPendingCode[playerid][0]=EOS;
    gGovUIPendingFundAmount[playerid]=(action==8)?gGovUIFundAmount[playerid]:0;
    if (action==3)
    {
        // These values must match the committed SQL result in OnWrite.
        gGovUIPendingIssue[playerid]=gGovUISelectedIssue[playerid];
        format(gGovUIPendingCode[playerid],65,"%s",gGovUISelectedCode[playerid]);
    }
    format(gGovUIPendingEvent[playerid],65,"%s",eventKey);
    gGovUIWriteSerial[playerid]++;
    if (!mysql_tquery(gDBPrimary,sql,"GovBond_UI_OnWrite","iiii",
        playerid,gAuthToken[playerid],gDBPrimaryAccountId[playerid],
        gGovUIWriteSerial[playerid]))
    {
        gGovUIPendingFundAmount[playerid]=0;
        gGovUIBusy[playerid]=false;
        gGovUIPendingAction[playerid]=0;
        gGovUIPendingIssue[playerid]=0;
        gGovUIPendingCode[playerid][0]=EOS;
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
        "SELECT COALESCE((SELECT m.rank_id FROM faction_members m JOIN factions f ON f.faction_id=m.faction_id WHERE m.account_id=%d AND m.faction_id=5 AND f.is_enabled=1 AND f.term_end>UNIX_TIMESTAMP()),0) AS finance_rank",
        gDBPrimaryAccountId[playerid]);
    if (!GovBond_UI_Query(playerid,query,1))
        SendClientMessage(playerid,COLOR_RED,"Реестр полномочий недоступен.");
#endif
    return 1;
}

stock bool:GovBond_UI_CanCreate(playerid)
{
    return gGovUIRole[playerid]>=2;
}
stock bool:GovBond_UI_CanApprove(playerid)
{
    return gGovUIRole[playerid]>=3;
}

stock GovBond_UI_DrawMain(playerid)
{
    new body[360];
    format(body,sizeof(body),
        "{E8EDF3}Новый государственный заём\n{E8EDF3}Реестр выпусков и решения\n{E8EDF3}Финансовая отчётность\n{E8EDF3}Обучение и полномочия министерства\n{E8EDF3}Пополнить страховой фонд (министр)");
    gGovUIStage[playerid]=1;
    ShowPlayerDialog(playerid,GOVUI_MAIN,DIALOG_STYLE_LIST,
        "{E8EDF3}TARKENSO {E8EDF3}| {E8EDF3}Государственное казначейство",
        body,"Выбрать","Закрыть");
    return 1;
}

stock GovBond_UI_LoadIssues(playerid,page=0)
{
#if defined TARKENSO_DB_PRIMARY
    if (!GovBond_UI_Authorized(playerid) || page<0 || page>500) return 1;
    new query[750];
    mysql_format(gDBPrimary,query,sizeof(query),
        "SELECT i.issue_id,i.issue_code,i.status,i.face_value,i.max_quantity,i.yield_basis_points,COALESCE(a.username,'') AS applicant FROM government_bond_issues i LEFT JOIN accounts a ON a.account_id=i.created_by_account_id ORDER BY (i.status='DRAFT') DESC,i.issue_id DESC LIMIT 11 OFFSET %d",
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
    new query[1800];
    mysql_format(gDBPrimary,query,sizeof(query),
        "SELECT i.issue_id,i.issue_code,i.title,i.purpose,i.issue_type,i.status,i.face_value,i.max_quantity,i.sold_quantity,i.yield_basis_points,i.insured_max_exposure,i.created_at,i.offer_start_at,i.offer_end_at,i.maturity_at,TIMESTAMPDIFF(DAY,i.offer_start_at,i.offer_end_at) AS offer_days,TIMESTAMPDIFF(DAY,i.offer_end_at,i.maturity_at) AS maturity_days,i.rejection_reason,i.returned_reason,i.created_by_account_id,COALESCE(a.username,'') AS applicant,COALESCE(rej.username,'') AS rejector,COALESCE((SELECT ma.username FROM faction_members fm JOIN factions fac ON fac.faction_id=fm.faction_id JOIN accounts ma ON ma.account_id=fm.account_id WHERE fm.faction_id=5 AND fm.rank_id=4 AND fac.is_enabled=1 AND fac.term_end>UNIX_TIMESTAMP() ORDER BY fm.account_id LIMIT 1), '') AS minister FROM government_bond_issues i LEFT JOIN accounts a ON a.account_id=i.created_by_account_id LEFT JOIN accounts rej ON rej.account_id=i.rejected_by_account_id WHERE i.issue_id=%d LIMIT 1",
        issueId);
    gGovUISelectedIssue[playerid]=issueId;
    if (!GovBond_UI_Query(playerid,query,3))
        SendClientMessage(playerid,COLOR_RED,"Выпуск не найден.");
#else
    #pragma unused playerid,issueId
#endif
    return 1;
}

// Read-only diagnostics after an ACTUAL rejected SQL signing transaction.
// The signing procedure remains the only authority: no reservation or approval
// is performed here. A fresh snapshot is important after concurrent changes.
stock bool:GovBond_UI_LoadApprovalReason(playerid)
{
#if defined TARKENSO_DB_PRIMARY
    if (!GovBond_UI_Authorized(playerid) ||
        !gGovUISelectedCode[playerid][0]) return false;
    new query[1950];
    mysql_format(gDBPrimary,query,sizeof(query),
        "SELECT x.issue_code,x.status,x.sold_quantity,x.is_signed,x.enabled,x.reserved,x.committed,x.util_bps,x.row_version,x.role_rank,CAST(x.exposure AS CHAR) AS exposure,CAST(CEIL((x.committed+x.exposure)*10000/NULLIF(x.util_bps,0)) AS CHAR) AS reserve_min,CAST(GREATEST(0,CEIL((x.committed+x.exposure)*10000/NULLIF(x.util_bps,0))-x.reserved) AS CHAR) AS reserve_short FROM (SELECT i.issue_code,i.status,i.sold_quantity,(i.approve_event_key IS NOT NULL) AS is_signed,t.bond_fund_enabled AS enabled,t.bond_fund_reserved AS reserved,t.bond_fund_committed AS committed,t.bond_fund_utilization_bps AS util_bps,t.row_version,COALESCE((SELECT m.rank_id FROM faction_members m JOIN factions f ON f.faction_id=m.faction_id WHERE m.account_id=%d AND m.faction_id=5 AND m.rank_id IN (3,4) AND f.is_enabled=1 AND f.term_end>UNIX_TIMESTAMP() LIMIT 1),0) AS role_rank,CEIL(CAST(i.face_value AS DECIMAL(20,0))*t.bond_fund_coverage_bps/10000)*i.max_quantity AS exposure FROM government_bond_issues i JOIN treasury_accounts t ON t.code='state' WHERE i.issue_code='%e' LIMIT 1) x",
        gDBPrimaryAccountId[playerid],gGovUISelectedCode[playerid]);
    return GovBond_UI_Query(playerid,query,5);
#else
    #pragma unused playerid
    return false;
#endif
}

// Re-read treasury capacity before offering any amount. This is advisory only;
// the stored procedure takes the treasury row lock and checks it again.
stock GovBond_UI_LoadFundBudget(playerid)
{
#if defined TARKENSO_DB_PRIMARY
    if(!GovBond_UI_Authorized(playerid) || gGovUIRole[playerid]!=4 ||
       !Leader_FinanceAuthorized(playerid,4))return 1;
    new query[710];
    mysql_format(gDBPrimary,query,sizeof(query),
        "SELECT balance,bond_fund_reserved,bond_fund_committed,bond_fund_enabled,minimum_reserve,max_transaction,GREATEST(0,CAST(balance AS SIGNED)-CAST(minimum_reserve AS SIGNED)-CAST(bond_fund_reserved AS SIGNED)) AS free_budget FROM treasury_accounts WHERE code='state'");
    if(!GovBond_UI_Query(playerid,query,6))
        SendClientMessage(playerid,COLOR_RED,
            "Не удалось получить свободный остаток казны. Попробуйте ещё раз.");
#else
    #pragma unused playerid
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
    if (kind==5)
    {
        // SQL read after rollback: no assumptions from an earlier UI snapshot.
        new code[65],status[20],reserve[32],minimum[32],shortage[32];
        new exposure[32],signed,sold,enabled,rank,util,version;
        if (rows!=1 || !cache_get_value_name(0,"issue_code",code,sizeof(code)) ||
            strcmp(code,gGovUISelectedCode[playerid]) ||
            !cache_get_value_name(0,"status",status,sizeof(status)) ||
            !cache_get_value_name(0,"reserved",reserve,sizeof(reserve)) ||
            !cache_get_value_name(0,"reserve_min",minimum,sizeof(minimum)) ||
            !cache_get_value_name(0,"reserve_short",shortage,sizeof(shortage)) ||
            !cache_get_value_name(0,"exposure",exposure,sizeof(exposure)) ||
            !cache_get_value_name_int(0,"is_signed",signed) ||
            !cache_get_value_name_int(0,"sold_quantity",sold) ||
            !cache_get_value_name_int(0,"enabled",enabled) ||
            !cache_get_value_name_int(0,"role_rank",rank) ||
            !cache_get_value_name_int(0,"util_bps",util) ||
            !cache_get_value_name_int(0,"row_version",version))
        {
            SendClientMessage(playerid,COLOR_RED,
                "Подпись отклонена. Причину сейчас не удалось уточнить. Обновите реестр.");
            return 1;
        }
        new body[950],reason[370];
        reason[0]=EOS;
        if(rank<3)
            format(reason,sizeof(reason),
                "У вас нет права подписывать облигации.\nЭто может сделать только заместитель или министр финансов (ранг 3-4).");
        else if(signed || strcmp(status,"DRAFT"))
            format(reason,sizeof(reason),
                "Выпуск уже подписан или больше не ожидает подписи.\nОбновите реестр и проверьте его состояние.");
        else if(sold>0)
            format(reason,sizeof(reason),
                "По этому выпуску уже есть покупки.\nПодписывать выпуск после начала продажи нельзя.");
        else if(util<=0 || !minimum[0] || !exposure[0])
            format(reason,sizeof(reason),
                "Не удалось рассчитать страховое обеспечение.\nПроверьте настройки страхового фонда.");
        else if(!enabled || strcmp(shortage,"0") || !strcmp(exposure,"0"))
        {
            // Read-only snapshot; the SQL approval transaction is authoritative.
            if(!strcmp(exposure,"0"))
                format(reason,sizeof(reason),
                    "Страховые правила сейчас не позволяют обеспечить выпуск.\nПопросите администратора проверить настройки фонда.");
            else
            {
                format(reason,sizeof(reason),
                    "Страховая база для этого выпуска:\n\nСейчас в резерве: $%s\nНужно минимум: $%s\nНе хватает: $%s\n\n",
                    reserve,minimum,shortage);
                if(strcmp(shortage,"0"))
                    strcat(reason,
                        "Недостаточно денег в страховом фонде.\nВыделите недостающую сумму из казны.",
                        sizeof(reason));
                if(!enabled)
                    strcat(reason,
                        "\nСтраховой фонд выключен. Его также нужно включить.",
                        sizeof(reason));
            }
        }
        else if(version>=2147483646)
            format(reason,sizeof(reason),
                "Казначейство достигло предела номера операции.\nОбратитесь к администратору: повторная подпись не поможет.");
        else
            format(reason,sizeof(reason),
                "Казначейство не подтвердило подпись.\nВозможно, состояние выпуска или казны изменилось.\nОбновите реестр. Если ошибка повторится, сообщите администратору.");
        format(body,sizeof(body),
            "{E8EDF3}ВЫПУСК %s НЕ ПОДПИСАН\n\n{E8EDF3}%s\n\n{E8EDF3}Деньги с казны не списаны, выпуск не утверждён.\n{E8EDF3}Показано состояние казны на момент проверки.",
            code,reason);
        gGovUIStage[playerid]=13;
        ShowPlayerDialog(playerid,GOVUI_APPROVAL_REASON,DIALOG_STYLE_MSGBOX,
            "{E8EDF3}Минфин | Почему отказано в подписи",body,"Понятно","");
        return 1;
    }
    if (kind==6)
    {
        new cash,reserved,committed,enabled,freeBudget,maxTxn,minReserve;
        if(rows!=1 || !cache_get_value_name_int(0,"balance",cash) ||
           !cache_get_value_name_int(0,"bond_fund_reserved",reserved) ||
           !cache_get_value_name_int(0,"bond_fund_committed",committed) ||
           !cache_get_value_name_int(0,"bond_fund_enabled",enabled) ||
           !cache_get_value_name_int(0,"minimum_reserve",minReserve) ||
           !cache_get_value_name_int(0,"max_transaction",maxTxn) ||
           !cache_get_value_name_int(0,"free_budget",freeBudget))
        {
            SendClientMessage(playerid,COLOR_RED,
                "Не удалось прочитать остаток казны. Деньги не изменены.");
            return 1;
        }
        if(gGovUIRole[playerid]!=4 || !Leader_FinanceAuthorized(playerid,4))
            return 1;
        new maximum=freeBudget;
        if(maximum>maxTxn)maximum=maxTxn;
        if(maximum>1000000000)maximum=1000000000;
        gGovUIFundMax[playerid]=maximum;
        gGovUIFundAmount[playerid]=0;
        if(maximum<=0)
        {
            SendClientMessage(playerid,COLOR_RED,
                "В казне нет свободных денег для страхового фонда.");
            return GovBond_UI_DrawMain(playerid);
        }
        new body[640];
        format(body,sizeof(body),
            "{E8EDF3}ПОПОЛНЕНИЕ СТРАХОВОГО ФОНДА\n\nКазна: $%d\nОбязательный остаток: $%d\nСтраховой резерв: $%d\nВыдано гарантий: $%d\nСвободно для перевода: $%d\n\nВведите сумму от $1 до $%d.\nДеньги останутся в казне, но станут страховым резервом.\nСтатус фонда: %s.",
            cash,minReserve,reserved,committed,freeBudget,maximum,
            enabled?"Включён":"Выключен");
        gGovUIStage[playerid]=14;
        ShowPlayerDialog(playerid,GOVUI_FUND_AMOUNT,DIALOG_STYLE_INPUT,
            "{E8EDF3}Минфин | Пополнить страховой фонд",
            body,"Далее","Назад");
        return 1;
    }
    if (kind==1)
    {
        new internalRank=0;
        if (rows!=1 || !cache_get_value_name_int(0,"finance_rank",internalRank))
            return 1;
        if (Leader_PlayerOrg(playerid)!=FACTION_FINANCE ||
            !Leader_ActiveTerm(FACTION_FINANCE) ||
            Leader_PlayerRank(playerid)!=internalRank) return 1;
        gGovUIRole[playerid]=Leader_PublicRank(FACTION_FINANCE,internalRank);
        if (!gGovUIRole[playerid])
        {
            SendClientMessage(playerid,COLOR_RED,
                "Вы не состоите в Министерстве финансов или срок лидерства истёк.");
            return 1;
        }
        return GovBond_UI_DrawMain(playerid);
    }
    if (kind==2)
    {
        new shown=rows>GOVUI_ROWS?GOVUI_ROWS:rows;
        gGovUICount[playerid]=shown;
        gGovUIHasMore[playerid]=rows>GOVUI_ROWS;
        new body[2700],line[225],code[65],status[20],statusLabel[40],applicant[40];
        new id,face,quantity,yield;
        format(body,sizeof(body),"{E8EDF3}Код\tЗаявитель\tКол-во\tНоминал\tСтатус\n");
        for(new i=0;i<shown;i++)
        {
            if (!cache_get_value_name_int(i,"issue_id",id) ||
                !cache_get_value_name(i,"issue_code",code,sizeof(code)) ||
                !cache_get_value_name(i,"status",status,sizeof(status)) ||
                !cache_get_value_name(i,"applicant",applicant,sizeof(applicant)) ||
                !cache_get_value_name_int(i,"face_value",face) ||
                !cache_get_value_name_int(i,"max_quantity",quantity) ||
                !cache_get_value_name_int(i,"yield_basis_points",yield)) return 1;
            // accounts.username is ascii_bin; keep Cyrillic fallbacks in Pawn
            // rather than combining incompatible SQL collations in COALESCE.
            if(!applicant[0])format(applicant,sizeof(applicant),"Неизвестен");
            gGovUIRows[playerid][i]=id;
            GovBond_UI_IssueStatusLabel(status,statusLabel,sizeof(statusLabel));
            format(line,sizeof(line),
                "{E8EDF3}%s\t{E8EDF3}%s\t{E8EDF3}%d\t{E8EDF3}$%d\t{E8EDF3}%s\n",
                code,applicant,quantity,face,statusLabel);
            strcat(body,line,sizeof(body));
        }
        if(!shown)
            strcat(body,"{E8EDF3}Заявок пока нет\t—\t—\t—\t—\n",sizeof(body));
        if(gGovUIHasMore[playerid])
            strcat(body,"{E8EDF3}Следующая страница\t\t\t\t\n",sizeof(body));
        if(gGovUIPage[playerid]>0)
            strcat(body,"{E8EDF3}Предыдущая страница\t\t\t\t\n",sizeof(body));
        strcat(body,"{E8EDF3}Назад\t\t\t\t\n",sizeof(body));
        gGovUIStage[playerid]=2;
        ShowPlayerDialog(playerid,GOVUI_LIST,DIALOG_STYLE_TABLIST_HEADERS,
            "{E8EDF3}Минфин | Заявки и выпуски",body,"Открыть","Назад");
        return 1;
    }
    if (kind==3)
    {
        new id,code[65],title[129],purpose[193],issueType[16],typeLabel[48],status[20];
        new face,total,sold,yield,exposure,offerDays,maturityDays,creatorId;
        new applicant[40],minister[40],rejector[40],reason[256],returnReason[256],statusLabel[40];
        new createdAt[32],offerStart[32],offerEnd[32],maturity[32];
        if(rows!=1 || !cache_get_value_name_int(0,"issue_id",id) ||
            id!=gGovUISelectedIssue[playerid] ||
            !cache_get_value_name(0,"issue_code",code,sizeof(code)) ||
            !cache_get_value_name(0,"title",title,sizeof(title)) ||
            !cache_get_value_name(0,"purpose",purpose,sizeof(purpose)) ||
            !cache_get_value_name(0,"issue_type",issueType,sizeof(issueType)) ||
            !cache_get_value_name(0,"status",status,sizeof(status)) ||
            !cache_get_value_name(0,"applicant",applicant,sizeof(applicant)) ||
            !cache_get_value_name(0,"minister",minister,sizeof(minister)) ||
            !cache_get_value_name(0,"rejector",rejector,sizeof(rejector)) ||
            !cache_get_value_name(0,"created_at",createdAt,sizeof(createdAt)) ||
            !cache_get_value_name(0,"offer_start_at",offerStart,sizeof(offerStart)) ||
            !cache_get_value_name(0,"offer_end_at",offerEnd,sizeof(offerEnd)) ||
            !cache_get_value_name(0,"maturity_at",maturity,sizeof(maturity)) ||
            !cache_get_value_name_int(0,"face_value",face) ||
            !cache_get_value_name_int(0,"max_quantity",total) ||
            !cache_get_value_name_int(0,"sold_quantity",sold) ||
            !cache_get_value_name_int(0,"yield_basis_points",yield) ||
            !cache_get_value_name_int(0,"insured_max_exposure",exposure) ||
            !cache_get_value_name_int(0,"created_by_account_id",creatorId) ||
            !cache_get_value_name_int(0,"offer_days",offerDays) ||
            !cache_get_value_name_int(0,"maturity_days",maturityDays))
            return 1;
        if(!applicant[0])format(applicant,sizeof(applicant),"Неизвестен");
        if(!minister[0])format(minister,sizeof(minister),"Не назначен");
        if(!rejector[0])format(rejector,sizeof(rejector),"Неизвестен");
        reason[0]=EOS;
        returnReason[0]=EOS;
        cache_get_value_name(0,"rejection_reason",reason,sizeof(reason));
        cache_get_value_name(0,"returned_reason",returnReason,sizeof(returnReason));
        gGovUIReviewCreator[playerid]=creatorId;
        gGovUISelectedIssue[playerid]=id;
        format(gGovUISelectedCode[playerid],65,"%s",code);
        format(gGovUISelectedStatus[playerid],20,"%s",status);
        new body[1700],extra[400];
        GovBond_UI_IssueStatusLabel(status,statusLabel,sizeof(statusLabel));
        GovBond_UI_TypeLabel(issueType,typeLabel,sizeof(typeLabel));
        format(body,sizeof(body),
            "{E8EDF3}ГОСУДАРСТВЕННАЯ ОБЛИГАЦИЯ\n\n{E8EDF3}Код выпуска: {E8EDF3}%s\n{E8EDF3}Подал заявку: {E8EDF3}%s\n{E8EDF3}Действующий министр: {E8EDF3}%s\n{E8EDF3}Состояние: {E8EDF3}%s\n\n{E8EDF3}Название: {E8EDF3}%s\n{E8EDF3}Назначение: {E8EDF3}%s\n{E8EDF3}Тип займа: {E8EDF3}%s\n{E8EDF3}Количество облигаций: {E8EDF3}%d\n{E8EDF3}Номинал: {E8EDF3}$%d\n{E8EDF3}Общий номинал: {E8EDF3}$%d\n{E8EDF3}Доходность: {E8EDF3}%d.%02d%%\n{E8EDF3}Продано: %d\n{E8EDF3}Гарантия: $%d\n\n{E8EDF3}Размещение: {E8EDF3}%d дней\n{E8EDF3}Погашение: {E8EDF3}через %d дней после размещения\n{E8EDF3}Дата подачи (UTC): {E8EDF3}%s\n{E8EDF3}Дата выпуска (UTC): {E8EDF3}%s\n{E8EDF3}Окончание размещения: {E8EDF3}%s\n{E8EDF3}Погашение (UTC): {E8EDF3}%s",
            code,applicant,minister,statusLabel,title,purpose,typeLabel,total,face,
            face*total,yield/100,yield%100,sold,exposure,
            offerDays,maturityDays,createdAt,offerStart,offerEnd,maturity);
        if(!strcmp(status,"REJECTED"))
        {
            format(extra,sizeof(extra),
                "\n\n{E8EDF3}ОТКЛОНЕНО\n{E8EDF3}Кто отклонил: {E8EDF3}%s\n{E8EDF3}Причина: {E8EDF3}%s",
                rejector,reason);
            strcat(body,extra,sizeof(body));
        }
        if(!strcmp(status,"RETURNED"))
        {
            format(extra,sizeof(extra),
                "\n\n{E8EDF3}ВОЗВРАТ НА ДОРАБОТКУ\n{E8EDF3}Причина: {E8EDF3}%s",
                returnReason);
            strcat(body,extra,sizeof(body));
        }
        if(!strcmp(status,"DRAFT") || !strcmp(status,"RETURNED") ||
            !strcmp(status,"APPROVED"))
            strcat(body,"\n{E8EDF3}Сроки отсчитываются от открытия размещения.",sizeof(body));
        gGovUIStage[playerid]=3;
        if (!strcmp(status,"RETURNED") && creatorId==gDBPrimaryAccountId[playerid])
            ShowPlayerDialog(playerid,GOVUI_ISSUE,DIALOG_STYLE_MSGBOX,
                "{E8EDF3}Минфин | Возврат на доработку",body,"Доработать","Назад");
        else if (!strcmp(status,"DRAFT") && GovBond_UI_CanApprove(playerid))
            ShowPlayerDialog(playerid,GOVUI_ISSUE,DIALOG_STYLE_MSGBOX,
                "{E8EDF3}Минфин | Заявка на подпись",body,"Подписать","Отклонить");
        else if (GovBond_UI_CanApprove(playerid) &&
                 strcmp(status,"REJECTED") && strcmp(status,"CANCELLED"))
            ShowPlayerDialog(playerid,GOVUI_ISSUE,DIALOG_STYLE_MSGBOX,
                "{E8EDF3}Минфин | Управление выпуском",body,"Действия","Назад");
        else
            ShowPlayerDialog(playerid,GOVUI_ISSUE,DIALOG_STYLE_MSGBOX,
                "{E8EDF3}Минфин | Информация о выпуске",body,"Назад","");
        return 1;
    }
    if (kind==4)
    {
        new cash,fund,committed,payout,issues,offering,sold,overdue,crisis[20],crisisLabel[40],body[900];
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
        GovBond_UI_CrisisStatusLabel(crisis,crisisLabel,sizeof(crisisLabel));
        format(body,sizeof(body),
            "{E8EDF3}ФИНАНСОВАЯ ОТЧЁТНОСТЬ\n\n{E8EDF3}Баланс казны: {E8EDF3}$%d\n{E8EDF3}Страховой резерв: {E8EDF3}$%d\n{E8EDF3}Принятые гарантии: {E8EDF3}$%d\n{E8EDF3}Резерв выплат: {E8EDF3}$%d\n\n{E8EDF3}Кризисный статус: {E8EDF3}%s\n{E8EDF3}Всего выпусков: %d\n{E8EDF3}На размещении: %d\n{E8EDF3}Продано облигаций: %d\n{E8EDF3}Необеспеченный долг: {E8EDF3}$%d\n\n{E8EDF3}Суммы получены из MySQL; отчёт без права списания.",
            cash,fund,committed,payout,crisisLabel,issues,offering,sold,overdue);
        gGovUIStage[playerid]=4;
        ShowPlayerDialog(playerid,GOVUI_REPORT,DIALOG_STYLE_MSGBOX,
            "{E8EDF3}TARKENSO | Отчёт казначейства",body,"Назад","");
    }
#else
    #pragma unused playerid,authToken,accountId,serial,kind
#endif
    return 1;
}

stock bool:GovBond_UI_ParsePositive(const input[],&number,maximum)
{
    // Reject overflow BEFORE multiplication (e.g. ten-digit 4294967297).
    new len=strlen(input);
    if(len<1 || len>10 || maximum<1)return false;
    number=0;
    for(new i=0;i<len;i++)
    {
        if(input[i]<'0' || input[i]>'9')return false;
        new digit=input[i]-'0';
        if(number>(maximum-digit)/10)return false;
        number=number*10+digit;
    }
    return number>=1 && number<=maximum;
}

// Player enters percent (1.5, 1,50, 0, 100), never basis points.
// The 0..10000 bp representation is internal and exact to 0.01%.
stock bool:GovBond_UI_ParsePercent(const input[],&basisPoints)
{
    new len=strlen(input),whole=0,fraction=0,fractionDigits=0;
    new bool:separator=false;
    if(len<1 || len>7)return false;
    for(new i=0;i<len;i++)
    {
        if(input[i]=='.' || input[i]==',')
        {
            if(separator || i==0 || i==len-1)return false;
            separator=true;
            continue;
        }
        if(input[i]<'0' || input[i]>'9')return false;
        if(separator)
        {
            fractionDigits++;
            if(fractionDigits>2)return false;
            fraction=fraction*10+(input[i]-'0');
        }
        else
        {
            whole=whole*10+(input[i]-'0');
            if(whole>100)return false;
        }
    }
    if(fractionDigits==1)fraction*=10;
    basisPoints=whole*100+fraction;
    return basisPoints<=10000;
}

stock GovBond_UI_Wizard(playerid,step)
{
    if (!GovBond_UI_Authorized(playerid) || !GovBond_UI_CanCreate(playerid))
        return 1;
    gGovUIWizardStep[playerid]=step;
    gGovUIStage[playerid]=5;
    new prompt[330],title[125];
    format(title,sizeof(title),"{E8EDF3}Новый выпуск | Этап %d/7",step);
    switch(step)
    {
        case 1: format(prompt,sizeof(prompt),
            "{E8EDF3}Название государственного займа\n{E8EDF3}От 3 до 128 символов.");
        case 2: format(prompt,sizeof(prompt),
            "{E8EDF3}Назначение займа\n{E8EDF3}От 3 до 192 символов.");
        case 3: format(prompt,sizeof(prompt),
            "{E8EDF3}Номинал одной облигации ($)\n{E8EDF3}Целое число 1—1000000000.");
        case 4: format(prompt,sizeof(prompt),
            "{E8EDF3}Количество облигаций в выпуске\n{E8EDF3}Целое число 5—1000000 (20%% на инвестора). Общий номинал не более 1 млрд.");
        case 5: format(prompt,sizeof(prompt),
            "{E8EDF3}Доходность за весь срок (%%)\n{E8EDF3}От 0 до 100%%; например, 1,5 = 1,50%%. До двух знаков после запятой.");
        case 6: format(prompt,sizeof(prompt),
            "{E8EDF3}Продолжительность размещения, дней\n{E8EDF3}От 1 до 30; начинается с открытия размещения.");
        case 7: format(prompt,sizeof(prompt),
            "{E8EDF3}Срок до погашения после завершения продажи, дней\n{E8EDF3}От 1 до 365.");
        default: return 1;
    }
    ShowPlayerDialog(playerid,GOVUI_INPUT,DIALOG_STYLE_INPUT,
        title,prompt,"Далее","Отмена");
    return 1;
}

stock GovBond_UI_ConfirmDraft(playerid)
{
    new body[900],typeLabel[48],codeHint[170];
    GovBond_UI_TypeLabel(gGovUIType[playerid],typeLabel,sizeof(typeLabel));
    format(body,sizeof(body),
        "{E8EDF3}ПРОЕКТ ГОСУДАРСТВЕННОГО ЗАЙМА\n\n{E8EDF3}Название: {E8EDF3}%s\n{E8EDF3}Цель: {E8EDF3}%s\n{E8EDF3}Тип: {E8EDF3}%s\n{E8EDF3}Номинал: {E8EDF3}$%d\n{E8EDF3}Количество: {E8EDF3}%d\n{E8EDF3}Общий номинал: {E8EDF3}$%d\n{E8EDF3}Доходность: {E8EDF3}%d.%02d%%\n{E8EDF3}Размещение: %d дней\n{E8EDF3}Погашение через: %d дней после размещения\n\n{E8EDF3}Отправка заявки не открывает продажу и не расходует бюджет.",
        gGovUITitle[playerid],gGovUIPurpose[playerid],typeLabel,
        gGovUIFace[playerid],gGovUIQuantity[playerid],
        gGovUIFace[playerid]*gGovUIQuantity[playerid],
        gGovUIYield[playerid]/100,gGovUIYield[playerid]%100,
        gGovUIOfferDays[playerid],gGovUIMaturityDays[playerid]);
    if(gGovUIEditing[playerid])
        format(codeHint,sizeof(codeHint),
            "\n{E8EDF3}Сохраняется код выпуска %s.",
            gGovUISelectedCode[playerid]);
    else format(codeHint,sizeof(codeHint),
            "\n{E8EDF3}Уникальный код выпуска назначит MySQL.");
    strcat(body,codeHint,sizeof(body));
    gGovUIStage[playerid]=6;
    ShowPlayerDialog(playerid,GOVUI_CONFIRM,DIALOG_STYLE_MSGBOX,
        "{E8EDF3}TARKENSO | Подтверждение проекта",body,
        gGovUIEditing[playerid]?"Отправить":"Создать","Отмена");
    return 1;
}

// A rejection is final for a draft. Store its reason in the issue itself,
// and an append-only REJECT event in the finance audit. Never touch balances.
stock GovBond_UI_RejectReason(playerid)
{
    if (!GovBond_UI_Authorized(playerid) ||
        !Leader_FinanceAuthorized(playerid,3) ||
        strcmp(gGovUISelectedStatus[playerid],"DRAFT")) return 1;
    gGovUIStage[playerid]=11;
    ShowPlayerDialog(playerid,GOVUI_INPUT,DIALOG_STYLE_INPUT,
        "{E8EDF3}Минфин | Отклонить заявку",
        "{E8EDF3}Укажите причину отказа (от 5 до 160 символов).\n{E8EDF3}Причину увидит автор заявки в реестре Минфина.\n{E8EDF3}Выпуск будет окончательно закрыт без продажи.",
        "Отклонить","Назад");
    return 1;
}

stock GovBond_UI_Reject(playerid,const reason[])
{
#if defined TARKENSO_DB_PRIMARY
    if (!GovBond_UI_Authorized(playerid) ||
        !Leader_FinanceAuthorized(playerid,3) ||
        strcmp(gGovUISelectedStatus[playerid],"DRAFT") ||
        gGovUISelectedIssue[playerid]<=0) return 1;
    new query[950],eventKey[65];
    GovBond_UI_Event(playerid,eventKey,sizeof(eventKey));
    mysql_format(gDBPrimary,query,sizeof(query),
        "CALL gov_bond_official_reject('%e','%e',%d,'%e')",
        eventKey,gGovUISelectedCode[playerid],
        gDBPrimaryAccountId[playerid],reason);
    gGovUIStage[playerid]=0; // consume the reason dialog before queueing SQL
    if (!GovBond_UI_Write(playerid,query,5,eventKey))
        SendClientMessage(playerid,COLOR_RED,"Отказ не зарегистрирован. Повторите через реестр.");
#else
    #pragma unused playerid,reason
#endif
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

stock GovBond_UI_ReturnReason(playerid)
{
    if (!GovBond_UI_Authorized(playerid) ||
        !Leader_FinanceAuthorized(playerid,3) ||
        strcmp(gGovUISelectedStatus[playerid],"DRAFT"))return 1;
    gGovUIStage[playerid]=13;
    ShowPlayerDialog(playerid,GOVUI_INPUT,DIALOG_STYLE_INPUT,
        "{E8EDF3}Минфин | Возврат на доработку",
        "{E8EDF3}Укажите, что должен исправить заявитель (5—160 символов).\n{E8EDF3}Причина будет видна в карточке выпуска.",
        "Вернуть","Назад");
    return 1;
}

stock GovBond_UI_Return(playerid,const reason[])
{
#if defined TARKENSO_DB_PRIMARY
    if (!GovBond_UI_Authorized(playerid) ||
        !Leader_FinanceAuthorized(playerid,3) ||
        strcmp(gGovUISelectedStatus[playerid],"DRAFT") ||
        gGovUISelectedIssue[playerid]<=0)return 1;
    new query[950],eventKey[65];
    GovBond_UI_Event(playerid,eventKey,sizeof(eventKey));
    mysql_format(gDBPrimary,query,sizeof(query),
        "CALL gov_bond_official_return('%e','%e',%d,'%e')",
        eventKey,gGovUISelectedCode[playerid],
        gDBPrimaryAccountId[playerid],reason);
    gGovUIStage[playerid]=0;
    if (!GovBond_UI_Write(playerid,query,7,eventKey))
        SendClientMessage(playerid,COLOR_RED,"Возврат заявки не принят.");
#else
    #pragma unused playerid,reason
#endif
    return 1;
}

stock GovBond_UI_Resubmit(playerid)
{
#if defined TARKENSO_DB_PRIMARY
    if (!GovBond_UI_Authorized(playerid) || !GovBond_UI_CanCreate(playerid) ||
        !gGovUIEditing[playerid] ||
        gGovUIReviewCreator[playerid]!=gDBPrimaryAccountId[playerid] ||
        strcmp(gGovUISelectedStatus[playerid],"RETURNED") ||
        gGovUISelectedIssue[playerid]<=0)return 1;
    new query[1500],eventKey[65];
    GovBond_UI_Event(playerid,eventKey,sizeof(eventKey));
    mysql_format(gDBPrimary,query,sizeof(query),
        "CALL gov_bond_issue_resubmit('%e','%e',%d,'%e','%e','%e',%d,%d,%d,%d,%d)",
        eventKey,gGovUISelectedCode[playerid],gDBPrimaryAccountId[playerid],
        gGovUITitle[playerid],gGovUIPurpose[playerid],gGovUIType[playerid],
        gGovUIFace[playerid],gGovUIQuantity[playerid],gGovUIYield[playerid],
        gGovUIOfferDays[playerid],gGovUIMaturityDays[playerid]);
    gGovUIStage[playerid]=0;
    if(!GovBond_UI_Write(playerid,query,6,eventKey))
        SendClientMessage(playerid,COLOR_RED,"Доработанная заявка не отправлена.");
    else gGovUIEditing[playerid]=false;
#else
    #pragma unused playerid
#endif
    return 1;
}

stock GovBond_UI_Actions(playerid)
{
    if (!GovBond_UI_Authorized(playerid) || gGovUISelectedIssue[playerid]<=0)
        return 1;
    if (!GovBond_UI_CanApprove(playerid))
    {
        SendClientMessage(playerid,COLOR_AMBER,
            "Стажёрам и специалистам доступен только просмотр. Решения принимает руководство.");
        return GovBond_UI_LoadIssues(playerid,gGovUIPage[playerid]);
    }
    gGovUIStage[playerid]=7;
    ShowPlayerDialog(playerid,GOVUI_ACTION,DIALOG_STYLE_LIST,
        "{E8EDF3}Казначейство | Действия по выпуску",
        "{E8EDF3}Утвердить выпуск (заместитель/министр)\n{E8EDF3}Открыть размещение (заместитель/министр)\n{E8EDF3}Отменить непроданный выпуск (заместитель/министр)\n{E8EDF3}Вернуться к реестру",
        "Выбрать","Закрыть");
    return 1;
}

stock GovBond_UI_ConfirmAction(playerid,action)
{
    if (!GovBond_UI_Authorized(playerid) || !gGovUISelectedCode[playerid][0])
        return 1;
    if ((action==2 || action==4) && !GovBond_UI_CanApprove(playerid))
        return SendClientMessage(playerid,COLOR_RED,"Действие доступно заместителю или министру финансов.");
    if (action==3 && !GovBond_UI_CanApprove(playerid))
        return SendClientMessage(playerid,COLOR_RED,"Действие доступно заместителю или министру финансов.");
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
        "{E8EDF3}ГОСУДАРСТВЕННОЕ РЕШЕНИЕ\n\n{E8EDF3}Выпуск: {E8EDF3}%s\n{E8EDF3}Операция: {E8EDF3}%s\n\n{E8EDF3}Проверки прав, фонда, сроков и держателей повторит MySQL.\n{E8EDF3}Подтвердить?",
        gGovUISelectedCode[playerid],verb);
    ShowPlayerDialog(playerid,GOVUI_CONFIRM,DIALOG_STYLE_MSGBOX,
        "{E8EDF3}TARKENSO | Утверждение решения",body,
        "Подтвердить","Отмена");
    return 1;
}

stock GovBond_UI_ExecuteFundTopup(playerid)
{
#if defined TARKENSO_DB_PRIMARY
    if(!GovBond_UI_Authorized(playerid) || gGovUIRole[playerid]!=4 ||
       !Leader_FinanceAuthorized(playerid,4) ||
       gGovUIFundAmount[playerid]<1 ||
       gGovUIFundAmount[playerid]>gGovUIFundMax[playerid])return 1;
    new eventKey[65],query[360];
    gGovUIEventSerial++;
    format(eventKey,sizeof(eventKey),"fund:gov5:%d:%d:%d:%d",
        gDBPrimaryAccountId[playerid],gettime(),
        gAuthToken[playerid],gGovUIEventSerial);
    mysql_format(gDBPrimary,query,sizeof(query),
        "CALL gov_bond_official_fund_capitalize('%e',%d,%d)",
        eventKey,gDBPrimaryAccountId[playerid],gGovUIFundAmount[playerid]);
    gGovUIStage[playerid]=0;
    if(!GovBond_UI_Write(playerid,query,8,eventKey))
        SendClientMessage(playerid,COLOR_RED,
            "Казначейство не приняло заявку. Попробуйте ещё раз.");
#else
    #pragma unused playerid
#endif
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
    if (action==3 && !GovBond_UI_CanApprove(playerid)) return 1;
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

public GovBond_UI_OnWrite(playerid,authToken,accountId,serial)
{
#if defined TARKENSO_DB_PRIMARY
    if (playerid<0 || playerid>=MAX_PLAYERS || !IsPlayerConnected(playerid) ||
        IsPlayerNPC(playerid) || !gGovUIBusy[playerid] ||
        gAuthState[playerid]!=AUTH_READY || gAuthToken[playerid]!=authToken ||
        !gDBPrimaryLoaded[playerid] || gDBPrimaryAccountId[playerid]!=accountId ||
        gGovUIPendingAccount[playerid]!=accountId ||
        gGovUIWriteSerial[playerid]!=serial) return 1;
    new rows=0,replayed,issueId=0,code[65],creatorId=0;
    new rejectionReason[256],decisionStatus[20];
    new action=gGovUIPendingAction[playerid];
    new bool:valid=mysql_errno(gDBPrimary)==0 &&
        cache_get_row_count(rows) && rows==1 &&
        cache_get_value_name_int(0,"replayed",replayed);
    new fundReserve=0,fundCommitted=0,fundBalance=0,fundVersion=0;
    new fundActor=0,fundApplied=0;
    if(valid && action==8)
        valid=cache_get_value_name_int(0,"fund_reserved",fundReserve) &&
              cache_get_value_name_int(0,"fund_committed",fundCommitted) &&
              cache_get_value_name_int(0,"treasury_balance",fundBalance) &&
              cache_get_value_name_int(0,"row_version",fundVersion) &&
              cache_get_value_name_int(0,"actor_account_id",fundActor) &&
              cache_get_value_name_int(0,"applied_amount",fundApplied) &&
              fundActor==accountId &&
              fundApplied==gGovUIPendingFundAmount[playerid] &&
              fundApplied>0 && fundReserve>=fundApplied &&
              fundReserve<=fundBalance && fundCommitted<=fundReserve &&
              fundVersion>=0;
    if (valid && (action==1 || action==6))
        valid=cache_get_value_name_int(0,"issue_id",issueId) &&
              cache_get_value_name(0,"issue_code",code,sizeof(code));
    if (valid && action==6)
        valid=issueId==gGovUISelectedIssue[playerid] &&
              !strcmp(code,gGovUISelectedCode[playerid]) &&
              cache_get_value_name(0,"issue_status",decisionStatus,sizeof(decisionStatus)) &&
              !strcmp(decisionStatus,"DRAFT");
    if (valid && action==3)
        valid=cache_get_value_name_int(0,"issue_id",issueId) &&
              cache_get_value_name(0,"issue_status",decisionStatus,sizeof(decisionStatus)) &&
              issueId==gGovUIPendingIssue[playerid] &&
              gGovUIPendingCode[playerid][0] &&
              (replayed==1 || !strcmp(decisionStatus,"OFFERING"));
    if (valid && (action==5 || action==7))
        valid=cache_get_value_name_int(0,"creator_account_id",creatorId) &&
              cache_get_value_name_int(0,"issue_id",issueId) &&
              cache_get_value_name(0,action==7?"return_reason":"rejection_reason",
                  rejectionReason,sizeof(rejectionReason)) &&
              cache_get_value_name(0,"issue_status",decisionStatus,sizeof(decisionStatus)) &&
              !strcmp(decisionStatus,action==7?"RETURNED":"REJECTED") &&
              issueId==gGovUISelectedIssue[playerid];
    // Unknown result may have COMMITTED. Disable retry for this session.
    if (!valid || (replayed!=0 && replayed!=1))
    {
        print("[GOV-05] Unconfirmed government operation; reload SQL audit.");
        gGovUIBusy[playerid]=false;
        gGovUIPendingEvent[playerid][0]=EOS;
        gGovUIPendingAction[playerid]=0;
        gGovUIPendingFundAmount[playerid]=0;
        gGovUIPendingIssue[playerid]=0;
        gGovUIPendingCode[playerid][0]=EOS;
        SendClientMessage(playerid,COLOR_RED,
            "Неизвестный результат операции. Проверьте реестр и журнал перед повтором.");
        return 1;
    }
    gGovUIBusy[playerid]=false;
    gGovUIPendingEvent[playerid][0]=EOS;
    gGovUIPendingAction[playerid]=0;
    gGovUIPendingFundAmount[playerid]=0;
    if(action==8)
    {
        // One faction notice for one confirmed NEW commit. SQL replay is silent.
        if(replayed==0)
        {
            new notice[150];
            format(notice,sizeof(notice),
                "[Минфин] %s пополнил страховой фонд на $%d. Резерв: $%d.",
                gAccountName[playerid],fundApplied,fundReserve);
            for(new i=0;i<MAX_PLAYERS;i++)
                if(IsPlayerConnected(i) && !IsPlayerNPC(i) &&
                   gAuthState[i]==AUTH_READY &&
                   Leader_PlayerOrg(i)==FACTION_FINANCE)
                    SendClientMessage(i,FACTION_COLOR,notice);
        }
        else
            SendClientMessage(playerid,COLOR_AMBER,
                "Это пополнение уже проводилось. Повторного перевода не было.");
        if(!BondFund_Reload())
            print("[GOV-08] Fund snapshot reload after top-up failed.");
        return 1;
    }
    if (action==3 && replayed==0)
    {
        // First committed APPROVED -> OFFERING transition only. Do not
        // advertise DRAFTs, signatures, failed calls, or idempotent replays.
        new notice[160];
        format(notice,sizeof(notice),
            "[Министерство финансов] Открыто размещение облигаций {FFFFFF}%s{D9AA55}.",
            gGovUIPendingCode[playerid]);
        SendClientMessageToAll(COLOR_AMBER,notice);
        SendClientMessageToAll(FACTION_COLOR,
            "[Министерство финансов] Облигации доступны для покупки: {FFFFFF}/bonds.");
    }
    gGovUIPendingIssue[playerid]=0;
    gGovUIPendingCode[playerid][0]=EOS;
    if (action==1 || action==6)
    {
        new message[150];
        format(message,sizeof(message),
            action==6?"Казначейство: доработанный проект %s повторно подан (ID %d).":
                "Казначейство: проект %s зарегистрирован (ID %d).",code,issueId);
        SendClientMessage(playerid,COLOR_AMBER,message);
        // Notify the existing faction chat ONLY after a successful SQL commit.
        // Rank 3 and 4 can sign their own bond using the same explicit command.
        if (replayed==0 && Leader_PlayerOrg(playerid)==FACTION_FINANCE)
        {
            new notice[175];
            format(notice,sizeof(notice),
                action==6?"[Минфин] %s повторно подал доработанный выпуск %s.":
                    "[Минфин] %s подал заявку на подпись выпуска %s.",
                gAccountName[playerid],code);
            for(new i=0;i<MAX_PLAYERS;i++)
                if(Leader_PlayerOrg(i)==FACTION_FINANCE)
                    SendClientMessage(i,FACTION_COLOR,notice);
            format(notice,sizeof(notice),
                "[Минфин] Для подписи откройте /bondgov -> Реестр выпусков (%s).",code);
            for(new i=0;i<MAX_PLAYERS;i++)
                if(Leader_PlayerOrg(i)==FACTION_FINANCE)
                    SendClientMessage(i,FACTION_COLOR,notice);
        }
    }
    else if(action==5 || action==7)
    {
        new notice[150];
        format(notice,sizeof(notice),
            action==7?"Заявка %s возвращена на доработку.":
                "Заявка %s отклонена. Причина сохранена в реестре.",
            gGovUISelectedCode[playerid]);
        SendClientMessage(playerid,COLOR_AMBER,notice);
        if(replayed==0)
        {
            // Online author sees the decision immediately. The FULL reason is
            // also stored in the issue and is readable after relogging.
            for(new i=0;i<MAX_PLAYERS;i++)
                if(IsPlayerConnected(i) && !IsPlayerNPC(i) &&
                   gAuthState[i]==AUTH_READY &&
                   gDBPrimaryAccountId[i]==creatorId)
                {
                    SendClientMessage(i,COLOR_RED,notice);
                    format(notice,sizeof(notice),
                        action==7?"Причина возврата: %.96s":"Причина отказа: %.96s",
                        rejectionReason);
                    SendClientMessage(i,COLOR_AMBER,notice);
                    SendClientMessage(i,COLOR_AMBER,
                        action==7?
                            "Полная причина: /bondgov -> Реестр выпусков -> На доработке.":
                            "Полная причина: /bondgov -> Реестр выпусков -> Отклонено.");
                    break;
                }
        }
        return GovBond_UI_LoadIssues(playerid,gGovUIPage[playerid]);
    }
    else
    {
        SendClientMessage(playerid,COLOR_AMBER,
            "Решение по выпуску зафиксировано в казначейском реестре.");
    }
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
    // Read-time SQL errors are NOT a database outage. The old default
    // OnQueryError handler marked MySQL down and kicked every player.
    // A failed reader must reset its pending state and remain recoverable.
    if(!strcmp(callback,"GovBond_UI_OnRead",true))
    {
        if(errorid==2002 || errorid==2003 || errorid==2006 ||
           errorid==2013 || errorid==2055)return false;
        printf("[GOV-05] Finance read failed: SQL code %d.",errorid);
        for(new i=0;i<MAX_PLAYERS;i++)
        {
            if(!gGovUILoading[i])continue;
            gGovUILoading[i]=false;
            gGovUIReadSerial[i]++;
            if(IsPlayerConnected(i) && !IsPlayerNPC(i) &&
               gAuthState[i]==AUTH_READY)
                SendClientMessage(i,COLOR_RED,
                    "Ошибка чтения реестра Минфина. Данные не изменены.");
        }
        return true;
    }
    if(errorid==2002 || errorid==2003 || errorid==2006 ||
       errorid==2013 || errorid==2055)return false;
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
        new failedAction=gGovUIPendingAction[playerid];
        gGovUIBusy[playerid]=false;
        gGovUIPendingEvent[playerid][0]=EOS;
        gGovUIPendingAction[playerid]=0;
        gGovUIPendingFundAmount[playerid]=0;
        gGovUIPendingIssue[playerid]=0;
        gGovUIPendingCode[playerid][0]=EOS;
        if(IsPlayerConnected(playerid) && gAuthState[playerid]==AUTH_READY)
        {
            if(failedAction==8)
                SendClientMessage(playerid,COLOR_RED,
                    "Не удалось пополнить фонд. Проверьте свободные деньги и полномочия министра.");
            else if(failedAction==2)
            {
                if(!GovBond_UI_LoadApprovalReason(playerid))
                    SendClientMessage(playerid,COLOR_RED,
                        "Подпись отклонена. Не удалось проверить причину. Обновите реестр.");
            }
            else
                SendClientMessage(playerid,COLOR_RED,
                    "Казначейство отклонило действие. Обновите выпуск и проверьте полномочия.");
        }
        break;
    }
#else
    #pragma unused errorid,callback,query
#endif
    return true;
}

stock GovBond_UI_OnDialog(playerid,dialogid,response,listitem,const inputtext[])
{
    if (dialogid<GOVUI_MAIN || dialogid>GOVUI_FUND_CONFIRM)return 0;
    if(!GovBond_UI_Authorized(playerid) || gGovUILoading[playerid] ||
       gGovUIBusy[playerid]) return 1;
    if(dialogid==GOVUI_MAIN)
    {
        if(gGovUIStage[playerid]!=1 || !response || listitem<0 || listitem>4)
            return 1;
        switch(listitem)
        {
            case 0:
            {
                gGovUIEditing[playerid]=false;
                if (!GovBond_UI_CanCreate(playerid))
                    return SendClientMessage(playerid,COLOR_RED,
                        "Создавать займы могут специалисты и руководство Минфина.");
                gGovUIStage[playerid]=9;
                ShowPlayerDialog(playerid,GOVUI_TYPE,DIALOG_STYLE_LIST,
                    "{E8EDF3}Казначейство | Тип государственного займа",
                    "Общий бюджетный заём\nЦелевое финансирование\nАнтикризисный выпуск",
                    "Выбрать","Назад");
                return 1;
            }
            case 1:return GovBond_UI_LoadIssues(playerid,0);
            case 2:return GovBond_UI_Report(playerid);
            case 4:
            {
                if(gGovUIRole[playerid]!=4 ||
                   !Leader_FinanceAuthorized(playerid,4))
                    return SendClientMessage(playerid,COLOR_RED,
                        "Пополнять страховой фонд может только министр финансов (ранг 4).");
                return GovBond_UI_LoadFundBudget(playerid);
            }
            case 3:
            {
                gGovUIStage[playerid]=10;
                ShowPlayerDialog(playerid,GOVUI_REPORT,DIALOG_STYLE_MSGBOX,
                    "{E8EDF3}Минфин | Обучение",
                    "{E8EDF3}1. Стажёр: изучение отчётов.\n{E8EDF3}2. Специалист: создаёт заявку.\n{E8EDF3}3. Заместитель: подписывает или отклоняет с причиной.\n{E8EDF3}4. Министр: подписывает или отклоняет с причиной.\n\n{E8EDF3}Подпись только в /bondgov -> Реестр выпусков.",
                    "Назад","");
                return 1;
            }
        }
        return 1;
    }
    if(dialogid==GOVUI_FUND_AMOUNT)
    {
        if(gGovUIStage[playerid]!=14)return 1;
        if(!response)return GovBond_UI_Main(playerid);
        new amount=0;
        if(!GovBond_UI_ParsePositive(inputtext,amount,gGovUIFundMax[playerid]))
        {
            SendClientMessage(playerid,COLOR_RED,
                "Введите сумму целым числом от $1 до разрешённого максимума.");
            return GovBond_UI_LoadFundBudget(playerid);
        }
        if(gGovUIRole[playerid]!=4 || !Leader_FinanceAuthorized(playerid,4))
            return 1;
        gGovUIFundAmount[playerid]=amount;
        new body[450];
        format(body,sizeof(body),
            "{E8EDF3}ПОДТВЕРЖДЕНИЕ ПОПОЛНЕНИЯ\n\nСумма: $%d\nИз свободных денег государственной казны.\nПосле перевода сумма будет защищена как страховой резерв.\n\nПодтвердить перевод?",
            amount);
        gGovUIStage[playerid]=15;
        ShowPlayerDialog(playerid,GOVUI_FUND_CONFIRM,DIALOG_STYLE_MSGBOX,
            "{E8EDF3}Минфин | Подтверждение перевода",
            body,"Подтвердить","Отмена");
        return 1;
    }
    if(dialogid==GOVUI_FUND_CONFIRM)
    {
        if(gGovUIStage[playerid]!=15)return 1;
        gGovUIStage[playerid]=0; // consume before SQL request
        if(!response)return GovBond_UI_Main(playerid);
        return GovBond_UI_ExecuteFundTopup(playerid);
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
        if(gGovUIStage[playerid]==13)
        {
            if(!response)return GovBond_UI_LoadIssue(playerid,gGovUISelectedIssue[playerid]);
            new len=strlen(inputtext),bool:nonspace=false;
            for(new i=0;i<len;i++)
            {
                if(inputtext[i]!=' ')nonspace=true;
                if(inputtext[i]=='{' || inputtext[i]=='}' || inputtext[i]<32)
                    return GovBond_UI_ReturnReason(playerid);
            }
            if(len<5 || len>160 || !nonspace)
                return GovBond_UI_ReturnReason(playerid);
            return GovBond_UI_Return(playerid,inputtext);
        }
        if(gGovUIStage[playerid]==11)
        {
            if(!response) return GovBond_UI_LoadIssue(playerid,gGovUISelectedIssue[playerid]);
            new len=strlen(inputtext),bool:nonspace=false;
            for(new i=0;i<len;i++)
            {
                if(inputtext[i]!=' ') nonspace=true;
                if(inputtext[i]=='{' || inputtext[i]=='}' ||
                    inputtext[i]<32)
                {
                    SendClientMessage(playerid,COLOR_RED,
                        "Недопустимый символ в причине отказа.");
                    return GovBond_UI_RejectReason(playerid);
                }
            }
            if(len<5 || len>160 || !nonspace)
            {
                SendClientMessage(playerid,COLOR_RED,
                    "Укажите содержательную причину от 5 до 160 символов.");
                return GovBond_UI_RejectReason(playerid);
            }
            return GovBond_UI_Reject(playerid,inputtext);
        }
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
            if(step==5)
            {
                if(!GovBond_UI_ParsePercent(inputtext,num))
                    return GovBond_UI_Wizard(playerid,step);
            }
            else if(!GovBond_UI_ParsePositive(inputtext,num,1000000000))
                return GovBond_UI_Wizard(playerid,step);
            switch(step)
            {
                case 3: gGovUIFace[playerid]=num;
                case 4:
                {
                    if(num<5 || num>1000000 || gGovUIFace[playerid]>1000000000/num)
                        return GovBond_UI_Wizard(playerid,step);
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
        if(mode==6)
        {
            if(gGovUIEditing[playerid])return GovBond_UI_Resubmit(playerid);
            return GovBond_UI_Create(playerid);
        }
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
        if(!strcmp(gGovUISelectedStatus[playerid],"RETURNED"))
        {
            if(!response)return GovBond_UI_LoadIssues(playerid,gGovUIPage[playerid]);
            if(gGovUIReviewCreator[playerid]!=gDBPrimaryAccountId[playerid])
                return 1;
            gGovUIEditing[playerid]=true;
            gGovUIStage[playerid]=9;
            ShowPlayerDialog(playerid,GOVUI_TYPE,DIALOG_STYLE_LIST,
                "{E8EDF3}Минфин | Тип доработанного займа",
                "Общий бюджетный заём\nЦелевое финансирование\nАнтикризисный выпуск",
                "Выбрать","Назад");
            return 1;
        }
        if(!strcmp(gGovUISelectedStatus[playerid],"DRAFT") &&
            GovBond_UI_CanApprove(playerid) &&
            Leader_FinanceAuthorized(playerid,3))
        {
            if(response)
            {
                // "Подписать" acts immediately: one confirmation, no extra command.
                gGovUIAction[playerid]=2;
                gGovUIStage[playerid]=0;
                return GovBond_UI_ExecuteAction(playerid);
            }
            // "Отклонить" lets leadership reject finally or return to author.
            gGovUIStage[playerid]=12;
            ShowPlayerDialog(playerid,GOVUI_ACTION,DIALOG_STYLE_LIST,
                "{E8EDF3}Минфин | Решение по заявке",
                "{E8EDF3}Отклонить окончательно (с причиной)\n{E8EDF3}Вернуть автору на доработку (с причиной)",
                "Выбрать","Назад");
            return 1;
        }
        if(!response || !GovBond_UI_CanApprove(playerid) ||
            !strcmp(gGovUISelectedStatus[playerid],"REJECTED") ||
            !strcmp(gGovUISelectedStatus[playerid],"CANCELLED"))
            return GovBond_UI_LoadIssues(playerid,gGovUIPage[playerid]);
        return GovBond_UI_Actions(playerid);
    }
    if(dialogid==GOVUI_ACTION)
    {
        if(gGovUIStage[playerid]==12)
        {
            if(!response)return GovBond_UI_LoadIssue(playerid,gGovUISelectedIssue[playerid]);
            if(!Leader_FinanceAuthorized(playerid,3) ||
                strcmp(gGovUISelectedStatus[playerid],"DRAFT"))return 1;
            if(listitem==0)return GovBond_UI_RejectReason(playerid);
            if(listitem==1)return GovBond_UI_ReturnReason(playerid);
            return 1;
        }
        if(gGovUIStage[playerid]!=7)return 1;
        if(!response)return GovBond_UI_LoadIssues(playerid,gGovUIPage[playerid]);
        if(listitem>=0 && listitem<=2)
            return GovBond_UI_ConfirmAction(playerid,listitem+2);
        return GovBond_UI_LoadIssues(playerid,gGovUIPage[playerid]);
    }
    if(dialogid==GOVUI_APPROVAL_REASON)
    {
        if(gGovUIStage[playerid]!=13)return 1;
        return GovBond_UI_LoadIssues(playerid,gGovUIPage[playerid]);
    }
    if(dialogid==GOVUI_REPORT)
    {
        if(gGovUIStage[playerid]==4 || gGovUIStage[playerid]==10)
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
