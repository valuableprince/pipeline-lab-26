#define TDS_TEST_MYSQL
#include "tdstudio_test_host.inc"
#include "tdstudio_movement.inc"

new hStep, hPrivate, hShared, hOther, hProject, hCopy;
new hExpected[8][E_TDS_ELEMENT], hGroups[TDS_GROUPS][E_TDS_GROUP];
#if !defined TDS_TEST_RESTART
    new bool:hDescriptionEdited, bool:hBadVersion;
#endif
#if defined TDS_TEST_RESTART
    #pragma unused hShared, hOther, hProject
#endif
forward LibraryNext();
forward OnQueryError(errorid, const error[], const callback[], const query[], MySQL:handle);

stock LibrarySQL(const query[])
{
    new Cache:c = mysql_query(gDBPrimary,query); Check(c != Cache:0,"library fixture SQL executed");
    if (c != Cache:0) cache_delete(c);
    return 1;
}

stock LibraryId(const name[], bool:project = false)
{
    new hex[128], sql[300], id; TDS_Hex(name,hex,sizeof(hex));
    format(sql,sizeof(sql),"SELECT %s AS id FROM %s WHERE name=CONVERT(X'%s' USING cp1251)",
        project ? ("project_id") : ("template_id"), project ? ("tdstudio_projects") : ("tdstudio_templates"),hex);
    new Cache:c = mysql_query(gDBPrimary,sql);
    Check(c != Cache:0 && cache_get_value_name_int(0,"id",id) != 0 && id > 0,"persisted library/project ID found");
    if (c != Cache:0) cache_delete(c);
    return id;
}

stock bool:LibraryPending()
{
    for (new j; j < TDS_JOB_LIMIT; j++) if (gTDSJob[j][tjUsed]) return true;
    return false;
}

stock LibrarySource()
{
    new s = TDS_Session(0); TDS_ClearProject(s); TDS_NewProject(0,"Template source");
    for (new i; i < 4; i++) TDS_AddElement(0,E_TDS_TYPE:i);
    TDS_Select(0,0); TDS_GeoExact(0,"10 20 40 20 0.2 1");
    TDS_ApplyInput(0,TDS_CONTENT,"Шаблон \"TARKENSO\" C:\\ui 100% ~g~текст");
    TDS_Select(0,1); TDS_GeoExact(0,"30 40 30 20 0.3 1.2");
    TDS_Select(0,2); TDS_GeoExact(0,"50 60 40 30 0.3 1.2");
    TDS_Select(0,3); TDS_GeoExact(0,"70 80 30 40 0.3 1.2"); TDS_ApplyInput(0,TDS_ROTATION,"10 -20 30 1.5");
    TDS_Select(0,0); TDS_Select(0,2,true); TDS_GroupCreate(0,"Логотип");
    TDS_Select(0,1); TDS_Select(0,3,true); TDS_GroupCreate(0,"Декор"); TDS_SelectAll(0,true);
    Check(TDS_ProjectValid(s) && TDS_PickCount(s) == 4,"library four-type two-group source validates");
    return s;
}

stock LibraryFields(playerid, const name[], visibility)
{
    new s = TDS_Session(playerid);
    format(gTDS[s][tsTplName],TDS_NAME,"%s",name);
    format(gTDS[s][tsTplDescription],TDS_DESCRIPTION,"Русский компонент: кавычки \" и слеш \\ 100%%");
    gTDS[s][tsTplCategory] = 5; gTDS[s][tsTplVisibility] = visibility;
    return 1;
}

stock LibrarySnapshot(s)
{
    new slots[TDS_ELEMENTS]; Check(TDS_Order(s,slots) == 8,"two components produce exactly eight ordinary elements");
    for (new i; i < 8; i++) for (new E_TDS_ELEMENT:f; f < E_TDS_ELEMENT; f++) hExpected[i][f] = gTDSElement[s][slots[i]][f];
    for (new g; g < TDS_GROUPS; g++) for (new E_TDS_GROUP:f; f < E_TDS_GROUP; f++) hGroups[g][f] = gTDSGroup[s][g][f];
    return 1;
}

stock LibraryCompare(s)
{
    Check(TDS_ProjectValid(s) && gTDS[s][tsCount] == 8 && gTDS[s][tsGroupCount] == 4 &&
        gTDS[s][tsNextId] == 9 && gTDS[s][tsNextGroupId] == 5,"library project metadata/ID counters restored");
    new slots[TDS_ELEMENTS]; TDS_Order(s,slots);
    for (new i; i < 8; i++)
    {
        new e = slots[i];
        for (new f; f < sizeof(gTDSFields); f++)
        {
            new E_TDS_ELEMENT:field = gTDSFields[f];
            if (TDS_FloatField(field)) Check(floatabs(Float:gTDSElement[s][e][field]-Float:hExpected[i][field]) < 0.00001,"template float roundtrip");
            else Check(gTDSElement[s][e][field] == hExpected[i][field],"template style/ID/order roundtrip");
        }
        Check(gTDSElement[s][e][tdGroup] == hExpected[i][tdGroup] && !strcmp(gTDSElement[s][e][tdText],hExpected[i][tdText],false),
            "template membership and CP1251 content roundtrip");
    }
    for (new g; g < TDS_GROUPS; g++) if (hGroups[g][tgId])
    {
        new at = TDS_GroupSlot(s,hGroups[g][tgId]);
        Check(at >= 0 && !strcmp(gTDSGroup[s][at][tgName],hGroups[g][tgName],false),"template remapped group name roundtrip");
    }
    return 1;
}

main() {}
public OnGameModeInit()
{
    gDBPrimary = mysql_connect_file();
    if (gDBPrimary == MySQL:0) { print("TDS FAIL: library MySQL connection"); return 0; }
    TDS_Init(); Ready(0,true); Ready(1); tNow = 1000;
    SetTimer("LibraryNext",50,true); return 1;
}
public OnQueryError(errorid, const error[], const callback[], const query[], MySQL:handle)
{
    #pragma unused errorid, error, handle
    if (!TDSDB_OnError(callback,query)) { print("TDS FAIL: library unhandled SQL error"); tFailures++; }
    return 1;
}

public LibraryNext()
{
    if (hStep == 99) return 1;
    tNow += 50;
    if (!gTDSDBReady || !gTDSTemplatesReady || LibraryPending()) return 1;
    new s = TDS_Session(0);
    if (s >= 0 && gTDS[s][tsBusy]) return 1;
#if defined TDS_TEST_RESTART
    switch (hStep)
    {
        case 0:
        {
            TDS_Open(0); s = LibrarySource(); TDS_TplCapture(0); TDS_ClearProject(s); TDS_NewProject(0,"Expected");
            TDS_TplInsert(0,0); TDS_TplInsert(0,1,100.0,100.0); TDS_Select(0,0); TDS_ApplyInput(0,TDS_COLOUR,"11223344"); LibrarySnapshot(s);
            TDS_Close(0); tNow += 600; TDS_Open(0); TDS_NewProject(0,"Fresh library consumer"); s = TDS_Session(0);
            hPrivate = LibraryId("Личный логотип"); gTDS[s][tsTplPlace] = 0; TDS_TplRequest(0,TDS_DB_TEMPLATE_INSERT,hPrivate);
        }
        case 1:
        {
            Check(gTDS[s][tsCount] == 4 && gTDS[s][tsGroupCount] == 2,"M fresh process loaded persisted template with groups");
            Check(!strcmp(gTDS[s][tsTplDescription],"Русский компонент: кавычки \" и слеш \\ 100%",false),"V template Cyrillic description restored");
            gTDS[s][tsTplPlace] = 1; gTDS[s][tsTplX] = 100.0; gTDS[s][tsTplY] = 100.0;
            TDS_TplRequest(0,TDS_DB_TEMPLATE_INSERT,hPrivate);
        }
        case 2:
        {
            TDS_Select(0,0); TDS_ApplyInput(0,TDS_COLOUR,"11223344"); LibraryCompare(s);
            hCopy = LibraryId("Template interface copy",true); TDSDB_Load(0,hCopy);
        }
        case 3:
        {
            LibraryCompare(s); Check(!TDS_HCount(s) && !TDS_HCount(s,true) && !gTDS[s][tsDirty] && gTDS[s][tsInterface] == 0,
                "restart loads only confirmed project state, not old operation history");
            gTDS[s][tsTplFilter] = 6; gTDS[s][tsTplPage] = 0; TDS_TplRequest(0,TDS_DB_TEMPLATE_LIST);
        }
        case 4:
        {
            Check(gTDS[s][tsTplRowCount] == 20 && gTDS[s][tsTplMore],"library first page contains bounded metadata with overfetch");
            Answer(0,TDS_TEMPLATE_LIST,1,20);
        }
        case 5:
        {
            Check(gTDS[s][tsTplRowCount] == 3 && !gTDS[s][tsTplMore] && gTDS[s][tsTplPage] == 1,"library next page uses independent template pagination");
        }
    }
    if (hStep++ < 5) return 1;
#else
    switch (hStep)
    {
        case 0:
        {
            TDS_Open(0); s = LibrarySource(); LibraryFields(0,"Личный логотип",0);
            new history = TDS_HCount(s); TDS_TplRequest(0,TDS_DB_TEMPLATE_SAVE);
            Check(gTDS[s][tsBusy] > 0 && !TDS_HStep(0) && TDS_HCount(s) == history,"L async template save excludes Undo and does not record a project operation");
        }
        case 1: { hPrivate = LibraryId("Личный логотип"); LibraryFields(0,"Общий логотип",1); TDS_TplRequest(0,TDS_DB_TEMPLATE_SAVE); }
        case 2:
        {
            hShared = LibraryId("Общий логотип"); TDS_Grant(0,1,true); TDS_Open(1); TDS_TplRequest(1,TDS_DB_TEMPLATE_INFO,hPrivate);
        }
        case 3:
        {
            new other = TDS_Session(1); Check(!gTDS[other][tsTplId],"R another developer cannot read personal template");
            TDS_NewProject(1,"Developer component"); TDS_AddElement(1,TDS_TEXT_TYPE); LibraryFields(1,"Forbidden public",1);
            new serial = gTDSJobSerial;
            Check(!TDS_TplRequest(1,TDS_DB_TEMPLATE_SAVE) && gTDSJobSerial == serial,"R ordinary developer cannot publish shared template");
            TDS_TplRequest(1,TDS_DB_TEMPLATE_INFO,hShared);
        }
        case 4:
        {
            new other = TDS_Session(1), serial = gTDSJobSerial;
            Check(gTDS[other][tsTplId] == hShared && !TDS_TplOwned(other),"R shared component visible without ownership");
            Check(!TDS_TplRequest(1,TDS_DB_TEMPLATE_UPDATE,hShared) && !TDS_TplRequest(1,TDS_DB_TEMPLATE_DELETE,hShared) &&
                gTDSJobSerial == serial,"R foreign shared edit/delete denied before enqueue");
            gTDS[other][tsTplPlace] = 0; TDS_TplRequest(1,TDS_DB_TEMPLATE_INSERT,hShared);
        }
        case 5:
        {
            new other = TDS_Session(1);
            Check(gTDS[other][tsCount] == 5 && gTDS[other][tsGroupCount] == 2,"R granted developer can insert a shared component");
            LibraryFields(1,"Личный разработчика",0); TDS_TplRequest(1,TDS_DB_TEMPLATE_SAVE);
        }
        case 6: { hOther = LibraryId("Личный разработчика"); TDS_TplRequest(0,TDS_DB_TEMPLATE_INFO,hOther); }
        case 7:
        {
            Check(gTDS[s][tsTplOwner] == 101 && TDS_TplOwned(s),"R authenticated founder can manage another owner's personal template");
            format(gTDS[s][tsTplName],TDS_NAME,"Переименован основателем"); TDS_TplRequest(0,TDS_DB_TEMPLATE_UPDATE,hOther);
        }
        case 8: TDS_TplRequest(0,TDS_DB_TEMPLATE_INFO,hOther);
        case 9:
        {
            if (!hDescriptionEdited)
            {
                Check(gTDS[s][tsTplRevision] == 2 && !strcmp(gTDS[s][tsTplName],"Переименован основателем",false),"template rename updates revision");
                TDS_TplDetail(0); Answer(0,TDS_TEMPLATE_DETAIL,1,2); Answer(0,TDS_TEMPLATE_DESC,1,-1,"Обновлённое описание 100%");
                hDescriptionEdited = true; hStep = 8; return 1;
            }
            Check(gTDS[s][tsTplRevision] == 3 && !strcmp(gTDS[s][tsTplDescription],"Обновлённое описание 100%",false),"template description UI persists Cyrillic and revision");
            TDS_TplDetail(0); Answer(0,TDS_TEMPLATE_DETAIL,1,3);
            new serial = gTDSJobSerial; Answer(0,TDS_TEMPLATE_DELETE,0,0);
            Check(gTDSJobSerial == serial,"template deletion requires affirmative confirmation");
            TDS_TplDetail(0); Answer(0,TDS_TEMPLATE_DETAIL,1,3); Answer(0,TDS_TEMPLATE_DELETE,1,0);
        }
        case 10:
        {
            TDS_Close(1); TDS_ClearProject(s); TDS_NewProject(0,"Template interface"); gTDS[s][tsTplPlace] = 0;
            TDS_TplRequest(0,TDS_DB_TEMPLATE_INSERT,hPrivate);
        }
        case 11:
        {
            Check(gTDS[s][tsCount] == 4 && gTDS[s][tsGroupCount] == 2 && TDS_HCount(s) == 1,"N real asynchronous insertion records one history entry");
            Check(TDS_HStep(0) == 1 && !gTDS[s][tsCount] && TDS_HStep(0,true) == 1 && gTDS[s][tsCount] == 4,"N inserted component Undo/Redo is atomic");
            gTDS[s][tsTplPlace] = 1; gTDS[s][tsTplX] = 100.0; gTDS[s][tsTplY] = 100.0; TDS_TplRequest(0,TDS_DB_TEMPLATE_INSERT,hPrivate);
        }
        case 12:
        {
            Check(gTDS[s][tsCount] == 8 && gTDS[s][tsGroupCount] == 4 && TDS_HCount(s) == 2,"O/P repeated real template creates independent IDs/groups");
            LibrarySnapshot(s); TDS_HClear(0); TDSDB_Save(0);
        }
        case 13:
        {
            hProject = gTDS[s][tsProjectId]; Check(hProject > 0 && !gTDS[s][tsDirty],"K project with templates committed");
            TDS_Select(0,0); TDS_ApplyInput(0,TDS_COLOUR,"11223344"); TDS_HStep(0);
            Check(!gTDS[s][tsDirty],"K Undo reaches actual MySQL save checkpoint"); TDSDB_Save(0);
        }
        case 14:
        {
            Check(gTDS[s][tsRevision] == 2 && !gTDS[s][tsDirty],"A save after Undo stores restored state");
            TDS_HStep(0,true); Check(gTDS[s][tsDirty],"B Redo changes state relative to real checkpoint"); TDSDB_Save(0);
        }
        case 15:
        {
            Check(gTDS[s][tsRevision] == 3 && !gTDS[s][tsDirty],"B save after Redo stores reapplied state"); LibrarySnapshot(s);
            TDS_HStep(0); Check(gTDS[s][tsDirty],"C Undo after real save is dirty"); TDS_HStep(0,true); Check(!gTDS[s][tsDirty],"E Redo back to real saved state is clean");
            TDS_Export(0);
        }
        case 16:
        {
            TDS_Main(0); Answer(0,TDS_MAIN,1,4); Answer(0,TDS_RENAME,1,-1,"Template interface copy");
            TDS_Settings(0); Answer(0,TDS_SETTINGS,1,0); TDSDB_Save(0,true);
        }
        case 17:
        {
            hCopy = gTDS[s][tsProjectId]; Check(hCopy != hProject && !gTDS[s][tsDirty] && gTDS[s][tsInterface] == 0,"Save As preserves components in Global project");
            LibraryCompare(s); TDS_Export(0);
        }
        case 18: { TDS_Close(0); tNow += 600; TDS_Open(0); TDSDB_Load(0,hCopy); }
        case 19:
        {
            LibraryCompare(s); Check(!TDS_HCount(s),"D loaded project starts with empty operation history");
            new sql[160]; format(sql,sizeof(sql),"INSERT INTO tdstudio_template_groups(template_id,group_id,name) VALUES(%d,99,'Broken')",hPrivate); LibrarySQL(sql);
            gTDS[s][tsTplPlace] = 0; TDS_TplRequest(0,TDS_DB_TEMPLATE_INSERT,hPrivate);
        }
        case 20:
        {
            LibraryCompare(s); Check(!TDS_HCount(s) && !gTDS[s][tsDirty],"corrupt template cannot partially insert or dirty current project");
            new sql[160];
            if (!hBadVersion)
            {
                format(sql,sizeof(sql),"DELETE FROM tdstudio_template_groups WHERE template_id=%d AND group_id=99",hPrivate); LibrarySQL(sql);
                LibrarySQL("ALTER TABLE tdstudio_templates ALTER CHECK chk_tdst_meta NOT ENFORCED");
                format(sql,sizeof(sql),"UPDATE tdstudio_templates SET format_version=3 WHERE template_id=%d",hPrivate); LibrarySQL(sql);
                hBadVersion = true; TDS_TplRequest(0,TDS_DB_TEMPLATE_INSERT,hPrivate); return 1;
            }
            format(sql,sizeof(sql),"UPDATE tdstudio_templates SET format_version=2 WHERE template_id=%d",hPrivate); LibrarySQL(sql);
            LibrarySQL("ALTER TABLE tdstudio_templates ALTER CHECK chk_tdst_meta ENFORCED");
            TDSDB_Load(0,LibraryId("Geometry saved copy",true));
        }
        case 21:
        {
            Check(gTDS[s][tsCount] == 5 && gTDS[s][tsFormat] == 2 && !TDS_HCount(s),"T real stage-3.2 project remains compatible");
            gTDS[s][tsTplPlace] = 1; gTDS[s][tsTplX] = 100.0; gTDS[s][tsTplY] = 100.0; TDS_TplRequest(0,TDS_DB_TEMPLATE_INSERT,hPrivate);
        }
        case 22:
        {
            Check(gTDS[s][tsCount] == 9 && gTDS[s][tsGroupCount] == 3 && TDS_HStep(0) == 1 && gTDS[s][tsCount] == 5,"T template insertion/Undo leaves old project intact");
            gTDS[s][tsTplPlace] = 0; TDS_TplRequest(0,TDS_DB_TEMPLATE_INSERT,hPrivate);
            TDS_Close(0); tNow += 600; TDS_Open(0); TDS_NewProject(0,"Safe template reopen");
        }
        case 23:
        {
            Check(!gTDS[s][tsCount] && !TDS_HCount(s) && !gTDS[s][tsTplId],"late template callback cannot populate a replacement lease");
        }
        case 24:
        {
            TDS_ClearProject(s); TDS_NewProject(0,"Maximum template source");
            new longest[TDS_TEXT]; for (new i; i < TDS_TEXT-1; i++) longest[i] = i%2 ? 92 : 34; longest[0] = 223;
            for (new i; i < TDS_ELEMENTS; i++) { TDS_AddElement(0,TDS_TEXT_TYPE); TDS_ApplyInput(0,TDS_CONTENT,longest); }
            for (new g; g < TDS_GROUPS; g++) { new name[32]; format(name,sizeof(name),"Maximum_component_%d",g); TDS_Select(0,g*2); TDS_Select(0,g*2+1,true); TDS_GroupCreate(0,name); }
            TDS_SelectAll(0,true); LibraryFields(0,"Maximum component",0); TDS_TplRequest(0,TDS_DB_TEMPLATE_SAVE);
        }
        case 25:
        {
            new maximum = LibraryId("Maximum component"); TDS_ClearProject(s); TDS_NewProject(0,"Maximum component consumer");
            gTDS[s][tsTplPlace] = 0; TDS_TplRequest(0,TDS_DB_TEMPLATE_INSERT,maximum);
        }
        case 26:
        {
            Check(gTDS[s][tsCount] == 64 && gTDS[s][tsGroupCount] == 16 && TDS_ProjectValid(s),"real MySQL maximum template restored 64 elements, 16 groups and 127-byte strings");
            Check(TDS_HStep(0) == 1 && !gTDS[s][tsCount] && TDS_HStep(0,true) == 1 && gTDS[s][tsCount] == 64,
                "maximum component remains one Undo/Redo operation");
        }
    }
    if (hStep++ < 26) return 1;
#endif
    TDS_Shutdown(); mysql_close(gDBPrimary); hStep = 99;
    Check(tCreate == tDestroy && !tBadHandle && !tDialogOverflow,"library DB lifecycle releases resources and keeps valid dialogs");
    printf("TDS TEST RESULT: checks=%d failures=%d",tChecks,tFailures);
    return 1;
}
