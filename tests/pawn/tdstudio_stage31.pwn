#define TDS_TEST_MYSQL
#include "tdstudio_test_host.inc"
#include "tdstudio_movement.inc"

new aStep, aSource, aCopy, aCount, aLegacy, aMaximum;
new aExpected[TDS_ELEMENTS][E_TDS_ELEMENT], aGroups[TDS_GROUPS][E_TDS_GROUP];
new aNextGroup, aNextElement, aHandle[TDS_ELEMENTS];
#if defined TDS_TEST_RESTART
    #pragma unused aSource, aLegacy, aMaximum, aHandle
#endif
forward AdvancedNext();
forward OnQueryError(errorid, const error[], const callback[], const query[], MySQL:handle);

stock AdvancedSQL(const query[])
{
    new Cache:c = mysql_query(gDBPrimary, query);
    Check(c != Cache:0, "advanced fixture SQL executed");
    if (c != Cache:0) cache_delete(c);
    return 1;
}

stock AdvancedId(const name[])
{
    new query[256], id, hex[128]; TDS_Hex(name, hex, sizeof(hex));
    format(query, sizeof(query), "SELECT project_id FROM tdstudio_projects WHERE name=CONVERT(X'%s' USING cp1251)", hex);
    new Cache:c = mysql_query(gDBPrimary, query);
    Check(c != Cache:0 && cache_get_value_name_int(0, "project_id", id) != 0, "persisted advanced project located");
    if (c != Cache:0) cache_delete(c);
    return id;
}

stock AdvancedFixture()
{
    new s = TDS_Session(0); TDS_NewProject(0, "Advanced source");
    for (new e; e < 4; e++)
    {
        TDS_AddElement(0, E_TDS_TYPE:e);
        new input[32]; format(input, sizeof(input), "%d %d", 10+e*20, 20+e*20);
        TDS_ApplyInput(0, TDS_POSITION, input);
    }
    TDS_Select(0, 0); TDS_ApplyInput(0, TDS_CONTENT, "Логотип \"TARKENSO\" C:\\ui 100% ~g~текст");
    TDS_SelectAll(0, true); Check(TDS_GroupCreate(0, "Логотип TARKENSO") == 1, "persistent four-type logo group");
    TDS_Select(0, 0); new copy = TDS_Duplicate(0), extra = TDS_AddElement(0, TDS_TEXT_TYPE);
    TDS_ApplyInput(0, TDS_POSITION, "310 270"); TDS_Select(0, copy); TDS_Select(0, extra, true);
    Check(TDS_GroupCreate(0, "Details") == 2, "persistent second group");
    TDS_GroupSelect(0, 1); TDS_SetStep(0, 2); TDSInput_Start(0); MovePacket(0); MovePacket(0, KEY_UP, KEY_RIGHT);
    // Use exact cardinal displacement in the saved fixture; diagonal behavior is
    // covered by the separate movement suite.
    TDSInput_Finish(0, false); TDSInput_Start(0); MovePacket(0);
    MovePacket(0, 0, KEY_RIGHT); MovePacket(0, KEY_UP); TDSInput_OnKeys(0, KEY_YES);
    Check(TDS_Layer(0, TDS_LAYER_FRONT), "persistent group brought to front");
    Check(TDS_ProjectValid(s), "advanced fixture validates");
    return s;
}

stock AdvancedSnapshot(s)
{
    new slots[TDS_ELEMENTS]; aCount = TDS_Order(s, slots);
    for (new e; e < aCount; e++) for (new E_TDS_ELEMENT:f; f < E_TDS_ELEMENT; f++) aExpected[e][f] = gTDSElement[s][slots[e]][f];
    for (new g; g < TDS_GROUPS; g++) for (new E_TDS_GROUP:f; f < E_TDS_GROUP; f++) aGroups[g][f] = gTDSGroup[s][g][f];
    aNextGroup = gTDS[s][tsNextGroupId]; aNextElement = gTDS[s][tsNextId];
    return 1;
}

stock AdvancedCompare(s)
{
    Check(TDS_ProjectValid(s) && gTDS[s][tsCount] == aCount && gTDS[s][tsNextGroupId] == aNextGroup &&
        gTDS[s][tsNextId] == aNextElement, "advanced metadata/next IDs restored");
    new slots[TDS_ELEMENTS]; TDS_Order(s, slots);
    for (new i; i < aCount; i++)
    {
        new e = slots[i];
        for (new f; f < sizeof(gTDSFields); f++)
        {
            new E_TDS_ELEMENT:field = gTDSFields[f];
            if (TDS_FloatField(field)) Check(floatabs(Float:gTDSElement[s][e][field]-Float:aExpected[i][field]) < 0.000001, "advanced float roundtrip");
            else Check(gTDSElement[s][e][field] == aExpected[i][field], "advanced scalar/order/ID roundtrip");
        }
        Check(gTDSElement[s][e][tdGroup] == aExpected[i][tdGroup] && !strcmp(gTDSElement[s][e][tdText], aExpected[i][tdText], false),
            "group membership and CP1251 content roundtrip");
    }
    new groups;
    for (new g; g < TDS_GROUPS; g++) if (aGroups[g][tgId])
    {
        new at = TDS_GroupSlot(s, aGroups[g][tgId]); groups++;
        Check(at >= 0 && !strcmp(gTDSGroup[s][at][tgName], aGroups[g][tgName], false), "stable group ID/name restored");
    }
    Check(gTDS[s][tsGroupCount] == groups, "group count restored");
    return 1;
}

stock bool:AdvancedPending()
{
    for (new j; j < TDS_JOB_LIMIT; j++) if (gTDSJob[j][tjUsed]) return true;
    return false;
}

main() {}
public OnGameModeInit()
{
    gDBPrimary = mysql_connect_file();
    if (gDBPrimary == MySQL:0) { print("TDS FAIL: advanced MySQL connection"); return 0; }
    TDS_Init(); Ready(0, true); Ready(1); tNow = 1000;
    SetTimer("AdvancedNext", 50, true);
    return 1;
}
public OnQueryError(errorid, const error[], const callback[], const query[], MySQL:handle)
{
    #pragma unused errorid, error, handle
    if (!TDSDB_OnError(callback, query)) { print("TDS FAIL: advanced unhandled MySQL error"); tFailures++; }
    return 1;
}

public AdvancedNext()
{
    if (aStep == 99) return 1;
    tNow += 50;
    if (!gTDSDBReady) return 1;
    new s = TDS_Session(0);
    if (s >= 0 && gTDS[s][tsBusy]) return 1;
#if defined TDS_TEST_RESTART
    if (!aStep)
    {
        TDS_Open(0); s = AdvancedFixture(); AdvancedSnapshot(s); TDS_ClearProject(s);
        aCopy = AdvancedId("Advanced saved copy"); TDSDB_Load(0, aCopy); aStep = 1; return 1;
    }
    AdvancedCompare(s);
    Check(!gTDS[s][tsDirty] && gTDS[s][tsInterface] == 0, "new process restored global grouped project cleanly");
    TDS_Shutdown(); mysql_close(gDBPrimary); aStep = 99;
    printf("TDS TEST RESULT: checks=%d failures=%d", tChecks, tFailures);
    return 1;
#else
    new sql[256];
    switch (aStep)
    {
        case 0:
        {
            TDS_Open(0); s = AdvancedFixture(); AdvancedSnapshot(s); TDSDB_Save(0);
            Check(gTDS[s][tsBusy] > 0 && !TDS_GroupRename(0, 1, "Busy") && TDS_Duplicate(0) < 0 &&
                !TDS_Layer(0, TDS_LAYER_BACK), "async save excludes conflicting layout changes");
        }
        case 1:
        {
            aSource = gTDS[s][tsProjectId];
            Check(aSource > 0 && !gTDS[s][tsDirty] && gTDS[s][tsGroupCount] == 2, "grouped save committed");
            TDS_Export(0);
        }
        case 2: { TDS_Close(0); tNow += 600; TDS_Open(0); TDSDB_Load(0, aSource); }
        case 3:
        {
            AdvancedCompare(s);
            Check(gTDS[s][tsSelectedGroup] == 0 && TDS_PickCount(s) == 1, "editor selection is not serialized");
            TDS_GroupSelect(0, 1); TDSInput_Start(0); MovePacket(0); MovePacket(0, KEY_DOWN); TDSInput_Finish(0, false);
            AdvancedCompare(s);
            format(gTDS[s][tsName], TDS_NAME, "Advanced saved copy"); gTDS[s][tsInterface] = 0; gTDS[s][tsDirty] = true;
            TDSDB_Save(0, true);
        }
        case 4:
        {
            aCopy = gTDS[s][tsProjectId];
            Check(aCopy > 0 && aCopy != aSource && gTDS[s][tsRevision] == 1 && !gTDS[s][tsDirty], "Save As creates independent project");
            AdvancedCompare(s); TDS_Export(0);
        }
        case 5: { TDS_Close(0); tNow += 600; TDS_Open(0); TDSDB_Load(0, aCopy); }
        case 6:
        {
            AdvancedCompare(s);
            new before = gTDSJobSerial, group = gTDSElement[s][0][tdGroup];
            gTDSElement[s][0][tdGroup] = 999;
            Check(!TDSDB_Save(0) && gTDSJobSerial == before, "invalid in-memory membership cannot enter SQL queue");
            gTDSElement[s][0][tdGroup] = group;
            for (new e; e < aCount; e++) aHandle[e] = _:gTDSDraw[s][e];
            format(sql, sizeof(sql), "INSERT INTO tdstudio_groups(project_id,group_id,name) VALUES(%d,99,'Empty corrupt group')", aCopy); AdvancedSQL(sql);
            TDSDB_Load(0, aCopy);
        }
        case 7:
        {
            new bool:kept = gTDS[s][tsGroupCount] == 2;
            for (new e; e < aCount; e++) if (_:gTDSDraw[s][e] != aHandle[e]) kept = false;
            Check(kept, "corrupt empty group load preserves all old groups and handles");
            format(sql, sizeof(sql), "DELETE FROM tdstudio_groups WHERE project_id=%d AND group_id=99", aCopy); AdvancedSQL(sql);
            tFailAfter = 1; TDSDB_Load(0, aSource);
        }
        case 8:
        {
            Check(tFailAfter == 0 && gTDS[s][tsProjectId] == aCopy && gTDS[s][tsGroupCount] == 2 && gTDS[s][tsInterface] == 0,
                "grouped load allocation failure preserves previous snapshot");
            tFailAfter = -1;
            TDS_Grant(0, 1, true); TDS_Open(1); TDS_NewProject(1, "Own draft"); TDSDB_Load(1, aCopy);
        }
        case 9:
        {
            if (AdvancedPending()) return 1;
            new other = TDS_Session(1);
            Check(other >= 0 && !gTDS[other][tsProjectId] && !gTDS[other][tsGroupCount], "foreign developer cannot load grouped project");
            TDS_Close(1); aLegacy = AdvancedId("Legacy format1"); TDSDB_Load(0, aLegacy);
        }
        case 10:
        {
            Check(gTDS[s][tsProjectId] == aLegacy && gTDS[s][tsFormat] == 2 && gTDS[s][tsGroupCount] == 0 &&
                gTDS[s][tsNextGroupId] == 1 && gTDS[s][tsCount] == 1 && !gTDSElement[s][0][tdGroup] &&
                gTDSElement[s][0][tdX] == 12.5 && gTDSElement[s][0][tdY] == 34.25,
                "real v1 stored-routine project loads with empty groups and unchanged coordinates");
            Check(!strcmp(gTDSElement[s][0][tdText], "Старый интерфейс", false), "legacy CP1251 text preserved");
            TDSDB_Save(0);
        }
        case 11:
        {
            Check(gTDS[s][tsRevision] == 2 && !gTDS[s][tsDirty], "v1 upgrade saved through revision-checked v2 procedure");
            TDS_ClearProject(s); TDS_NewProject(0, "Maximum grouped");
            new longest[TDS_TEXT]; for (new i; i < TDS_TEXT-1; i++) longest[i] = i%2 ? 92 : 34; longest[0] = 223;
            for (new e; e < TDS_ELEMENTS; e++) { TDS_AddElement(0, TDS_TEXT_TYPE); TDS_ApplyInput(0, TDS_CONTENT, longest); }
            for (new g; g < TDS_GROUPS; g++)
            {
                new name[TDS_NAME]; format(name, sizeof(name), "Maximum_group_%d", g);
                TDS_Select(0, g*2); TDS_Select(0, g*2+1, true); Check(TDS_GroupCreate(0, name) > 0, "maximum grouped fixture");
            }
            Check(TDS_ProjectValid(s), "maximum groups/elements payload validates"); TDSDB_Save(0);
        }
        case 12:
        {
            aMaximum = gTDS[s][tsProjectId];
            Check(aMaximum > 0 && !gTDS[s][tsDirty] && gTDS[s][tsGroupCount] == 16, "maximum grouped payload committed");
            TDS_Close(0); tNow += 600; TDS_Open(0); TDSDB_Load(0, aMaximum);
        }
        case 13:
        {
            Check(gTDS[s][tsCount] == 64 && gTDS[s][tsGroupCount] == 16 && gTDS[s][tsNextGroupId] == 17 &&
                TDS_ProjectValid(s), "maximum grouped payload loaded");
            TDS_Export(0);
        }
        case 14:
        {
            TDSDB_Load(0, aCopy); TDS_Close(0); tNow += 600; TDS_Open(0); TDS_NewProject(0, "Safe reopen");
        }
        case 15:
        {
            if (AdvancedPending()) return 1;
            Check(!gTDS[s][tsGroupCount] && !gTDS[s][tsProjectId] && !strcmp(gTDS[s][tsName], "Safe reopen", false),
                "late grouped load cannot populate a new lease");
            TDS_Shutdown(); mysql_close(gDBPrimary); aStep = 99;
            Check(tCreate == tDestroy && !tBadHandle, "advanced lifecycle frees all owned resources");
            printf("TDS TEST RESULT: checks=%d failures=%d", tChecks, tFailures); return 1;
        }
    }
    aStep++;
    return 1;
#endif
}
