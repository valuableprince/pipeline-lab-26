#if defined _tarkenso_crime_contacts_included
    #endinput
#endif
#define _tarkenso_crime_contacts_included

#define CRIME_CONTACT_COUNT (3)
#define CRIME_CONTACT_RADIUS (5.0)
#define CRIME_TRUST_REWARD (5)

forward CrimeContacts_Shutdown();

new bool:gCrimeContactEnabled[CRIME_CONTACT_COUNT];
new gCrimeContactActor[CRIME_CONTACT_COUNT];
new gCrimeContactName[CRIME_CONTACT_COUNT][64];
new gCrimeContactCity[CRIME_CONTACT_COUNT][32];
new Float:gCrimeContactPos[CRIME_CONTACT_COUNT][3];

stock bool:CrimeContacts_Init()
{
#if defined TARKENSO_DB_PRIMARY
    new Cache:c = mysql_query(gDBPrimary,
        "SELECT contact_id,name,city,actor_model,pos_x,pos_y,pos_z,facing FROM crime_contacts WHERE is_enabled=1 ORDER BY contact_id");
    new rows = 0;
    if (c == Cache:0 || mysql_errno(gDBPrimary) ||
        !cache_get_row_count(rows) || rows != CRIME_CONTACT_COUNT)
    {
        if (c != Cache:0) cache_delete(c);
        print("[CRIME-02.2] NPC registry unavailable.");
        return false;
    }
    for (new i; i < CRIME_CONTACT_COUNT; i++)
    {
        new cid, model, Float:facing;
        if (!cache_get_value_name_int(i, "contact_id", cid) ||
            cid != i + 1 ||
            !cache_get_value_name_int(i, "actor_model", model) ||
            !cache_get_value_name(i, "name", gCrimeContactName[i], 64) ||
            !cache_get_value_name(i, "city", gCrimeContactCity[i], 32) ||
            !cache_get_value_name_float(i, "pos_x", gCrimeContactPos[i][0]) ||
            !cache_get_value_name_float(i, "pos_y", gCrimeContactPos[i][1]) ||
            !cache_get_value_name_float(i, "pos_z", gCrimeContactPos[i][2]) ||
            !cache_get_value_name_float(i, "facing", facing) ||
            model < 1 || model > 311)
        {
            cache_delete(c);
            CrimeContacts_Shutdown();
            print("[CRIME-02.2] NPC registry contains invalid data.");
            return false;
        }
        gCrimeContactActor[i] = CreateActor(model, gCrimeContactPos[i][0],
            gCrimeContactPos[i][1], gCrimeContactPos[i][2], facing);
        if (gCrimeContactActor[i] == INVALID_ACTOR_ID)
        {
            cache_delete(c);
            CrimeContacts_Shutdown();
            print("[CRIME-02.2] Failed to create NPC actor.");
            return false;
        }
        SetActorInvulnerable(gCrimeContactActor[i], true);
        gCrimeContactEnabled[i] = true;
    }
    cache_delete(c);
    print("[CRIME-02.2] Three world NPC intermediaries loaded.");
    return true;
#else
    return false;
#endif
}

stock CrimeContacts_Shutdown()
{
    for (new i; i < CRIME_CONTACT_COUNT; i++)
    {
        if (gCrimeContactEnabled[i]) DestroyActor(gCrimeContactActor[i]);
        gCrimeContactEnabled[i] = false;
        gCrimeContactActor[i] = INVALID_ACTOR_ID;
    }
    return 1;
}

stock bool:CrimeContacts_Near(playerid, contactId)
{
    if (contactId < 1 || contactId > CRIME_CONTACT_COUNT ||
        !gCrimeContactEnabled[contactId - 1] ||
        GetPlayerVirtualWorld(playerid) != 0 || GetPlayerInterior(playerid) != 0)
        return false;
    new i = contactId - 1;
    return IsPlayerInRangeOfPoint(playerid, CRIME_CONTACT_RADIUS,
        gCrimeContactPos[i][0], gCrimeContactPos[i][1], gCrimeContactPos[i][2]);
}

stock CrimeContacts_TemplateContact(templateId)
{
#if defined TARKENSO_DB_PRIMARY
    if (templateId <= 0) return 0;
    new q[240], rows = 0, contactId = 0;
    mysql_format(gDBPrimary, q, sizeof(q),
        "SELECT contact_id FROM crime_contract_templates WHERE template_id=%d AND mode_id=1 AND is_enabled=1",
        templateId);
    new Cache:c = mysql_query(gDBPrimary, q);
    if (c == Cache:0 || mysql_errno(gDBPrimary) ||
        !cache_get_row_count(rows) || rows != 1 ||
        !cache_get_value_name_int(0, "contact_id", contactId))
    {
        if (c != Cache:0) cache_delete(c);
        return 0;
    }
    cache_delete(c);
    return contactId;
#else
    #pragma unused templateId
    return 0;
#endif
}

stock bool:CrimeContacts_Progress(playerid, contactId, &trust, &completed, &introduced)
{
    trust = 0;
    completed = 0;
    introduced = 0;
#if defined TARKENSO_DB_PRIMARY
    if (!Crime_Available(playerid) ||
        contactId < 1 || contactId > CRIME_CONTACT_COUNT) return false;
    new q[280], rows = 0;
    mysql_format(gDBPrimary, q, sizeof(q),
        "SELECT trust,completed_count FROM crime_contact_progress WHERE account_id=%d AND contact_id=%d LIMIT 1",
        gDBPrimaryAccountId[playerid], contactId);
    new Cache:c = mysql_query(gDBPrimary, q);
    if (c == Cache:0 || mysql_errno(gDBPrimary) ||
        !cache_get_row_count(rows) || rows > 1)
    {
        if (c != Cache:0) cache_delete(c);
        return false;
    }
    if (rows == 1)
    {
        if (!cache_get_value_name_int(0, "trust", trust) ||
            !cache_get_value_name_int(0, "completed_count", completed))
        {
            cache_delete(c);
            return false;
        }
        introduced = 1;
    }
    cache_delete(c);
    return true;
#else
    #pragma unused playerid, contactId
    return false;
#endif
}

stock bool:CrimeContacts_Introduce(playerid, contactId)
{
#if defined TARKENSO_DB_PRIMARY
    if (!Crime_Available(playerid) ||
        !Leader_IsCriminal(Leader_PlayerOrg(playerid)) ||
        !CrimeContacts_Near(playerid, contactId)) return false;
    new q[390], changed;
    mysql_format(gDBPrimary, q, sizeof(q),
        "INSERT INTO crime_contact_progress(account_id,contact_id,trust,completed_count,first_met_at,last_met_at) SELECT %d,contact_id,0,0,UNIX_TIMESTAMP(),UNIX_TIMESTAMP() FROM crime_contacts WHERE contact_id=%d AND is_enabled=1 ON DUPLICATE KEY UPDATE last_met_at=UNIX_TIMESTAMP()",
        gDBPrimaryAccountId[playerid], contactId);
    return DBPrimary_Execute(q, changed);
#else
    #pragma unused playerid, contactId
    return false;
#endif
}
