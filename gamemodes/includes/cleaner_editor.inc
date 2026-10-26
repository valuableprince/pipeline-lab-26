#if defined _tarkenso_cleaner_editor_included
    #endinput
#endif
#define _tarkenso_cleaner_editor_included

// Founder-only Studio object editor. Drafts are private until SQL COMMIT.
#define CLEANER_EDITOR_LIST (6340)
#define CLEANER_EDITOR_ACTION (6341)
#define CLEANER_EDITOR_BAGS (6342)
#define CLEANER_EDITOR_PICK_BAG (6343)
#define CLEANER_EDITOR_CONFIRM_DELETE (6344)
#define CLEANER_EDITOR_NO_OWNER (65535)
#define CLEANER_EDITOR_FILL_INPUT (6345)

new gCleanerEditorSlot = -1;
new bool:gCleanerEditorNew;
new gCleanerEditorStage; // 0 idle, 1 bin edit, 2 bag edit, 3 waiting
new gCleanerEditorBagSlot;
new bool:gCleanerEditorBagWasEnabled;
new gCleanerEditorObject = INVALID_OBJECT_ID;
new gCleanerEditorBagObject[CLEANER_DUMP_BAG_OBJECTS];
new bool:gCleanerEditorBagEnabled[CLEANER_DUMP_BAG_OBJECTS];
new Float:gCleanerEditorPos[4];
new Float:gCleanerEditorBagPos[CLEANER_DUMP_BAG_OBJECTS][4];
new gCleanerEditorSelection[MAX_PLAYERS];

stock bool:CleanerEditor_Allowed(playerid)
{
    return Studio_Allowed(playerid) && !Race_IsParticipant(playerid) &&
        !AdminPunish_IsJailed(playerid) && gStoryStage[playerid] >= STORY_DONE &&
        GetPlayerVirtualWorld(playerid) == 0 &&
        GetPlayerInterior(playerid) == 0 &&
        GetPlayerState(playerid) == PLAYER_STATE_ONFOOT;
}

stock CleanerEditor_Cancel()
{
    new owner = gCleanerEditorOwner;
    if (owner != CLEANER_EDITOR_NO_OWNER && IsPlayerConnected(owner))
    {
        if (gCleanerEditorObject != INVALID_OBJECT_ID)
            DestroyPlayerObject(owner, gCleanerEditorObject);
        for (new i = 0; i < CLEANER_DUMP_BAG_OBJECTS; i++)
            if (gCleanerEditorBagObject[i] != INVALID_OBJECT_ID)
                DestroyPlayerObject(owner, gCleanerEditorBagObject[i]);
    }
    gCleanerEditorObject = INVALID_OBJECT_ID;
    for (new i = 0; i < CLEANER_DUMP_BAG_OBJECTS; i++)
        gCleanerEditorBagObject[i] = INVALID_OBJECT_ID;
    gCleanerEditorOwner = CLEANER_EDITOR_NO_OWNER;
    gCleanerEditorSlot = -1;
    gCleanerEditorStage = 0;
    return 1;
}

stock CleanerEditor_List(playerid)
{
    if (!CleanerEditor_Allowed(playerid))
        return SendClientMessage(playerid, ADMIN_ERROR, "Редактор доступен Founder пешком вне гонок и сюжета.");
    if (!CleanerDump_IsReady())
        return SendClientMessage(playerid, ADMIN_ERROR, "Хранилище контейнеров недоступно.");
    if (gCleanerEditorOwner != CLEANER_EDITOR_NO_OWNER &&
        gCleanerEditorOwner != playerid)
        return SendClientMessage(playerid, ADMIN_ERROR, "Другой редактор сейчас занят.");
    new body[1800] = "Создать контейнер\n", line[75];
    for (new i = 0; i < gCleanerDumpCount; i++)
    {
        format(line, sizeof(line), "ID %d | %d/60 + %d/40\n",
            gCleanerDumpId[i], gCleanerDumpBin[i], gCleanerDumpOverflow[i]);
        strcat(body, line, sizeof(body));
    }
    return ShowPlayerDialog(playerid, CLEANER_EDITOR_LIST, DIALOG_STYLE_LIST,
        "Tarkenso RPG Studio | Контейнеры", body, "Выбрать", "Закрыть");
}

stock CleanerEditor_PreviewBags()
{
    new owner = gCleanerEditorOwner;
    for (new i = 0; i < CLEANER_DUMP_BAG_OBJECTS; i++)
    {
        if (gCleanerEditorBagObject[i] != INVALID_OBJECT_ID)
            DestroyPlayerObject(owner, gCleanerEditorBagObject[i]);
        gCleanerEditorBagObject[i] = INVALID_OBJECT_ID;
        if (!gCleanerEditorBagEnabled[i]) continue;
        gCleanerEditorBagObject[i] = CreatePlayerObject(owner, CLEANER_DUMP_BAG_MODEL,
            gCleanerEditorBagPos[i][0], gCleanerEditorBagPos[i][1],
            gCleanerEditorBagPos[i][2], 0.0, 0.0, gCleanerEditorBagPos[i][3]);
    }
    return 1;
}

stock CleanerEditor_BagMenu(playerid)
{
    if (playerid != gCleanerEditorOwner) return 0;
    gCleanerEditorStage = 3;
    return ShowPlayerDialog(playerid, CLEANER_EDITOR_BAGS, DIALOG_STYLE_LIST,
        "Контейнер | Мешки переполнения",
        "Настроить место мешка\nСохранить контейнер и мешки\nОтменить без сохранения",
        "Выбрать", "Назад");
}

stock CleanerEditor_BagList(playerid)
{
    new body[360], line[38];
    for (new i = 0; i < CLEANER_DUMP_BAG_OBJECTS; i++)
    {
        format(line, sizeof(line), "Мешок %d | %s\n", i + 1,
            gCleanerEditorBagEnabled[i] ? "задан" : "не задан");
        strcat(body, line, sizeof(body));
    }
    return ShowPlayerDialog(playerid, CLEANER_EDITOR_PICK_BAG, DIALOG_STYLE_LIST,
        "Места мешков (максимум 10)", body, "Разместить", "Назад");
}

stock CleanerEditor_Begin(playerid, slot)
{
    if (!CleanerEditor_Allowed(playerid)) return 0;
    if (gCleanerEditorOwner != CLEANER_EDITOR_NO_OWNER)
        return SendClientMessage(playerid, ADMIN_ERROR, "Закончите текущий черновик.");
    if (slot < -1 || slot >= gCleanerDumpCount ||
        (slot == -1 && gCleanerDumpCount >= CLEANER_DUMP_LIMIT))
        return SendClientMessage(playerid, ADMIN_ERROR, "Достигнут лимит контейнеров.");
    gCleanerEditorOwner = playerid;
    gCleanerEditorSlot = slot;
    gCleanerEditorNew = slot == -1;
    gCleanerEditorBagSlot = -1;
    new Float:x, Float:y, Float:z;
    GetPlayerPos(playerid, x, y, z);
    gCleanerEditorPos[0] = slot < 0 ? x + 2.0 : gCleanerDumpPos[slot][0];
    gCleanerEditorPos[1] = slot < 0 ? y : gCleanerDumpPos[slot][1];
    gCleanerEditorPos[2] = slot < 0 ? z : gCleanerDumpPos[slot][2];
    gCleanerEditorPos[3] = slot < 0 ? 90.0 : gCleanerDumpPos[slot][3];
    for (new i = 0; i < CLEANER_DUMP_BAG_OBJECTS; i++)
    {
        gCleanerEditorBagObject[i] = INVALID_OBJECT_ID;
        gCleanerEditorBagEnabled[i] = slot >= 0 && gCleanerDumpBagDefined[slot][i];
        for (new n = 0; n < 4; n++)
            gCleanerEditorBagPos[i][n] = slot >= 0 ? gCleanerDumpBagPos[slot][i][n] : 0.0;
    }
    gCleanerEditorObject = CreatePlayerObject(playerid, CLEANER_DUMP_MODEL,
        gCleanerEditorPos[0], gCleanerEditorPos[1], gCleanerEditorPos[2]-0.85,
        0.0, 0.0, gCleanerEditorPos[3]);
    if (gCleanerEditorObject == INVALID_OBJECT_ID)
    {
        CleanerEditor_Cancel();
        return SendClientMessage(playerid, ADMIN_ERROR, "Не удалось создать черновик.");
    }
    CleanerEditor_PreviewBags();
    gCleanerEditorStage = 1;
    EditPlayerObject(playerid, gCleanerEditorObject);
    return SendClientMessage(playerid, STUDIO_ZONE_COLOR,
        "Перемещайте контейнер стрелками, подтвердите галочкой; ESC отменяет.");
}

stock CleanerEditor_Write()
{
    if (gCleanerEditorOwner == CLEANER_EDITOR_NO_OWNER) return 0;
    new owner = gCleanerEditorOwner, bagCount, q[390], changed;
    for (new i = 0; i < CLEANER_DUMP_BAG_OBJECTS; i++)
        if (gCleanerEditorBagEnabled[i]) bagCount++;
    if (!bagCount)
        return SendClientMessage(owner, ADMIN_ERROR, "Сначала разместите хотя бы один мешок.");

    new detail[90];
    format(detail, sizeof(detail), "slot=%d bags=%d new=%d",
        gCleanerEditorSlot, bagCount, _:gCleanerEditorNew);
    if (!Admin_LogPlayer(owner, "DUMP_EDITOR_SAVE", detail))
        return SendClientMessage(owner, ADMIN_ERROR, "Аудит недоступен.");
    if (!DBWorld_Exec("START TRANSACTION"))
        return SendClientMessage(owner, ADMIN_ERROR, "Ошибка начала транзакции.");
    new id = 0;
    if (gCleanerEditorNew)
    {
        mysql_format(gDBPrimary, q, sizeof(q),
            "INSERT INTO cleaner_containers(x,y,z,rz,world_id,interior_id,enabled) VALUES(%.4f,%.4f,%.4f,%.3f,0,0,1)",
            gCleanerEditorPos[0],gCleanerEditorPos[1],gCleanerEditorPos[2],gCleanerEditorPos[3]);
        if (!DBPrimary_Execute(q, changed) || changed != 1)
        { DBWorld_Rollback(); return SendClientMessage(owner, ADMIN_ERROR, "Не удалось создать контейнер."); }
        new Cache:c = mysql_query(gDBPrimary, "SELECT LAST_INSERT_ID() AS id");
        new rows;
        if (c == Cache:0 || mysql_errno(gDBPrimary) || !cache_get_row_count(rows) ||
            rows != 1 || !cache_get_value_name_int(0,"id",id) || id < 1)
        {
            if (c != Cache:0) cache_delete(c);
            DBWorld_Rollback();
            return SendClientMessage(owner, ADMIN_ERROR, "Не удалось получить ID контейнера.");
        }
        cache_delete(c);
    }
    else
    {
        id = gCleanerDumpId[gCleanerEditorSlot];
        mysql_format(gDBPrimary, q, sizeof(q),
            "UPDATE cleaner_containers SET x=%.4f,y=%.4f,z=%.4f,rz=%.3f WHERE container_id=%d",
            gCleanerEditorPos[0],gCleanerEditorPos[1],gCleanerEditorPos[2],
            gCleanerEditorPos[3], id);
        if (!DBWorld_Exec(q))
        { DBWorld_Rollback(); return SendClientMessage(owner, ADMIN_ERROR, "Не удалось сохранить позицию."); }
        mysql_format(gDBPrimary, q, sizeof(q),
            "DELETE FROM cleaner_container_bag_positions WHERE container_id=%d", id);
        if (!DBWorld_Exec(q))
        { DBWorld_Rollback(); return SendClientMessage(owner, ADMIN_ERROR, "Ошибка сохранения мешков."); }
    }
    for (new i = 0; i < CLEANER_DUMP_BAG_OBJECTS; i++)
    {
        if (!gCleanerEditorBagEnabled[i]) continue;
        mysql_format(gDBPrimary, q, sizeof(q),
            "INSERT INTO cleaner_container_bag_positions(container_id,slot_id,x,y,z,rz) VALUES(%d,%d,%.4f,%.4f,%.4f,%.3f)",
            id,i,gCleanerEditorBagPos[i][0],gCleanerEditorBagPos[i][1],
            gCleanerEditorBagPos[i][2],gCleanerEditorBagPos[i][3]);
        if (!DBWorld_Exec(q))
        { DBWorld_Rollback(); return SendClientMessage(owner, ADMIN_ERROR, "Не удалось сохранить мешки."); }
    }
    if (!DBWorld_Exec("COMMIT"))
    { DBWorld_Rollback(); return SendClientMessage(owner, ADMIN_ERROR, "Ошибка COMMIT."); }
    CleanerEditor_Cancel();
    for (new p = 0; p < MAX_PLAYERS; p++)
        if (IsPlayerConnected(p)) CleanerDump_NavClear(p);
    CleanerDump_ClearObjects();
    gCleanerDumpReady = CleanerDump_Load();
    if (gCleanerDumpReady) CleanerDump_BuildObjects();
    else print("[Cleaner] ERROR: reload after editor; jobs disabled until restart.");
    return SendClientMessage(owner, STUDIO_ZONE_COLOR, "Контейнер и точки мешков сохранены в MariaDB.");
}

stock CleanerEditor_Delete(playerid, slot)
{
    if (!CleanerEditor_Allowed(playerid) ||
        slot < 0 || slot >= gCleanerDumpCount) return 0;
    if (gCleanerDumpBin[slot] || gCleanerDumpOverflow[slot] || gCleanerDumpCount <= 1)
        return SendClientMessage(playerid, ADMIN_ERROR,
            "Нельзя удалять заполненный или единственный контейнер.");
    if (gCleanerDumpStaged || gCleanerEditorOwner != CLEANER_EDITOR_NO_OWNER)
        return SendClientMessage(playerid, ADMIN_ERROR, "Контейнер занят.");
    new detail[72], q[140], changed;
    format(detail, sizeof(detail), "container=%d", gCleanerDumpId[slot]);
    if (!Admin_LogPlayer(playerid, "DUMP_EDITOR_DELETE", detail))
        return SendClientMessage(playerid, ADMIN_ERROR, "Аудит недоступен.");
    mysql_format(gDBPrimary, q, sizeof(q),
        "DELETE FROM cleaner_containers WHERE container_id=%d AND bin_units=0 AND overflow_units=0",
        gCleanerDumpId[slot]);
    if (!DBPrimary_Execute(q, changed) || changed != 1)
        return SendClientMessage(playerid, ADMIN_ERROR, "Удаление не подтверждено MariaDB.");
    for (new p = 0; p < MAX_PLAYERS; p++)
        if (IsPlayerConnected(p)) CleanerDump_NavClear(p);
    CleanerDump_ClearObjects();
    gCleanerDumpReady = CleanerDump_Load();
    if (gCleanerDumpReady) CleanerDump_BuildObjects();
    return SendClientMessage(playerid, STUDIO_ZONE_COLOR, "Пустой контейнер удалён.");
}

stock CleanerEditor_OnDialog(playerid, dialogid, response, listitem, const inputtext[])
{
    if (dialogid < CLEANER_EDITOR_LIST || dialogid > CLEANER_EDITOR_FILL_INPUT) return 0;
    if (!CleanerEditor_Allowed(playerid))
    {
        if (gCleanerEditorOwner == playerid) CleanerEditor_Cancel();
        return 1;
    }
    if (dialogid == CLEANER_EDITOR_LIST)
    {
        if (!response) return 1;
        if (listitem == 0) return CleanerEditor_Begin(playerid, -1);
        gCleanerEditorSelection[playerid] = listitem - 1;
        if (gCleanerEditorSelection[playerid] < 0 ||
            gCleanerEditorSelection[playerid] >= gCleanerDumpCount) return 1;
        return ShowPlayerDialog(playerid, CLEANER_EDITOR_ACTION, DIALOG_STYLE_LIST,
            "Действие с контейнером", "Переместить и настроить мешки\nУдалить (если пуст)\nОпустошить контейнер\nУстановить заполнение (0-100)",
            "Выбрать", "Назад");
    }
    if (dialogid == CLEANER_EDITOR_ACTION)
    {
        if (!response) return CleanerEditor_List(playerid);
        if (listitem == 0) return CleanerEditor_Begin(playerid, gCleanerEditorSelection[playerid]);
        if (listitem == 2)
        {
            new slot = gCleanerEditorSelection[playerid];
            if (!CleanerDump_SetFill(playerid, slot, 0))
                SendClientMessage(playerid, ADMIN_ERROR, "Не удалось опустошить контейнер.");
            return CleanerEditor_List(playerid);
        }
        if (listitem == 3)
            return ShowPlayerDialog(playerid, CLEANER_EDITOR_FILL_INPUT, DIALOG_STYLE_INPUT,
                "Tarkenso RPG Studio | Заполнение контейнера",
                "Введите от 0 до 100 единиц.\n0-60 внутри; 61-100 снаружи.",
                "Сохранить", "Отмена");
        if (listitem == 1)
            return ShowPlayerDialog(playerid, CLEANER_EDITOR_CONFIRM_DELETE,
                DIALOG_STYLE_MSGBOX, "Удаление контейнера",
                "Удалить пустой контейнер без возможности отмены?", "Удалить", "Отмена");
        return 1;
    }
    if (dialogid == CLEANER_EDITOR_FILL_INPUT)
    {
        if (!response) return CleanerEditor_List(playerid);
        if (!inputtext[0] || strlen(inputtext) > 3)
            return SendClientMessage(playerid, ADMIN_ERROR, "Допустимы только целые числа от 0 до 100.");
        for (new i = 0; inputtext[i]; i++)
            if (inputtext[i] < '0' || inputtext[i] > '9')
                return SendClientMessage(playerid, ADMIN_ERROR, "Недопустимое значение заполнения.");
        new total = strval(inputtext);
        if (total > CLEANER_DUMP_CAP + CLEANER_DUMP_OVERFLOW_CAP)
            return SendClientMessage(playerid, ADMIN_ERROR, "Заполнение не может превышать 100.");
        if (!CleanerDump_SetFill(playerid, gCleanerEditorSelection[playerid], total))
            return SendClientMessage(playerid, ADMIN_ERROR, "Сохранение отменено: контейнер изменился или БД недоступна.");
        return CleanerEditor_List(playerid);
    }
    if (dialogid == CLEANER_EDITOR_CONFIRM_DELETE)
    {
        if (response) CleanerEditor_Delete(playerid, gCleanerEditorSelection[playerid]);
        return 1;
    }
    if (gCleanerEditorOwner != playerid) return 1;
    if (dialogid == CLEANER_EDITOR_BAGS)
    {
        if (!response || listitem == 2) { CleanerEditor_Cancel(); return 1; }
        if (listitem == 0) return CleanerEditor_BagList(playerid);
        if (listitem == 1) return CleanerEditor_Write();
        return 1;
    }
    if (dialogid == CLEANER_EDITOR_PICK_BAG)
    {
        if (!response) return CleanerEditor_BagMenu(playerid);
        if (listitem < 0 || listitem >= CLEANER_DUMP_BAG_OBJECTS) return 1;
        gCleanerEditorBagSlot = listitem;
        gCleanerEditorBagWasEnabled = gCleanerEditorBagEnabled[listitem];
        if (!gCleanerEditorBagEnabled[listitem])
        {
            gCleanerEditorBagPos[listitem][0] = gCleanerEditorPos[0] + 2.5 + float(listitem / 2);
            gCleanerEditorBagPos[listitem][1] = gCleanerEditorPos[1] + float(listitem % 2);
            gCleanerEditorBagPos[listitem][2] = gCleanerEditorPos[2] - 0.87;
            gCleanerEditorBagPos[listitem][3] = 0.0;
            gCleanerEditorBagEnabled[listitem] = true;
        }
        CleanerEditor_PreviewBags();
        gCleanerEditorStage = 2;
        EditPlayerObject(playerid, gCleanerEditorBagObject[listitem]);
        return 1;
    }
    return 1;
}

stock CleanerEditor_OnEdit(playerid, playerobject, objectid, EDIT_RESPONSE:response,
    Float:x, Float:y, Float:z, Float:rx, Float:ry, Float:rz)
{
    #pragma unused rx, ry
    if (!playerobject || gCleanerEditorOwner != playerid) return 0;
    if (!CleanerEditor_Allowed(playerid))
    { CleanerEditor_Cancel(); return 1; }
    if (gCleanerEditorStage == 1 && objectid == gCleanerEditorObject)
    {
        if (response == EDIT_RESPONSE_CANCEL) { CleanerEditor_Cancel(); return 1; }
        if (response == EDIT_RESPONSE_FINAL)
        {
            gCleanerEditorPos[0] = x;
            gCleanerEditorPos[1] = y;
            gCleanerEditorPos[2] = z + 0.85;
            gCleanerEditorPos[3] = rz;
            CleanerEditor_BagMenu(playerid);
        }
        return 1;
    }
    new i = gCleanerEditorBagSlot;
    if (gCleanerEditorStage == 2 && i >= 0 &&
        i < CLEANER_DUMP_BAG_OBJECTS && objectid == gCleanerEditorBagObject[i])
    {
        if (response == EDIT_RESPONSE_CANCEL)
        {
            gCleanerEditorBagEnabled[i] = gCleanerEditorBagWasEnabled;
            CleanerEditor_PreviewBags();
            CleanerEditor_BagMenu(playerid);
        }
        else if (response == EDIT_RESPONSE_FINAL)
        {
            gCleanerEditorBagPos[i][0] = x;
            gCleanerEditorBagPos[i][1] = y;
            gCleanerEditorBagPos[i][2] = z;
            gCleanerEditorBagPos[i][3] = rz;
            CleanerEditor_PreviewBags();
            CleanerEditor_BagMenu(playerid);
        }
        return 1;
    }
    return 0;
}
