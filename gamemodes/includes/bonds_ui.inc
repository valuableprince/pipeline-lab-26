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
#define BONDS_DLG_QUANTITY   (6707)
#define BONDS_DLG_COUPONS    (6708)
#define BONDS_DLG_COUPON     (6709)
#define BONDS_DLG_COUPON_PAY (6710)
#define BONDS_DLG_SELL_QTY (6711)
#define BONDS_DLG_SELL_CONFIRM (6712)
#define BONDS_DLG_POSITION_ACTION (6713)
#define BONDS_UI_PAGE_SIZE   (10)
#define BONDS_UI_PAGE_LIMIT  (1000)
#define BONDS_UI_NONE        (0)
#define BONDS_UI_OFFERS      (1)
#define BONDS_UI_PORTFOLIO   (2)
#define BONDS_UI_OFFER       (3)
#define BONDS_UI_POSITION    (4)
#define BONDS_UI_PENDING_BUY (5)
#define BONDS_UI_PENDING_PAY (6)
#define BONDS_UI_QUANTITY    (7)
#define BONDS_UI_COUPONS     (8)
#define BONDS_UI_COUPON      (9)
#define BONDS_UI_PENDING_COUPON (10)
#define BONDS_UI_SELL_QTY (11)
#define BONDS_UI_SELL_CONFIRM (12)

// Localize only user-visible debt states. SQL keeps stable technical enums.
stock BondsUI_DebtStatusLabel(const status[],output[],size)
{
    if (!strcmp(status,"NOT_DUE")) format(output,size,"Срок не наступил");
    else if (!strcmp(status,"DUE")) format(output,size,"К погашению");
    else if (!strcmp(status,"PARTIALLY_PAID")) format(output,size,"Частично погашен");
    else if (!strcmp(status,"OVERDUE")) format(output,size,"Просрочен");
    else if (!strcmp(status,"DEFAULTED")) format(output,size,"Дефолт");
    else if (!strcmp(status,"RESTRUCTURED")) format(output,size,"Реструктурирован");
    else if (!strcmp(status,"SETTLED")) format(output,size,"Погашен полностью");
    else format(output,size,"Неизвестный статус");
    return 1;
}

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
new gBondsUIMaxBuy[MAX_PLAYERS];
new gBondsUIOwned[MAX_PLAYERS];
new gBondsUIHoldLimit[MAX_PLAYERS];
new gBondsUIPurchaseQty[MAX_PLAYERS];
new gBondsUIPendingQty[MAX_PLAYERS];
new gBondsUIClaimId[MAX_PLAYERS];
new gBondsUIClaimAmount[MAX_PLAYERS];
new gBondsUICouponAmount[MAX_PLAYERS];
new gBondsUISellMax[MAX_PLAYERS];
new gBondsUISellUnitPrice[MAX_PLAYERS];
new gBondsUISellQuantity[MAX_PLAYERS];
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
    gBondsUIMaxBuy[playerid] = 0;
    gBondsUIOwned[playerid] = 0;
    gBondsUIHoldLimit[playerid] = 0;
    gBondsUIPurchaseQty[playerid] = 0;
    gBondsUIPendingQty[playerid] = 0;
    gBondsUIClaimId[playerid] = 0;
    gBondsUIClaimAmount[playerid] = 0;
    gBondsUICouponAmount[playerid] = 0;
    gBondsUISellMax[playerid] = 0;
    gBondsUISellUnitPrice[playerid] = 0;
    gBondsUISellQuantity[playerid] = 0;
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
        "{E8EDF3}TARKENSO {E8EDF3}| {E8EDF3}Государственные облигации",
        "{E8EDF3}Государственные займы {E8EDF3}— купить облигацию\n{E8EDF3}Мой инвестиционный портфель\n{E8EDF3}Купонный счёт\n{E8EDF3}Условия и инвестиционные риски\n{E8EDF3}Публичная отчётность казны",
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
    new query[1150], offset = page * BONDS_UI_PAGE_SIZE;
    if (kind == BONDS_UI_OFFERS)
    {
        mysql_format(gDBPrimary, query, sizeof(query),
            "SELECT issue_id,issue_code,face_value,max_quantity-sold_quantity AS remaining,yield_basis_points FROM government_bond_issues WHERE status='OFFERING' AND offer_start_at<=UTC_TIMESTAMP() AND offer_end_at>UTC_TIMESTAMP() AND sold_quantity<max_quantity ORDER BY offer_end_at,issue_id LIMIT 11 OFFSET %d",
            offset);
    }
    else if (kind == BONDS_UI_PORTFOLIO)
    {
        mysql_format(gDBPrimary, query, sizeof(query),
            "SELECT i.issue_id,i.issue_code,COALESCE(h.quantity,0) AS quantity,i.face_value,i.debt_status,COALESCE(c.principal_budget_awarded+c.interest_budget_awarded+c.insurance_awarded-c.amount_collected,0) AS available FROM (SELECT issue_id FROM government_bond_holdings WHERE account_id=%d UNION SELECT issue_id FROM government_bond_claims WHERE account_id=%d) own JOIN government_bond_issues i ON i.issue_id=own.issue_id LEFT JOIN government_bond_holdings h ON h.issue_id=i.issue_id AND h.account_id=%d LEFT JOIN government_bond_claims c ON c.issue_id=i.issue_id AND c.account_id=%d ORDER BY i.issue_id DESC LIMIT 11 OFFSET %d",
            gDBPrimaryAccountId[playerid],gDBPrimaryAccountId[playerid],gDBPrimaryAccountId[playerid],gDBPrimaryAccountId[playerid], offset);
    }
    else if(kind==BONDS_UI_COUPONS)
    {
        if(!BondCoupon_IsReady())return 1;
        mysql_format(gDBPrimary,query,sizeof(query),
            "SELECT w.issue_id,i.issue_code,i.face_value,w.accrued,w.collected,w.converted,w.accrued-w.collected-w.converted AS available FROM government_bond_coupon_wallets w JOIN government_bond_issues i ON i.issue_id=w.issue_id WHERE w.account_id=%d ORDER BY w.issue_id DESC LIMIT 11 OFFSET %d",
            gDBPrimaryAccountId[playerid],offset);
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
            "{E8EDF3}Выпуск\tНоминал\tДоходность\tДоступно\n");
    else if(kind==BONDS_UI_COUPONS)
        format(body,sizeof(body),
            "{E8EDF3}Выпуск\tНачислено\tПолучено\tДоступно\n");
    else
        format(body,sizeof(body),
            "{E8EDF3}Выпуск\tКоличество\tНоминал\tК выплате\n");
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
                "{E8EDF3}%s\t{E8EDF3}$%d\t{E8EDF3}%d.%02d%%\t{E8EDF3}%d шт.\n",
                code,face,rate/100,rate%100,remain);
        }
        else if(kind==BONDS_UI_COUPONS)
        {
            new accrued,collected,converted;
            if(!cache_get_value_name_int(i,"accrued",accrued) ||
                !cache_get_value_name_int(i,"collected",collected) ||
                !cache_get_value_name_int(i,"converted",converted) ||
                !cache_get_value_name_int(i,"available",available)) return 1;
            format(line,sizeof(line),
                "{E8EDF3}%s\t{E8EDF3}$%d\t{E8EDF3}$%d\t{E8EDF3}$%d\n",
                code,accrued,collected,available);
        }
        else
        {
            if (!cache_get_value_name_int(i,"quantity",count) ||
                !cache_get_value_name_int(i,"available",available)) return 1;
            if (available > 0)
                format(line,sizeof(line),
                    "{E8EDF3}%s\t{E8EDF3}%d шт.\t{E8EDF3}$%d\t{E8EDF3}$%d\n",
                    code,count,face,available);
            else
                format(line,sizeof(line),
                    "{E8EDF3}%s\t{E8EDF3}%d шт.\t{E8EDF3}$%d\t{E8EDF3}$0\n",
                    code,count,face);
        }
        strcat(body,line,sizeof(body));
    }
    // Empty-state row is selectable in the client, but never maps to an issue.
    if (shown==0)
    {
        if (kind==BONDS_UI_OFFERS)
            strcat(body,
                "{E8EDF3}Сейчас нет открытых выпусков\t—\t—\t—\n",
                sizeof(body));
        else if(kind==BONDS_UI_COUPONS)
            strcat(body,"{E8EDF3}Купоны пока не начислены\t—\t—\t—\n",
                sizeof(body));
        else
            strcat(body,
                "{E8EDF3}В вашем портфеле пока нет облигаций\t—\t—\t—\n",
                sizeof(body));
    }
    if (gBondsUIHasMore[playerid])
        strcat(body,"{E8EDF3}» Следующая страница\t\t\t\n",sizeof(body));
    if (gBondsUIPage[playerid]>0)
        strcat(body,"{E8EDF3}« Предыдущая страница\t\t\t\n",sizeof(body));
    strcat(body,"{E8EDF3}‹ Вернуться в меню\t\t\t\n",sizeof(body));

    if(kind==BONDS_UI_OFFERS)
    {
        format(title,sizeof(title),
            "{E8EDF3}TARKENSO {E8EDF3}| {E8EDF3}Госзаймы {E8EDF3}[стр. %d]",
            gBondsUIPage[playerid]+1);
        ShowPlayerDialog(playerid,BONDS_DLG_OFFERS,DIALOG_STYLE_TABLIST_HEADERS,
            title,body,"Выбрать","Закрыть");
    }
    else if(kind==BONDS_UI_COUPONS)
    {
        format(title,sizeof(title),
            "{E8EDF3}TARKENSO | Купонный счёт [стр. %d]",
            gBondsUIPage[playerid]+1);
        ShowPlayerDialog(playerid,BONDS_DLG_COUPONS,DIALOG_STYLE_TABLIST_HEADERS,
            title,body,"Открыть","Назад");
    }
    else
    {
        format(title,sizeof(title),
            "{E8EDF3}TARKENSO {E8EDF3}| {E8EDF3}Мой портфель {E8EDF3}[стр. %d]",
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
    new query[1600];
    if (kind==BONDS_UI_OFFER)
    {
        mysql_format(gDBPrimary,query,sizeof(query),
            "SELECT i.issue_id,i.issue_code,i.title,i.face_value,i.yield_basis_points,i.coverage_basis_points,i.premium_basis_points,i.maturity_at,i.max_quantity-i.sold_quantity AS remaining,COALESCE(h.quantity,0) AS owned,i.max_quantity DIV 5 AS per_account_limit,COALESCE((SELECT LEAST(max_transaction,1000000000) FROM treasury_accounts WHERE code='state'),0) AS purchase_cap,IF(i.status='OFFERING' AND i.offer_start_at<=UTC_TIMESTAMP() AND i.offer_end_at>UTC_TIMESTAMP(),1,0) AS offer_open FROM government_bond_issues i LEFT JOIN government_bond_holdings h ON h.issue_id=i.issue_id AND h.account_id=%d WHERE i.issue_id=%d LIMIT 1",
            gDBPrimaryAccountId[playerid],issueId);
    }
    else if (kind==BONDS_UI_POSITION)
    {
        mysql_format(gDBPrimary,query,sizeof(query),
            "SELECT i.issue_id,i.issue_code,i.face_value,i.yield_basis_points,i.debt_status,COALESCE(h.quantity,0) AS quantity,COALESCE(c.claim_id,0) AS claim_id,COALESCE(c.principal_budget_awarded+c.interest_budget_awarded+c.insurance_awarded-c.amount_collected,0) AS available,COALESCE(c.principal_due-c.principal_budget_awarded-c.insurance_awarded,0) AS principal_remaining,LEAST(t.max_transaction,GREATEST(0,t.balance-t.minimum_reserve-t.bond_fund_reserved-t.bond_payout_reserved)) AS treasury_free,IF(i.maturity_at>UTC_TIMESTAMP() AND i.maturity_event_key IS NULL AND i.status IN ('OFFERING','APPROVED','CLOSED') AND t.enabled=1 AND t.expense_enabled=1,1,0) AS sale_open FROM government_bond_issues i LEFT JOIN government_bond_holdings h ON h.issue_id=i.issue_id AND h.account_id=%d LEFT JOIN government_bond_claims c ON c.issue_id=i.issue_id AND c.account_id=%d CROSS JOIN treasury_accounts t WHERE t.code='state' AND i.issue_id=%d AND (h.issue_id IS NOT NULL OR c.claim_id IS NOT NULL) LIMIT 1",
            gDBPrimaryAccountId[playerid],gDBPrimaryAccountId[playerid],issueId);
    }
    else if(kind==BONDS_UI_COUPON)
    {
        if(!BondCoupon_IsReady())return 1;
        mysql_format(gDBPrimary,query,sizeof(query),
            "SELECT i.issue_id,i.issue_code,i.face_value,i.yield_basis_points,w.accrued,w.collected,w.converted,w.accrued-w.collected-w.converted AS available,LEAST(t.max_transaction,GREATEST(0,t.balance-t.minimum_reserve-t.bond_fund_reserved-t.bond_payout_reserved)) AS treasury_free,i.maturity_at FROM government_bond_coupon_wallets w JOIN government_bond_issues i ON i.issue_id=w.issue_id CROSS JOIN treasury_accounts t WHERE t.code='state' AND w.issue_id=%d AND w.account_id=%d LIMIT 1",
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
    new rows=0,id,face,rate,code[65],title[129],body[1120];
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
        new covered, premium, remain, owned,holdLimit,purchaseCap,offerOpen,maturity[32];
        if (!cache_get_value_name(0,"title",title,sizeof(title)) ||
            !cache_get_value_name_int(0,"coverage_basis_points",covered) ||
            !cache_get_value_name_int(0,"premium_basis_points",premium) ||
            !cache_get_value_name_int(0,"remaining",remain) ||
            !cache_get_value_name_int(0,"owned",owned) ||
            !cache_get_value_name_int(0,"per_account_limit",holdLimit) ||
            !cache_get_value_name_int(0,"purchase_cap",purchaseCap) ||
            !cache_get_value_name_int(0,"offer_open",offerOpen) ||
            !cache_get_value_name(0,"maturity_at",maturity,sizeof(maturity)))
            return 1;
        if (face<1 || covered<0 || covered>10000)
        {
            SendClientMessage(playerid,COLOR_RED,"Выпуск больше недоступен.");
            return 1;
        }
        // Strict 20% of EACH issue, less already owned, remaining supply,
        // available wallet and the single-transaction treasury ceiling.
        // SQL rechecks every amount and holding under InnoDB locks.
        new capacity=holdLimit-owned;
        if(capacity<0)capacity=0;
        new affordable=gAccountMoney[playerid]/face;
        if(affordable<0)affordable=0;
        new maxBuy=remain;
        if(maxBuy>capacity)maxBuy=capacity;
        if(maxBuy>affordable)maxBuy=affordable;
        if(maxBuy>purchaseCap/face)maxBuy=purchaseCap/face;
        if(maxBuy<0)maxBuy=0;
        // Explain the first current blocking condition from the SQL snapshot.
        // The finance procedure still revalidates all constraints at COMMIT.
        new reason[165];
        reason[0]=EOS;
        if(!offerOpen)
            format(reason,sizeof(reason),"Размещение закрыто либо ещё не началось.");
        else if(remain<=0)
            format(reason,sizeof(reason),"Все облигации этого выпуска распроданы.");
        else if(capacity<=0)
            format(reason,sizeof(reason),"Достигнут лимит владения: 20%% выпуска.");
        else if(affordable<=0)
            format(reason,sizeof(reason),"Недостаточно денег для покупки одной облигации.");
        else if(purchaseCap<face)
            format(reason,sizeof(reason),"Лимит одной операции казны меньше номинала.");
        if(!offerOpen)maxBuy=0;
        new interest = (face / 10000)*rate+((face%10000)*rate)/10000;
        format(body,sizeof(body),
            "{E8EDF3}ГОСУДАРСТВЕННЫЙ ЗАЁМ\n{E8EDF3}%s\n{E8EDF3}%s\n\n{E8EDF3}Номинал: {E8EDF3}$%d\n{E8EDF3}Доход за срок: {E8EDF3}%d.%02d%% (+$%d за 1 шт.)\n{E8EDF3}Погашение (UTC): {E8EDF3}%s\n{E8EDF3}Страхование номинала: {E8EDF3}%d.%02d%%\n{E8EDF3}Страховой взнос: {E8EDF3}%d.%02d%% {E8EDF3}(в цене)\n{E8EDF3}Остаток выпуска: {E8EDF3}%d шт.\n{E8EDF3}Ваш лимит (20%%): {E8EDF3}%d шт.\n{E8EDF3}Уже куплено: {E8EDF3}%d шт.\n{E8EDF3}Доступно вам для покупки: %d шт.\n\n{E8EDF3}Риск дефолта: доход не застрахован.",
            code,title,face,rate/100,rate%100,interest,maturity,
            covered/100,covered%100,premium/100,premium%100,
            remain,holdLimit,owned,maxBuy);
        if(maxBuy==0)
        {
            strcat(body,"\n\nПричина недоступности: ",sizeof(body));
            strcat(body,reason,sizeof(body));
        }
        gBondsUIAvailable[playerid]=remain;
        gBondsUIMaxBuy[playerid]=maxBuy;
        gBondsUIOwned[playerid]=owned;
        gBondsUIHoldLimit[playerid]=holdLimit;
        gBondsUIPurchaseQty[playerid]=0;
        gBondsUIStage[playerid]=BONDS_UI_OFFER;
        ShowPlayerDialog(playerid,BONDS_DLG_OFFER,DIALOG_STYLE_MSGBOX,
            "{E8EDF3}TARKENSO {E8EDF3}| {E8EDF3}Условия займа",body,
            maxBuy>0?"Купить":"Назад",maxBuy>0?"Назад":"Закрыть");
    }
    else if(kind==BONDS_UI_COUPON)
    {
        new accrued,collected,converted,available,freeTreasury,maturity[32];
        if(!cache_get_value_name_int(0,"accrued",accrued) ||
           !cache_get_value_name_int(0,"collected",collected) ||
           !cache_get_value_name_int(0,"converted",converted) ||
           !cache_get_value_name_int(0,"available",available) ||
           !cache_get_value_name_int(0,"treasury_free",freeTreasury) ||
           !cache_get_value_name(0,"maturity_at",maturity,sizeof(maturity)))
            return 1;
        new canGet=available;
        if(canGet>freeTreasury)canGet=freeTreasury;
        if(canGet>ACCOUNT_MAX_MONEY-gAccountMoney[playerid])
            canGet=ACCOUNT_MAX_MONEY-gAccountMoney[playerid];
        if(canGet<0)canGet=0;
        gBondsUICouponAmount[playerid]=canGet;
        format(body,sizeof(body),
            "{E8EDF3}КУПОННЫЙ СЧЁТ\n{E8EDF3}%s\n\n{E8EDF3}Доходность за срок: {E8EDF3}%d.%02d%%\n{E8EDF3}Начислено купонов: {E8EDF3}$%d\n{E8EDF3}Ранее получено: {E8EDF3}$%d\n{E8EDF3}Перенесено в погашение: {E8EDF3}$%d\n{E8EDF3}На условном счёте: {E8EDF3}$%d\n{E8EDF3}Доступно к переводу сейчас: {E8EDF3}$%d\n{E8EDF3}Погашение (UTC): {E8EDF3}%s\n\n{E8EDF3}Купоны начисляются каждый PayDay даже офлайн. Перевод выполняется вручную при наличии денег в казне.",
            code,rate/100,rate%100,accrued,collected,converted,available,canGet,maturity);
        gBondsUIStage[playerid]=BONDS_UI_COUPON;
        ShowPlayerDialog(playerid,BONDS_DLG_COUPON,DIALOG_STYLE_MSGBOX,
            "{E8EDF3}TARKENSO | Купонный счёт",body,
            canGet>0?"Получить":"Обновить","Назад");
    }
    else
    {
        new qty,claimId,available,remaining,debt[24],debtLabel[40];
        new treasuryFree,saleOpen;
        if (!cache_get_value_name_int(0,"quantity",qty) ||
            !cache_get_value_name_int(0,"claim_id",claimId) ||
            !cache_get_value_name_int(0,"available",available) ||
            !cache_get_value_name_int(0,"principal_remaining",remaining) ||
            !cache_get_value_name_int(0,"treasury_free",treasuryFree) ||
            !cache_get_value_name_int(0,"sale_open",saleOpen) ||
            !cache_get_value_name(0,"debt_status",debt,sizeof(debt)))
            return 1;
        gBondsUIClaimId[playerid]=claimId;
        // Partial collection avoids wallet cap overflow.
        new walletFree=ACCOUNT_MAX_MONEY-gAccountMoney[playerid];
        gBondsUIClaimAmount[playerid]=available>walletFree?walletFree:available;
        if (gBondsUIClaimAmount[playerid]<0)gBondsUIClaimAmount[playerid]=0;
        // The fixed 98% unit price is rounded DOWN once per security.
        // The SQL procedure independently recalculates this under locks.
        gBondsUISellUnitPrice[playerid]=(face/100)*98+((face%100)*98)/100;
        gBondsUISellMax[playerid]=qty;
        if(gBondsUISellUnitPrice[playerid]<1 || !gBondBuybackReady || !saleOpen)
            gBondsUISellMax[playerid]=0;
        else
        {
            new cap=treasuryFree/gBondsUISellUnitPrice[playerid];
            new walletCap=walletFree/gBondsUISellUnitPrice[playerid];
            if(gBondsUISellMax[playerid]>cap)gBondsUISellMax[playerid]=cap;
            if(gBondsUISellMax[playerid]>walletCap)gBondsUISellMax[playerid]=walletCap;
        }
        gBondsUISellQuantity[playerid]=0;
        BondsUI_DebtStatusLabel(debt,debtLabel,sizeof(debtLabel));
        new debtColor[9];
        if (!strcmp(debt,"DEFAULTED",true) || !strcmp(debt,"OVERDUE",true))
            format(debtColor,sizeof(debtColor),"{E8EDF3}");
        else if (!strcmp(debt,"SETTLED",true))
            format(debtColor,sizeof(debtColor),"{E8EDF3}");
        else
            format(debtColor,sizeof(debtColor),"{E8EDF3}");
        format(body,sizeof(body),
            "{E8EDF3}ИНВЕСТИЦИОННЫЙ ПОРТФЕЛЬ\n{E8EDF3}%s\n\n{E8EDF3}Облигации: {E8EDF3}%d шт.\n{E8EDF3}Номинал одной: {E8EDF3}$%d\n{E8EDF3}Доход за срок: {E8EDF3}%d.%02d%%\n{E8EDF3}Статус долга: %s%s\n{E8EDF3}Непогашенный номинал: {E8EDF3}$%d\n\n{E8EDF3}Начислено к получению: {E8EDF3}$%d\n{E8EDF3}Можно получить сейчас: {E8EDF3}$%d\n\n{E8EDF3}Выплаты по оставшейся части долга могут задерживаться.",
            code,qty,face,rate/100,rate%100,debtColor,debtLabel,remaining,available,gBondsUIClaimAmount[playerid]);
        gBondsUIStage[playerid]=BONDS_UI_POSITION;
        ShowPlayerDialog(playerid,BONDS_DLG_POSITION,DIALOG_STYLE_MSGBOX,
            "{E8EDF3}TARKENSO {E8EDF3}| {E8EDF3}Позиция портфеля",body,
            "Действия","Назад");
    }
#else
    #pragma unused playerid,authToken,accountId,requestSeq,kind
#endif
    return 1;
}

// GOV-09: quote is informative, and SQL is always authoritative.
stock BondsUI_ShowSellQuantity(playerid)
{
    if(gBondsUIStage[playerid]!=BONDS_UI_POSITION ||
       !BondsUI_Authorized(playerid))return 1;
    if(gBondsUISellMax[playerid]<1)
    {
        SendClientMessage(playerid,COLOR_AMBER,
            "Досрочная продажа недоступна: проверьте казну, срок займа и баланс.");
        return BondsUI_LoadDetail(playerid,BONDS_UI_POSITION,gBondsUISelectedIssue[playerid]);
    }
    new body[450];
    format(body,sizeof(body),
        "{E8EDF3}ДОСРОЧНАЯ ПРОДАЖА ГОСУДАРСТВУ\\n\\n{E8EDF3}Выпуск: %s\\n{E8EDF3}Цена за облигацию: $%d (98%% номинала)\\n{E8EDF3}Можно продать: от 1 до %d шт.\\n\\n{E8EDF3}Ранее начисленные купоны остаются вам. По проданным бумагам новые купоны не начисляются.",
        gBondsUISelectedCode[playerid],gBondsUISellUnitPrice[playerid],
        gBondsUISellMax[playerid]);
    gBondsUIStage[playerid]=BONDS_UI_SELL_QTY;
    ShowPlayerDialog(playerid,BONDS_DLG_SELL_QTY,DIALOG_STYLE_INPUT,
        "{E8EDF3}TARKENSO | Досрочный выкуп",body,"Далее","Назад");
    return 1;
}

stock BondsUI_ConfirmSell(playerid,quantity)
{
    if(gBondsUIStage[playerid]!=BONDS_UI_SELL_QTY ||
       quantity<1 || quantity>gBondsUISellMax[playerid] ||
       gBondsUISellUnitPrice[playerid]<1 ||
       gBondsUISellUnitPrice[playerid]>1000000000/quantity ||
       !BondsUI_Authorized(playerid))return 1;
    gBondsUISellQuantity[playerid]=quantity;
    new body[460],payment=quantity*gBondsUISellUnitPrice[playerid];
    format(body,sizeof(body),
        "{E8EDF3}ПОДТВЕРЖДЕНИЕ ДОСРОЧНОЙ ПРОДАЖИ\\n\\n{E8EDF3}Выпуск: %s\\n{E8EDF3}Количество: %d шт.\\n{E8EDF3}Цена одной: $%d (98%%)\\n{E8EDF3}К получению: $%d\\n\\n{E8EDF3}Государство погасит выкупленные бумаги. Будущие купоны прекратятся. Операция состоится только после проверки средств казны.",
        gBondsUISelectedCode[playerid],quantity,
        gBondsUISellUnitPrice[playerid],payment);
    gBondsUIStage[playerid]=BONDS_UI_SELL_CONFIRM;
    ShowPlayerDialog(playerid,BONDS_DLG_SELL_CONFIRM,DIALOG_STYLE_MSGBOX,
        "{E8EDF3}TARKENSO | Подтвердить продажу",body,"Продать","Назад");
    return 1;
}

// Input maximum is a snapshot, not a permission: MySQL is authoritative.
stock BondsUI_ShowQuantity(playerid)
{
    if(gBondsUIStage[playerid]!=BONDS_UI_OFFER &&
       gBondsUIStage[playerid]!=BONDS_UI_QUANTITY)return 1;
    if(!BondsUI_Authorized(playerid) || gBondsUISelectedIssue[playerid]<=0 ||
       !gBondsUISelectedCode[playerid][0])return 1;
    if(gBondsUIMaxBuy[playerid]<1)
    {
        // A stale or forged click must fetch a fresh SQL snapshot.
        return BondsUI_LoadDetail(playerid,BONDS_UI_OFFER,gBondsUISelectedIssue[playerid]);
    }
    new body[430];
    format(body,sizeof(body),
        "{E8EDF3}Введите количество облигаций от 1 до %d.\n\n{E8EDF3}Выпуск: {E8EDF3}%s\n{E8EDF3}Номинал: {E8EDF3}$%d\n{E8EDF3}Ваш лимит (20%%): {E8EDF3}%d шт.\n{E8EDF3}Уже принадлежит вам: {E8EDF3}%d шт.\n\n{E8EDF3}Ограничение считается отдельно по каждому выпуску.",
        gBondsUIMaxBuy[playerid],gBondsUISelectedCode[playerid],
        gBondsUIPrice[playerid],gBondsUIHoldLimit[playerid],
        gBondsUIOwned[playerid]);
    gBondsUIStage[playerid]=BONDS_UI_QUANTITY;
    ShowPlayerDialog(playerid,BONDS_DLG_QUANTITY,DIALOG_STYLE_INPUT,
        "{E8EDF3}TARKENSO | Количество облигаций",body,"Далее","Назад");
    return 1;
}

stock BondsUI_StartPurchase(playerid,quantity)
{
    if(gBondsUIStage[playerid]!=BONDS_UI_QUANTITY ||
       gBondsUISelectedIssue[playerid]<=0 ||
       !gBondsUISelectedCode[playerid][0] ||
       gBondsUIPrice[playerid]<1 || gBondsUIMaxBuy[playerid]<1 ||
       quantity<1 || quantity>gBondsUIMaxBuy[playerid] ||
       gBondsUIPrice[playerid]>1000000000/quantity ||
       !BondsUI_Authorized(playerid))
        return 1;
    gBondsUIPurchaseQty[playerid]=quantity;
    new body[430],total=gBondsUIPrice[playerid]*quantity;
    format(body,sizeof(body),
        "{E8EDF3}ПОДТВЕРЖДЕНИЕ ПОКУПКИ\n\n{E8EDF3}Выпуск: %s\n{E8EDF3}Количество: {E8EDF3}%d шт.\n{E8EDF3}Номинал одной: {E8EDF3}$%d\n{E8EDF3}Итого к оплате: {E8EDF3}$%d\n{E8EDF3}На кошельке: {E8EDF3}$%d\n\n{E8EDF3}Цена и лимит будут проверены MySQL перед списанием.",
        gBondsUISelectedCode[playerid],quantity,gBondsUIPrice[playerid],
        total,gAccountMoney[playerid]);
    gBondsUIStage[playerid]=BONDS_UI_PENDING_BUY;
    ShowPlayerDialog(playerid,BONDS_DLG_CONFIRM,DIALOG_STYLE_MSGBOX,
        "{E8EDF3}TARKENSO | Покупка облигаций",body,"Подтвердить","Назад");
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
        "{E8EDF3}ПОЛУЧЕНИЕ ВЫПЛАТЫ\n\n{E8EDF3}Сумма: {E8EDF3}$%d\n{E8EDF3}Выпуск: {E8EDF3}%s\n\n{E8EDF3}Деньги будут зачислены после подтверждения MySQL.",
        gBondsUIClaimAmount[playerid],gBondsUISelectedCode[playerid]);
    ShowPlayerDialog(playerid,BONDS_DLG_COLLECT,DIALOG_STYLE_MSGBOX,
        "{E8EDF3}TARKENSO {E8EDF3}| {E8EDF3}Получение выплаты",text,"Получить","Назад");
    return 1;
}

stock BondsUI_OnDialog(playerid,dialogid,response,listitem,const inputtext[])
{
    if(dialogid<BONDS_DLG_MAIN || dialogid>BONDS_DLG_POSITION_ACTION) return 0;
    if(!IsPlayerConnected(playerid) || gAuthState[playerid]!=AUTH_READY) return 1;
    if(gBondsUILoading[playerid] || gGovTransferBusy[playerid]) return 1;
    if(dialogid==BONDS_DLG_MAIN)
    {
        if(!response || listitem<0 || listitem>4)return 1;
        switch(listitem)
        {
            case 0:return BondsUI_LoadList(playerid,BONDS_UI_OFFERS);
            case 1:return BondsUI_LoadList(playerid,BONDS_UI_PORTFOLIO);
            case 2:return BondsUI_LoadList(playerid,BONDS_UI_COUPONS);
            case 4:return GovBond_UI_PublicReport(playerid);
            case 3:
            {
                ShowPlayerDialog(playerid,BONDS_DLG_OFFER,DIALOG_STYLE_MSGBOX,
                    "{E8EDF3}TARKENSO {E8EDF3}| {E8EDF3}Риски инвестиций",
                    "{E8EDF3}ОБЛИГАЦИИ НЕ ЯВЛЯЮТСЯ ВКЛАДОМ\n\n{E8EDF3}Гарантия покрывает только установленную часть невыплаченного номинала (обычно 20%).\n\n{E8EDF3}Проценты не застрахованы.\n{E8EDF3}Доходность указана за полный срок выпуска.\n{E8EDF3}При дефолте оставшийся долг может быть выплачен с задержкой.",
                    "Назад","Закрыть");
                gBondsUIStage[playerid]=BONDS_UI_NONE;
                return 1;
            }
        }
    }
    if(dialogid==BONDS_DLG_OFFERS || dialogid==BONDS_DLG_PORTFOLIO || dialogid==BONDS_DLG_COUPONS)
    {
        new kind=dialogid==BONDS_DLG_OFFERS?BONDS_UI_OFFERS:(dialogid==BONDS_DLG_COUPONS?BONDS_UI_COUPONS:BONDS_UI_PORTFOLIO);
        if(gBondsUIStage[playerid]!=kind)return 1;
        if(!response)return 1;
        if(listitem>=0 && listitem<gBondsUIListCount[playerid])
        {
            new issueId=gBondsUIRowIds[playerid][listitem];
            return BondsUI_LoadDetail(playerid,
                kind==BONDS_UI_OFFERS?BONDS_UI_OFFER:(kind==BONDS_UI_COUPONS?BONDS_UI_COUPON:BONDS_UI_POSITION),issueId);
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
        if(!response || gBondsUIMaxBuy[playerid]<1)
            return BondsUI_LoadList(playerid,BONDS_UI_OFFERS,gBondsUIPage[playerid]);
        return BondsUI_ShowQuantity(playerid);
    }
    if(dialogid==BONDS_DLG_QUANTITY)
    {
        if(gBondsUIStage[playerid]!=BONDS_UI_QUANTITY)return 1;
        if(!response)
            return BondsUI_LoadDetail(playerid,BONDS_UI_OFFER,gBondsUISelectedIssue[playerid]);
        // Digits only, never accept signs, whitespace, huge numbers or formatting.
        new count=0,len=strlen(inputtext),bool:valid=(len>=1 && len<=7);
        if(valid)
        {
            for(new i=0;i<len;i++)
            {
                if(inputtext[i]<'0' || inputtext[i]>'9')
                {
                    valid=false;
                    break;
                }
                count=count*10+(inputtext[i]-'0');
                if(count>gBondsUIMaxBuy[playerid])
                {
                    valid=false;
                    break;
                }
            }
        }
        if(!valid || count<1 || count>gBondsUIMaxBuy[playerid])
        {
            SendClientMessage(playerid,COLOR_RED,
                "Неверное количество. Введите целое число в указанном диапазоне.");
            return BondsUI_ShowQuantity(playerid);
        }
        return BondsUI_StartPurchase(playerid,count);
    }
    if(dialogid==BONDS_DLG_CONFIRM)
    {
        if(gBondsUIStage[playerid]!=BONDS_UI_PENDING_BUY)return 1;
        if(!response)return BondsUI_LoadDetail(playerid,BONDS_UI_OFFER,gBondsUISelectedIssue[playerid]);
        if(gBondsUIPurchaseQty[playerid]<1 ||
           gBondsUIPurchaseQty[playerid]>gBondsUIMaxBuy[playerid])
            return 1;
        // Consume confirmation BEFORE queueing: forged/replayed dialog responses
        // cannot create a second event_key and purchase a second bond.
        gBondsUIStage[playerid]=BONDS_UI_NONE;
#if defined TARKENSO_DB_PRIMARY
        new eventKey[65];
        BondsUI_NewKey(playerid,eventKey,sizeof(eventKey));
        gBondsUIPendingAction[playerid]=1;
        gBondsUIPendingQty[playerid]=gBondsUIPurchaseQty[playerid];
        if(!BondSettlement_Buy(playerid,eventKey,gBondsUISelectedCode[playerid],
            gBondsUIPurchaseQty[playerid]))
        {
            gBondsUIPendingAction[playerid]=0;
            gBondsUIPendingQty[playerid]=0;
            SendClientMessage(playerid,COLOR_RED,
                "Покупка не принята. Проверьте баланс, лимит или выпуск.");
        }
        else
        {
            new notice[150];
            format(notice,sizeof(notice),
                "Заявка на покупку %d шт. отправлена в казначейство.",
                gBondsUIPendingQty[playerid]);
            SendClientMessage(playerid,COLOR_AMBER,notice);
        }
#endif
        return 1;
    }
    if(dialogid==BONDS_DLG_COUPON)
    {
        if(gBondsUIStage[playerid]!=BONDS_UI_COUPON)return 1;
        if(!response)return BondsUI_LoadList(playerid,BONDS_UI_COUPONS,gBondsUIPage[playerid]);
        if(gBondsUICouponAmount[playerid]<=0)
            return BondsUI_LoadDetail(playerid,BONDS_UI_COUPON,gBondsUISelectedIssue[playerid]);
        new body[290];
        format(body,sizeof(body),
            "{E8EDF3}Подтвердить получение купонов?\n\n{E8EDF3}Выпуск: {E8EDF3}%s\n{E8EDF3}К переводу: $%d\n\n{E8EDF3}Деньги поступят на игровой кошелёк после проверки казны.",
            gBondsUISelectedCode[playerid],gBondsUICouponAmount[playerid]);
        ShowPlayerDialog(playerid,BONDS_DLG_COUPON_PAY,DIALOG_STYLE_MSGBOX,
            "{E8EDF3}TARKENSO | Получение купонов",body,"Получить","Назад");
        return 1;
    }
    if(dialogid==BONDS_DLG_COUPON_PAY)
    {
        if(gBondsUIStage[playerid]!=BONDS_UI_COUPON)return 1;
        if(!response)return BondsUI_LoadDetail(playerid,BONDS_UI_COUPON,gBondsUISelectedIssue[playerid]);
        gBondsUIStage[playerid]=BONDS_UI_NONE;
#if defined TARKENSO_DB_PRIMARY
        if(gBondsUICouponAmount[playerid]<=0 || !BondCoupon_IsReady())return 1;
        new eventKey[65];
        BondsUI_NewKey(playerid,eventKey,sizeof(eventKey));
        gBondsUIPendingAction[playerid]=3;
        if(!BondSettlement_CollectCoupon(playerid,eventKey,
            gBondsUISelectedIssue[playerid],gBondsUICouponAmount[playerid]))
        {
            gBondsUIPendingAction[playerid]=0;
            SendClientMessage(playerid,COLOR_RED,
                "Не удалось запросить выплату купонов.");
        }
        else SendClientMessage(playerid,COLOR_AMBER,
            "Выплата купонов запрошена; ожидается подтверждение MySQL.");
#endif
        return 1;
    }
    if(dialogid==BONDS_DLG_POSITION)
    {
        if(gBondsUIStage[playerid]!=BONDS_UI_POSITION)return 1;
        if(!response)return BondsUI_LoadList(playerid,BONDS_UI_PORTFOLIO,gBondsUIPage[playerid]);
        ShowPlayerDialog(playerid,BONDS_DLG_POSITION_ACTION,DIALOG_STYLE_LIST,
            "{E8EDF3}TARKENSO | Действия с облигациями",
            "{E8EDF3}Получить выплату по погашению\\n{E8EDF3}Продать государству досрочно (98%)\\n{E8EDF3}Обновить карточку",
            "Выбрать","Назад");
        return 1;
    }
    if(dialogid==BONDS_DLG_POSITION_ACTION)
    {
        if(gBondsUIStage[playerid]!=BONDS_UI_POSITION)return 1;
        if(!response)
            return BondsUI_LoadDetail(playerid,BONDS_UI_POSITION,gBondsUISelectedIssue[playerid]);
        switch(listitem)
        {
            case 0:return BondsUI_StartCollect(playerid);
            case 1:return BondsUI_ShowSellQuantity(playerid);
            case 2:return BondsUI_LoadDetail(playerid,BONDS_UI_POSITION,gBondsUISelectedIssue[playerid]);
        }
        return 1;
    }
    if(dialogid==BONDS_DLG_SELL_QTY)
    {
        if(gBondsUIStage[playerid]!=BONDS_UI_SELL_QTY)return 1;
        if(!response)
            return BondsUI_LoadDetail(playerid,BONDS_UI_POSITION,gBondsUISelectedIssue[playerid]);
        new length=strlen(inputtext);
        if(length<1 || length>7)
            return BondsUI_LoadDetail(playerid,BONDS_UI_POSITION,gBondsUISelectedIssue[playerid]);
        for(new i=0;i<length;i++)
            if(inputtext[i]<'0' || inputtext[i]>'9')
                return BondsUI_LoadDetail(playerid,BONDS_UI_POSITION,gBondsUISelectedIssue[playerid]);
        new count=strval(inputtext);
        if(count<1 || count>gBondsUISellMax[playerid])
        {
            SendClientMessage(playerid,COLOR_RED,"Количество больше доступного для выкупа.");
            return BondsUI_LoadDetail(playerid,BONDS_UI_POSITION,gBondsUISelectedIssue[playerid]);
        }
        return BondsUI_ConfirmSell(playerid,count);
    }
    if(dialogid==BONDS_DLG_SELL_CONFIRM)
    {
        if(gBondsUIStage[playerid]!=BONDS_UI_SELL_CONFIRM)return 1;
        if(!response)
            return BondsUI_LoadDetail(playerid,BONDS_UI_POSITION,gBondsUISelectedIssue[playerid]);
        gBondsUIStage[playerid]=BONDS_UI_NONE;
#if defined TARKENSO_DB_PRIMARY
        if(gBondsUISellQuantity[playerid]<=0 || !gBondBuybackReady)return 1;
        new eventKey[65];
        BondsUI_NewKey(playerid,eventKey,sizeof(eventKey));
        gBondsUIPendingAction[playerid]=4;
        gBondsUIPendingQty[playerid]=gBondsUISellQuantity[playerid];
        if(!BondSettlement_Buyback(playerid,eventKey,gBondsUISelectedIssue[playerid],
                                   gBondsUISellQuantity[playerid]))
        {
            gBondsUIPendingAction[playerid]=0;
            gBondsUIPendingQty[playerid]=0;
            SendClientMessage(playerid,COLOR_RED,"Не удалось начать досрочный выкуп.");
        }
        else SendClientMessage(playerid,COLOR_AMBER,
            "Продажа отправлена в казначейство. Ожидается подтверждение MySQL.");
#endif
        return 1;
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
    new bool:buyback=(gBondsUIPendingAction[playerid]==4);
    gBondsUIPendingAction[playerid]=0;
    gBondsUIPendingQty[playerid]=0;
    if(buyback && IsPlayerConnected(playerid) && gAuthState[playerid]==AUTH_READY)
    {
        SendClientMessage(playerid,COLOR_RED,
            "Выкуп отклонён: обновите портфель, проверьте срок и свободные средства казны.");
        return 1;
    }
    if(IsPlayerConnected(playerid) && gAuthState[playerid]==AUTH_READY)
        SendClientMessage(playerid,COLOR_RED,
            "Казначейство отклонило операцию. Обновите карточку для актуальной причины отказа.");
    return 1;
}

public BondsUI_OnSettlement(playerid,bool:replayed)
{
    if(!IsPlayerConnected(playerid) || gAuthState[playerid]!=AUTH_READY)
        return 1;
    new action=gBondsUIPendingAction[playerid],quantity=gBondsUIPendingQty[playerid];
    gBondsUIPendingAction[playerid]=0;
    gBondsUIPendingQty[playerid]=0;
    if(action==1 && quantity>0)
    {
        new notice[150];
        format(notice,sizeof(notice),
            replayed?"Покупка %d шт. уже проведена ранее.":
                "Покупка %d облигаций подтверждена. Проверьте портфель.",
            quantity);
        SendClientMessage(playerid,COLOR_AMBER,notice);
    }
    else if(action==2)
        SendClientMessage(playerid,COLOR_AMBER,
            replayed?"Выплата уже была перечислена.":"Выплата подтверждена и зачислена на кошелёк.");
    else if(action==3)
        SendClientMessage(playerid,COLOR_AMBER,
            replayed?"Купоны уже перечислены ранее.":
                "Купоны перечислены на ваш кошелёк. Остаток смотрите в /bonds.");
    else if(action==4 && quantity>0)
    {
        new notice[160];
        format(notice,sizeof(notice),
            replayed?"Выкуп %d облигаций уже проведён ранее.":
                     "Государство выкупило %d облигаций за 98%% номинала. Деньги на кошельке.",
            quantity);
        SendClientMessage(playerid,COLOR_AMBER,notice);
    }
    return 1;
}
