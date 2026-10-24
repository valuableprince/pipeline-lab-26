#if defined _tarkenso_npc_editor_included
    #endinput
#endif
#define _tarkenso_npc_editor_included

// NPC-CORE-01: dialogues and short /nset command.
// Database identities are immutable. No NPC is published before explicit confirm.
#define NPC_ED_MAIN       (6320)
#define NPC_ED_SKIN       (6321)
#define NPC_ED_NAME       (6322)
#define NPC_ED_MODE       (6323)
#define NPC_ED_ROUTE      (6324)
#define NPC_ED_CONFIRM    (6325)
#define NPC_ED_LIST       (6326)
#define NPC_ED_ACTION     (6327)
#define NPC_ED_DELETE     (6328)
#define NPC_ED_PAGE_SIZE  (12)

#if defined TARKENSO_DB_PRIMARY
new gNpcEdOwner = INVALID_PLAYER_ID;
new gNpcEdId;
new gNpcEdVersion;
new gNpcEdSkin;
new gNpcEdGender[9];
new gNpcEdName[25];
new gNpcEdMode;
new gNpcEdRoute;
new bool:gNpcEdCaptured;
new Float:gNpcEdX;
new Float:gNpcEdY;
new Float:gNpcEdZ;
new Float:gNpcEdFacing;
new gNpcEdWorld;
new gNpcEdInterior;
new gNpcEdPreview = INVALID_OBJECT_ID;

new gNpcEdPage[MAX_PLAYERS];
new gNpcEdCount[MAX_PLAYERS];
new gNpcEdRow[MAX_PLAYERS][NPC_ED_PAGE_SIZE];
new gNpcEdTarget[MAX_PLAYERS];

stock NpcEditor_Cancel()
{
    if (gNpcEdPreview != INVALID_OBJECT_ID && gNpcEdOwner != INVALID_PLAYER_ID &&
        IsPlayerConnected(gNpcEdOwner))
        DestroyPlayerObject(gNpcEdOwner, gNpcEdPreview);
    gNpcEdPreview = INVALID_OBJECT_ID;
    gNpcEdOwner = INVALID_PLAYER_ID;
    gNpcEdId = 0;
    gNpcEdVersion = 0;
    gNpcEdSkin = 0;
    gNpcEdName[0] = EOS;
    gNpcEdGender[0] = EOS;
    gNpcEdMode = NPC_CORE_IDLE;
    gNpcEdRoute = -1;
    gNpcEdCaptured = false;
    return 1;
}

stock bool:NpcEditor_Editable(playerid)
{
    if (!NpcCore_Require(playerid)) return false;
    if (gNpcEdOwner != playerid)
    {
        SendClientMessage(playerid, ADMIN_ERROR, "Откройте NPC в Studio и начните создание или редактирование.");
        return false;
    }
    return true;
}

stock bool:NpcEditor_NameOK(const name[])
{
    new len = strlen(name);
    if (len < 2 || len > 23) return false;
    if (!((name[0] >= 'A' && name[0] <= 'Z') ||
        (name[0] >= 'a' && name[0] <= 'z'))) return false;
    for (new i = 1; i < len; i++)
    {
        if (!((name[i] >= 'A' && name[i] <= 'Z') ||
            (name[i] >= 'a' && name[i] <= 'z') ||
            (name[i] >= '0' && name[i] <= '9') || name[i] == '_'))
            return false;
    }
    return true;
}

stock bool:NpcEditor_AutoName(name[], maxlen)
{
    new q[320], Cache:c, rows;
    mysql_format(gDBPrimary, q, sizeof(q),
        "SELECT n.display_name FROM npc_names n WHERE n.gender='%e' AND n.is_active=1 AND NOT EXISTS(SELECT 1 FROM npcs p WHERE LOWER(p.display_name)=LOWER(n.display_name)) ORDER BY RAND() LIMIT 1", gNpcEdGender);
    if (!NpcCore_Query(q, c)) return false;
    new bool:ok = cache_get_row_count(rows) && rows == 1 &&
        cache_get_value_name(0, "display_name", name, maxlen);
    cache_delete(c);
    return ok && NpcEditor_NameOK(name);
}

stock NpcEditor_SkinPrompt(playerid)
{
    ShowPlayerDialog(playerid, NPC_ED_SKIN, DIALOG_STYLE_INPUT,
        "STUDIO | Скин персонажа",
        "Введите ID скина из справочника npc_skins MariaDB.\nПол определяется по этому справочнику.\nНеизвестные ID отклоняются.", "Далее", "Отмена");
    return 1;
}

stock NpcEditor_NamePrompt(playerid)
{
    new text[200];
    format(text, sizeof(text),
        "Пол скина: %s.\nВведите имя из латинских букв/цифр (2–23 символа).\nОставьте пустым для автоматического выбора из npc_names.",
        !strcmp(gNpcEdGender, "female") ? ("женский") : ("мужской"));
    ShowPlayerDialog(playerid, NPC_ED_NAME, DIALOG_STYLE_INPUT,
        "STUDIO | Имя персонажа", text, "Далее", "Отмена");
    return 1;
}

stock NpcEditor_ModePrompt(playerid)
{
    ShowPlayerDialog(playerid, NPC_ED_MODE, DIALOG_STYLE_LIST,
        "STUDIO | Поведение NPC",
        "Стоит на месте (позиция через /nset)\nДвигается по маршруту",
        "Выбрать", "Назад");
    return 1;
}

stock NpcEditor_Routes(playerid)
{
    new list[1150], line[64];
    for (new i = 0; i < NPCROUTE_COUNT; i++)
    {
        format(line, sizeof(line), "Маршрут %d | %s\n", i,
            gNPCRouteInvalid[i] ? ("повреждён") :
            (NPCRoute_Has(i) ? ("сохранён") : ("пустой")));
        strcat(list, line, sizeof(list));
    }
    strcat(list, "Проложить новый маршрут сейчас", sizeof(list));
    ShowPlayerDialog(playerid, NPC_ED_ROUTE, DIALOG_STYLE_LIST,
        "STUDIO | Выберите маршрут", list, "Выбрать", "Назад");
    return 1;
}

stock NpcEditor_Confirm(playerid)
{
    if (!NpcEditor_Editable(playerid)) return 1;
    new info[360], kind[30], place[95];
    format(kind, sizeof(kind), "%s",
        gNpcEdMode == NPC_CORE_IDLE ? ("стоит") : ("двигается"));
    if (gNpcEdMode == NPC_CORE_IDLE)
        format(place, sizeof(place), "XYZ: %.2f / %.2f / %.2f",
            gNpcEdX, gNpcEdY, gNpcEdZ);
    else
        format(place, sizeof(place), "Маршрут: %d (должен быть сохранён)",
            gNpcEdRoute);
    format(info, sizeof(info),
        "NPC ID: %d (%s)\nИмя: %s\nСкин: %d (%s)\nПоведение: %s\n%s\nЗаписать изменения в MariaDB и опубликовать?",
        gNpcEdId, gNpcEdId ? ("редактирование") : ("новый ID автоматически"),
        gNpcEdName, gNpcEdSkin, gNpcEdGender, kind, place);
    ShowPlayerDialog(playerid, NPC_ED_CONFIRM, DIALOG_STYLE_MSGBOX,
        "STUDIO | Подтвердить NPC", info, "Сохранить", "Назад");
    return 1;
}

stock NpcEditor_Open(playerid)
{
    if (!NpcCore_Require(playerid)) return 1;
    new info[220];
    format(info, sizeof(info),
        "Создать нового NPC\nСписок NPC / управление\n%s\nОтменить черновик\nОбновить NPC из MariaDB",
        gNpcEdOwner == playerid ? ("Сохранить открытый черновик") :
        ("Нет открытого черновика"));
    ShowPlayerDialog(playerid, NPC_ED_MAIN, DIALOG_STYLE_LIST,
        "TARKENSO STUDIO | Персонажи NPC", info, "Открыть", "Закрыть");
    return 1;
}

stock NpcEditor_List(playerid)
{
    if (!NpcCore_Require(playerid)) return 1;
    new q[160], Cache:c, rows, list[1350], line[78];
    format(q, sizeof(q),
        "SELECT npc_id,display_name,is_enabled,behavior FROM npcs ORDER BY npc_id LIMIT %d,%d",
        gNpcEdPage[playerid] * NPC_ED_PAGE_SIZE, NPC_ED_PAGE_SIZE + 1);
    if (!NpcCore_Query(q, c))
        return SendClientMessage(playerid, ADMIN_ERROR, "Ошибка чтения списка NPC.");
    if (!cache_get_row_count(rows)) { cache_delete(c); return 1; }
    gNpcEdCount[playerid] = rows < NPC_ED_PAGE_SIZE ? rows : NPC_ED_PAGE_SIZE;
    for (new i = 0; i < gNpcEdCount[playerid]; i++)
    {
        new id, enabled, behavior, name[25];
        cache_get_value_name_int(i, "npc_id", id);
        cache_get_value_name_int(i, "is_enabled", enabled);
        cache_get_value_name_int(i, "behavior", behavior);
        cache_get_value_name(i, "display_name", name, sizeof(name));
        gNpcEdRow[playerid][i] = id;
        format(line, sizeof(line), "#%d %s | %s | %s\n",
            id, name, enabled ? ("активен") : ("отключён"),
            behavior == NPC_CORE_IDLE ? ("стоит") : ("ходит"));
        strcat(list, line, sizeof(list));
    }
    if (gNpcEdPage[playerid] > 0)
        strcat(list, "Предыдущая страница\n", sizeof(list));
    if (rows > NPC_ED_PAGE_SIZE)
        strcat(list, "Следующая страница\n", sizeof(list));
    cache_delete(c);
    if (!list[0]) format(list, sizeof(list), "Пусто: вернуться");
    ShowPlayerDialog(playerid, NPC_ED_LIST, DIALOG_STYLE_LIST,
        "TARKENSO STUDIO | База NPC", list, "Выбрать", "Назад");
    return 1;
}

stock NpcEditor_Action(playerid)
{
    if (!NpcCore_Require(playerid)) return 1;
    new title[84];
    format(title, sizeof(title), "STUDIO | NPC #%d", gNpcEdTarget[playerid]);
    ShowPlayerDialog(playerid, NPC_ED_ACTION, DIALOG_STYLE_LIST, title,
        "Изменить NPC\nВключить NPC\nВыключить NPC\nУдалить NPC",
        "Выбрать", "Назад");
    return 1;
}

stock NpcEditor_Create(playerid)
{
    if (!NpcCore_Require(playerid)) return 1;
    if (gNpcEdOwner != INVALID_PLAYER_ID && gNpcEdOwner != playerid)
        return SendClientMessage(playerid, ADMIN_ERROR, "Другой основатель редактирует NPC.");
    NpcEditor_Cancel();
    gNpcEdOwner = playerid;
    return NpcEditor_SkinPrompt(playerid);
}

stock NpcEditor_Edit(playerid, npcId)
{
    if (!NpcCore_Require(playerid)) return 1;
    if (gNpcEdOwner != INVALID_PLAYER_ID && gNpcEdOwner != playerid)
        return SendClientMessage(playerid, ADMIN_ERROR, "Черновик NPC занят.");
    new q[320], Cache:c, rows;
    format(q, sizeof(q),
        "SELECT n.npc_id,n.display_name,n.skin_id,s.gender,n.behavior,n.route_slot,n.pos_x,n.pos_y,n.pos_z,n.facing,n.world_id,n.interior_id,n.row_version FROM npcs n JOIN npc_skins s ON s.skin_id=n.skin_id WHERE n.npc_id=%d LIMIT 1", npcId);
    if (!NpcCore_Query(q, c)) return SendClientMessage(playerid, ADMIN_ERROR, "Ошибка загрузки NPC.");
    if (!cache_get_row_count(rows) || rows != 1) { cache_delete(c); return 1; }
    NpcEditor_Cancel();
    gNpcEdOwner = playerid;
    gNpcEdId = npcId;
    cache_get_value_name_int(0, "skin_id", gNpcEdSkin);
    cache_get_value_name_int(0, "row_version", gNpcEdVersion);
    cache_get_value_name_int(0, "behavior", gNpcEdMode);
    if (gNpcEdMode == NPC_CORE_ROUTE)
        cache_get_value_name_int(0, "route_slot", gNpcEdRoute);
    cache_get_value_name(0, "gender", gNpcEdGender, sizeof(gNpcEdGender));
    cache_get_value_name(0, "display_name", gNpcEdName, sizeof(gNpcEdName));
    cache_get_value_name_float(0, "pos_x", gNpcEdX);
    cache_get_value_name_float(0, "pos_y", gNpcEdY);
    cache_get_value_name_float(0, "pos_z", gNpcEdZ);
    cache_get_value_name_float(0, "facing", gNpcEdFacing);
    cache_get_value_name_int(0, "world_id", gNpcEdWorld);
    cache_get_value_name_int(0, "interior_id", gNpcEdInterior);
    gNpcEdCaptured = true;
    cache_delete(c);
    return NpcEditor_SkinPrompt(playerid);
}

stock NpcEditor_Capture(playerid)
{
    if (!NpcEditor_Editable(playerid)) return 1;
    if (gNpcEdMode != NPC_CORE_IDLE)
        return SendClientMessage(playerid, ADMIN_ERROR,
            "Для ходящего NPC назначьте сохранённый маршрут и используйте /nsave.");
    if (GetPlayerState(playerid) != PLAYER_STATE_ONFOOT ||
        IsPlayerInAnyVehicle(playerid))
        return SendClientMessage(playerid, ADMIN_ERROR, "Встаньте пешком в нужное место.");
    new Float:x, Float:y, Float:z, Float:angle;
    GetPlayerPos(playerid, x, y, z);
    GetPlayerFacingAngle(playerid, angle);
    new world = GetPlayerVirtualWorld(playerid);
    new interior = GetPlayerInterior(playerid);
    if (!NPCRoute_PointOK(x, y, z) || world < 0 || world > 100000 ||
        interior < 0 || interior > 255 || angle < 0.0 || angle > 360.0)
        return SendClientMessage(playerid, ADMIN_ERROR, "Координаты или мир вне допустимого диапазона.");
    if (gNpcEdPreview != INVALID_OBJECT_ID)
        DestroyPlayerObject(playerid, gNpcEdPreview);
    gNpcEdX = x; gNpcEdY = y; gNpcEdZ = z;
    gNpcEdFacing = angle;
    gNpcEdWorld = world; gNpcEdInterior = interior;
    gNpcEdCaptured = true;
    // Private location marker, not an NPC visible to other players.
    gNpcEdPreview = CreatePlayerObject(playerid, 19131,
        x, y, z + 0.45, 0.0, 0.0, 0.0);
    SendClientMessage(playerid, NPC_CORE_COLOR,
        "Позиция сохранена в черновик. Метка видна только вам. Подтвердите публикацию.");
    return NpcEditor_Confirm(playerid);
}

stock NpcEditor_Save(playerid)
{
    if (!NpcEditor_Editable(playerid)) return 1;
    if (!NpcEditor_NameOK(gNpcEdName))
        return SendClientMessage(playerid, ADMIN_ERROR, "Недопустимое имя NPC.");
    if (gNpcEdMode == NPC_CORE_IDLE && !gNpcEdCaptured)
        return SendClientMessage(playerid, ADMIN_ERROR, "Сначала встаньте на место и введите /nset.");
    if (gNpcEdMode == NPC_CORE_ROUTE)
    {
        // A freshly recorded route has been committed to SQL but may not yet
        // be present in the runtime cache until apply/reload.
        if (gNpcEdRoute >= 0 && !NPCRoute_Has(gNpcEdRoute) &&
            !gNPCRouteRecording)
            NPCRoute_Load(gNpcEdRoute);
        if (!NPCRoute_Has(gNpcEdRoute))
            return SendClientMessage(playerid, ADMIN_ERROR,
                "Маршрут ещё не сохранён. Завершите /npcroute stop и /npcroute save.");
        NPCRoute_GetPoint(gNpcEdRoute, 0, gNpcEdX, gNpcEdY, gNpcEdZ);
        gNpcEdFacing = 0.0; gNpcEdWorld = 0; gNpcEdInterior = 0;
    }
    new q[760], Cache:c, changed, createdId = 0;
    new routeSql[12];
    if (gNpcEdMode == NPC_CORE_ROUTE)
        format(routeSql, sizeof(routeSql), "%d", gNpcEdRoute);
    else format(routeSql, sizeof(routeSql), "NULL");
    if (!gNpcEdId)
    {
        mysql_format(gDBPrimary, q, sizeof(q),
            "INSERT INTO npcs (display_name,skin_id,behavior,route_slot,pos_x,pos_y,pos_z,facing,world_id,interior_id,is_enabled) VALUES ('%e',%d,%d,%s,%.4f,%.4f,%.4f,%.3f,%d,%d,1)",
            gNpcEdName, gNpcEdSkin, gNpcEdMode,
            routeSql,
            gNpcEdX,gNpcEdY,gNpcEdZ,gNpcEdFacing,gNpcEdWorld,gNpcEdInterior);

        if (!NpcCore_Query(q, c))
            return SendClientMessage(playerid, ADMIN_ERROR,
                "SQL INSERT отклонён (возможно имя уже занято). Черновик сохранён.");
        createdId = cache_insert_id();
        cache_delete(c);
        if (createdId <= 0)
            return SendClientMessage(playerid, ADMIN_ERROR, "Нет подтверждённого ID после INSERT.");
    }
    else
    {
        // optimistic concurrency: edited revision must still be current
        mysql_format(gDBPrimary, q, sizeof(q),
            "UPDATE npcs SET display_name='%e',skin_id=%d,behavior=%d,route_slot=%s,pos_x=%.4f,pos_y=%.4f,pos_z=%.4f,facing=%.3f,world_id=%d,interior_id=%d,row_version=row_version+1 WHERE npc_id=%d AND row_version=%d",
            gNpcEdName,gNpcEdSkin,gNpcEdMode,
            routeSql,
            gNpcEdX,gNpcEdY,gNpcEdZ,gNpcEdFacing,
            gNpcEdWorld,gNpcEdInterior,gNpcEdId,gNpcEdVersion);

        if (!NpcCore_Query(q, c))
            return SendClientMessage(playerid, ADMIN_ERROR, "Ошибка UPDATE (имя/схема/доступ).");
        changed = cache_affected_rows();
        cache_delete(c);
        if (changed != 1)
            return SendClientMessage(playerid, ADMIN_ERROR,
                "Конфликт ревизии: запись изменилась после открытия. Отмените и загрузите заново.");
        createdId = gNpcEdId;
    }
    NpcEditor_Cancel();
    new msg[120];
    format(msg, sizeof(msg), "[NPC-CORE] Сохранён персонаж #%d.", createdId);
    SendClientMessage(playerid, NPC_CORE_COLOR, msg);
    if (!NpcCore_Reload())
        SendClientMessage(playerid, ADMIN_ERROR,
            "SQL сохранён, но мир не обновился. Проверьте записи NPC и лимиты.");
    return NpcEditor_Open(playerid);
}

stock NpcEditor_Toggle(playerid, npcId, bool:enabled)
{
    if (!NpcCore_Require(playerid)) return 1;
    new q[235], Cache:c, rows, mode, route;
    format(q, sizeof(q),
        "SELECT behavior,route_slot FROM npcs WHERE npc_id=%d LIMIT 1",npcId);
    if (!NpcCore_Query(q,c)) return 1;
    new bool:ok = cache_get_row_count(rows) && rows == 1 &&
        cache_get_value_name_int(0,"behavior",mode);
    if (ok && mode == NPC_CORE_ROUTE)
        ok = cache_get_value_name_int(0,"route_slot",route) && NPCRoute_Has(route);
    cache_delete(c);
    if (!ok)
        return SendClientMessage(playerid,ADMIN_ERROR,"NPC или сохранённый маршрут отсутствует.");
    format(q,sizeof(q),
        "UPDATE npcs SET is_enabled=%d,row_version=row_version+1 WHERE npc_id=%d",
        _:enabled,npcId);
    if (!NpcCore_Exec(q))
        return SendClientMessage(playerid,ADMIN_ERROR,"SQL не сохранил состояние NPC.");
    if (!NpcCore_Reload())
        SendClientMessage(playerid,ADMIN_ERROR,"Не удалось обновить мир NPC.");
    return NpcEditor_List(playerid);
}

stock NpcEditor_Delete(playerid,npcId)
{
    if (!NpcCore_Require(playerid)) return 1;
    new q[95];
    format(q,sizeof(q),"DELETE FROM npcs WHERE npc_id=%d",npcId);
    if (!NpcCore_Exec(q))
        return SendClientMessage(playerid,ADMIN_ERROR,"SQL не позволил удалить NPC.");
    if (gNpcEdOwner == playerid && gNpcEdId == npcId) NpcEditor_Cancel();
    if (!NpcCore_Reload())
        SendClientMessage(playerid,ADMIN_ERROR,"Ошибка обновления мира NPC.");
    return NpcEditor_List(playerid);
}

stock NpcEditor_Command(playerid, const params[])
{
    if (!NpcCore_Require(playerid)) return 1;
    if (isnull(params)) return NpcEditor_Open(playerid);
    if (!strcmp(params,"new",true)) return NpcEditor_Create(playerid);
    if (!strcmp(params,"list",true)) return NpcEditor_List(playerid);
    if (!strcmp(params,"cancel",true))
    {
        if (gNpcEdOwner != playerid) return 1;
        NpcEditor_Cancel();
        return NpcEditor_Open(playerid);
    }
    if (!strcmp(params,"publish",true) || !strcmp(params,"save",true))
        return NpcEditor_Confirm(playerid);
    if (!strcmp(params,"refresh",true))
    {
        if (!NpcCore_Reload())
            return SendClientMessage(playerid,ADMIN_ERROR,"Ошибка обновления NPC.");
        return NpcEditor_Open(playerid);
    }
    SendClientMessage(playerid,NPC_CORE_COLOR,
        "/studio npc: меню; /studio npc new|list|publish|cancel|refresh.");
    return 1;
}

stock NpcEditor_OnDialog(playerid, dialogid, response, listitem, const inputtext[])
{
    if (dialogid < NPC_ED_MAIN || dialogid > NPC_ED_DELETE) return 0;
    if (!NpcCore_Require(playerid)) return 1;
    if (!response)
    {
        switch(dialogid)
        {
            case NPC_ED_LIST: return NpcEditor_Open(playerid);
            case NPC_ED_ACTION: return NpcEditor_List(playerid);
            case NPC_ED_MAIN: return 1;
            case NPC_ED_ROUTE: return NpcEditor_ModePrompt(playerid);
            case NPC_ED_CONFIRM: return NpcEditor_Open(playerid);
            case NPC_ED_DELETE: return NpcEditor_Action(playerid);
        }
        if (gNpcEdOwner == playerid) NpcEditor_Cancel();
        return NpcEditor_Open(playerid);
    }
    if (dialogid == NPC_ED_MAIN)
    {
        switch(listitem)
        {
            case 0: return NpcEditor_Create(playerid);
            case 1: return NpcEditor_List(playerid);
            case 2:
            {
                if (gNpcEdOwner == playerid) return NpcEditor_Confirm(playerid);
                return NpcEditor_Open(playerid);
            }
            case 3:
            {
                if (gNpcEdOwner == playerid) NpcEditor_Cancel();
                return NpcEditor_Open(playerid);
            }
            case 4:
            {
                if (!NpcCore_Reload())
                    SendClientMessage(playerid,ADMIN_ERROR,"NPC reload failed.");
                return NpcEditor_Open(playerid);
            }
        }
        return 1;
    }
    if (dialogid == NPC_ED_SKIN)
    {
        if (!NpcEditor_Editable(playerid)) return 1;
        new skin;
        if (sscanf(inputtext,"i",skin) || skin < 0 || skin > 311)
        {
            SendClientMessage(playerid,ADMIN_ERROR,"Скин: числовой ID от 0 до 311.");
            return NpcEditor_SkinPrompt(playerid);
        }
        new q[120], Cache:c, rows;
        format(q,sizeof(q),"SELECT gender FROM npc_skins WHERE skin_id=%d AND is_active=1",skin);
        if (!NpcCore_Query(q,c))
            return SendClientMessage(playerid,ADMIN_ERROR,"Ошибка справочника скинов.");
        new bool:found = cache_get_row_count(rows) && rows == 1 &&
            cache_get_value_name(0,"gender",gNpcEdGender,sizeof(gNpcEdGender));
        cache_delete(c);
        if (!found)
        {
            SendClientMessage(playerid,ADMIN_ERROR,
                "Скин не найден в npc_skins. Добавьте проверенный ID/пол в phpMyAdmin.");
            return NpcEditor_SkinPrompt(playerid);
        }
        gNpcEdSkin = skin;
        return NpcEditor_NamePrompt(playerid);
    }
    if (dialogid == NPC_ED_NAME)
    {
        if (!NpcEditor_Editable(playerid)) return 1;
        new name[25];
        if (!inputtext[0])
        {
            if (!NpcEditor_AutoName(name,sizeof(name)))
            {
                SendClientMessage(playerid,ADMIN_ERROR,
                    "Нет свободных имён нужного пола в npc_names. Пополните список.");
                return NpcEditor_NamePrompt(playerid);
            }
        }
        else
        {
            if (!NpcEditor_NameOK(inputtext))
            {
                SendClientMessage(playerid,ADMIN_ERROR,
                    "Имя: 2–23 символа, латиница, цифры, _. Первой должна быть буква.");
                return NpcEditor_NamePrompt(playerid);
            }
            format(name,sizeof(name),"%s",inputtext);
        }
        format(gNpcEdName,sizeof(gNpcEdName),"%s",name);
        return NpcEditor_ModePrompt(playerid);
    }
    if (dialogid == NPC_ED_MODE)
    {
        if (!NpcEditor_Editable(playerid)) return 1;
        if (listitem < 0 || listitem > 1) return 1;
        gNpcEdMode = listitem;
        if (gNpcEdMode == NPC_CORE_IDLE)
        {
            gNpcEdRoute = -1;
            gNpcEdCaptured = false;
            SendClientMessage(playerid,NPC_CORE_COLOR,
                "Встаньте на нужное место и введите /nset (сохранить позицию).");
            return 1;
        }
        return NpcEditor_Routes(playerid);
    }
    if (dialogid == NPC_ED_ROUTE)
    {
        if (!NpcEditor_Editable(playerid)) return 1;
        if (listitem >= 0 && listitem < NPCROUTE_COUNT)
        {
            if (!NPCRoute_Has(listitem))
            {
                SendClientMessage(playerid,ADMIN_ERROR,
                    "Маршрут пуст. Выберите «Проложить новый» или другой слот.");
                return NpcEditor_Routes(playerid);
            }
            gNpcEdRoute = listitem;
            return NpcEditor_Confirm(playerid);
        }
        if (listitem == NPCROUTE_COUNT)
        {
            new slot = -1;
            for (new i = 0; i < NPCROUTE_COUNT; i++)
                if (!NPCRoute_Has(i) && !gNPCRouteInvalid[i]) { slot = i; break; }
            if (slot < 0)
            {
                SendClientMessage(playerid,ADMIN_ERROR,
                    "Нет свободных слотов. Старые маршруты защищены от перезаписи.");
                return NpcEditor_Routes(playerid);
            }
            gNpcEdRoute = slot;
            new cmd[32];
            format(cmd,sizeof(cmd),"record %d",slot);
            NPCRoute_Command(playerid,cmd);
            if (gNPCRouteOwner == playerid && gNPCRouteSlot == slot &&
                gNPCRouteRecording)
            {
                NPCRoute_Help(playerid);
                SendClientMessage(playerid,NPC_CORE_COLOR,
                    "После записи и сохранения маршрута: /studio npc publish.");
            }
        }
        return 1;
    }
    if (dialogid == NPC_ED_CONFIRM)
        return NpcEditor_Save(playerid);
    if (dialogid == NPC_ED_LIST)
    {
        if (listitem >= 0 && listitem < gNpcEdCount[playerid])
        {
            gNpcEdTarget[playerid] = gNpcEdRow[playerid][listitem];
            return NpcEditor_Action(playerid);
        }
        new at = gNpcEdCount[playerid];
        if (gNpcEdPage[playerid] > 0)
        {
            if (listitem == at)
            {
                gNpcEdPage[playerid]--;
                return NpcEditor_List(playerid);
            }
            at++;
        }
        if (listitem == at)
        {
            gNpcEdPage[playerid]++;
            return NpcEditor_List(playerid);
        }
        return NpcEditor_Open(playerid);
    }
    if (dialogid == NPC_ED_ACTION)
    {
        new id = gNpcEdTarget[playerid];
        if (id <= 0) return 1;
        switch(listitem)
        {
            case 0: return NpcEditor_Edit(playerid,id);
            case 1: return NpcEditor_Toggle(playerid,id,true);
            case 2: return NpcEditor_Toggle(playerid,id,false);
            case 3:
            {
                new message[144];
                format(message,sizeof(message),"Удалить NPC #%d из MariaDB?\nДействие необратимо.",id);
                ShowPlayerDialog(playerid,NPC_ED_DELETE,DIALOG_STYLE_MSGBOX,
                    "STUDIO | Удаление NPC",message,"Удалить","Назад");
            }
        }
        return 1;
    }
    if (dialogid == NPC_ED_DELETE)
        return NpcEditor_Delete(playerid,gNpcEdTarget[playerid]);
    return 1;
}

stock NpcEditor_OnDisconnect(playerid)
{
    if (gNpcEdOwner == playerid) NpcEditor_Cancel();
    gNpcEdPage[playerid] = 0;
    gNpcEdCount[playerid] = 0;
    gNpcEdTarget[playerid] = 0;
    return 1;
}
#else
stock NpcEditor_Open(playerid) { return NpcCore_Require(playerid); }
stock NpcEditor_Command(playerid,const params[])
{
    if (params[0]) return NpcCore_Require(playerid);
    return NpcCore_Require(playerid);
}
stock NpcEditor_Capture(playerid) { return NpcCore_Require(playerid); }
stock NpcEditor_OnDialog(playerid,dialogid,response,listitem,const inputtext[])
{
    if (playerid < 0 || dialogid < 0 || response < 0 || listitem < -1 || inputtext[0]) return 0;
    return 0;
}
stock NpcEditor_OnDisconnect(playerid) { if (playerid < 0) return 0; return 1; }
#endif
