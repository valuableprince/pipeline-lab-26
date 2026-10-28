#if defined _tarkenso_bonds_ui_included
    #endinput
#endif
#define _tarkenso_bonds_ui_included

// Citizen-facing dialog UI. Every financial amount and ownership is rechecked
// in InnoDB by GOV-03, never trusted from this cached dialog.
#define BONDS_DLG_MAIN       (6700)
#define BONDS_DLG_OFFERS     (6701)
#define BONDS_DLG_OFFER      (6702)
#define BONDS_DLG_CONFIRM    (6703)
#define BONDS_DLG_PORTFOLIO  (6704)
#define BONDS_DLG_POSITION   (6705)
#define BONDS_DLG_COLLECT    (6706)
#define BONDS_UI_PAGE_SIZE   (10)
#define BONDS_UI_PAGE_LIMIT  (1000)
#define BONDS_UI_NONE        (0)
#define BONDS_UI_OFFERS      (1)
#define BONDS_UI_PORTFOLIO   (2)
#define BONDS_UI_OFFER       (3)
#define BONDS_UI_POSITION    (4)
#define BONDS_UI_PENDING_BUY (5)
#define BONDS_UI_PENDING_PAY (6)

new gBondsUIStage[MAX_PLAYERS];
new bool:gBondsUILoading[MAX_PLAYERS];
new gBondsUIQuerySeq[MAX_PLAYERS];
new gBondsUIPage[MAX_PLAYERS];
new gBondsUIListCount[MAX_PLAYERS];
new bool:gBondsUIHasMore[MAX_PLAYERS];
new gBondsUIRowIds[MAX_PLAYERS][BONDS_UI_PAGE_SIZE];
new gBondsUISelectedIssue[MAX_PLAYERS];
new gBondsUISelectedCode[MAX_PLAYERS][65];
new gBondsUIPrice[MAX_PLAYERS];
new gBondsUIAvailable[MAX_PLAYERS];
new gBondsUIClaimId[MAX_PLAYERS];
new gBondsUIClaimAmount[MAX_PLAYERS];
new gBondsUIPendingAction[MAX_PLAYERS];
new gBondsUIEventSerial;

forward BondsUI_OnList(playerid, authToken, accountId, requestSeq, kind);
forward BondsUI_OnDetail(playerid, authToken, accountId, requestSeq, kind);
forward BondsUI_OnSettlement(playerid, bool:replayed);
forward BondsUI_OnRejected(playerid);

stock BondsUI_Reset(playerid)
{
    gBondsUIStage[playerid] = BONDS_UI_NONE;
    gBondsUILoading[playerid] = false;
    gBondsUIQuerySeq[playerid]++;
    gBondsUIPage[playerid] = 0;
    gBondsUIListCount[playerid] = 0;
    gBondsUIHasMore[playerid] = false;
    gBondsUISelectedIssue[playerid] = 0;
    gBondsUISelectedCode[playerid][0] = EOS;
    gBondsUIPrice[playerid] = 0;
    gBondsUIAvailable[playerid] = 0;
    gBondsUIClaimId[playerid] = 0;
    gBondsUIClaimAmount[playerid] = 0;
    gBondsUIPendingAction[playerid] = 0;
    return 1;
}

stock bool:BondsUI_Authorized(playerid)
{
#if defined TARKENSO_DB_PRIMARY
    return playerid >= 0 && playerid < MAX_PLAYERS &&
        IsPlayerConnected(playerid) && !IsPlayerNPC(playerid) &&
        gAuthState[playerid] == AUTH_READY &&
        gDBPrimaryLoaded[playerid] &&
        gDBPrimaryAccountId[playerid] > 0 &&
        BondSettlement_IsReady() && !gGovTransferBusy[playerid] &&
        !gBondSettlementBusy[playerid];
#else
    #pragma unused playerid
    return false;
#endif
}

stock BondsUI_Main(playerid)
{
    if (!BondsUI_Authorized(playerid))
    {
        SendClientMessage(playerid, COLOR_RED,
            "Облигации временно недоступны. Требуется авторизация и MySQL.");
        return 1;
    }
    if (gBondsUILoading[playerid])
    {
        SendClientMessage(playerid, COLOR_AMBER,
            "Финансовый реестр загружается. Повторите действие позднее.");
        return 1;
    }
    gBondsUIStage[playerid] = BONDS_UI_NONE;
    ShowPlayerDialog(playerid, BONDS_DLG_MAIN, DIALOG_STYLE_LIST,
        "{D9B872}TARKENSO {98A6B9}| {F2F4F8}Государственные облигации",
        "{E9C783}Государственные займы {AAB5C4}— купить облигацию\n{79D5AD}Мой инвестиционный портфель\n{AAB5C4}Условия и инвестиционные риски",
        "Выбрать", "Закрыть");
    return 1;
}

stock bool:BondsUI_Query(playerid, const query[], kind, bool:detail = false)
{
#if defined TARKENSO_DB_PRIMARY
    if (!BondsUI_Authorized(playerid) || gBondsUILoading[playerid]) return false;
    gBondsUILoading[playerid] = true;
    gBondsUIQuerySeq[playerid]++;
    new bool:ok;
    if (detail)
        ok = mysql_tquery(gDBPrimary, query, "BondsUI_OnDetail", "iiiii",
            playerid, gAuthToken[playerid], gDBPrimaryAccountId[playerid],
            gBondsUIQuerySeq[playerid], kind) != 0;
    else
        ok = mysql_tquery(gDBPrimary, query, "BondsUI_OnList", "iiiii",
            playerid, gAuthToken[playerid], gDBPrimaryAccountId[playerid],
            gBondsUIQuerySeq[playerid], kind) != 0;
    if (!ok) gBondsUILoading[playerid] = false;
    return ok;
#else
    #pragma unused playerid, query, kind, detail
    return false;
#endif
}

stock bool:BondsUI_CallbackValid(playerid, authToken, accountId, requestSeq)
{
#if defined TARKENSO_DB_PRIMARY
    return BondsUI_Authorized(playerid) && gBondsUILoading[playerid] &&
        gAuthToken[playerid] == authToken &&
        gDBPrimaryAccountId[playerid] == accountId &&
        gBondsUIQuerySeq[playerid] == requestSeq;
#else
    #pragma unused playerid, authToken, accountId, requestSeq
    return false;
#endif
}

stock BondsUI_LoadList(playerid, kind, page = 0)
{
#if defined TARKENSO_DB_PRIMARY
    if (!BondsUI_Authorized(playerid) || gBondsUILoading[playerid]) return 1;
    if (page < 0 || page > BONDS_UI_PAGE_LIMIT) return BondsUI_Main(playerid);
    new query[800], offset = page * BONDS_UI_PAGE_SIZE;
    if (kind == BONDS_UI_OFFERS)
    {
        mysql_format(gDBPrimary, query, sizeof(query),
            "SELECT issue_id,issue_code,face_value,max_quantity-sold_quantity AS remaining,yield_basis_points FROM government_bond_issues WHERE status='OFFERING' AND offer_start_at<=UTC_TIMESTAMP() AND offer_end_at>UTC_TIMESTAMP() AND sold_quantity<max_quantity ORDER BY offer_end_at,issue_id LIMIT 11 OFFSET %d",
            offset);
    }
    else if (kind == BONDS_UI_PORTFOLIO)
    {
        mysql_format(gDBPrimary, query, sizeof(query),
            "SELECT i.issue_id,i.issue_code,h.quantity,i.face_value,i.debt_status,COALESCE(c.principal_budget_awarded+c.interest_budget_awarded+c.insurance_awarded-c.amount_collected,0) AS available FROM government_bond_holdings h INNER JOIN government_bond_issues i ON i.issue_id=h.issue_id LEFT JOIN government_bond_claims c ON c.issue_id=i.issue_id AND c.account_id=h.account_id WHERE h.account_id=%d ORDER BY i.issue_id DESC LIMIT 11 OFFSET %d",
            gDBPrimaryAccountId[playerid], offset);
    }
    else return 1;

    gBondsUIStage[playerid] = kind;
    gBondsUIPage[playerid] = page;
    gBondsUIListCount[playerid] = 0;
    if (!BondsUI_Query(playerid,query,kind))
        SendClientMessage(playerid,COLOR_RED,"Не удалось открыть финансовый реестр.");
#else
    #pragma unused playerid,kind,page
#endif
    return 1;
}

public BondsUI_OnList(playerid,authToken,accountId,requestSeq,kind)
{
#if defined TARKENSO_DB_PRIMARY
    if (!BondsUI_CallbackValid(playerid,authToken,accountId,requestSeq)) return 1;
    gBondsUILoading[playerid] = false;
    new rows=0, body[3500],line[260],code[65],face,count,remain,rate,available,id;
    new title[140];
    if (mysql_errno(gDBPrimary)!=0 || !cache_get_row_count(rows) ||
        rows < 0 || rows > BONDS_UI_PAGE_SIZE+1)
    {
        SendClientMessage(playerid,COLOR_RED,"Ошибка чтения реестра облигаций.");
        return 1;
    }
    // TABLIST_HEADERS: first row contains four columns and is NOT selectable.
    // The native client removes the header from listitem indexing.
    if (kind==BONDS_UI_OFFERS)
        format(body,sizeof(body),
            "{DDBB77}Выпуск\tНоминал\tДоходность\tДоступно\n");
    else
        format(body,sizeof(body),
            "{DDBB77}Выпуск\tКоличество\tНоминал\tК выплате\n");
    new shown = rows > BONDS_UI_PAGE_SIZE ? BONDS_UI_PAGE_SIZE : rows;
    gBondsUIListCount[playerid] = shown;
    gBondsUIHasMore[playerid] = rows > BONDS_UI_PAGE_SIZE;
    for(new i=0;i<shown;i++)
    {
        if (!cache_get_value_name_int(i,"issue_id",id) ||
            !cache_get_value_name(i,"issue_code",code,sizeof(code)) ||
            !cache_get_value_name_int(i,"face_value",face) || id<=0)
        {
            gBondsUIListCount[playerid]=0;
            SendClientMessage(playerid,COLOR_RED,"Неверные данные финансового реестра.");
            return 1;
        }
        gBondsUIRowIds[playerid][i]=id;
        if (kind==BONDS_UI_OFFERS)
        {
            if (!cache_get_value_name_int(i,"remaining",remain) ||
                !cache_get_value_name_int(i,"yield_basis_points",rate)) return 1;
            // Four actual TABLIST cells; per-cell colors prevent text bleed.
            format(line,sizeof(line),
                "{F1F4F8}%s\t{E9C67E}$%d\t{78D8AA}%d.%02d%%\t{B8C6D7}%d шт.\n",
                code,face,rate/100,rate%100,remain);
        }
        else
        {
            if (!cache_get_value_name_int(i,"quantity",count) ||
                !cache_get_value_name_int(i,"available",available)) return 1;
            if (available > 0)
                format(line,sizeof(line),
                    "{F1F4F8}%s\t{B8C6D7}%d шт.\t{E9C67E}$%d\t{78D8AA}$%d\n",
                    code,count,face,available);
            else
                format(line,sizeof(line),
                    "{F1F4F8}%s\t{B8C6D7}%d шт.\t{E9C67E}$%d\t{8795A5}$0\n",
                    code,count,face);
        }
        strcat(body,line,sizeof(body));
    }
    // Empty-state row is selectable in the client, but never maps to an issue.
    if (shown==0)
    {
        if (kind==BONDS_UI_OFFERS)
            strcat(body,
                "{92A0AF}Сейчас нет открытых выпусков\t—\t—\t—\n",
                sizeof(body));
        else
            strcat(body,
                "{92A0AF}В вашем портфеле пока нет облигаций\t—\t—\t—\n",
                sizeof(body));
    }
    if (gBondsUIHasMore[playerid])
        strcat(body,"{E6C37F}» Следующая страница\t\t\t\n",sizeof(body));
    if (gBondsUIPage[playerid]>0)
        strcat(body,"{E6C37F}« Предыдущая страница\t\t\t\n",sizeof(body));
    strcat(body,"{BAC6D5}‹ Вернуться в меню\t\t\t\n",sizeof(body));

    if(kind==BONDS_UI_OFFERS)
    {
        format(title,sizeof(title),
            "{D9B872}TARKENSO {A0ADBD}| {F1F4F8}Госзаймы {92A0AF}[стр. %d]",
            gBondsUIPage[playerid]+1);
        ShowPlayerDialog(playerid,BONDS_DLG_OFFERS,DIALOG_STYLE_TABLIST_HEADERS,
            title,body,"Выбрать","Закрыть");
    }
    else
    {
        format(title,sizeof(title),
            "{D9B872}TARKENSO {A0ADBD}| {F1F4F8}Мой портфель {92A0AF}[стр. %d]",
            gBondsUIPage[playerid]+1);
        ShowPlayerDialog(playerid,BONDS_DLG_PORTFOLIO,DIALOG_STYLE_TABLIST_HEADERS,
            title,body,"Выбрать","Закрыть");
    }
#else
    #pragma unused playerid,authToken,accountId,requestSeq,kind
#endif
    return 1;
}

stock BondsUI_LoadDetail(playerid, kind, issueId)
{
#if defined TARKENSO_DB_PRIMARY
    if (!BondsUI_Authorized(playerid) || issueId<=0 || gBondsUILoading[playerid])
        return 1;
    new query[850];
    if (kind==BONDS_UI_OFFER)
    {
        mysql_format(gDBPrimary,query,sizeof(query),
            "SELECT issue_id,issue_code,title,face_value,yield_basis_points,coverage_basis_points,premium_basis_points,maturity_at,max_quantity-sold_quantity AS remaining FROM government_bond_issues WHERE issue_id=%d AND status='OFFERING' AND offer_start_at<=UTC_TIMESTAMP() AND offer_end_at>UTC_TIMESTAMP() AND sold_quantity<max_quantity LIMIT 1",
            issueId);
    }
    else if (kind==BONDS_UI_POSITION)
    {
        mysql_format(gDBPrimary,query,sizeof(query),
            "SELECT i.issue_id,i.issue_code,i.face_value,i.yield_basis_points,i.debt_status,h.quantity,COALESCE(c.claim_id,0) AS claim_id,COALESCE(c.principal_budget_awarded+c.interest_budget_awarded+c.insurance_awarded-c.amount_collected,0) AS available,COALESCE(c.principal_due-c.principal_budget_awarded-c.insurance_awarded,0) AS principal_remaining FROM government_bond_holdings h INNER JOIN government_bond_issues i ON i.issue_id=h.issue_id LEFT JOIN government_bond_claims c ON c.issue_id=i.issue_id AND c.account_id=h.account_id WHERE i.issue_id=%d AND h.account_id=%d LIMIT 1",
            issueId,gDBPrimaryAccountId[playerid]);
    }
    else return 1;
    gBondsUISelectedIssue[playerid] = issueId;
    gBondsUIStage[playerid] = kind;
    if (!BondsUI_Query(playerid,query,kind,true))
        SendClientMessage(playerid,COLOR_RED,"Не удалось загрузить выпуск.");
#else
    #pragma unused playerid,kind,issueId
#endif
    return 1;
}

public BondsUI_OnDetail(playerid,authToken,accountId,requestSeq,kind)
{
#if defined TARKENSO_DB_PRIMARY
    if (!BondsUI_CallbackValid(playerid,authToken,accountId,requestSeq)) return 1;
    gBondsUILoading[playerid] = false;
    new rows=0,id,face,rate,code[65],title[129],body[900];
    if (mysql_errno(gDBPrimary)!=0 || !cache_get_row_count(rows) ||
        rows!=1 || !cache_get_value_name_int(0,"issue_id",id) ||
        !cache_get_value_name(0,"issue_code",code,sizeof(code)) ||
        !cache_get_value_name_int(0,"face_value",face) ||
        !cache_get_value_name_int(0,"yield_basis_points",rate) ||
        id!=gBondsUISelectedIssue[playerid])
    {
        SendClientMessage(playerid,COLOR_RED,
            "Выпуск больше недоступен. Обновите список.");
        return 1;
    }
    format(gBondsUISelectedCode[playerid],65,"%s",code);
    gBondsUIPrice[playerid]=face;
    if (kind==BONDS_UI_OFFER)
    {
        new covered, premium, remain, maturity[32];
        if (!cache_get_value_name(0,"title",title,sizeof(title)) ||
            !cache_get_value_name_int(0,"coverage_basis_points",covered) ||
            !cache_get_value_name_int(0,"premium_basis_points",premium) ||
            !cache_get_value_name_int(0,"remaining",remain) ||
            !cache_get_value_name(0,"maturity_at",maturity,sizeof(maturity)))
            return 1;
        if (remain<1 || face<1 || covered<0 || covered>10000)
        {
            SendClientMessage(playerid,COLOR_RED,"Выпуск больше недоступен.");
            return 1;
        }
        // For one bond the payout calculation is deterministic and within 1e9.
        new interest = (face / 10000)*rate+((face%10000)*rate)/10000;
        format(body,sizeof(body),
            "{DDBB77}ГОСУДАРСТВЕННЫЙ ЗАЁМ\n{F1F4F8}%s\n{AAB8C8}%s\n\n{B8C6D7}Номинал: {E9C67E}$%d\n{B8C6D7}Доход за срок: {78D8AA}%d bp (+$%d)\n{B8C6D7}Погашение (UTC): {F1F4F8}%s\n{B8C6D7}Страхование номинала: {78D8AA}%d bp\n{B8C6D7}Страховой взнос: {DDBB77}%d bp {92A0AF}(включён в цену)\n{B8C6D7}Доступно облигаций: {F1F4F8}%d\n\n{E9C67E}Купить 1 облигацию за $%d?\n{E48A8A}Риск дефолта: доход не застрахован.",
            code,title,face,rate,interest,maturity,covered,premium,remain,face);
        gBondsUIAvailable[playerid]=remain;
        gBondsUIStage[playerid]=BONDS_UI_OFFER;
        ShowPlayerDialog(playerid,BONDS_DLG_OFFER,DIALOG_STYLE_MSGBOX,
            "{D9B872}TARKENSO {A0ADBD}| {F1F4F8}Условия займа",body,"Купить 1","Назад");
    }
    else
    {
        new qty,claimId,available,remaining,debt[24];
        if (!cache_get_value_name_int(0,"quantity",qty) ||
            !cache_get_value_name_int(0,"claim_id",claimId) ||
            !cache_get_value_name_int(0,"available",available) ||
            !cache_get_value_name_int(0,"principal_remaining",remaining) ||
            !cache_get_value_name(0,"debt_status",debt,sizeof(debt)))
            return 1;
        gBondsUIClaimId[playerid]=claimId;
        // Partial collection avoids wallet cap overflow.
        new walletFree=ACCOUNT_MAX_MONEY-gAccountMoney[playerid];
        gBondsUIClaimAmount[playerid]=available>walletFree?walletFree:available;
        if (gBondsUIClaimAmount[playerid]<0)gBondsUIClaimAmount[playerid]=0;
        new debtColor[9];
        if (!strcmp(debt,"DEFAULTED",true) || !strcmp(debt,"OVERDUE",true))
            format(debtColor,sizeof(debtColor),"{E48A8A}");
        else if (!strcmp(debt,"SETTLED",true))
            format(debtColor,sizeof(debtColor),"{78D8AA}");
        else
            format(debtColor,sizeof(debtColor),"{E9C67E}");
        format(body,sizeof(body),
            "{DDBB77}ИНВЕСТИЦИОННЫЙ ПОРТФЕЛЬ\n{F1F4F8}%s\n\n{B8C6D7}Облигации: {F1F4F8}%d шт.\n{B8C6D7}Номинал одной: {E9C67E}$%d\n{B8C6D7}Доход за срок: {78D8AA}%d bp\n{B8C6D7}Статус долга: %s%s\n{B8C6D7}Непогашенный номинал: {E48A8A}$%d\n\n{B8C6D7}Начислено к получению: {78D8AA}$%d\n{B8C6D7}Можно получить сейчас: {78D8AA}$%d\n\n{92A0AF}Выплаты по оставшейся части долга могут задерживаться.",
            code,qty,face,rate,debtColor,debt,remaining,available,gBondsUIClaimAmount[playerid]);
        gBondsUIStage[playerid]=BONDS_UI_POSITION;
        ShowPlayerDialog(playerid,BONDS_DLG_POSITION,DIALOG_STYLE_MSGBOX,
            "{D9B872}TARKENSO {A0ADBD}| {F1F4F8}Позиция портфеля",body,
            available>0?"Получить":"Обновить","Назад");
    }
#else
    #pragma unused playerid,authToken,accountId,requestSeq,kind
#endif
    return 1;
}

stock BondsUI_StartPurchase(playerid)
{
    if (gBondsUIStage[playerid]!=BONDS_UI_OFFER ||
        gBondsUISelectedIssue[playerid]<=0 ||
        !gBondsUISelectedCode[playerid][0] ||
        gBondsUIPrice[playerid]<1 || gBondsUIAvailable[playerid]<1 ||
        !BondsUI_Authorized(playerid))
        return 1;
    new body[400];
    format(body,sizeof(body),
        "{DDBB77}ПОДТВЕРЖДЕНИЕ ПОКУПКИ\n\n{F1F4F8}Выпуск: %s\n{B8C6D7}Количество: {F1F4F8}1 облигация\n{B8C6D7}Стоимость: {E9C67E}$%d\n{B8C6D7}На кошельке: {F1F4F8}$%d\n\n{92A0AF}Списание произойдёт после подтверждения MySQL.\n\n{78D8AA}Подтвердить покупку?",
        gBondsUISelectedCode[playerid],gBondsUIPrice[playerid],gAccountMoney[playerid]);
    gBondsUIStage[playerid]=BONDS_UI_PENDING_BUY;
    ShowPlayerDialog(playerid,BONDS_DLG_CONFIRM,DIALOG_STYLE_MSGBOX,
        "{D9B872}TARKENSO {A0ADBD}| {F1F4F8}Покупка облигации",body,"Подтвердить","Отмена");
    return 1;
}

stock BondsUI_NewKey(playerid, output[], size)
{
    // Key is generated SERVER-SIDE; retry uses the same event key at SQL layer.
    // Account + seconds + session token + counter + random suffix across restarts.
    gBondsUIEventSerial++;
    format(output,size,"bond:ui:%d:%d:%d:%d:%d",
        gDBPrimaryAccountId[playerid],gettime(),gAuthToken[playerid],
        gBondsUIEventSerial,random(1000000000));
    return 1;
}

stock BondsUI_StartCollect(playerid)
{
    if(gBondsUIStage[playerid]!=BONDS_UI_POSITION || !BondsUI_Authorized(playerid))
        return 1;
    if(gBondsUIClaimId[playerid]<=0 || gBondsUIClaimAmount[playerid]<=0)
    {
        SendClientMessage(playerid,COLOR_AMBER,
            "Доступных выплат нет либо кошелёк достиг лимита.");
        return BondsUI_LoadList(playerid,BONDS_UI_PORTFOLIO,gBondsUIPage[playerid]);
    }
    new text[280];
    format(text,sizeof(text),
        "{DDBB77}ПОЛУЧЕНИЕ ВЫПЛАТЫ\n\n{B8C6D7}Сумма: {78D8AA}$%d\n{B8C6D7}Выпуск: {F1F4F8}%s\n\n{92A0AF}Деньги будут зачислены после подтверждения MySQL.",
        gBondsUIClaimAmount[playerid],gBondsUISelectedCode[playerid]);
    ShowPlayerDialog(playerid,BONDS_DLG_COLLECT,DIALOG_STYLE_MSGBOX,
        "{D9B872}TARKENSO {A0ADBD}| {F1F4F8}Получение выплаты",text,"Получить","Назад");
    return 1;
}

stock BondsUI_OnDialog(playerid,dialogid,response,listitem)
{
    if(dialogid<BONDS_DLG_MAIN || dialogid>BONDS_DLG_COLLECT) return 0;
    if(!IsPlayerConnected(playerid) || gAuthState[playerid]!=AUTH_READY) return 1;
    if(gBondsUILoading[playerid] || gGovTransferBusy[playerid]) return 1;
    if(dialogid==BONDS_DLG_MAIN)
    {
        if(!response || listitem<0 || listitem>2)return 1;
        switch(listitem)
        {
            case 0:return BondsUI_LoadList(playerid,BONDS_UI_OFFERS);
            case 1:return BondsUI_LoadList(playerid,BONDS_UI_PORTFOLIO);
            case 2:
            {
                ShowPlayerDialog(playerid,BONDS_DLG_OFFER,DIALOG_STYLE_MSGBOX,
                    "{D9B872}TARKENSO {A0ADBD}| {F1F4F8}Риски инвестиций",
                    "{E48A8A}ОБЛИГАЦИИ НЕ ЯВЛЯЮТСЯ ВКЛАДОМ\n\n{B8C6D7}Гарантия покрывает только установленную часть невыплаченного номинала (обычно 20%).\n\n{DDBB77}Проценты не застрахованы.\n{B8C6D7}Доходность указана за полный срок выпуска.\n{B8C6D7}При дефолте оставшийся долг может быть выплачен с задержкой.",
                    "Назад","Закрыть");
                gBondsUIStage[playerid]=BONDS_UI_NONE;
                return 1;
            }
        }
    }
    if(dialogid==BONDS_DLG_OFFERS || dialogid==BONDS_DLG_PORTFOLIO)
    {
        new kind=dialogid==BONDS_DLG_OFFERS?BONDS_UI_OFFERS:BONDS_UI_PORTFOLIO;
        if(gBondsUIStage[playerid]!=kind)return 1;
        if(!response)return 1;
        if(listitem>=0 && listitem<gBondsUIListCount[playerid])
        {
            new issueId=gBondsUIRowIds[playerid][listitem];
            return BondsUI_LoadDetail(playerid,
                kind==BONDS_UI_OFFERS?BONDS_UI_OFFER:BONDS_UI_POSITION,issueId);
        }
        // A placeholder occupies listitem=0 when there are no records.
        // Headers are not counted in listitem by TABLIST_HEADERS.
        new index=gBondsUIListCount[playerid];
        if(index==0)
        {
            if(listitem==0)return 1; // empty state is informational
            index=1;
        }
        if(gBondsUIHasMore[playerid])
        {
            if(listitem==index)
                return BondsUI_LoadList(playerid,kind,gBondsUIPage[playerid]+1);
            index++;
        }
        if(gBondsUIPage[playerid]>0)
        {
            if(listitem==index)
                return BondsUI_LoadList(playerid,kind,gBondsUIPage[playerid]-1);
            index++;
        }
        if(listitem==index)return BondsUI_Main(playerid);
        return 1;
    }
    if(dialogid==BONDS_DLG_OFFER)
    {
        if(gBondsUIStage[playerid]==BONDS_UI_NONE)return BondsUI_Main(playerid);
        if(gBondsUIStage[playerid]!=BONDS_UI_OFFER)return 1;
        if(!response)return BondsUI_LoadList(playerid,BONDS_UI_OFFERS,gBondsUIPage[playerid]);
        return BondsUI_StartPurchase(playerid);
    }
    if(dialogid==BONDS_DLG_CONFIRM)
    {
        if(gBondsUIStage[playerid]!=BONDS_UI_PENDING_BUY)return 1;
        if(!response)return BondsUI_LoadList(playerid,BONDS_UI_OFFERS,gBondsUIPage[playerid]);
        // Consume confirmation BEFORE queueing: forged/replayed dialog responses
        // cannot create a second event_key and purchase a second bond.
        gBondsUIStage[playerid]=BONDS_UI_NONE;
#if defined TARKENSO_DB_PRIMARY
        new eventKey[65];
        BondsUI_NewKey(playerid,eventKey,sizeof(eventKey));
        gBondsUIPendingAction[playerid]=1;
        if(!BondSettlement_Buy(playerid,eventKey,gBondsUISelectedCode[playerid],1))
        {
            gBondsUIPendingAction[playerid]=0;
            SendClientMessage(playerid,COLOR_RED,
                "Покупка не принята. Проверьте баланс, лимит или выпуск.");
        }
        else SendClientMessage(playerid,COLOR_AMBER,
            "Заявка на 1 облигацию отправлена. Ожидается подтверждение казначейства.");
#endif
        return 1;
    }
    if(dialogid==BONDS_DLG_POSITION)
    {
        if(gBondsUIStage[playerid]!=BONDS_UI_POSITION)return 1;
        if(!response)return BondsUI_LoadList(playerid,BONDS_UI_PORTFOLIO,gBondsUIPage[playerid]);
        if(gBondsUIClaimAmount[playerid]<=0)
            return BondsUI_LoadDetail(playerid,BONDS_UI_POSITION,gBondsUISelectedIssue[playerid]);
        return BondsUI_StartCollect(playerid);
    }
    if(dialogid==BONDS_DLG_COLLECT)
    {
        if(gBondsUIStage[playerid]!=BONDS_UI_POSITION)return 1;
        if(!response)return BondsUI_LoadDetail(playerid,BONDS_UI_POSITION,gBondsUISelectedIssue[playerid]);
        gBondsUIStage[playerid]=BONDS_UI_NONE;
#if defined TARKENSO_DB_PRIMARY
        new eventKey[65];
        BondsUI_NewKey(playerid,eventKey,sizeof(eventKey));
        gBondsUIPendingAction[playerid]=2;
        if(!BondSettlement_Collect(playerid,eventKey,
            gBondsUIClaimId[playerid],gBondsUIClaimAmount[playerid]))
        {
            gBondsUIPendingAction[playerid]=0;
            SendClientMessage(playerid,COLOR_RED,
                "Выплата не принята. Обновите портфель.");
        }
        else SendClientMessage(playerid,COLOR_AMBER,
            "Запрос на получение выплаты отправлен в казначейство.");
#endif
        return 1;
    }
    return 1;
}

public BondsUI_OnRejected(playerid)
{
    gBondsUIPendingAction[playerid]=0;
    if(IsPlayerConnected(playerid) && gAuthState[playerid]==AUTH_READY)
        SendClientMessage(playerid,COLOR_RED,
            "Казначейство отклонило операцию. Проверьте баланс, лимит и срок выпуска; откройте /bonds снова.");
    return 1;
}

public BondsUI_OnSettlement(playerid,bool:replayed)
{
    if(!IsPlayerConnected(playerid) || gAuthState[playerid]!=AUTH_READY)
        return 1;
    new action=gBondsUIPendingAction[playerid];
    gBondsUIPendingAction[playerid]=0;
    if(action==1)
        SendClientMessage(playerid,COLOR_AMBER,
            replayed?"Покупка уже проведена ранее.":"Покупка 1 облигации подтверждена. Проверьте портфель.");
    else if(action==2)
        SendClientMessage(playerid,COLOR_AMBER,
            replayed?"Выплата уже была перечислена.":"Выплата подтверждена и зачислена на кошелёк.");
    return 1;
}
