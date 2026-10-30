#if defined _tarkenso_crime_assets_included
    #endinput
#endif
#define _tarkenso_crime_assets_included

// CRIME-02.9: player-local visual assets, faction-wide solo service missions.
// A client cannot award itself influence; every award is an InnoDB transaction.
#define CRIME_ASSET_RADIUS (3.0)
#define CRIME_ASSET_COOLDOWN (1800)

new bool:gCrimeAssetVisible[MAX_PLAYERS][2];
new gCrimeAssetObject[MAX_PLAYERS][2];
new gCrimeAssetContact[MAX_PLAYERS];
new gCrimeAssetFaction[MAX_PLAYERS];
new gCrimeAssetAccount[MAX_PLAYERS];
new gCrimeAssetToken[MAX_PLAYERS];

stock CrimeAssets_Reset(playerid)
{
    for (new i; i < 2; i++)
    {
        if (gCrimeAssetVisible[playerid][i])
            DestroyPlayerObject(playerid, gCrimeAssetObject[playerid][i]);
        gCrimeAssetVisible[playerid][i] = false;
        gCrimeAssetObject[playerid][i] = 0;
    }
    gCrimeAssetContact[playerid] = 0;
    gCrimeAssetFaction[playerid] = 0;
    gCrimeAssetAccount[playerid] = 0;
    gCrimeAssetToken[playerid] = 0;
    return 1;
}

stock CrimeAssets_Position(contactId, assetCode, &Float:x, &Float:y, &Float:z)
{
    if (contactId < 1 || contactId > CRIME_CONTACT_COUNT ||
        assetCode < 1 || assetCode > 2) return 0;
    new idx = contactId - 1;
    // Two nearby physical stations at each established NPC. No global
    // checkpoints or global object limits; per-player stream range 100m.
    x = gCrimeContactPos[idx][0] + (assetCode == 1 ? 3.0 : -3.0);
    y = gCrimeContactPos[idx][1] + 3.0;
    z = gCrimeContactPos[idx][2];
    return 1;
}

stock CrimeAssets_Show(playerid, contactId, stash, network)
{
    CrimeAssets_Reset(playerid);
#if defined TARKENSO_DB_PRIMARY
    if (!Crime_Available(playerid) ||
        !Leader_IsCriminal(Leader_PlayerOrg(playerid)) ||
        GetPlayerVirtualWorld(playerid) != 0 ||
        GetPlayerInterior(playerid) != 0 ||
        contactId < 1 || contactId > CRIME_CONTACT_COUNT) return 0;
    new owned[2];
    owned[0] = stash;
    owned[1] = network;
    gCrimeAssetContact[playerid] = contactId;
    gCrimeAssetFaction[playerid] = Leader_PlayerOrg(playerid);
    gCrimeAssetAccount[playerid] = gDBPrimaryAccountId[playerid];
    gCrimeAssetToken[playerid] = gAuthToken[playerid];
    for (new i; i < 2; i++)
    {
        if (!owned[i]) continue;
        new Float:x, Float:y, Float:z;
        CrimeAssets_Position(contactId, i+1, x, y, z);
        gCrimeAssetObject[playerid][i] = CreatePlayerObject(playerid,
            i == 0 ? 1271 : 2894, x, y, z-0.85, 0.0, 0.0, 0.0, 100.0);
        if (gCrimeAssetObject[playerid][i] == INVALID_OBJECT_ID)
        {
            CrimeAssets_Reset(playerid);
            return 0;
        }
        gCrimeAssetVisible[playerid][i] = true;
    }
    return 1;
#else
    #pragma unused contactId, stash, network
    return 0;
#endif
}

stock CrimeAssets_Service(playerid, contactId, assetCode)
{
#if defined TARKENSO_DB_PRIMARY
    if (!Crime_Available(playerid) || gGovTransferBusy[playerid] ||
        !Leader_IsCriminal(Leader_PlayerOrg(playerid)) ||
        IsPlayerInAnyVehicle(playerid) ||
        GetPlayerVirtualWorld(playerid) != 0 ||
        GetPlayerInterior(playerid) != 0 ||
        contactId < 1 || contactId > CRIME_CONTACT_COUNT ||
        assetCode < 1 || assetCode > 2)
        return SendClientMessage(playerid, CRIME_ERROR, "Обслуживание актива недоступно.");
    new Float:x, Float:y, Float:z;
    if (!CrimeAssets_Position(contactId, assetCode, x, y, z) ||
        !IsPlayerInRangeOfPoint(playerid, CRIME_ASSET_RADIUS, x, y, z))
        return SendClientMessage(playerid, CRIME_ERROR,
            "Подойдите ближе к объекту синдиката.");
    new faction = Leader_PlayerOrg(playerid), q[900], affected = 0;
    if (!Crime_Exec("START TRANSACTION"))
        return SendClientMessage(playerid, CRIME_ERROR, "Нет соединения с базой.");
    // The UPDATE atomically claims this 30-minute maintenance opportunity.
    // Only one concurrent player can advance the service counter.
    mysql_format(gDBPrimary, q, sizeof(q),
        "UPDATE crime_syndicate_assets a SET a.activated_at=IF(a.activated_at=0,UNIX_TIMESTAMP(),a.activated_at),a.last_serviced_at=UNIX_TIMESTAMP(),a.service_count=a.service_count+1 WHERE a.faction_id=%d AND a.contact_id=%d AND a.asset_code=%d AND a.service_count<1000000000 AND (a.last_serviced_at=0 OR a.last_serviced_at<=UNIX_TIMESTAMP()-%d) AND EXISTS(SELECT 1 FROM faction_members m WHERE m.account_id=%d AND m.faction_id=a.faction_id)",
        faction, contactId, assetCode, CRIME_ASSET_COOLDOWN,
        gDBPrimaryAccountId[playerid]);
    if (!DBPrimary_Execute(q, affected) || affected != 1)
    {
        Crime_Rollback();
        return SendClientMessage(playerid, CRIME_ERROR,
            "Актив ещё восстанавливается (30 мин) или больше не принадлежит синдикату.");
    }
    // Store ACTUAL earned points at the cap, not an imaginary +2.
    mysql_format(gDBPrimary, q, sizeof(q),
        "INSERT INTO crime_syndicate_asset_services(faction_id,contact_id,asset_code,service_count,actor_account_id,influence_delta,standing_delta,serviced_at) SELECT a.faction_id,a.contact_id,a.asset_code,a.service_count,%d,CASE WHEN a.asset_code=1 THEN LEAST(2,1000000-p.influence) ELSE 0 END,CASE WHEN a.asset_code=2 THEN LEAST(2,100-c.standing) ELSE 0 END,UNIX_TIMESTAMP() FROM crime_syndicate_assets a JOIN crime_syndicate_progress p ON p.faction_id=a.faction_id JOIN crime_syndicate_contacts c ON c.faction_id=a.faction_id AND c.contact_id=a.contact_id WHERE a.faction_id=%d AND a.contact_id=%d AND a.asset_code=%d",
        gDBPrimaryAccountId[playerid], faction, contactId, assetCode);
    if (!DBPrimary_Execute(q, affected) || affected != 1)
    {
        Crime_Rollback();
        return SendClientMessage(playerid, CRIME_ERROR, "Не удалось записать обслуживание.");
    }
    if (assetCode == 1)
    {
        mysql_format(gDBPrimary, q, sizeof(q),
            "UPDATE crime_syndicate_progress p JOIN crime_syndicate_asset_services s ON s.faction_id=p.faction_id SET p.influence=p.influence+s.influence_delta WHERE s.faction_id=%d AND s.contact_id=%d AND s.asset_code=1 AND s.service_count=(SELECT service_count FROM crime_syndicate_assets a WHERE a.faction_id=%d AND a.contact_id=%d AND a.asset_code=1)",
            faction, contactId, faction, contactId);
    }
    else
    {
        mysql_format(gDBPrimary, q, sizeof(q),
            "UPDATE crime_syndicate_contacts c JOIN crime_syndicate_asset_services s ON s.faction_id=c.faction_id AND s.contact_id=c.contact_id SET c.standing=c.standing+s.standing_delta WHERE s.faction_id=%d AND s.contact_id=%d AND s.asset_code=2 AND s.service_count=(SELECT service_count FROM crime_syndicate_assets a WHERE a.faction_id=%d AND a.contact_id=%d AND a.asset_code=2)",
            faction, contactId, faction, contactId);
    }
    if (!DBPrimary_Execute(q, affected))
    {
        Crime_Rollback();
        return SendClientMessage(playerid, CRIME_ERROR,
            "Не удалось начислить влияние. Обслуживание отменено.");
    }
    if (!Crime_Exec("COMMIT"))
        return SendClientMessage(playerid, CRIME_ERROR,
            "Результат неизвестен. Обновите меню, прежде чем повторять.");
    return SendClientMessage(playerid, CRIME_COLOR,
        assetCode == 1 ?
        ("Тайник обслужен: синдикат получил до 2 влияния.") :
        ("Информаторы собрали сведения: отношения улучшены до 2 пунктов."));
#else
    #pragma unused playerid, contactId, assetCode
    return 1;
#endif
}

stock CrimeAssets_OnKeys(playerid, KEY:newkeys, KEY:oldkeys)
{
    if (!(newkeys & KEY_YES) || (oldkeys & KEY_YES)) return 0;
#if defined TARKENSO_DB_PRIMARY
    if (!Crime_Available(playerid) ||
        IsPlayerInAnyVehicle(playerid) ||
        GetPlayerVirtualWorld(playerid) != 0 ||
        GetPlayerInterior(playerid) != 0 ||
        gCrimeUIDialog[playerid] != 0 ||
        gCrimeAssetContact[playerid] < 1 ||
        gCrimeAssetFaction[playerid] != Leader_PlayerOrg(playerid) ||
        gCrimeAssetAccount[playerid] != gDBPrimaryAccountId[playerid] ||
        gCrimeAssetToken[playerid] != gAuthToken[playerid])
        return 0;
    for (new i; i < 2; i++)
    {
        if (!gCrimeAssetVisible[playerid][i]) continue;
        new Float:x, Float:y, Float:z;
        CrimeAssets_Position(gCrimeAssetContact[playerid], i+1, x, y, z);
        if (!IsPlayerInRangeOfPoint(playerid, CRIME_ASSET_RADIUS, x, y, z))
            continue;
        CrimeAssets_Service(playerid, gCrimeAssetContact[playerid], i+1);
        return 1;
    }
#endif
    return 0;
}
