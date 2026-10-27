#if defined _tarkenso_tds_templates
    #endinput
#endif
#define _tarkenso_tds_templates

// One short-lived component, not a per-player library cache. SQL callbacks are
// serial Pawn invocations; these buffers never cross an asynchronous boundary.
new gTDSTemplate[TDS_ELEMENTS][E_TDS_ELEMENT];
new gTDSTemplateGroups[TDS_GROUPS][E_TDS_GROUP];
new gTDSTemplateNext, gTDSTemplateNextGroup;
new Float:gTDSTemplateX, Float:gTDSTemplateY;
new const gTDSTemplateCategories[][] = {"Кнопки", "Панели", "HUD", "Текстовые блоки", "Логотипы", "Прочее"};

stock TDS_TplSession(playerid)
{
    new s = TDS_RequireSession(playerid);
    if (s < 0 || gTDS[s][tsBusy] || gTDS[s][tsMode] == TDS_MOVE_MODE) return -1;
    return s;
}

stock bool:TDS_TplOwned(s)
{
    return s >= 0 && s < TDS_SESSIONS && gTDS[s][tsTplId] > 0 &&
        (gTDS[s][tsTplOwner] == TDSHost_Account(gTDS[s][tsOwner]) || TDSHost_Founder(gTDS[s][tsOwner]));
}

stock bool:TDS_TplBounds(&Float:l, &Float:t, &Float:r, &Float:b)
{
    new count;
    for (new e; e < TDS_ELEMENTS; e++) if (gTDSTemplate[e][tdId])
    {
        new Float:data[E_TDS_GEOMETRY], Float:el, Float:et, Float:er, Float:eb;
        for (new f; f < sizeof(gTDSGeoFields); f++) data[E_TDS_GEOMETRY:f] = Float:gTDSTemplate[e][gTDSGeoFields[f]];
        TDS_GeoRect(data, gTDSTemplate[e][tdType], gTDSTemplate[e][tdAlign], el, et, er, eb);
        if (!count || el < l) l = el;
        if (!count || et < t) t = et;
        if (!count || er > r) r = er;
        if (!count || eb > b) b = eb;
        count++;
    }
    return count > 0;
}

stock bool:TDS_TplValid(&count, &groups)
{
    if (!TDS_StateValid(gTDSTemplate, gTDSTemplateGroups, gTDSTemplateNext, gTDSTemplateNextGroup, count, groups) || !count) return false;
    new Float:l, Float:t, Float:r, Float:b;
    if (!TDS_TplBounds(l, t, r, b) || !TDS_InRange(gTDSTemplateX, -640.0, 640.0) ||
        !TDS_InRange(gTDSTemplateY, 0.0, 448.0)) return false;
    return floatabs(l-gTDSTemplateX) <= 0.0001 && floatabs(t-gTDSTemplateY) <= 0.0001;
}

stock bool:TDS_TplCapture(playerid)
{
    new s = TDS_LayoutSession(playerid), slots[TDS_ELEMENTS];
    if (s < 0) return false;
    new count = TDS_Targets(s, slots);
    if (!count || !TDS_ProjectValid(s)) return false;
    for (new e; e < TDS_ELEMENTS; e++) for (new E_TDS_ELEMENT:f; f < E_TDS_ELEMENT; f++) gTDSTemplate[e][f] = 0;
    for (new g; g < TDS_GROUPS; g++) for (new E_TDS_GROUP:f; f < E_TDS_GROUP; f++) gTDSTemplateGroups[g][f] = 0;
    // Preserve relative layer order even when the selected storage slots differ.
    new order[TDS_ELEMENTS], n = TDS_Order(s, order), at;
    for (new i; i < n; i++) for (new j; j < count; j++) if (slots[j] == order[i])
    {
        for (new E_TDS_ELEMENT:f; f < E_TDS_ELEMENT; f++) gTDSTemplate[at][f] = gTDSElement[s][order[i]][f];
        gTDSTemplate[at][tdOrder] = at; at++;
    }
    new groupCount;
    for (new g; g < TDS_GROUPS; g++) if (gTDSGroup[s][g][tgId])
    {
        new bool:used;
        for (new e; e < count; e++) if (gTDSTemplate[e][tdGroup] == gTDSGroup[s][g][tgId]) used = true;
        if (!used) continue; // partial selection keeps only its selected members
        for (new E_TDS_GROUP:f; f < E_TDS_GROUP; f++) gTDSTemplateGroups[groupCount][f] = gTDSGroup[s][g][f];
        groupCount++;
    }
    gTDSTemplateNext = gTDS[s][tsNextId]; gTDSTemplateNextGroup = gTDS[s][tsNextGroupId];
    new Float:r, Float:b; TDS_TplBounds(gTDSTemplateX, gTDSTemplateY, r, b);
    return TDS_TplValid(count, groupCount);
}

stock bool:TDS_TplGroupNameAvailable(const name[])
{
    for (new g; g < TDS_GROUPS; g++) if (gTDSStageGroups[g][tgId] && !strcmp(name, gTDSStageGroups[g][tgName], false)) return false;
    return TDS_ValidName(name);
}

// Trusted internal entry: caller has checked the lease and owns the DB busy lock
// or has passed LayoutSession. No SQL, callbacks or allocations until StateApply.
stock bool:TDS_TplInsertData(s, mode, Float:x = 0.0, Float:y = 0.0)
{
    new count, groups;
    if (s < 0 || s >= TDS_SESSIONS || !gTDS[s][tsUsed] || !gTDS[s][tsProject] ||
        gTDS[s][tsMode] == TDS_MOVE_MODE || !TDS_ProjectValid(s) || !TDS_TplValid(count, groups) ||
        count > TDS_ELEMENTS-gTDS[s][tsCount] || groups > TDS_GROUPS-gTDS[s][tsGroupCount] ||
        gTDS[s][tsNextId] > 2147483646-count || gTDS[s][tsNextGroupId] > 2147483646-groups ||
        mode < 0 || mode > 2 || !TDS_InRange(x, -1280.0, 1280.0) || !TDS_InRange(y, -896.0, 896.0)) return false;
    new Float:l, Float:t, Float:r, Float:b; TDS_TplBounds(l, t, r, b);
    if (mode == 0) { x = 320.0-(r-l)/2.0; y = 240.0-(b-t)/2.0; }
    if (mode == 2)
    {
        new active = gTDS[s][tsSelected]; if (!TDS_ElementValid(s, active)) return false;
        x += gTDSElement[s][active][tdX]; y += gTDSElement[s][active][tdY];
    }
    // Screen centering is exact, like GeoScreen; its center is already a node
    // for every supported grid step. Explicit point/offset placement snaps once.
    if (gTDS[s][tsSnap] && mode != 0)
    {
        if (gTDS[s][tsGridStep] < 0 || gTDS[s][tsGridStep] >= sizeof(gTDSGridSteps)) return false;
        new Float:step = gTDSGridSteps[gTDS[s][tsGridStep]];
        x = float(floatround(x/step))*step; y = float(floatround(y/step))*step;
    }
    for (new e; e < TDS_ELEMENTS; e++) for (new E_TDS_ELEMENT:f; f < E_TDS_ELEMENT; f++) gTDSStage[e][f] = gTDSElement[s][e][f];
    for (new g; g < TDS_GROUPS; g++) for (new E_TDS_GROUP:f; f < E_TDS_GROUP; f++) gTDSStageGroups[g][f] = gTDSGroup[s][g][f];
    new groupIds[TDS_GROUPS], firstId = gTDS[s][tsNextId], nextGroup = gTDS[s][tsNextGroupId];
    for (new g; g < TDS_GROUPS; g++) if (gTDSTemplateGroups[g][tgId])
    {
        new name[TDS_NAME]; format(name, sizeof(name), "%s", gTDSTemplateGroups[g][tgName]);
        if (!TDS_TplGroupNameAvailable(name))
        {
            new bool:found;
            for (new attempt; attempt <= TDS_GROUPS; attempt++)
            {
                new suffix[24]; format(suffix, sizeof(suffix), "_%d_%d", nextGroup, attempt);
                strmid(name, gTDSTemplateGroups[g][tgName], 0, TDS_NAME-1-strlen(suffix), sizeof(name));
                strcat(name, suffix, sizeof(name));
                if (TDS_TplGroupNameAvailable(name)) { found = true; break; }
            }
            if (!found) return false;
        }
        for (new to; to < TDS_GROUPS; to++) if (!gTDSStageGroups[to][tgId])
        {
            groupIds[g] = nextGroup++; gTDSStageGroups[to][tgId] = groupIds[g];
            format(gTDSStageGroups[to][tgName], TDS_NAME, "%s", name); break;
        }
    }
    new order[TDS_ELEMENTS], n = TDS_StateOrder(gTDSTemplate, order), next = firstId, lastOrder = -1;
    for (new e; e < TDS_ELEMENTS; e++) if (gTDSStage[e][tdId] && gTDSStage[e][tdOrder] > lastOrder) lastOrder = gTDSStage[e][tdOrder];
    if (lastOrder > 2147483646-count) return false;
    for (new i; i < n; i++)
    {
        new source = order[i], to;
        while (to < TDS_ELEMENTS && gTDSStage[to][tdId]) to++;
        if (to == TDS_ELEMENTS) return false;
        for (new E_TDS_ELEMENT:f; f < E_TDS_ELEMENT; f++) gTDSStage[to][f] = gTDSTemplate[source][f];
        gTDSStage[to][tdId] = next++; gTDSStage[to][tdOrder] = ++lastOrder;
        gTDSStage[to][tdX] = x+(gTDSTemplate[source][tdX]-gTDSTemplateX);
        gTDSStage[to][tdY] = y+(gTDSTemplate[source][tdY]-gTDSTemplateY);
        if (gTDSTemplate[source][tdGroup])
            for (new g; g < TDS_GROUPS; g++) if (gTDSTemplateGroups[g][tgId] == gTDSTemplate[source][tdGroup]) gTDSStage[to][tdGroup] = groupIds[g];
    }
    if (!TDS_StateApply(s, gTDS[s][tsName], gTDS[s][tsDescription], gTDS[s][tsInterface], next, nextGroup)) return false;
    gTDS[s][tsSelected] = -1; gTDS[s][tsSelectedGroup] = 0;
    for (new e; e < TDS_ELEMENTS; e++)
    {
        gTDSPicked[s][e] = TDS_ElementValid(s, e) && gTDSElement[s][e][tdId] >= firstId && gTDSElement[s][e][tdId] < next;
        if (gTDSPicked[s][e] && gTDS[s][tsSelected] < 0) gTDS[s][tsSelected] = e;
    }
    gTDS[s][tsDirty] = true; TDS_HRecord(s, TDS_H_INSERT, gTDS[s][tsTplId]); TDS_RenderStatus(s);
    return true;
}

stock bool:TDS_TplInsert(playerid, mode, Float:x = 0.0, Float:y = 0.0)
{
    new s = TDS_LayoutSession(playerid);
    return s >= 0 && TDS_TplInsertData(s, mode, x, y);
}

#if defined TARKENSO_DB_PRIMARY && !defined TDS_TEST_NO_DB
stock TDS_TplRequest(playerid, operation, id = 0)
{
    new s = TDS_TplSession(playerid);
    if (s < 0) return 0;
    if (!gTDSTemplatesReady)
        return SendClientMessage(playerid, ADMIN_ERROR, "Библиотека недоступна: примените миграцию 016_tdstudio_templates.sql.");
    if (operation < TDS_DB_TEMPLATE_SAVE || operation > TDS_DB_TEMPLATE_DELETE ||
        (operation != TDS_DB_TEMPLATE_SAVE && operation != TDS_DB_TEMPLATE_LIST && id <= 0)) return 0;
    new name[TDS_NAME*2], description[TDS_DESCRIPTION*2];
    if (operation == TDS_DB_TEMPLATE_SAVE || operation == TDS_DB_TEMPLATE_UPDATE)
    {
        if (!TDS_ValidName(gTDS[s][tsTplName]) || (gTDS[s][tsTplDescription][0] && !TDS_SafeText(gTDS[s][tsTplDescription], TDS_DESCRIPTION-1)) ||
            !TDS_Hex(gTDS[s][tsTplName], name, sizeof(name)) || !TDS_Hex(gTDS[s][tsTplDescription], description, sizeof(description))) return 0;
    }
    if (operation == TDS_DB_TEMPLATE_SAVE &&
        (gTDS[s][tsTplCategory] < 1 || gTDS[s][tsTplCategory] > 65535 ||
        gTDS[s][tsTplVisibility] < 0 || gTDS[s][tsTplVisibility] > 1 ||
        (gTDS[s][tsTplVisibility] && !TDSHost_Founder(playerid)) ||
        !TDS_TplCapture(playerid) || !TDS_EncodeData(gTDSTemplate, gTDSTemplateGroups))) return 0;
    if ((operation == TDS_DB_TEMPLATE_UPDATE || operation == TDS_DB_TEMPLATE_DELETE) &&
        (!TDS_TplOwned(s) || gTDS[s][tsTplId] != id || gTDS[s][tsTplRevision] < 1)) return 0;
    if (operation == TDS_DB_TEMPLATE_INSERT && !gTDS[s][tsProject]) return 0;
    if (operation == TDS_DB_TEMPLATE_LIST && (gTDS[s][tsTplPage] < 0 || gTDS[s][tsTplPage] > 50000 ||
        gTDS[s][tsTplFilter] < 0 || gTDS[s][tsTplFilter] > 65535)) return 0;
    new j = TDSDB_Begin(playerid, operation, id);
    if (j < 0) return 0;
    new actor = TDSHost_Account(playerid), priv = _:TDSHost_Founder(playerid);
    switch (operation)
    {
        case TDS_DB_TEMPLATE_SAVE:
        {
            mysql_format(gDBPrimary, gTDSQuery, sizeof(gTDSQuery),
                "/*TDS:%d*/ CALL tds_template_save(%d,%d,X'%s',X'%s',%d,%d,%d,%d,CONVERT(X'",
                gTDSJob[j][tjSerial], actor, priv, name, description, gTDS[s][tsTplCategory], gTDS[s][tsTplVisibility],
                gTDS[s][tsInterface], gTDSTemplateNext);
            if (!TDSDB_QueryHex(gTDSWire)) return TDSDB_Fail(j, "Шаблон превышает размер запроса.");
            new tail[128]; format(tail, sizeof(tail), "' USING ascii),%d,CONVERT(X'", gTDSTemplateNextGroup);
            if (!TDS_Append(gTDSQuery, sizeof(gTDSQuery), tail) || !TDSDB_QueryHex(gTDSGroupWire)) return TDSDB_Fail(j, "Группы шаблона превышают размер запроса.");
            format(tail, sizeof(tail), "' USING ascii),%.9f,%.9f)", gTDSTemplateX, gTDSTemplateY);
            if (!TDS_Append(gTDSQuery, sizeof(gTDSQuery), tail)) return TDSDB_Fail(j, "Шаблон превышает размер запроса.");
        }
        case TDS_DB_TEMPLATE_LIST: mysql_format(gDBPrimary, gTDSQuery, sizeof(gTDSQuery),
            "/*TDS:%d*/ CALL tds_template_list(%d,%d,%d,%d)", gTDSJob[j][tjSerial], actor, priv, gTDS[s][tsTplFilter], gTDS[s][tsTplPage]*TDS_PROJECT_PAGE);
        case TDS_DB_TEMPLATE_INFO: mysql_format(gDBPrimary, gTDSQuery, sizeof(gTDSQuery),
            "/*TDS:%d*/ CALL tds_template_info(%d,%d,%d)", gTDSJob[j][tjSerial], id, actor, priv);
        case TDS_DB_TEMPLATE_INSERT: mysql_format(gDBPrimary, gTDSQuery, sizeof(gTDSQuery),
            "/*TDS:%d*/ CALL tds_template_load(%d,%d,%d)", gTDSJob[j][tjSerial], id, actor, priv);
        case TDS_DB_TEMPLATE_UPDATE: mysql_format(gDBPrimary, gTDSQuery, sizeof(gTDSQuery),
            "/*TDS:%d*/ CALL tds_template_update(%d,%d,%d,%d,X'%s',X'%s')", gTDSJob[j][tjSerial], id, actor, priv, gTDS[s][tsTplRevision], name, description);
        case TDS_DB_TEMPLATE_DELETE: mysql_format(gDBPrimary, gTDSQuery, sizeof(gTDSQuery),
            "/*TDS:%d*/ CALL tds_template_delete(%d,%d,%d,%d)", gTDSJob[j][tjSerial], id, actor, priv, gTDS[s][tsTplRevision]);
    }
    return TDSDB_Submit(j);
}

stock bool:TDS_TplReadMeta(s, id)
{
    new actual, owner, revision, version, category, visibility, name[TDS_NAME], description[TDS_DESCRIPTION], created[20], updated[20];
    if (!cache_get_value_name_int(0,"template_id",actual) || actual != id ||
        !cache_get_value_name_int(0,"creator_id",owner) || owner <= 0 ||
        !cache_get_value_name_int(0,"revision",revision) || revision < 1 ||
        !cache_get_value_name_int(0,"format_version",version) || version != TDS_FORMAT ||
        !cache_get_value_name_int(0,"category",category) || category < 1 || category > 65535 ||
        !cache_get_value_name_int(0,"visibility",visibility) || visibility < 0 || visibility > 1 ||
        (owner != TDSHost_Account(gTDS[s][tsOwner]) && !TDSHost_Founder(gTDS[s][tsOwner]) && !visibility) ||
        !TDSDB_ReadHex(0,"name_hex",name,sizeof(name)) || !TDS_ValidName(name) ||
        !TDSDB_ReadHex(0,"description_hex",description,sizeof(description)) || (description[0] && !TDS_SafeText(description,TDS_DESCRIPTION-1)) ||
        !cache_get_value_name(0,"created_at",created,sizeof(created)) || !cache_get_value_name(0,"updated_at",updated,sizeof(updated))) return false;
    gTDS[s][tsTplId] = id; gTDS[s][tsTplOwner] = owner; gTDS[s][tsTplRevision] = revision;
    gTDS[s][tsTplCategory] = category; gTDS[s][tsTplVisibility] = visibility;
    format(gTDS[s][tsTplName], TDS_NAME, "%s", name); format(gTDS[s][tsTplDescription], TDS_DESCRIPTION, "%s", description);
    format(gTDS[s][tsTplCreated], 20, "%s", created); format(gTDS[s][tsTplUpdated], 20, "%s", updated);
    return true;
}

stock TDS_TplResult(job, rows)
{
    new s = gTDSJob[job][tjSession], p = gTDSJob[job][tjPlayer], operation = gTDSJob[job][tjOperation];
    if (operation == TDS_DB_TEMPLATE_LIST)
    {
        if (rows < 0 || rows > TDS_PROJECT_PAGE+1) return TDSDB_Fail(job, "Некорректная страница шаблонов.");
        new body[3000], line[128], count = rows > TDS_PROJECT_PAGE ? TDS_PROJECT_PAGE : rows;
        for (new i; i < count; i++)
        {
            new id, visibility, category, elements, name[TDS_NAME];
            if (!cache_get_value_name_int(i,"template_id",id) || id <= 0 ||
                !cache_get_value_name_int(i,"visibility",visibility) || visibility < 0 || visibility > 1 ||
                !cache_get_value_name_int(i,"category",category) || category < 1 || category > 65535 ||
                !cache_get_value_name_int(i,"element_count",elements) || elements < 1 || elements > TDS_ELEMENTS ||
                !TDSDB_ReadHex(i,"name_hex",name,sizeof(name)) || !TDS_ValidName(name)) return TDSDB_Fail(job, "Некорректные данные списка шаблонов.");
            gTDS[s][tsTplRows][i] = id;
            format(line,sizeof(line),"#%d | %s | %s | %d элементов\n",id,name,visibility ? ("Общий") : ("Личный"),elements); strcat(body,line,sizeof(body));
        }
        gTDS[s][tsTplRowCount] = count; gTDS[s][tsTplMore] = rows > TDS_PROJECT_PAGE;
        gTDS[s][tsBusy] = 0; gTDSJob[job][tjUsed] = false;
        if (!count) SendClientMessage(p,TDS_COLOR,"В этой категории/странице шаблонов нет.");
        strcat(body,"Следующая страница\nПредыдущая страница",sizeof(body));
        return TDS_Dialog(p,TDS_TEMPLATE_LIST,DIALOG_STYLE_LIST,"TD Studio | Библиотека шаблонов",body);
    }
    if (operation == TDS_DB_TEMPLATE_INFO || operation == TDS_DB_TEMPLATE_INSERT)
    {
        if (rows < 1 || rows > TDS_ELEMENTS || (operation == TDS_DB_TEMPLATE_INFO && rows != 1) ||
            !TDS_TplReadMeta(s,gTDSJob[job][tjProject])) return TDSDB_Fail(job,"Шаблон недоступен или имеет несовместимый формат.");
        if (operation == TDS_DB_TEMPLATE_INSERT)
        {
            new count, nextId, nextGroup, groups;
            if (!cache_get_value_name_int(0,"element_count",count) || count < 1 || count > TDS_ELEMENTS || rows != count ||
                !cache_get_value_name_int(0,"next_element_id",nextId) || !cache_get_value_name_int(0,"next_group_id",nextGroup) ||
                !cache_get_value_name_int(0,"group_count",groups) || groups < 0 || groups > TDS_GROUPS ||
                !TDSDB_ReadData(count, TDS_FORMAT, nextId, nextGroup, groups) ||
                !cache_get_value_name_float(0,"origin_x",gTDSTemplateX) || !cache_get_value_name_float(0,"origin_y",gTDSTemplateY))
                return TDSDB_Fail(job,"Шаблон повреждён. Проект не изменён.");
            for (new e; e < TDS_ELEMENTS; e++) for (new E_TDS_ELEMENT:f; f < E_TDS_ELEMENT; f++) gTDSTemplate[e][f] = gTDSStage[e][f];
            for (new g; g < TDS_GROUPS; g++) for (new E_TDS_GROUP:f; f < E_TDS_GROUP; f++) gTDSTemplateGroups[g][f] = gTDSStageGroups[g][f];
            gTDSTemplateNext = nextId; gTDSTemplateNextGroup = nextGroup;
            if (!TDS_TplInsertData(s,gTDS[s][tsTplPlace],gTDS[s][tsTplX],gTDS[s][tsTplY]))
                return TDSDB_Fail(job,"Вставка отклонена: лимит элементов/групп, координаты или нехватка хэндлов. Проект не изменён.");
        }
        gTDS[s][tsBusy] = 0; gTDSJob[job][tjUsed] = false;
        return operation == TDS_DB_TEMPLATE_INFO ? TDS_TplDetail(p) : TDS_TplMenu(p);
    }
    new ok;
    if (rows != 1 || !cache_get_value_name_int(0,"ok",ok) || ok != 1)
        return TDSDB_Fail(job,"Операция с шаблоном отклонена: права, ревизия, лимит или ошибка БД.");
    if (operation == TDS_DB_TEMPLATE_SAVE)
    {
        new id;
        if (!cache_get_value_name_int(0,"template_id",id) || id <= 0) return TDSDB_Fail(job,"Не удалось подтвердить сохранение шаблона.");
    }
    gTDS[s][tsTplRowCount] = 0; gTDS[s][tsTplId] = 0;
    gTDS[s][tsBusy] = 0; gTDSJob[job][tjUsed] = false;
    SendClientMessage(p,TDS_COLOR,"Операция с шаблоном выполнена.");
    return TDS_TplMenu(p);
}
#else
stock TDS_TplRequest(playerid, operation, id = 0)
{
    #pragma unused operation, id
    return SendClientMessage(playerid,ADMIN_ERROR,"Библиотека шаблонов требует MySQL и миграцию 016.");
}
#endif
