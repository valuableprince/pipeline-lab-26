#if defined _tarkenso_tds_db
    #endinput
#endif
#define _tarkenso_tds_db

#define TDS_DB_SAVE (1)
#define TDS_DB_LOAD (2)
#define TDS_DB_LIST (3)
#define TDS_DB_DELETE (4)
#define TDS_JOB_LIMIT (8)

enum E_TDS_JOB
{
    bool:tjUsed, bool:tjExpired, tjSerial, tjSession, tjLease, tjPlayer, tjToken, tjAccount,
    tjOperation, tjStarted, tjProject, tjRevision, tjPage
};
new gTDSJob[TDS_JOB_LIMIT][E_TDS_JOB];

stock TDS_ClearProject(session)
{
    TDS_GridHide(session);
    TDSInput_End(session, false);
    TDS_LayoutReset(session);
    TDS_StopMouse(session);
    for (new i; i < TDS_ELEMENTS; i++)
    {
        TDS_RenderDestroyElement(session,i);
        gTDSElement[session][i][tdId] = 0;
    }
    gTDS[session][tsProject] = false; gTDS[session][tsProjectId] = 0;
    gTDS[session][tsCreator] = TDSHost_Account(gTDS[session][tsOwner]);
    gTDS[session][tsRevision] = 0; gTDS[session][tsFormat] = TDS_FORMAT;
    gTDS[session][tsInterface] = 1; gTDS[session][tsNextId] = 1;
    gTDS[session][tsCount] = 0; gTDS[session][tsSelected] = -1;
    gTDS[session][tsName][0] = EOS; gTDS[session][tsDescription][0] = EOS;
    gTDS[session][tsCreated][0] = EOS; gTDS[session][tsUpdated][0] = EOS;
    gTDS[session][tsDirty] = false; gTDS[session][tsMode] = TDS_MENU_MODE;
    return 1;
}

stock bool:TDSDB_Live(job)
{
    new p = gTDSJob[job][tjPlayer], s = gTDSJob[job][tjSession];
    return TDS_Session(p) == s && gTDS[s][tsLease] == gTDSJob[job][tjLease] &&
        gTDS[s][tsBusy] == gTDSJob[job][tjSerial] && TDS_Allowed(p) &&
        TDSHost_Account(p) == gTDSJob[job][tjAccount];
}

stock TDSDB_Fail(job, const message[], bool:release = true)
{
    if (TDSDB_Live(job))
    {
        new p = gTDSJob[job][tjPlayer];
        gTDS[gTDSJob[job][tjSession]][tsBusy] = 0;
        SendClientMessage(p,ADMIN_ERROR,message);
        TDS_Main(p);
    }
    if (release) gTDSJob[job][tjUsed] = false;
    return 1;
}

stock TDSDB_Expire()
{
    for (new j; j < TDS_JOB_LIMIT; j++) if (gTDSJob[j][tjUsed] && !gTDSJob[j][tjExpired] &&
        GetTickCount()-gTDSJob[j][tjStarted] > 30000)
    {
        gTDSJob[j][tjExpired] = true;
        TDSDB_Fail(j,"Время ожидания БД истекло. Результат неизвестен: дождитесь ответа или проверьте список после переподключения.",false);
    }
    return 1;
}

#if defined TARKENSO_DB_PRIMARY && !defined TDS_TEST_NO_DB
new const gTDSColumns[][] = {"element_id", "draw_order", "kind", "x", "y", "width", "height", "letter_x", "letter_y", "colour", "box_colour", "background", "font", "alignment", "outline", "shadow", "use_box", "proportional", "selectable", "sprite", "model", "rot_x", "rot_y", "rot_z", "zoom", "vehicle_colour1", "vehicle_colour2"};
new gTDSJobSerial;
new gTDSQuery[110000];
new gTDSLoad[TDS_ELEMENTS][E_TDS_ELEMENT];
new gTDSLoadedGroups[TDS_GROUPS][E_TDS_GROUP];
new bool:gTDSDBReady;
forward TDSDB_Ready();
forward TDSDB_Result(job, serial, playerid, token);
forward bool:TDSDB_QueryHex(const source[]);

stock TDSDB_Init()
{
    gTDSDBReady = false;
    if (!DBPrimary_IsReady()) return 0;
    return mysql_tquery(gDBPrimary,"SELECT version FROM schema_migrations WHERE version=15 LIMIT 1","TDSDB_Ready");
}

public TDSDB_Ready()
{
    new rows;
    gTDSDBReady = cache_get_row_count(rows) != 0 && rows == 1;
    if (!gTDSDBReady) print("[TDSTUDIO] Apply migrations 012 and 015; persistent editor operations disabled.");
    return 1;
}

stock TDSDB_Begin(playerid, operation, project = 0)
{
    new s = TDS_RequireSession(playerid);
    if (s < 0 || gTDS[s][tsBusy] || gTDS[s][tsMode] == TDS_MOVE_MODE) return -1;
    if (!gTDSDBReady || !DBPrimary_IsReady())
    {
        SendClientMessage(playerid,ADMIN_ERROR,"Хранение TextDraw недоступно. Проверьте БД и миграции 012/015.");
        return -1;
    }
    if (gTDSJobSerial >= 2147483646) return -1;
    for (new j; j < TDS_JOB_LIMIT; j++) if (gTDSJob[j][tjUsed] &&
        gTDSJob[j][tjSession] == s && gTDSJob[j][tjLease] == gTDS[s][tsLease])
    {
        SendClientMessage(playerid,ADMIN_ERROR,"Предыдущий запрос ещё выполняется в БД. Повторный запрос в этой сессии не отправлен.");
        return -1;
    }
    for (new j; j < TDS_JOB_LIMIT; j++)
    {
        if (gTDSJob[j][tjUsed]) continue;
        if (!TDS_SetMode(playerid,TDS_MENU_MODE)) return -1;
        gTDSJob[j][tjUsed] = true; gTDSJob[j][tjExpired] = false; gTDSJob[j][tjSerial] = ++gTDSJobSerial;
        gTDSJob[j][tjSession] = s; gTDSJob[j][tjLease] = gTDS[s][tsLease];
        gTDSJob[j][tjPlayer] = playerid; gTDSJob[j][tjToken] = TDSHost_Token(playerid);
        gTDSJob[j][tjAccount] = TDSHost_Account(playerid);
        gTDSJob[j][tjOperation] = operation; gTDSJob[j][tjProject] = project;
        gTDSJob[j][tjRevision] = gTDS[s][tsRevision]; gTDSJob[j][tjPage] = gTDS[s][tsPage];
        gTDSJob[j][tjStarted] = GetTickCount(); gTDS[s][tsBusy] = gTDSJobSerial;
        return j;
    }
    SendClientMessage(playerid,ADMIN_ERROR,"Очередь TextDraw Studio занята. Повторите позже.");
    return -1;
}

stock TDSDB_Submit(job)
{
    if (!mysql_tquery(gDBPrimary,gTDSQuery,"TDSDB_Result","iiii",job,gTDSJob[job][tjSerial],
        gTDSJob[job][tjPlayer],gTDSJob[job][tjToken]))
        return TDSDB_Fail(job,"Запрос не принят БД. Проект не помечен сохранённым.");
    return TDS_Main(gTDSJob[job][tjPlayer]);
}

stock TDSDB_Save(playerid, bool:asNew = false)
{
    new s = TDS_RequireSession(playerid);
    if (s < 0 || gTDS[s][tsBusy] || gTDS[s][tsMode] == TDS_MOVE_MODE || !TDS_Encode(s)) return 0;
    new name[TDS_NAME*2], description[TDS_DESCRIPTION*2];
    if (!TDS_Hex(gTDS[s][tsName],name,sizeof(name)) ||
        !TDS_Hex(gTDS[s][tsDescription],description,sizeof(description))) return 0;
    new id = asNew ? 0 : gTDS[s][tsProjectId], j = TDSDB_Begin(playerid,TDS_DB_SAVE,id);
    if (j < 0) return TDS_Main(playerid);
    // R41 has no prepared-statement/bind API. Typed stored-routine parameters;
    // all text is an alphabet-restricted HEX literal, independent of sql_mode.
    mysql_format(gDBPrimary,gTDSQuery,sizeof(gTDSQuery),
        "/*TDS:%d*/ CALL tds_save_v2(%d,%d,%d,%d,X'%s',X'%s',%d,%d,CONVERT(X'",
        gTDSJob[j][tjSerial],id,TDSHost_Account(playerid),_:TDSHost_Founder(playerid),
        gTDS[s][tsRevision],name,description,gTDS[s][tsInterface],gTDS[s][tsNextId]);
    if (!TDSDB_QueryHex(gTDSWire)) return TDSDB_Fail(j,"Проект превышает размер запроса.");
    new tail[64]; format(tail, sizeof(tail), "' USING ascii),%d,CONVERT(X'", gTDS[s][tsNextGroupId]);
    if (!TDS_Append(gTDSQuery, sizeof(gTDSQuery), tail) || !TDSDB_QueryHex(gTDSGroupWire) ||
        strlen(gTDSQuery)+32 >= sizeof(gTDSQuery)) return TDSDB_Fail(j,"Группы превышают размер запроса.");
    strcat(gTDSQuery,"' USING ascii))",sizeof(gTDSQuery));
    return TDSDB_Submit(j);
}

stock bool:TDSDB_QueryHex(const source[])
{
    new start = strlen(gTDSQuery), length = strlen(source);
    if (start+length*2+32 >= sizeof(gTDSQuery)) return false;
    new const digits[] = "0123456789ABCDEF";
    for (new i; i < length; i++)
    {
        if (source[i] < 1 || source[i] > 127) return false;
        gTDSQuery[start++] = digits[(source[i] >>> 4) & 15];
        gTDSQuery[start++] = digits[source[i] & 15];
    }
    gTDSQuery[start] = EOS;
    return true;
}

stock TDSDB_Load(playerid, project)
{
    new s = TDS_RequireSession(playerid);
    if (s < 0 || gTDS[s][tsMode] == TDS_MOVE_MODE) return 0;
    if (project <= 0) return 0;
    new j = TDSDB_Begin(playerid,TDS_DB_LOAD,project);
    if (j < 0) return TDS_Main(playerid);
    mysql_format(gDBPrimary,gTDSQuery,sizeof(gTDSQuery),"/*TDS:%d*/ CALL tds_load_v2(%d,%d,%d)",
        gTDSJob[j][tjSerial],project,TDSHost_Account(playerid),_:TDSHost_Founder(playerid));
    return TDSDB_Submit(j);
}

stock TDSDB_List(playerid, page = 0)
{
    new s = TDS_RequireSession(playerid);
    if (s < 0 || page < 0 || page > 50000 || gTDS[s][tsBusy] || gTDS[s][tsMode] == TDS_MOVE_MODE) return 0;
    gTDS[s][tsPage] = page;
    new j = TDSDB_Begin(playerid,TDS_DB_LIST);
    if (j < 0) return TDS_Main(playerid);
    mysql_format(gDBPrimary,gTDSQuery,sizeof(gTDSQuery),"/*TDS:%d*/ CALL tds_list(%d,%d,%d)",
        gTDSJob[j][tjSerial],TDSHost_Account(playerid),_:TDSHost_Founder(playerid),page*TDS_PROJECT_PAGE);
    return TDSDB_Submit(j);
}

stock TDSDB_Delete(playerid)
{
    new s = TDS_RequireSession(playerid);
    if (s < 0 || gTDS[s][tsProjectId] <= 0 || gTDS[s][tsBusy] || gTDS[s][tsMode] == TDS_MOVE_MODE) return 0;
    new j = TDSDB_Begin(playerid,TDS_DB_DELETE,gTDS[s][tsProjectId]);
    if (j < 0) return TDS_Main(playerid);
    mysql_format(gDBPrimary,gTDSQuery,sizeof(gTDSQuery),"/*TDS:%d*/ CALL tds_delete(%d,%d,%d,%d)",
        gTDSJob[j][tjSerial],gTDS[s][tsProjectId],TDSHost_Account(playerid),_:TDSHost_Founder(playerid),gTDS[s][tsRevision]);
    return TDSDB_Submit(j);
}

stock bool:TDSDB_ReadHex(row, const column[], dest[], capacity)
{
    new hex[TDS_DESCRIPTION*2];
    if (!cache_get_value_name(row,column,hex,sizeof(hex))) return false;
    return TDS_Unhex(hex,dest,capacity);
}

stock bool:TDSDB_LoadResult(session, target, rows)
{
    new id, creator, revision, version, interfaceType, nextId, count, nextGroup, groupCount, groupsRead;
    new name[TDS_NAME], description[TDS_DESCRIPTION], created[20], updated[20];
    if (rows < 1 || rows > TDS_ELEMENTS ||
        !cache_get_value_name_int(0,"project_id",id) || id != target ||
        !cache_get_value_name_int(0,"creator_id",creator) || creator <= 0 ||
        !cache_get_value_name_int(0,"revision",revision) || revision <= 0 ||
        !cache_get_value_name_int(0,"format_version",version) || (version != 1 && version != TDS_FORMAT) ||
        !cache_get_value_name_int(0,"interface_type",interfaceType) || interfaceType < 0 || interfaceType > 1 ||
        !cache_get_value_name_int(0,"next_element_id",nextId) || nextId < 1 || nextId > 2147483646 ||
        !cache_get_value_name_int(0,"next_group_id",nextGroup) || nextGroup < 1 || nextGroup > 2147483646 ||
        !cache_get_value_name_int(0,"group_count",groupCount) || groupCount < 0 || groupCount > TDS_GROUPS ||
        (version == 1 && groupCount != 0) ||
        !cache_get_value_name_int(0,"element_count",count) || count < 0 || count > TDS_ELEMENTS ||
        rows != (count ? count : 1) || !TDSDB_ReadHex(0,"name_hex",name,sizeof(name)) || !TDS_ValidName(name) ||
        !TDSDB_ReadHex(0,"description_hex",description,sizeof(description)) ||
        (description[0] && !TDS_SafeText(description,TDS_DESCRIPTION-1)) ||
        !cache_get_value_name(0,"created_at",created,sizeof(created)) ||
        !cache_get_value_name(0,"updated_at",updated,sizeof(updated))) return false;
    if (creator != TDSHost_Account(gTDS[session][tsOwner]) && !TDSHost_Founder(gTDS[session][tsOwner])) return false;
    for (new e; e < TDS_ELEMENTS; e++) gTDSLoad[e][tdId] = 0;
    for (new g; g < TDS_GROUPS; g++) gTDSLoadedGroups[g][tgId] = 0;
    for (new e; e < count; e++)
    {
        for (new E_TDS_ELEMENT:f; f < E_TDS_ELEMENT; f++) gTDSLoad[e][f] = 0;
        new ordinal;
        if (!cache_get_value_name_int(e,"ordinal",ordinal) || ordinal != e) return false;
        for (new i; i < sizeof(gTDSFields); i++)
        {
            new E_TDS_ELEMENT:field = gTDSFields[i];
            if (TDS_FloatField(field))
            {
                new Float:value;
                if (!cache_get_value_name_float(e,gTDSColumns[i],value)) return false;
                gTDSLoad[e][field] = _:value;
            }
            else if (!cache_get_value_name_int(e,gTDSColumns[i],gTDSLoad[e][field])) return false;
        }
        if (!cache_get_value_name_int(e,"group_id",gTDSLoad[e][tdGroup]) || gTDSLoad[e][tdGroup] < 0) return false;
        if (gTDSLoad[e][tdGroup])
        {
            if (version == 1) return false;
            new g = -1, groupName[TDS_NAME];
            if (!TDSDB_ReadHex(e,"group_name_hex",groupName,sizeof(groupName)) || !TDS_ValidName(groupName)) return false;
            for (new i; i < groupsRead; i++) if (gTDSLoadedGroups[i][tgId] == gTDSLoad[e][tdGroup]) g = i;
            if (g < 0)
            {
                if (groupsRead >= TDS_GROUPS) return false;
                g = groupsRead++; gTDSLoadedGroups[g][tgId] = gTDSLoad[e][tdGroup];
                format(gTDSLoadedGroups[g][tgName],TDS_NAME,"%s",groupName);
            }
            else if (strcmp(gTDSLoadedGroups[g][tgName],groupName,false)) return false;
        }
        if (!TDSDB_ReadHex(e,"content_hex",gTDSLoad[e][tdText],TDS_TEXT) ||
            !TDS_DataValid(gTDSLoad[e]) || gTDSLoad[e][tdId] >= nextId) return false;
        if (e && gTDSLoad[e][tdOrder] <= gTDSLoad[e-1][tdOrder]) return false;
        for (new i; i < e; i++) if (gTDSLoad[i][tdId] == gTDSLoad[e][tdId]) return false;
    }
    if (groupsRead != groupCount || !TDS_GroupDataValid(gTDSLoad, gTDSLoadedGroups, nextGroup, groupCount)) return false;
    // Allocate off-screen first. Failure preserves the current model AND handles.
    new PlayerText:draws[TDS_ELEMENTS], playerid = gTDS[session][tsOwner];
    for (new i; i < TDS_ELEMENTS; i++) draws[i] = INVALID_PLAYER_TEXT_DRAW;
    for (new e; e < count; e++)
    {
        if (TDS_RenderData(playerid,gTDSLoad[e],draws[e],false,false)) continue;
        for (new i; i <= e; i++) if (draws[i] != INVALID_PLAYER_TEXT_DRAW) PlayerTextDrawDestroy(playerid,draws[i]);
        return false;
    }
    TDS_ClearProject(session);
    for (new e; e < count; e++)
    {
        for (new E_TDS_ELEMENT:f; f < E_TDS_ELEMENT; f++) gTDSElement[session][e][f] = gTDSLoad[e][f];
        gTDSDraw[session][e] = draws[e]; PlayerTextDrawShow(playerid,draws[e]);
    }
    gTDS[session][tsProject] = true; gTDS[session][tsProjectId] = id; gTDS[session][tsCreator] = creator;
    gTDS[session][tsRevision] = revision; gTDS[session][tsFormat] = TDS_FORMAT; gTDS[session][tsInterface] = interfaceType;
    gTDS[session][tsNextId] = nextId; gTDS[session][tsCount] = count;
    gTDS[session][tsNextGroupId] = nextGroup; gTDS[session][tsGroupCount] = groupCount;
    for (new g; g < groupsRead; g++) for (new E_TDS_GROUP:f; f < E_TDS_GROUP; f++) gTDSGroup[session][g][f] = gTDSLoadedGroups[g][f];
    if (count) TDS_PickOne(session, 0);
    format(gTDS[session][tsName],TDS_NAME,"%s",name);
    format(gTDS[session][tsDescription],TDS_DESCRIPTION,"%s",description);
    format(gTDS[session][tsCreated],20,"%s",created); format(gTDS[session][tsUpdated],20,"%s",updated);
    return true;
}

public TDSDB_Result(job, serial, playerid, token)
{
    if (job < 0 || job >= TDS_JOB_LIMIT || !gTDSJob[job][tjUsed] ||
        gTDSJob[job][tjSerial] != serial || gTDSJob[job][tjPlayer] != playerid || gTDSJob[job][tjToken] != token) return 1;
    if (!TDSDB_Live(job)) { gTDSJob[job][tjUsed] = false; return 1; }
    new s = gTDSJob[job][tjSession], operation = gTDSJob[job][tjOperation], rows, ok;
    if (!cache_get_row_count(rows)) return TDSDB_Fail(job,"БД не вернула результат. Проект не помечен сохранённым.");
    if (operation == TDS_DB_LIST)
    {
        if (rows < 0 || rows > TDS_PROJECT_PAGE+1) return TDSDB_Fail(job,"Некорректный список проектов.");
        new body[2048], name[TDS_NAME], line[96], count = rows > TDS_PROJECT_PAGE ? TDS_PROJECT_PAGE : rows;
        for (new i; i < count; i++)
        {
            new id;
            if (!cache_get_value_name_int(i,"project_id",id) || id <= 0 ||
                !TDSDB_ReadHex(i,"name_hex",name,sizeof(name)) || !TDS_ValidName(name))
                return TDSDB_Fail(job,"Некорректный список проектов.");
            gTDS[s][tsProjectRows][i] = id;
            format(line,sizeof(line),"#%d | %s\n",id,name); strcat(body,line,sizeof(body));
        }
        gTDS[s][tsProjectRowCount] = count; gTDS[s][tsMore] = rows > TDS_PROJECT_PAGE;
        gTDS[s][tsBusy] = 0; gTDSJob[job][tjUsed] = false;
        if (!count) SendClientMessage(playerid,TDS_COLOR,"На этой странице сохранённых проектов нет.");
        strcat(body,"Следующая страница\nПредыдущая страница",sizeof(body));
        return TDS_Dialog(playerid,TDS_PROJECTS,DIALOG_STYLE_LIST,"TD Studio | Сохранённые проекты",body);
    }
    if (operation == TDS_DB_LOAD)
    {
        if (!TDSDB_LoadResult(s,gTDSJob[job][tjProject],rows))
            return TDSDB_Fail(job,"Проект недоступен, формат/данные неверны или не хватает ресурсов. Текущий проект сохранён в памяти.");
    }
    else
    {
        if (rows != 1 || !cache_get_value_name_int(0,"ok",ok) || ok != 1)
            return TDSDB_Fail(job,"Операция БД отклонена: ошибка, лимит, права или конфликт ревизии. Изменения остаются в памяти.");
        if (operation == TDS_DB_SAVE)
        {
            new id, creator, revision, created[20], updated[20];
            if (!cache_get_value_name_int(0,"project_id",id) || id <= 0 ||
                !cache_get_value_name_int(0,"creator_id",creator) || creator <= 0 ||
                !cache_get_value_name_int(0,"revision",revision) || revision <= 0 ||
                !cache_get_value_name(0,"created_at",created,sizeof(created)) ||
                !cache_get_value_name(0,"updated_at",updated,sizeof(updated)) ||
                (gTDSJob[job][tjProject] && (id != gTDSJob[job][tjProject] || revision != gTDSJob[job][tjRevision]+1 || creator != gTDS[s][tsCreator])) ||
                (!gTDSJob[job][tjProject] && (revision != 1 || creator != TDSHost_Account(playerid))))
                return TDSDB_Fail(job,"Не удалось подтвердить результат сохранения. Проверьте список проектов.");
            gTDS[s][tsProjectId] = id; gTDS[s][tsCreator] = creator; gTDS[s][tsRevision] = revision;
            format(gTDS[s][tsCreated],20,"%s",created); format(gTDS[s][tsUpdated],20,"%s",updated);
            gTDS[s][tsDirty] = false;
        }
        else if (operation == TDS_DB_DELETE) TDS_ClearProject(s);
    }
    gTDS[s][tsBusy] = 0; gTDSJob[job][tjUsed] = false;
    SendClientMessage(playerid,TDS_COLOR,"Операция TextDraw Studio выполнена.");
    return TDS_Main(playerid);
}

stock bool:TDSDB_OnError(const callback[], const query[])
{
    if (!strcmp(callback,"TDSDB_Ready",false))
    {
        gTDSDBReady = false; print("[TDSTUDIO] Database check failed; apply migration 012."); return true;
    }
    if (strcmp(callback,"TDSDB_Result",false)) return false;
    // Error callbacks have no user arguments: recover the bounded request cookie.
    if (strfind(query,"/*TDS:",false) == 0)
    {
        new value, at = 6;
        while (query[at] >= '0' && query[at] <= '9')
        {
            new digit = query[at++]-'0';
            if (value > (2147483646-digit)/10) return true;
            value = value*10+digit;
        }
        if (query[at] != '*' || query[at+1] != '/') return true;
        for (new j; j < TDS_JOB_LIMIT; j++) if (gTDSJob[j][tjUsed] && gTDSJob[j][tjSerial] == value)
            TDSDB_Fail(j,"Ошибка соединения/запроса БД. Изменения не помечены сохранёнными.");
    }
    return true; // editor errors must not kick unrelated players
}
#else
stock TDSDB_Init() { return 0; }
stock TDSDB_Save(playerid, bool:asNew = false)
{
    #pragma unused asNew
    return SendClientMessage(playerid,ADMIN_ERROR,"Хранение проектов требует MySQL и миграцию 012.");
}
stock TDSDB_Load(playerid, project) {
    #pragma unused project
    return TDSDB_Save(playerid); }
stock TDSDB_List(playerid, page = 0) {
    #pragma unused page
    return TDSDB_Save(playerid); }
stock TDSDB_Delete(playerid) { return TDSDB_Save(playerid); }
stock bool:TDSDB_OnError(const callback[], const query[])
{
    #pragma unused callback, query
    return false;
}
#endif
