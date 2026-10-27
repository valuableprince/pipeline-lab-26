#define TDS_TEST_MYSQL
#include "tdstudio_test_host.inc"
#include "tdstudio_movement.inc"

new zStep, zSource, zCopy;
new zExpected[5][E_TDS_ELEMENT];
#if defined TDS_TEST_RESTART
    #pragma unused zSource
#endif
forward GeometryNext();
forward OnQueryError(errorid, const error[], const callback[], const query[], MySQL:handle);

stock GeometryId(const name[])
{
    new query[256], id, hex[128]; TDS_Hex(name, hex, sizeof(hex));
    format(query, sizeof(query), "SELECT project_id FROM tdstudio_projects WHERE name=CONVERT(X'%s' USING cp1251)", hex);
    new Cache:c = mysql_query(gDBPrimary, query);
    Check(c != Cache:0 && cache_get_value_name_int(0, "project_id", id) != 0, "geometry project found in real database");
    if (c != Cache:0) cache_delete(c);
    return id;
}

stock GeometryFixture()
{
    new s = TDS_Session(0); TDS_NewProject(0, "Geometry source");
    for (new e; e < 4; e++) TDS_AddElement(0, E_TDS_TYPE:e);
    TDS_Select(0, 0); TDS_GeoExact(0, "100 100 40 20 0.2 1"); TDS_ApplyInput(0, TDS_ALIGN, "2");
    TDS_ApplyInput(0, TDS_CONTENT, "Геометрия \"TARKENSO\" C:\\ui 100% ~g~текст");
    TDS_Select(0, 1); TDS_GeoExact(0, "160 100 30 20 0.3 1.2");
    TDS_Select(0, 2); TDS_GeoExact(0, "100 140 40 30 0.3 1.2");
    TDS_Select(0, 3); TDS_GeoExact(0, "160 140 30 40 0.3 1.2");
    TDS_ApplyInput(0, TDS_ROTATION, "10 -20 30 1.5");
    TDS_SelectAll(0, true); Check(TDS_GroupCreate(0, "Геометрия TARKENSO") == 1, "geometry four-type group");
    new jobs = gTDSJobSerial;
    Check(TDS_GeoScale(0, 2.0, 0.5) && TDS_GeoScreen(0, 0), "geometry scales then screen-centers group");
    TDS_GridStep(0, 2); gTDS[s][tsSnap] = true;
    Check(TDS_GeoPosition(0, 333.0, 187.0), "geometry snaps common group center");
    gTDS[s][tsSnap] = false;
    TDS_Select(0, 2); TDSInput_Start(0, TDS_TRANS_RESIZE); MovePacket(0); MovePacket(0, 0, KEY_RIGHT); MovePacket(0, KEY_DOWN);
    TDSInput_Finish(0, true);
    TDS_Select(0, 0); gTDS[s][tsResizeStep] = 0;
    TDSInput_Start(0, TDS_TRANS_RESIZE, true); MovePacket(0); MovePacket(0, 0, KEY_RIGHT); TDSInput_Finish(0, true);
    Check(gTDSJobSerial == jobs, "all geometry previews/commits perform zero SQL jobs");
    TDS_Select(0, 3); Check(TDS_GeoExact(0, "380.125 190.25 60.125 20.375 0.3 1.2"), "fractional model geometry remains editable independently within group");
    TDS_AddElement(0, TDS_RECT_TYPE); TDS_GeoExact(0, "20 20 10 10 0.3 1.2");
    TDS_GroupSelect(0, 1); Check(TDS_Layer(0, TDS_LAYER_FRONT), "geometry grouped layers canonicalized");
    Check(TDS_ProjectValid(s) && gTDSElement[s][0][tdX] == 260.0 && gTDSElement[s][0][tdY] == 170.0 &&
        gTDSElement[s][2][tdW] == 81.0 && gTDSElement[s][2][tdH] == 16.0, "geometry expected transformed anchors and sizes");
    return s;
}

stock GeometrySnapshot(s)
{
    new slots[TDS_ELEMENTS]; Check(TDS_Order(s, slots) == 5, "geometry fixture has five ordinary TDs");
    for (new i; i < 5; i++) for (new E_TDS_ELEMENT:f; f < E_TDS_ELEMENT; f++) zExpected[i][f] = gTDSElement[s][slots[i]][f];
    return 1;
}

stock GeometryCompare(s)
{
    Check(TDS_ProjectValid(s) && gTDS[s][tsCount] == 5 && gTDS[s][tsGroupCount] == 1 &&
        gTDS[s][tsFormat] == 2 && gTDS[s][tsNextId] == 6 && gTDS[s][tsNextGroupId] == 2, "geometry format2/group/ID metadata roundtrip");
    new slots[TDS_ELEMENTS]; TDS_Order(s, slots);
    for (new i; i < 5; i++)
    {
        new e = slots[i];
        for (new f; f < sizeof(gTDSFields); f++)
        {
            new E_TDS_ELEMENT:field = gTDSFields[f];
            if (TDS_FloatField(field)) Check(floatabs(Float:gTDSElement[s][e][field]-Float:zExpected[i][field]) < 0.00001, "geometry float restored from MySQL");
            else Check(gTDSElement[s][e][field] == zExpected[i][field], "geometry scalar/layer/ID restored from MySQL");
        }
        Check(gTDSElement[s][e][tdGroup] == zExpected[i][tdGroup] &&
            !strcmp(gTDSElement[s][e][tdText], zExpected[i][tdText], false), "geometry group link and CP1251 text restored");
    }
    new g = TDS_GroupSlot(s, 1);
    Check(g >= 0 && !strcmp(gTDSGroup[s][g][tgName], "Геометрия TARKENSO", false) && TDS_GroupMembers(s, 1) == 4,
        "geometry logical group name/membership restored");
    return 1;
}

main() {}
public OnGameModeInit()
{
    gDBPrimary = mysql_connect_file();
    if (gDBPrimary == MySQL:0) { print("TDS FAIL: geometry MySQL connection"); return 0; }
    TDS_Init(); Ready(0, true); tNow = 1000;
    SetTimer("GeometryNext", 50, true);
    return 1;
}
public OnQueryError(errorid, const error[], const callback[], const query[], MySQL:handle)
{
    #pragma unused errorid, error, handle
    if (!TDSDB_OnError(callback, query)) { print("TDS FAIL: geometry unhandled MySQL error"); tFailures++; }
    return 1;
}

public GeometryNext()
{
    if (zStep == 99) return 1;
    tNow += 50;
    if (!gTDSDBReady) return 1;
    new s = TDS_Session(0);
    if (s >= 0 && gTDS[s][tsBusy]) return 1;
#if defined TDS_TEST_RESTART
    if (!zStep)
    {
        TDS_Open(0); s = GeometryFixture(); GeometrySnapshot(s);
        TDS_Close(0); tNow += 600; TDS_Open(0); zCopy = GeometryId("Geometry saved copy");
        TDSDB_Load(0, zCopy); zStep = 1; return 1;
    }
    GeometryCompare(s);
    Check(!gTDS[s][tsDirty] && !gTDS[s][tsGridShow] && !gTDS[s][tsSnap] && gTDS[s][tsInterface] == 0,
        "new server process restores project geometry without editor settings");
#else
    switch (zStep)
    {
        case 0:
        {
            TDS_Open(0); s = GeometryFixture(); GeometrySnapshot(s); TDSDB_Save(0);
            Check(gTDS[s][tsBusy] > 0 && !TDS_GeoScale(0, 2.0, 2.0) && !TDS_GeoScreen(0, 0) &&
                !TDS_GeoPosition(0, 100.0, 100.0) && !TDSInput_Start(0, TDS_TRANS_SCALE), "async save excludes geometric operations");
        }
        case 1:
        {
            zSource = gTDS[s][tsProjectId];
            Check(zSource > 0 && !gTDS[s][tsDirty], "geometric project transaction committed"); TDS_Export(0);
        }
        case 2: { TDS_Close(0); tNow += 600; TDS_Open(0); TDSDB_Load(0, zSource); }
        case 3:
        {
            GeometryCompare(s); TDS_GroupSelect(0, 1); gTDS[s][tsDirty] = false;
            TDSInput_Start(0, TDS_TRANS_SCALE); MovePacket(0); MovePacket(0, KEY_DOWN); TDSInput_Finish(0, false);
            GeometryCompare(s); Check(!gTDS[s][tsDirty], "loaded group scale cancel retains clean revision");
            format(gTDS[s][tsName], TDS_NAME, "Geometry saved copy"); gTDS[s][tsInterface] = 0; gTDS[s][tsDirty] = true;
            TDSDB_Save(0, true);
        }
        case 4:
        {
            zCopy = gTDS[s][tsProjectId]; Check(zCopy > 0 && zCopy != zSource && !gTDS[s][tsDirty], "geometry Save As creates independent grouped project");
            GeometryCompare(s); TDS_Export(0);
        }
        case 5:
        {
            new sql[160]; format(sql, sizeof(sql), "UPDATE tdstudio_projects SET revision=revision+1 WHERE project_id=%d", zCopy);
            new Cache:c = mysql_query(gDBPrimary, sql); if (c != Cache:0) cache_delete(c);
            TDS_Select(0, 0); TDS_GeoExact(0, "260 170 90 10 0.5 0.5"); TDSDB_Save(0);
        }
        case 6:
        {
            Check(gTDS[s][tsDirty] && gTDS[s][tsRevision] == 1 && gTDSElement[s][0][tdW] == 90.0,
                "revision conflict does not claim geometry saved or overwrite working values");
            TDSDB_Load(0, zCopy);
        }
        case 7:
        {
            GeometryCompare(s); Check(gTDS[s][tsRevision] == 2 && !gTDS[s][tsDirty], "conflict reload restores last committed geometry");
        }
    }
    if (zStep++ < 7) return 1;
#endif
    TDS_Shutdown(); mysql_close(gDBPrimary); zStep = 99;
    Check(tCreate == tDestroy && !tBadHandle && !tDialogOverflow, "geometry DB lifecycle releases every resource");
    printf("TDS TEST RESULT: checks=%d failures=%d", tChecks, tFailures);
    return 1;
}
