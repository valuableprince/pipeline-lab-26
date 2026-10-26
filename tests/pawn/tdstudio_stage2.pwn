#define TDS_TEST_MYSQL
#include "tdstudio_test_host.inc"

new tStep, tProject, tClone, tExpectedCount, tMaximum;
new tExpected[TDS_ELEMENTS][E_TDS_ELEMENT];
new tCreated[20];
#if defined TDS_TEST_RESTART
    #pragma unused tProject, tCreated, tMaximum
#endif
forward TDS_TestNext();
forward OnQueryError(errorid, const error[], const callback[], const query[], MySQL:handle);

stock TestSQL(const sql[])
{
    new Cache:c = mysql_query(gDBPrimary,sql);
    Check(c != Cache:0,"test SQL execution");
    if (c != Cache:0) cache_delete(c);
    return 1;
}

stock BuildFixture()
{
    TDS_NewProject(0,"Тест интерфейса");
    new s = TDS_Session(0);
    format(gTDS[s][tsDescription],TDS_DESCRIPTION,"Описание 'проекта' с кириллицей");
    TDS_AddElement(0,TDS_TEXT_TYPE);
    Check(TDS_ApplyInput(0,TDS_POSITION,"12.345 67.89"),"fixture position");
    Check(TDS_ApplyInput(0,TDS_SIZE,"123.45 16.75"),"fixture dimensions");
    Check(TDS_ApplyInput(0,TDS_LETTERS,"0.333 0.999"),"fixture letters");
    Check(TDS_ApplyInput(0,TDS_CONTENT,"Привет, \"Дима\"! C:\\test 100% ~g~текст"),"fixture Cyrillic quotes slash percent");
    Check(TDS_ApplyInput(0,TDS_COLOUR,"AABBCCDD"),"fixture signed RGBA");
    Check(TDS_ApplyInput(0,TDS_BOX_COLOUR,"01020304"),"fixture box RGBA");
    Check(TDS_ApplyInput(0,TDS_BACKGROUND,"ABCD0F40"),"fixture background");
    Check(TDS_ApplyInput(0,TDS_FONT,"3"),"fixture font");
    Check(TDS_ApplyInput(0,TDS_ALIGN,"2"),"fixture alignment");
    Check(TDS_ApplyInput(0,TDS_OUTLINE,"2"),"fixture outline");
    Check(TDS_ApplyInput(0,TDS_SHADOW,"1"),"fixture shadow");
    TDS_Toggle(0,TDS_BOX); TDS_Toggle(0,TDS_PROPORTIONAL); TDS_Toggle(0,TDS_SELECTABLE);
    TDS_AddElement(0,TDS_RECT_TYPE);
    TDS_ApplyInput(0,TDS_COLOUR,"12345688");
    new sprite = TDS_AddElement(0,TDS_SPRITE_TYPE);
    TDS_AddElement(0,TDS_MODEL_TYPE);
    TDS_ApplyInput(0,TDS_ROTATION,"-14.25 90 33.5 1.25");
    TDS_ApplyInput(0,TDS_VEH_COLOURS,"123 255");
    gTDSElement[s][gTDS[s][tsSelected]][tdModel] = 560;
    TDS_DeleteElement(0,sprite); sprite = TDS_AddElement(0,TDS_SPRITE_TYPE);
    gTDSElement[s][sprite][tdSprite] = 4; TDS_RenderElement(s,sprite);
    gTDS[s][tsSelected] = 0;
    Check(TDS_ProjectValid(s),"fixture complete validation");
    return s;
}

stock Snapshot(s)
{
    new slots[TDS_ELEMENTS]; tExpectedCount = TDS_Order(s,slots);
    for (new e; e < tExpectedCount; e++)
        for (new E_TDS_ELEMENT:f; f < E_TDS_ELEMENT; f++) tExpected[e][f] = gTDSElement[s][slots[e]][f];
    return 1;
}

stock Compare(s)
{
    Check(gTDS[s][tsCount] == tExpectedCount,"roundtrip count");
    for (new e; e < tExpectedCount; e++)
    {
        for (new f; f < sizeof(gTDSFields); f++)
        {
            new E_TDS_ELEMENT:field = gTDSFields[f];
            if (TDS_FloatField(field)) Check(floatabs(Float:gTDSElement[s][e][field]-Float:tExpected[e][field]) < 0.000001,"roundtrip float property");
            else Check(gTDSElement[s][e][field] == tExpected[e][field],"roundtrip scalar property");
        }
        Check(!strcmp(gTDSElement[s][e][tdText],tExpected[e][tdText],false),"roundtrip CP1251 text");
    }
    return 1;
}

stock bool:JobsPending()
{
    for (new j; j < TDS_JOB_LIMIT; j++) if (gTDSJob[j][tjUsed]) return true;
    return false;
}

main() {}
public OnGameModeInit()
{
    gDBPrimary = mysql_connect_file();
    if (gDBPrimary == MySQL:0) { print("TDS FAIL: MySQL connection"); return 0; }
    TDS_Init();
    Ready(0,true); Ready(1); tNow = 1000;
    SetTimer("TDS_TestNext",50,true);
    return 1;
}

public OnQueryError(errorid, const error[], const callback[], const query[], MySQL:handle)
{
    #pragma unused errorid, error, handle
    if (!TDSDB_OnError(callback,query)) { print("TDS FAIL: unhandled MySQL error"); tFailures++; }
    return 1;
}

public TDS_TestNext()
{
    if (tStep == 99) return 1;
    tNow += 50;
    if (!gTDSDBReady) return 1;
    new s = TDS_Session(0);
    if (s >= 0 && gTDS[s][tsBusy]) return 1;
#if defined TDS_TEST_RESTART
    if (tStep == 0)
    {
        TDS_Open(0); s = BuildFixture();
        TDS_ApplyInput(0,TDS_POSITION,"444 200"); Snapshot(s);
        TDS_ClearProject(s);
        new Cache:c = mysql_query(gDBPrimary,"SELECT project_id FROM tdstudio_projects WHERE name='Persistent clone' AND creator_id=100");
        new rows;
        Check(c != Cache:0 && cache_get_row_count(rows) != 0 && rows == 1,"restart persisted row exists");
        cache_get_value_name_int(0,"project_id",tClone); cache_delete(c);
        TDSDB_Load(0,tClone); tStep = 1; return 1;
    }
    Compare(s);
    Check(gTDS[s][tsInterface] == 0 && !gTDS[s][tsDirty],"restart interface type and clean state");
    Check(!strcmp(gTDS[s][tsDescription],"Описание 'проекта' с кириллицей",false),"restart Cyrillic description");
    Check(TDS_ProjectValid(s),"restart validated model");
    TDS_Shutdown(); mysql_close(gDBPrimary); tStep = 99;
    printf("TDS TEST RESULT: checks=%d failures=%d",tChecks,tFailures);
    return 1;
#else
    switch (tStep)
    {
        case 0:
        {
            TDS_Open(0); s = BuildFixture(); Snapshot(s);
            TDSDB_Save(0);
            Check(gTDS[s][tsBusy] > 0,"save is asynchronous");
            Check(!TDS_ApplyInput(0,TDS_POSITION,"111 111"),"editing blocked during save");
            Check(TDSDB_Save(0) == 0,"duplicate save blocked");
        }
        case 1:
        {
            Check(gTDS[s][tsProjectId] > 0 && gTDS[s][tsRevision] == 1 && !gTDS[s][tsDirty],"save committed and confirmed");
            tProject = gTDS[s][tsProjectId];
            format(tCreated,sizeof(tCreated),"%s",gTDS[s][tsCreated]);
            TDS_Export(0);
        }
        case 2:
        {
            Check(!gTDSExportFile[s],"personal export completed");
            TDS_Close(0); tNow += 600; TDS_Open(0); TDSDB_Load(0,tProject);
            new serial = gTDSJobSerial; TDSDB_Load(0,tProject);
            Check(gTDSJobSerial == serial,"duplicate concurrent load is not queued");
        }
        case 3:
        {
            Compare(s);
            Check(!strcmp(gTDS[s][tsName],"Тест интерфейса",false),"Cyrillic project name restored");
            Check(gTDS[s][tsCreator] == 100,"creator restored");
            Answer(0,TDS_MAIN,1,4); Answer(0,TDS_RENAME,1,-1,"Переименованный");
            Check(gTDS[s][tsDirty] && !strcmp(gTDS[s][tsName],"Переименованный",false),"rename via UI marks dirty");
            Answer(0,TDS_MAIN,1,8); Answer(0,TDS_SETTINGS,1,0); Answer(0,TDS_SETTINGS,0,-1);
            Check(gTDS[s][tsInterface] == 0,"interface type via UI");
            Answer(0,TDS_MAIN,1,2);
        }
        case 4:
        {
            Check(gTDS[s][tsRevision] == 2 && !strcmp(gTDS[s][tsCreated],tCreated,false),"update revision and preserve creation time");
            TDS_Export(0);
        }
        case 5:
        {
            format(gTDS[s][tsName],TDS_NAME,"Persistent clone"); gTDS[s][tsDirty] = true;
            TDSDB_Save(0,true);
        }
        case 6:
        {
            tClone = gTDS[s][tsProjectId];
            Check(tClone != tProject && tClone > 0 && gTDS[s][tsRevision] == 1,"save-as new ID");
            gTDS[s][tsSelected] = 0; TDS_ApplyInput(0,TDS_POSITION,"444 200");
            TDSDB_Save(0); TDS_Close(0); tNow += 600; TDS_Open(0); TDS_NewProject(0,"New lease");
        }
        case 7:
        {
            if (JobsPending()) return 1;
            Check(!strcmp(gTDS[s][tsName],"New lease",false) && !gTDS[s][tsProjectId] && gTDS[s][tsDirty],"stale save does not attach to reopened session");
            TDSDB_Load(0,tProject); TDS_Reset(0,false); Ready(0,true); tNow += 600; TDS_Open(0); TDS_NewProject(0,"Reconnect");
        }
        case 8:
        {
            if (JobsPending()) return 1;
            Check(!strcmp(gTDS[s][tsName],"Reconnect",false) && !gTDS[s][tsProjectId],"stale load rejected after disconnect/reconnect");
            TDS_Grant(0,1,true); TDS_Open(1); TDS_NewProject(1,"Own project"); TDSDB_Load(1,tProject);
        }
        case 9:
        {
            if (JobsPending()) return 1;
            new other = TDS_Session(1);
            Check(other >= 0 && !gTDS[other][tsProjectId] && !strcmp(gTDS[other][tsName],"Own project",false),"foreign project denied and draft preserved");
            TDS_Close(1); TDSDB_Load(0,tProject);
        }
        case 10:
        {
            TestSQL("UPDATE tdstudio_projects SET revision=revision+1 WHERE creator_id=100 AND project_id=(SELECT p.id FROM (SELECT MIN(project_id) id FROM tdstudio_projects) p)");
            gTDS[s][tsSelected] = 0; TDS_ApplyInput(0,TDS_POSITION,"321 123"); TDSDB_Save(0);
        }
        case 11:
        {
            Check(gTDS[s][tsDirty] && gTDS[s][tsRevision] == 2,"revision conflict keeps dirty draft");
            TDSDB_Load(0,tProject);
        }
        case 12:
        {
            Check(gTDS[s][tsRevision] == 3,"conflict resolved by explicit reload");
            TestSQL("UPDATE tdstudio_projects SET format_version=2 WHERE project_id=(SELECT p.id FROM (SELECT MIN(project_id) id FROM tdstudio_projects) p)");
            TDSDB_Load(0,tProject);
        }
        case 13:
        {
            Check(gTDS[s][tsProjectId] == tProject && gTDS[s][tsFormat] == 1 && gTDS[s][tsCount] == 4,"unsupported format leaves old project intact");
            TestSQL("UPDATE tdstudio_projects SET format_version=1,next_element_id=1 WHERE project_id=(SELECT p.id FROM (SELECT MIN(project_id) id FROM tdstudio_projects) p)");
            TDSDB_Load(0,tProject);
        }
        case 14:
        {
            Check(gTDS[s][tsNextId] == 6 && gTDS[s][tsCount] == 4,"corrupt IDs leave old project intact");
            TestSQL("UPDATE tdstudio_projects SET next_element_id=6 WHERE project_id=(SELECT p.id FROM (SELECT MIN(project_id) id FROM tdstudio_projects) p)");
            tFailAfter = 1; TDSDB_Load(0,tProject);
        }
        case 15:
        {
            Check(gTDS[s][tsProjectId] == tProject && gTDS[s][tsCount] == 4 && tFailAfter == 0,"load allocation failure preserves old project");
            tFailAfter = -1; TDSDB_Delete(0);
        }
        case 16:
        {
            Check(!gTDS[s][tsProject],"delete cleared project only after DB confirmation");
            TDSDB_Load(0,tProject);
        }
        case 17:
        {
            Check(!gTDS[s][tsProject],"deleted project cannot load"); TDSDB_Load(0,tClone);
        }
        case 18:
        {
            Check(gTDS[s][tsProjectId] == tClone,"another project survives deletion");
            gTDS[s][tsDirty] = true;
            new j = TDSDB_Begin(0,TDS_DB_SAVE,tClone);
            format(gTDSQuery,sizeof(gTDSQuery),"/*TDS:%d*/ SELECT * FROM tdstudio_nonexistent",gTDSJob[j][tjSerial]);
            TDSDB_Submit(j);
        }
        case 19:
        {
            Check(gTDS[s][tsDirty] && DBPrimary_IsReady(),"editor SQL error releases busy without disabling host DB");
            TDS_ClearProject(s); TDS_NewProject(0,"Empty project"); TDS_Export(0);
        }
        case 20:
        {
            Check(!gTDSExportFile[s],"empty export completed");
            gTDS[s][tsInterface] = 1; TDS_Export(0);
        }
        case 21:
        {
            TDS_Export(0);
            new temporary[128]; format(temporary,sizeof(temporary),"%s",gTDSExportPath[s]);
            TDS_Close(0);
            Check(!fexist(temporary),"closing cancels partial export and removes temporary file");
            tNow+=600; TDS_Open(0); TDS_NewProject(0,"Empty project");
            TDSDB_Save(0); TDS_Close(0); tNow+=600; TDS_Open(0); TDS_NewProject(0,"New after save");
        }
        case 22:
        {
            if (JobsPending()) return 1;
            Check(!strcmp(gTDS[s][tsName],"New after save",false) && !gTDS[s][tsProjectId],"late save after close does not corrupt new draft");
            new job = TDSDB_Begin(0,TDS_DB_SAVE), cookie = gTDSJob[job][tjSerial];
            tNow += 31001; TDSDB_Expire();
            Check(!gTDS[s][tsBusy] && gTDS[s][tsDirty] && gTDSJob[job][tjUsed] && gTDSJob[job][tjExpired],"timeout leaves outcome unknown and draft dirty");
            new before = gTDSJobSerial; TDSDB_Load(0,tClone);
            Check(gTDSJobSerial == before,"timed-out backend operation cannot grow same-session queue");
            TDSDB_Result(job,cookie,0,TDSHost_Token(0));
            Check(!gTDS[s][tsProjectId] && gTDS[s][tsDirty],"expired callback cannot mark draft saved");
            TDS_ClearProject(s); TDS_NewProject(0,"Maximum project");
            new longest[TDS_TEXT];
            for (new i; i < TDS_TEXT-1; i++) longest[i] = (i%2) ? 92 : 34;
            longest[0] = 223; // CP1251 capital Ya
            for (new i; i < TDS_ELEMENTS; i++)
            {
                TDS_AddElement(0,TDS_TEXT_TYPE); TDS_ApplyInput(0,TDS_CONTENT,longest);
            }
            Check(TDS_ProjectValid(s),"maximum project before persistence");
            TDSDB_Save(0);
        }
        case 23:
        {
            Check(gTDS[s][tsProjectId] > 0 && gTDS[s][tsCount] == 64 && !gTDS[s][tsDirty],"maximum payload saved");
            tMaximum = gTDS[s][tsProjectId]; TDS_Close(0); tNow+=600; TDS_Open(0); TDSDB_Load(0,tMaximum);
        }
        case 24:
        {
            Check(gTDS[s][tsCount] == 64 && TDS_ProjectValid(s),"maximum payload loaded");
            for (new e; e < 64; e++) Check(strlen(gTDSElement[s][e][tdText]) == 127 && gTDSElement[s][e][tdText][0] == 223,"maximum CP1251 string restored");
            TDS_Export(0);
        }
        case 25:
        {
            gTDS[s][tsDirty] = true;
            mysql_close(gDBPrimary);
            TDSDB_Save(0);
            Check(!gTDS[s][tsBusy] && gTDS[s][tsDirty],"closed connection enqueue failure keeps draft unsaved");
            TDS_Shutdown();
            Check(tCreate == tDestroy && tBadHandle == 0,"all editor handles released");
            printf("TDS TEST RESULT: checks=%d failures=%d",tChecks,tFailures); tStep = 99; return 1;
        }
    }
    tStep++;
    return 1;
#endif
}
