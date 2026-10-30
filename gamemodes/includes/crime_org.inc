#if defined _tarkenso_crime_org_included
    #endinput
#endif
#define _tarkenso_crime_org_included

// CRIME-02.8: organization-wide influence and contact-local assets.
// No passive money, no timers, no personal wallet spending.
stock bool:CrimeOrg_ReadFaction(playerid, &influence, &completed)
{
    influence = 0;
    completed = 0;
#if defined TARKENSO_DB_PRIMARY
    if (!Crime_Available(playerid)) return false;
    new org = Leader_PlayerOrg(playerid), q[210], rows = 0;
    if (!Leader_IsCriminal(org)) return false;
    mysql_format(gDBPrimary, q, sizeof(q),
        "SELECT influence,completed_count FROM crime_syndicate_progress WHERE faction_id=%d LIMIT 1", org);
    new Cache:c = mysql_query(gDBPrimary, q);
    if (c == Cache:0 || mysql_errno(gDBPrimary) ||
        !cache_get_row_count(rows) || rows != 1 ||
        !cache_get_value_name_int(0, "influence", influence) ||
        !cache_get_value_name_int(0, "completed_count", completed))
    {
        if (c != Cache:0) cache_delete(c);
        return false;
    }
    cache_delete(c);
    return influence >= 0 && influence <= 1000000;
#else
    #pragma unused playerid
    return false;
#endif
}

stock bool:CrimeOrg_ReadContact(playerid, contactId, &standing,
    &completed, &stash, &network)
{
    standing = 0;
    completed = 0;
    stash = 0;
    network = 0;
#if defined TARKENSO_DB_PRIMARY
    if (!Crime_Available(playerid) ||
        contactId < 1 || contactId > CRIME_CONTACT_COUNT) return false;
    new org = Leader_PlayerOrg(playerid), q[370], rows = 0;
    if (!Leader_IsCriminal(org)) return false;
    mysql_format(gDBPrimary, q, sizeof(q),
        "SELECT p.standing,p.completed_count,EXISTS(SELECT 1 FROM crime_syndicate_assets a WHERE a.faction_id=p.faction_id AND a.contact_id=p.contact_id AND a.asset_code=1) stash,EXISTS(SELECT 1 FROM crime_syndicate_assets a WHERE a.faction_id=p.faction_id AND a.contact_id=p.contact_id AND a.asset_code=2) network FROM crime_syndicate_contacts p WHERE p.faction_id=%d AND p.contact_id=%d LIMIT 1",
        org, contactId);
    new Cache:c = mysql_query(gDBPrimary, q);
    if (c == Cache:0 || mysql_errno(gDBPrimary) ||
        !cache_get_row_count(rows) || rows != 1 ||
        !cache_get_value_name_int(0, "standing", standing) ||
        !cache_get_value_name_int(0, "completed_count", completed) ||
        !cache_get_value_name_int(0, "stash", stash) ||
        !cache_get_value_name_int(0, "network", network))
    {
        if (c != Cache:0) cache_delete(c);
        return false;
    }
    cache_delete(c);
    return standing >= 0 && standing <= 100;
#else
    #pragma unused playerid, contactId
    return false;
#endif
}

stock CrimeOrg_AssetCost(assetCode)
{
    switch (assetCode)
    {
        case 1: return 15;
        case 2: return 30;
    }
    return 0;
}

stock CrimeOrg_AssetStanding(assetCode)
{
    switch (assetCode)
    {
        case 1: return 10;
        case 2: return 20;
    }
    return 101;
}

stock CrimeOrg_Buy(playerid, contactId, assetCode)
{
    if (!Crime_Available(playerid) ||
        !Leader_IsCriminal(Leader_PlayerOrg(playerid)) ||
        Leader_PlayerRank(playerid) < 4 ||
        contactId < 1 || contactId > CRIME_CONTACT_COUNT ||
        (assetCode != 1 && assetCode != 2) ||
        gGovTransferBusy[playerid])
        return SendClientMessage(playerid, CRIME_ERROR,
            "Покупка доступна только руководству синдиката (ранг 4-5).");
#if defined TARKENSO_DB_PRIMARY
    new faction = Leader_PlayerOrg(playerid);
    new cost = CrimeOrg_AssetCost(assetCode),
        requiredStanding = CrimeOrg_AssetStanding(assetCode),
        q[850], changed;
    if (!Crime_Exec("START TRANSACTION"))
        return SendClientMessage(playerid, CRIME_ERROR, "База недоступна.");
    // Lock the faction row by debiting it first. Concurrent purchases serialize
    // on this row; every later failure rolls the influence debit back.
    mysql_format(gDBPrimary, q, sizeof(q),
        "UPDATE crime_syndicate_progress p SET p.influence=p.influence-%d WHERE p.faction_id=%d AND p.influence>=%d AND EXISTS(SELECT 1 FROM faction_members m WHERE m.account_id=%d AND m.faction_id=p.faction_id AND m.rank_id>=4) AND EXISTS(SELECT 1 FROM crime_syndicate_contacts c WHERE c.faction_id=p.faction_id AND c.contact_id=%d AND c.standing>=%d) AND NOT EXISTS(SELECT 1 FROM crime_syndicate_assets a WHERE a.faction_id=p.faction_id AND a.contact_id=%d AND a.asset_code=%d)",
        cost, faction, cost, gDBPrimaryAccountId[playerid],
        contactId, requiredStanding, contactId, assetCode);
    if (!DBPrimary_Execute(q, changed) || changed != 1)
    {
        Crime_Rollback();
        return SendClientMessage(playerid, CRIME_ERROR,
            "Актив недоступен: не хватает влияния/отношений или он уже приобретён.");
    }
    mysql_format(gDBPrimary, q, sizeof(q),
        "INSERT INTO crime_syndicate_assets(faction_id,contact_id,asset_code,cost_influence,acquired_by,acquired_at) VALUES (%d,%d,%d,%d,%d,UNIX_TIMESTAMP())",
        faction, contactId, assetCode, cost, gDBPrimaryAccountId[playerid]);
    if (!DBPrimary_Execute(q, changed) || changed != 1)
    {
        Crime_Rollback();
        return SendClientMessage(playerid, CRIME_ERROR,
            "Не удалось зарегистрировать актив. Влияние не списано.");
    }
    if (!Crime_Exec("COMMIT"))
        return SendClientMessage(playerid, CRIME_ERROR,
            "Итог операции неизвестен. Обновите меню, не повторяйте покупку.");
    return SendClientMessage(playerid, CRIME_COLOR,
        "Актив синдиката приобретён. Его бонус уже действует для новых контрактов.");
#else
    #pragma unused contactId, assetCode
    return 1;
#endif
}
