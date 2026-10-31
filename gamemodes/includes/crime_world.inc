#if defined _tarkenso_crime_world_included
    #endinput
#endif
#define _tarkenso_crime_world_included

// CRIME-02.5 | 3 persistent witnesses + one personal clue object per player.
// World geometry is read from templates, never trusted to the client.
// No per-tick SQL and no global script checkpoint or map-icon slot usage.
#define CRIME_WORLD_COUNT (3)

new bool:gCrimeWitnessEnabled[CRIME_WORLD_COUNT];
new gCrimeWitnessActor[CRIME_WORLD_COUNT];
new Float:gCrimeWitnessPos[CRIME_WORLD_COUNT][3];
new bool:gCrimePropEnabled[MAX_PLAYERS];
new gCrimePropObject[MAX_PLAYERS];
new gCrimePropContract[MAX_PLAYERS];

stock CrimeWorld_Reset(playerid)
{
    if (gCrimePropEnabled[playerid])
        DestroyPlayerObject(playerid, gCrimePropObject[playerid]);
    gCrimePropEnabled[playerid] = false;
    gCrimePropObject[playerid] = 0;
    gCrimePropContract[playerid] = 0;
    return 1;
}

stock CrimeWorld_Shutdown()
{
    for (new i; i < CRIME_WORLD_COUNT; i++)
    {
        if (gCrimeWitnessEnabled[i]) DestroyActor(gCrimeWitnessActor[i]);
        gCrimeWitnessEnabled[i] = false;
        gCrimeWitnessActor[i] = 0;
    }
    return 1;
}

stock bool:CrimeWorld_Init()
{
#if defined TARKENSO_DB_PRIMARY
    new Cache:c = mysql_query(gDBPrimary,
        "SELECT scenario_kind,delivery_x,delivery_y,delivery_z FROM crime_contract_templates WHERE template_id IN(4,5,6) AND is_enabled=1 ORDER BY scenario_kind");
    new rows = 0;
    if (c == Cache:0 || mysql_errno(gDBPrimary) ||
        !cache_get_row_count(rows) || rows != CRIME_WORLD_COUNT)
    {
        if (c != Cache:0) cache_delete(c);
        print("[CRIME-02.5] Witness locations unavailable.");
        return false;
    }
    // A reliable stock GTA skin for each role: courier witness, security guard, negotiator.
    new skins[CRIME_WORLD_COUNT] = { 50, 71, 59 };
    for (new i; i < CRIME_WORLD_COUNT; i++)
    {
        new kind = 0;
        if (!cache_get_value_name_int(i, "scenario_kind", kind) ||
            kind != i+1 ||
            !cache_get_value_name_float(i, "delivery_x", gCrimeWitnessPos[i][0]) ||
            !cache_get_value_name_float(i, "delivery_y", gCrimeWitnessPos[i][1]) ||
            !cache_get_value_name_float(i, "delivery_z", gCrimeWitnessPos[i][2]))
        {
            cache_delete(c);
            CrimeWorld_Shutdown();
            print("[CRIME-02.5] Invalid witness geometry.");
            return false;
        }
        gCrimeWitnessActor[i] = CreateActor(skins[i],
            gCrimeWitnessPos[i][0], gCrimeWitnessPos[i][1],
            gCrimeWitnessPos[i][2], 90.0);
        if (gCrimeWitnessActor[i] == INVALID_ACTOR_ID)
        {
            cache_delete(c);
            CrimeWorld_Shutdown();
            print("[CRIME-02.5] Failed to create witness actor.");
            return false;
        }
        SetActorInvulnerable(gCrimeWitnessActor[i], true);
        gCrimeWitnessEnabled[i] = true;
    }
    cache_delete(c);
    print("[CRIME-02.5] Three NPC scene participants created.");
    return true;
#else
    return false;
#endif
}

// Visual aid only. Server validates exact mission, account and distance again.
stock CrimeWorld_ShowClue(playerid, contractId, kind, phase, Float:x, Float:y, Float:z)
{
    if (phase != 1 || kind < 1 || kind > CRIME_WORLD_COUNT ||
        !Crime_Available(playerid) ||
        GetPlayerInterior(playerid) != 0 ||
        GetPlayerVirtualWorld(playerid) != 0)
    {
        CrimeWorld_Reset(playerid);
        return 1;
    }
    if (gCrimePropEnabled[playerid] &&
        gCrimePropContract[playerid] == contractId) return 1;
    CrimeWorld_Reset(playerid);
    new models[CRIME_WORLD_COUNT] = { 1210, 1271, 2894 };
    gCrimePropObject[playerid] = CreatePlayerObject(playerid,
        models[kind-1], x, y, z - 0.8, 0.0, 0.0, 0.0, 80.0);
    if (gCrimePropObject[playerid] == INVALID_OBJECT_ID)
    {
        gCrimePropObject[playerid] = 0;
        return 0;
    }
    gCrimePropEnabled[playerid] = true;
    gCrimePropContract[playerid] = contractId;
    return 1;
}

stock bool:CrimeWorld_EvidenceReady(playerid, id)
{
#if defined TARKENSO_DB_PRIMARY
    if (!Crime_Available(playerid) || id <= 0) return false;
    new q[210], rows = 0, timestamp = 0;
    mysql_format(gDBPrimary, q, sizeof(q),
        "SELECT evidence_found_at FROM crime_contracts WHERE contract_id=%d AND account_id=%d AND state=1 LIMIT 1",
        id, gDBPrimaryAccountId[playerid]);
    new Cache:c = mysql_query(gDBPrimary, q);
    if (c == Cache:0 || mysql_errno(gDBPrimary) ||
        !cache_get_row_count(rows) || rows != 1 ||
        !cache_get_value_name_int(0, "evidence_found_at", timestamp))
    {
        if (c != Cache:0) cache_delete(c);
        return false;
    }
    cache_delete(c);
    return timestamp > 0;
#else
    #pragma unused playerid, id
    return false;
#endif
}

stock bool:CrimeWorld_Collect(playerid, id, faction)
{
#if defined TARKENSO_DB_PRIMARY
    if (!Crime_Available(playerid) || gGovTransferBusy[playerid] ||
        !Leader_IsCriminal(Leader_PlayerOrg(playerid)) ||
        Leader_PlayerOrg(playerid) != faction) return false;
    if (CrimeWorld_EvidenceReady(playerid, id)) return true;
    new q[430], affected = 0;
    if (!Crime_Exec("START TRANSACTION")) return false;
    mysql_format(gDBPrimary, q, sizeof(q),
        "UPDATE crime_contracts SET evidence_found_at=UNIX_TIMESTAMP() WHERE contract_id=%d AND account_id=%d AND faction_id=%d AND scenario_kind IN(1,2,3) AND state=1 AND evidence_found_at=0 AND expires_at>UNIX_TIMESTAMP()",
        id, gDBPrimaryAccountId[playerid], faction);
    if (!DBPrimary_Execute(q, affected) || affected != 1)
    {
        Crime_Rollback();
        return false;
    }
    mysql_format(gDBPrimary, q, sizeof(q),
        "INSERT INTO crime_contract_events(contract_id,actor_account_id,event_type) VALUES (%d,%d,'EVIDENCE_FOUND')",
        id, gDBPrimaryAccountId[playerid]);
    if (!DBPrimary_Execute(q, affected) || affected != 1)
    {
        Crime_Rollback();
        return false;
    }
    if (!Crime_Exec("COMMIT")) return false;
    return true;
#else
    #pragma unused playerid, id, faction
    return false;
#endif
}


stock bool:CrimeWorld_InterviewInfo(playerid, id, &question, &interviewedAt)
{
    question = 0;
    interviewedAt = 0;
#if defined TARKENSO_DB_PRIMARY
    if (!Crime_Available(playerid) || id <= 0) return false;
    new q[260], rows = 0;
    mysql_format(gDBPrimary, q, sizeof(q),
        "SELECT interview_question,interviewed_at FROM crime_contracts WHERE account_id=%d AND contract_id=%d AND scenario_kind IN(1,2,3) AND state=2 LIMIT 1",
        gDBPrimaryAccountId[playerid], id);
    new Cache:c = mysql_query(gDBPrimary, q);
    if (c == Cache:0 || mysql_errno(gDBPrimary) ||
        !cache_get_row_count(rows) || rows != 1 ||
        !cache_get_value_name_int(0, "interview_question", question) ||
        !cache_get_value_name_int(0, "interviewed_at", interviewedAt))
    {
        if (c != Cache:0) cache_delete(c);
        return false;
    }
    cache_delete(c);
    return question >= 0 && question <= 2 && interviewedAt >= 0;
#else
    #pragma unused playerid, id
    return false;
#endif
}

stock bool:CrimeWorld_InterviewReady(playerid, id)
{
    new question, interviewedAt;
    return CrimeWorld_InterviewInfo(playerid, id, question, interviewedAt) &&
        interviewedAt > 0;
}

stock bool:CrimeWorld_Interview(playerid, id, faction, question)
{
#if defined TARKENSO_DB_PRIMARY
    if (!Crime_Available(playerid) || gGovTransferBusy[playerid] ||
        !Leader_IsCriminal(faction) ||
        faction != Leader_PlayerOrg(playerid) ||
        (question != 1 && question != 2) ||
        IsPlayerInAnyVehicle(playerid) ||
        GetPlayerInterior(playerid) != 0 ||
        GetPlayerVirtualWorld(playerid) != 0) return false;

    // The position and stage are checked a second time on the dialog click,
    // rather than trusting the NPC interaction which originally opened it.
    new contractId, phase, contractFaction, reward, deadline, title[68];
    new Float:pickup[3], Float:delivery[3];
    if (!Crime_Expire(playerid) ||
        !Crime_LoadActive(playerid, contractId, phase, contractFaction,
            reward, deadline, pickup, delivery, title, sizeof(title)) ||
        id != contractId || phase != 2 || faction != contractFaction ||
        !IsPlayerInRangeOfPoint(playerid, CRIME_WORLD_RADIUS,
            delivery[0], delivery[1], delivery[2])) return false;

    new q[450], affected = 0;
    if (!Crime_Exec("START TRANSACTION")) return false;
    mysql_format(gDBPrimary, q, sizeof(q),
        "UPDATE crime_contracts SET interview_question=%d,interviewed_at=UNIX_TIMESTAMP() WHERE contract_id=%d AND account_id=%d AND faction_id=%d AND scenario_kind IN(1,2,3) AND state=2 AND scenario_choice IN(1,2) AND interviewed_at=0 AND expires_at>UNIX_TIMESTAMP()",
        question, id, gDBPrimaryAccountId[playerid], faction);
    if (!DBPrimary_Execute(q, affected) || affected != 1)
    {
        Crime_Rollback();
        return false;
    }
    mysql_format(gDBPrimary, q, sizeof(q),
        "INSERT INTO crime_contract_events(contract_id,actor_account_id,event_type) VALUES (%d,%d,'WITNESS_INTERVIEWED')",
        id, gDBPrimaryAccountId[playerid]);
    if (!DBPrimary_Execute(q, affected) || affected != 1)
    {
        Crime_Rollback();
        return false;
    }
    if (!Crime_Exec("COMMIT")) return false;
    return true;
#else
    #pragma unused playerid, id, faction, question
    return false;
#endif
}

// KEY_YES is the standard GTA Y/affirmative interaction key.
// The mission is optional and this callback never consumes unrelated keys.
stock CrimeWorld_OnKeys(playerid, KEY:newkeys, KEY:oldkeys)
{
    if (!(newkeys & KEY_YES) || (oldkeys & KEY_YES)) return 0;
    if (!Crime_Available(playerid) || IsPlayerInAnyVehicle(playerid) ||
        GetPlayerVirtualWorld(playerid) != 0 || GetPlayerInterior(playerid) != 0 ||
        gCrimeUIDialog[playerid] != 0) return 0;

    new id, phase, faction, reward, deadline, title[68];
    new Float:pickup[3], Float:delivery[3];
#if defined TARKENSO_DB_PRIMARY
    if (!Crime_Expire(playerid) ||
        !Crime_LoadActive(playerid, id, phase, faction, reward,
            deadline, pickup, delivery, title, sizeof(title)) || !id)
        return 0;
#else
    return 0;
#endif
    if (faction != Leader_PlayerOrg(playerid)) return 0;
    new kind, choice;
    if (!CrimeScenario_Info(playerid, id, kind, choice) ||
        kind < 1 || kind > CRIME_WORLD_COUNT) return 0;

    new Float:x = phase == 1 ? pickup[0] : delivery[0],
        Float:y = phase == 1 ? pickup[1] : delivery[1],
        Float:z = phase == 1 ? pickup[2] : delivery[2];
    if (!IsPlayerInRangeOfPoint(playerid, CRIME_WORLD_RADIUS, x, y, z)) return 0;
    if (phase == 2 && !gCrimeWitnessEnabled[kind-1]) return 0;
    if (phase == 1 && !CrimeWorld_Collect(playerid, id, faction))
    {
        SendClientMessage(playerid, CRIME_ERROR,
            "Улику не удалось зарегистрировать. Попробуйте ещё раз.");
        return 1;
    }
    if (phase == 2) CrimeUI_Witness(playerid);
    else CrimeUI_Scene(playerid);
    return 1;
}
