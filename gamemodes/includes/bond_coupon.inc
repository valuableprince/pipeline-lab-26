#if defined _tarkenso_bond_coupon_included
    #endinput
#endif
#define _tarkenso_bond_coupon_included

// GOV-07: global PayDay accrual. No per-player work and no connection state:
// every order belongs to a durable account ID, even while offline.
// The SQL event key (issue_id,hour_key) makes this retriable after restart.
#define BOND_COUPON_SCAN_LIMIT (8)
#define BOND_COUPON_BACKSTOP_MS (30000)

new bool:gBondCouponReady;
new bool:gBondCouponScanBusy;
new gBondCouponPending;
new gBondCouponHourKey;
new bool:gBondCouponHadError;
new gBondCouponTimer;

forward BondCoupon_Tick();
forward BondCoupon_OnScan();
forward BondCoupon_OnAccrued(issueId);

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
    return 1;
}

stock BondCoupon_Init()
{
    BondCoupon_Shutdown();
#if defined TARKENSO_DB_PRIMARY
    if(!BondSettlement_IsReady())return 1;
    new Cache:c=mysql_query(gDBPrimary,
        "SELECT version FROM schema_migrations WHERE version=26 LIMIT 1");
    if(c==Cache:0)return 1;
    new rows=0;
    new bool:ready=(mysql_errno(gDBPrimary)==0 &&
        cache_get_row_count(rows) && rows==1);
    cache_delete(c);
    if(!ready)
    {
        print("[GOV-07] Coupon migration 026 missing; scheduler disabled.");
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
    if(rows==0) return 1;
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
