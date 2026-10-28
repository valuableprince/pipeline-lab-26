#if defined _tarkenso_bond_coupon_included
    #endinput
#endif
#define _tarkenso_bond_coupon_included

// GOV-07: global PayDay accrual. No per-player work and no connection state:
// every order belongs to a durable account ID, even while offline.
// The SQL event key (issue_id,hour_key) makes this retriable after restart.
#define BOND_COUPON_SCAN_LIMIT (8)
#define BOND_COUPON_BACKSTOP_MS (30000)
#define BOND_COUPON_NOTICE_TOP (3)
#define BOND_COUPON_CHAT_COLOR (0xFFFFFFFF)

new bool:gBondCouponReady;
new bool:gBondCouponScanBusy;
new gBondCouponPending;
new gBondCouponHourKey;
new bool:gBondCouponHadError;
new gBondCouponTimer;
new gBondCouponNotifiedHour;
new bool:gBondCouponNotifyBusy[MAX_PLAYERS];
new gBondCouponNotifySeq[MAX_PLAYERS];

forward BondCoupon_Tick();
forward BondCoupon_OnScan();
forward BondCoupon_OnAccrued(issueId);
forward BondCoupon_OnNotices(playerid,authToken,accountId,seq,offline);
forward BondCoupon_OnNoticeMarked(playerid,authToken,accountId,seq);

stock bool:BondCoupon_IsReady()
{
#if defined TARKENSO_DB_PRIMARY
    return gBondCouponReady && BondSettlement_IsReady() && DBPrimary_IsReady();
#else
    return false;
#endif
}

stock BondCoupon_Shutdown()
{
    gBondCouponReady=false;
    if(gBondCouponTimer)KillTimer(gBondCouponTimer);
    gBondCouponTimer=0;
    gBondCouponScanBusy=false;
    gBondCouponPending=0;
    gBondCouponNotifiedHour=-1;
    return 1;
}

stock BondCoupon_Init()
{
    BondCoupon_Shutdown();
#if defined TARKENSO_DB_PRIMARY
    if(!BondSettlement_IsReady())return 1;
    new Cache:c=mysql_query(gDBPrimary,
        "SELECT version FROM schema_migrations WHERE version=27 LIMIT 1");
    if(c==Cache:0)return 1;
    new rows=0;
    new bool:ready=(mysql_errno(gDBPrimary)==0 &&
        cache_get_row_count(rows) && rows==1);
    cache_delete(c);
    if(!ready)
    {
        print("[GOV-07] Coupon notice migration 027 missing; scheduler disabled.");
        return 1;
    }
    gBondCouponReady=true;
    gBondCouponHourKey=gettime()/3600;
    gBondCouponTimer=SetTimer("BondCoupon_Tick",BOND_COUPON_BACKSTOP_MS,true);
    print("[GOV-07] Hourly offline coupon accrual initialized.");
    BondCoupon_Tick(); // reconcile at startup, no double payment
#else
    print("[GOV-07] Coupon scheduler requires MySQL-primary mode.");
#endif
    return 1;
}

// Trigger immediately from PayDay_Tick; 30-second backstop also covers missed
// clock ticks/server restarts and batches issues beyond eight per cycle.
stock BondCoupon_OnPayday(hourKey)
{
#if defined TARKENSO_DB_PRIMARY
    if(!BondCoupon_IsReady() || hourKey<=0)return 1;
    if(hourKey>gBondCouponHourKey)gBondCouponHourKey=hourKey;
    BondCoupon_Tick();
#else
    #pragma unused hourKey
#endif
    return 1;
}

public BondCoupon_Tick()
{
#if defined TARKENSO_DB_PRIMARY
    if(!BondCoupon_IsReady() || gBondCouponScanBusy || gBondCouponPending>0)
        return 1;
    new current=gettime()/3600;
    if(current>gBondCouponHourKey)gBondCouponHourKey=current;
    gBondCouponScanBusy=true;
    new query[650];
    mysql_format(gDBPrimary,query,sizeof(query),
        "SELECT i.issue_id FROM government_bond_issues i WHERE i.status='OFFERING' AND i.maturity_at>UTC_TIMESTAMP() AND i.offer_start_at<=UTC_TIMESTAMP() AND NOT EXISTS (SELECT 1 FROM government_bond_coupon_events e WHERE e.issue_id=i.issue_id AND e.hour_key=%d) ORDER BY i.issue_id LIMIT %d",
        gBondCouponHourKey,BOND_COUPON_SCAN_LIMIT);
    if(!mysql_tquery(gDBPrimary,query,"BondCoupon_OnScan",""))
        gBondCouponScanBusy=false;
#endif
    return 1;
}

public BondCoupon_OnScan()
{
#if defined TARKENSO_DB_PRIMARY
    gBondCouponScanBusy=false;
    if(!BondCoupon_IsReady())return 1;
    new rows=0;
    if(mysql_errno(gDBPrimary)!=0 || !cache_get_row_count(rows) ||
       rows>BOND_COUPON_SCAN_LIMIT)
    {
        print("[GOV-07] Coupon issue scan failed; retry on backstop.");
        return 1;
    }
    gBondCouponHadError=false;
    for(new i=0;i<rows;i++)
    {
        new issueId,query[160];
        if(!cache_get_value_name_int(i,"issue_id",issueId) || issueId<=0)
            continue;
        mysql_format(gDBPrimary,query,sizeof(query),
            "CALL gov_bond_coupon_accrue(%d,%d)",issueId,gBondCouponHourKey);
        gBondCouponPending++;
        if(!mysql_tquery(gDBPrimary,query,"BondCoupon_OnAccrued","i",issueId))
        {
            gBondCouponPending--;
            gBondCouponHadError=true;
        }
    }
    if(rows==0)
    {
        // Notify only after all committed coupon accruals for this hour.
        // Failed SQL work is retried by the backstop before delivery.
        if(gBondCouponNotifiedHour!=gBondCouponHourKey)
        {
            gBondCouponNotifiedHour=gBondCouponHourKey;
            BondCoupon_NotifyOnline();
        }
        return 1;
    }
    if(gBondCouponPending==0)gBondCouponHadError=true;
#endif
    return 1;
}

public BondCoupon_OnAccrued(issueId)
{
#if defined TARKENSO_DB_PRIMARY
    if(gBondCouponPending>0)gBondCouponPending--;
    new rows=0,actualIssue,accrued,replayed;
    if(mysql_errno(gDBPrimary)!=0 || !cache_get_row_count(rows) || rows!=1 ||
       !cache_get_value_name_int(0,"issue_id",actualIssue) ||
       !cache_get_value_name_int(0,"newly_accrued",accrued) ||
       !cache_get_value_name_int(0,"replayed",replayed) ||
       actualIssue!=issueId || accrued<0 || (replayed!=0 && replayed!=1))
    {
        printf("[GOV-07] Coupon accrual failed for issue=%d.",issueId);
        gBondCouponHadError=true;
    }
    if(gBondCouponPending==0 && !gBondCouponHadError)
        BondCoupon_Tick(); // next page, no blocking server thread
#else
    #pragma unused issueId
#endif
    return 1;
}

// Coupon notifications are independent of the money transfer. An immutable
// event enters the queue inside the SAME SQL transaction as coupon accrual.
// Only after a successful async SELECT can we show a chat message.
stock BondCoupon_NotifyReset(playerid)
{
    gBondCouponNotifySeq[playerid]++;
    gBondCouponNotifyBusy[playerid]=false;
    return 1;
}

stock BondCoupon_NotifyPlayer(playerid,bool:offline=false)
{
#if defined TARKENSO_DB_PRIMARY
    if(!BondCoupon_IsReady() || playerid<0 || playerid>=MAX_PLAYERS ||
       !IsPlayerConnected(playerid) || IsPlayerNPC(playerid) ||
       gAuthState[playerid]!=AUTH_READY || !gDBPrimaryLoaded[playerid] ||
       gDBPrimaryAccountId[playerid]<=0 || gBondCouponNotifyBusy[playerid])
        return 1;
    gBondCouponNotifyBusy[playerid]=true;
    gBondCouponNotifySeq[playerid]++;
    new query[1250];
    // Window totals cover ALL series; LIMIT shows only the three largest.
    // The max notice ID is a committed snapshot, so an ACK cannot consume
    // newer PayDay events that appear while the player reads the chat.
    mysql_format(gDBPrimary,query,sizeof(query),
        "SELECT v.issue_code,CAST(v.amount AS CHAR) AS amount,CAST(SUM(v.amount) OVER() AS CHAR) AS total,COUNT(*) OVER() AS series_count,CAST(MAX(v.last_id) OVER() AS CHAR) AS max_id FROM (SELECT i.issue_code,SUM(q.amount) AS amount,MAX(q.notice_id) AS last_id FROM government_bond_coupon_notice_queue q JOIN government_bond_issues i ON i.issue_id=q.issue_id LEFT JOIN government_bond_coupon_notice_deliveries d ON d.notice_id=q.notice_id WHERE q.account_id=%d AND d.notice_id IS NULL GROUP BY q.issue_id,i.issue_code) v ORDER BY v.amount DESC,v.issue_code LIMIT %d",
        gDBPrimaryAccountId[playerid],BOND_COUPON_NOTICE_TOP);
    if(!mysql_tquery(gDBPrimary,query,"BondCoupon_OnNotices","iiiii",
        playerid,gAuthToken[playerid],gDBPrimaryAccountId[playerid],
        gBondCouponNotifySeq[playerid],_:offline))
        gBondCouponNotifyBusy[playerid]=false;
#else
    #pragma unused playerid,offline
#endif
    return 1;
}

stock BondCoupon_NotifyOnline()
{
#if defined TARKENSO_DB_PRIMARY
    if(!BondCoupon_IsReady())return 1;
    for(new playerid=0;playerid<MAX_PLAYERS;playerid++)
        if(IsPlayerConnected(playerid) && !IsPlayerNPC(playerid) &&
            gAuthState[playerid]==AUTH_READY)
            BondCoupon_NotifyPlayer(playerid);
#endif
    return 1;
}

stock bool:BondCoupon_NoticeValid(playerid,authToken,accountId,seq)
{
#if defined TARKENSO_DB_PRIMARY
    return playerid>=0 && playerid<MAX_PLAYERS && IsPlayerConnected(playerid) &&
        !IsPlayerNPC(playerid) && gAuthState[playerid]==AUTH_READY &&
        gDBPrimaryLoaded[playerid] && gDBPrimaryAccountId[playerid]==accountId &&
        gAuthToken[playerid]==authToken &&
        gBondCouponNotifySeq[playerid]==seq &&
        gBondCouponNotifyBusy[playerid];
#else
    #pragma unused playerid,authToken,accountId,seq
    return false;
#endif
}

public BondCoupon_OnNotices(playerid,authToken,accountId,seq,offline)
{
#if defined TARKENSO_DB_PRIMARY
    if(!BondCoupon_NoticeValid(playerid,authToken,accountId,seq))return 1;
    new rows=0;
    if(mysql_errno(gDBPrimary)!=0 || !cache_get_row_count(rows) ||
        rows>BOND_COUPON_NOTICE_TOP)
    {
        gBondCouponNotifyBusy[playerid]=false;
        return 1;
    }
    if(rows==0)
    {
        gBondCouponNotifyBusy[playerid]=false;
        return 1; // no positive coupons: do not spam PayDay
    }
    new total[24],maxId[24],amount[24],code[65],series=0;
    if(!cache_get_value_name(0,"total",total,sizeof(total)) ||
       !cache_get_value_name(0,"max_id",maxId,sizeof(maxId)) ||
       !cache_get_value_name_int(0,"series_count",series) || series<rows ||
       !total[0] || !maxId[0])
    {
        gBondCouponNotifyBusy[playerid]=false;
        return 1;
    }
    // Guard numeric SQL values before reusing a high-water ID in an UPDATE.
    for(new i=0;i<strlen(maxId);i++)
        if(maxId[i]<'0' || maxId[i]>'9')
        {
            gBondCouponNotifyBusy[playerid]=false;
            return 1;
        }
    SendClientMessage(playerid,BOND_COUPON_CHAT_COLOR,offline?
        "{D9AA55}[Госзаймы] За время отсутствия начислены купоны:":
        "{D9AA55}[Госзаймы] Начислены купоны за PayDay:");
    for(new i=0;i<rows;i++)
    {
        if(!cache_get_value_name(i,"issue_code",code,sizeof(code)) ||
           !cache_get_value_name(i,"amount",amount,sizeof(amount)))
            continue;
        new message[145];
        format(message,sizeof(message),"{FFFFFF}%s: {B6E4B3}+$%s",
            code,amount);
        SendClientMessage(playerid,BOND_COUPON_CHAT_COLOR,message);
    }
    if(series>rows)
    {
        new extra[110];
        format(extra,sizeof(extra),
            "{9FAAB8}Других выпусков с начислениями: %d",series-rows);
        SendClientMessage(playerid,BOND_COUPON_CHAT_COLOR,extra);
    }
    new summary[145];
    format(summary,sizeof(summary),
        "{D9AA55}Всего начислено: {B6E4B3}+$%s",total);
    SendClientMessage(playerid,BOND_COUPON_CHAT_COLOR,summary);
    SendClientMessage(playerid,BOND_COUPON_CHAT_COLOR,
        "{9FAAB8}На условном купонном счёте. Подробнее: /bonds");

    // ACK only after messages were sent, only up to the snapshot max ID.
    // If ACK fails, messages may repeat, but the user never loses coupons.
    new query[670];
    mysql_format(gDBPrimary,query,sizeof(query),
        "INSERT IGNORE INTO government_bond_coupon_notice_deliveries (notice_id,account_id) SELECT q.notice_id,q.account_id FROM government_bond_coupon_notice_queue q LEFT JOIN government_bond_coupon_notice_deliveries d ON d.notice_id=q.notice_id WHERE q.account_id=%d AND q.notice_id<=CAST('%e' AS UNSIGNED) AND d.notice_id IS NULL",
        accountId,maxId);
    if(!mysql_tquery(gDBPrimary,query,"BondCoupon_OnNoticeMarked","iiii",
        playerid,authToken,accountId,seq))
        gBondCouponNotifyBusy[playerid]=false;
#else
    #pragma unused playerid,authToken,accountId,seq,offline
#endif
    return 1;
}

public BondCoupon_OnNoticeMarked(playerid,authToken,accountId,seq)
{
#if defined TARKENSO_DB_PRIMARY
    if(!BondCoupon_NoticeValid(playerid,authToken,accountId,seq))return 1;
    if(mysql_errno(gDBPrimary)!=0)
        print("[GOV-07] Coupon notification ACK failed; retry on next PayDay.");
    gBondCouponNotifyBusy[playerid]=false;
#else
    #pragma unused playerid,authToken,accountId,seq
#endif
    return 1;
}
