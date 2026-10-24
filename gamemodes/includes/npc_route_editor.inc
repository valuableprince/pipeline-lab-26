#if defined _tarkenso_npc_route_editor_included
    #endinput
#endif
#define _tarkenso_npc_route_editor_included

// NPC-ROUTE-EDITOR-01. Администратор обходит тротуары пешком, сервер пишет XYZ.
// Маршруты NPC 0..15 сохраняются в scriptfiles/npc_routes/slot_N.route.
#define NPCROUTE_DIR            "npc_routes"
#define NPCROUTE_COUNT          (16)
#define NPCROUTE_MAX_POINTS     (128)
#define NPCROUTE_SAMPLE_MS      (350)
#define NPCROUTE_SAMPLE_SQ      (4.0)
#define NPCROUTE_MAX_JUMP_SQ    (100.0)
#define NPCROUTE_MAX_EDGE_SQ    (100.0)
#define NPCROUTE_MAX_HEIGHT     (3.0)
#define NPCROUTE_COLOR          (0x84DAB6FF)
#define NPCROUTE_CROSS_COLOR    (0xFFD17AFF)

new gNPCRouteCount[NPCROUTE_COUNT];
new bool:gNPCRouteEnabled[NPCROUTE_COUNT];
new bool:gNPCRouteInvalid[NPCROUTE_COUNT];
new Float:gNPCRouteX[NPCROUTE_COUNT][NPCROUTE_MAX_POINTS];
new Float:gNPCRouteY[NPCROUTE_COUNT][NPCROUTE_MAX_POINTS];
new Float:gNPCRouteZ[NPCROUTE_COUNT][NPCROUTE_MAX_POINTS];
new gNPCRouteCross[NPCROUTE_COUNT][NPCROUTE_MAX_POINTS];

// Один редактор на сервере, чтобы два администратора не перезаписали путь.
new gNPCRouteOwner = INVALID_PLAYER_ID;
new gNPCRouteSlot = -1;
new gNPCRouteDraftCount;
new bool:gNPCRouteRecording;
new bool:gNPCRouteCrossMode;
new Float:gNPCRouteDraftX[NPCROUTE_MAX_POINTS];
new Float:gNPCRouteDraftY[NPCROUTE_MAX_POINTS];
new Float:gNPCRouteDraftZ[NPCROUTE_MAX_POINTS];
new gNPCRouteDraftCross[NPCROUTE_MAX_POINTS];
new gNPCRouteMarkers[NPCROUTE_MAX_POINTS];
new gNPCRouteTimer;

// Предпросмотр использует последовательные точки и не зависит от светофоров.
new gNPCRoutePreview = INVALID_NPC_ID;
new gNPCRoutePreviewPath = INVALID_PATH_ID;

forward NPCRoute_Tick();

stock NPCRoute_Path(slot, path[], length)
{
    format(path, length, NPCROUTE_DIR "/slot_%d.route", slot);
    return 1;
}

stock bool:NPCRoute_ValidSlot(slot)
{
    return slot >= 0 && slot < NPCROUTE_COUNT;
}

stock bool:NPCRoute_Has(slot)
{
    return NPCRoute_ValidSlot(slot) && gNPCRouteEnabled[slot] &&
        gNPCRouteCount[slot] >= 2;
}

stock bool:NPCRoute_GetPoint(slot, index, &Float:x, &Float:y, &Float:z)
{
    if (!NPCRoute_Has(slot) || index < 0 || index >= gNPCRouteCount[slot]) return false;
    x = gNPCRouteX[slot][index];
    y = gNPCRouteY[slot][index];
    z = gNPCRouteZ[slot][index];
    return true;
}

// Флаг принадлежит ребру index -> index + 1. Работает и при развороте.
stock bool:NPCRoute_IsCrossEdge(slot, first, second)
{
    if (!NPCRoute_Has(slot) || first < 0 || second < 0 ||
        first >= gNPCRouteCount[slot] || second >= gNPCRouteCount[slot])
        return false;
    new index = first < second ? first : second;
    return gNPCRouteCross[slot][index] != 0;
}

stock bool:NPCRoute_BuildPath(slot, pathid)
{
    if (!NPCRoute_Has(slot) || !NPC_IsValidPath(pathid)) return false;
    for (new i = 0; i < gNPCRouteCount[slot]; i++)
        if (!NPC_AddPointToPath(pathid, gNPCRouteX[slot][i],
            gNPCRouteY[slot][i], gNPCRouteZ[slot][i], 0.8))
            return false;
    return true;
}

stock bool:NPCRoute_PointOK(Float:x, Float:y, Float:z)
{
    return x >= -3000.0 && x <= 3000.0 && y >= -3000.0 &&
        y <= 3000.0 && z >= -150.0 && z <= 2000.0;
}

stock bool:NPCRoute_Digits(const text[])
{
    new length = strlen(text);
    if (!length || length > 3) return false;
    for (new i = 0; i < length; i++)
        if (text[i] < '0' || text[i] > '9') return false;
    return true;
}

// Если файл повреждён, не используем часть маршрута и запрещаем запись поверх него.
stock bool:NPCRoute_Load(slot)
{
    if (!NPCRoute_ValidSlot(slot)) return false;
    gNPCRouteCount[slot] = 0;
    gNPCRouteEnabled[slot] = false;
    gNPCRouteInvalid[slot] = false;
#if defined TARKENSO_DB_PRIMARY
    return DBWorld_RouteLoad(slot);
#else
    new path[80], backup[96];
    NPCRoute_Path(slot, path, sizeof(path));
    format(backup, sizeof(backup), "%s.bak", path);
    if (!fexist(path) && fexist(backup)) frename(backup, path);
    if (!fexist(path)) return true;
    new File:file = fopen(path, io_read);
    if (!file) { gNPCRouteInvalid[slot] = true; return false; }
    new line[128], countText[16], bool:ok = true;
    if (!fread(file, line, sizeof(line))) ok = false;
    if (ok)
    {
        Auth_StripLine(line);
        if (strcmp(line, "TARKENSO_NPC_ROUTE|1")) ok = false;
    }
    if (ok && !fread(file, countText, sizeof(countText))) ok = false;
    new count;
    if (ok)
    {
        Auth_StripLine(countText);
        if (!NPCRoute_Digits(countText)) ok = false;
        else count = strval(countText);
        if (count < 2 || count > NPCROUTE_MAX_POINTS) ok = false;
    }
    new Float:x, Float:y, Float:z, flag;
    new Float:oldX, Float:oldY, Float:oldZ;
    for (new i = 0; ok && i < count; i++)
    {
        if (!fread(file, line, sizeof(line)) ||
            sscanf(line, "fffi", x, y, z, flag) ||
            !NPCRoute_PointOK(x, y, z) || (flag != 0 && flag != 1))
        {
            ok = false;
            break;
        }
        if (i > 0)
        {
            new Float:dx = x - oldX, Float:dy = y - oldY, Float:dz = z - oldZ;
            if (dx * dx + dy * dy + dz * dz > NPCROUTE_MAX_EDGE_SQ ||
                floatabs(dz) > NPCROUTE_MAX_HEIGHT) { ok = false; break; }
        }
        gNPCRouteX[slot][i] = x;
        gNPCRouteY[slot][i] = y;
        gNPCRouteZ[slot][i] = z;
        gNPCRouteCross[slot][i] = flag;
        oldX = x; oldY = y; oldZ = z;
    }
    if (ok && gNPCRouteCross[slot][count - 1] != 0) ok = false;
    fclose(file);
    if (!ok)
    {
        gNPCRouteInvalid[slot] = true;
        printf("[NPCRoute] Invalid route file in slot %d; overwriting is blocked.", slot);
        return false;
    }
    gNPCRouteCount[slot] = count;
    gNPCRouteEnabled[slot] = true;
    return true;
#endif
}

stock NPCRoute_LoadAll()
{
    for (new slot = 0; slot < NPCROUTE_COUNT; slot++) NPCRoute_Load(slot);
    return 1;
}

// Atomic temp -> backup -> target: ошибка записи не уничтожает предыдущий маршрут.
stock bool:NPCRoute_Save()
{
    if (!NPCRoute_ValidSlot(gNPCRouteSlot) || gNPCRouteDraftCount < 2 ||
        gNPCRouteRecording || gNPCRouteCrossMode ||
        gNPCRouteInvalid[gNPCRouteSlot]) return false;
    for (new i = 1; i < gNPCRouteDraftCount; i++)
    {
        new Float:dx = gNPCRouteDraftX[i] - gNPCRouteDraftX[i-1];
        new Float:dy = gNPCRouteDraftY[i] - gNPCRouteDraftY[i-1];
        new Float:dz = gNPCRouteDraftZ[i] - gNPCRouteDraftZ[i-1];
        if (dx*dx + dy*dy + dz*dz > NPCROUTE_MAX_EDGE_SQ ||
            floatabs(dz) > NPCROUTE_MAX_HEIGHT) return false;
    }
    #if defined TARKENSO_DB_PRIMARY
    return DBWorld_RouteSave();
    #else
    new path[80], temp[96], backup[96], line[128];
    NPCRoute_Path(gNPCRouteSlot, path, sizeof(path));
    format(temp, sizeof(temp), "%s.tmp", path);
    format(backup, sizeof(backup), "%s.bak", path);
    if (fexist(temp)) fremove(temp);
    new File:file = fopen(temp, io_write);
    if (!file) return false;
    new bool:ok = fwrite(file, "TARKENSO_NPC_ROUTE|1\n") > 0;
    format(line, sizeof(line), "%d\n", gNPCRouteDraftCount);
    if (fwrite(file, line) <= 0) ok = false;
    for (new i = 0; i < gNPCRouteDraftCount; i++)
    {
        format(line, sizeof(line), "%.4f %.4f %.4f %d\n",
            gNPCRouteDraftX[i], gNPCRouteDraftY[i], gNPCRouteDraftZ[i],
            gNPCRouteDraftCross[i]);
        if (fwrite(file, line) <= 0) ok = false;
    }
    fclose(file);
    if (!ok) { fremove(temp); return false; }
    new bool:hadOld = fexist(path) != 0;
    if (hadOld)
    {
        if (fexist(backup)) fremove(backup);
        if (!frename(path, backup)) { fremove(temp); return false; }
    }
    if (!frename(temp, path))
    {
        if (hadOld) frename(backup, path);
        fremove(temp);
        return false;
    }
    if (hadOld && fexist(backup)) fremove(backup);
    return true;
    #endif
}

stock NPCRoute_ClearMarkers()
{
    for (new i = 0; i < NPCROUTE_MAX_POINTS; i++)
    {
        if (gNPCRouteMarkers[i] == INVALID_OBJECT_ID) continue;
        if (gNPCRouteOwner != INVALID_PLAYER_ID && IsPlayerConnected(gNPCRouteOwner))
            DestroyPlayerObject(gNPCRouteOwner, gNPCRouteMarkers[i]);
        gNPCRouteMarkers[i] = INVALID_OBJECT_ID;
    }
    return 1;
}

stock NPCRoute_ShowMarkers()
{
    NPCRoute_ClearMarkers();
    if (gNPCRouteOwner == INVALID_PLAYER_ID || !IsPlayerConnected(gNPCRouteOwner)) return 0;
    for (new i = 0; i < gNPCRouteDraftCount; i++)
        gNPCRouteMarkers[i] = CreatePlayerObject(gNPCRouteOwner, 19131,
            gNPCRouteDraftX[i], gNPCRouteDraftY[i],
            gNPCRouteDraftZ[i] + 0.5, 0.0, 0.0, 0.0);
    return 1;
}

stock NPCRoute_StopTest()
{
    if (NPC_IsValid(gNPCRoutePreview)) NPC_Destroy(gNPCRoutePreview);
    gNPCRoutePreview = INVALID_NPC_ID;
    if (NPC_IsValidPath(gNPCRoutePreviewPath))
        NPC_DestroyPath(gNPCRoutePreviewPath);
    gNPCRoutePreviewPath = INVALID_PATH_ID;
    return 1;
}

stock NPCRoute_ResetEditor()
{
    NPCRoute_StopTest();
    NPCRoute_ClearMarkers();
    gNPCRouteOwner = INVALID_PLAYER_ID;
    gNPCRouteSlot = -1;
    gNPCRouteDraftCount = 0;
    gNPCRouteRecording = false;
    gNPCRouteCrossMode = false;
    return 1;
}

stock bool:NPCRoute_AddPoint(playerid, bool:forced = false)
{
    if (gNPCRouteDraftCount >= NPCROUTE_MAX_POINTS) return false;
    if (GetPlayerState(playerid) != PLAYER_STATE_ONFOOT ||
        GetPlayerVirtualWorld(playerid) != 0 || GetPlayerInterior(playerid) != 0)
        return false;
    new Float:x, Float:y, Float:z;
    GetPlayerPos(playerid, x, y, z);
    if (!NPCRoute_PointOK(x, y, z)) return false;
    new i = gNPCRouteDraftCount;
    if (i > 0)
    {
        new Float:dx = x - gNPCRouteDraftX[i-1];
        new Float:dy = y - gNPCRouteDraftY[i-1];
        new Float:dz = z - gNPCRouteDraftZ[i-1];
        new Float:d = dx*dx + dy*dy + dz*dz;
        if (d > NPCROUTE_MAX_JUMP_SQ || floatabs(dz) > NPCROUTE_MAX_HEIGHT)
            return false;
        if (d < NPCROUTE_SAMPLE_SQ && !forced) return true;
        if (d < 0.04) return true;
        gNPCRouteDraftCross[i-1] = _:gNPCRouteCrossMode;
    }
    gNPCRouteDraftX[i] = x;
    gNPCRouteDraftY[i] = y;
    gNPCRouteDraftZ[i] = z;
    gNPCRouteDraftCross[i] = 0;
    gNPCRouteDraftCount++;
    return true;
}

public NPCRoute_Tick()
{
    if (!gNPCRouteRecording) return 1;
    new owner = gNPCRouteOwner;
    if (owner == INVALID_PLAYER_ID || !IsPlayerConnected(owner) ||
        !Admin_IsFounder(owner) || !gAdminDuty[owner] ||
        GetPlayerState(owner) != PLAYER_STATE_ONFOOT ||
        GetPlayerVirtualWorld(owner) != 0 || GetPlayerInterior(owner) != 0)
    {
        gNPCRouteRecording = false;
        if (owner != INVALID_PLAYER_ID && IsPlayerConnected(owner))
            SendClientMessage(owner, NPCROUTE_COLOR, "Запись остановлена: смена состояния игрока.");
        return 1;
    }
    if (!NPCRoute_AddPoint(owner))
    {
        gNPCRouteRecording = false;
        SendClientMessage(owner, NPCROUTE_COLOR, "Запись остановлена: телепорт, перепад высоты или лимит точек.");
    }
    if (gNPCRouteDraftCount >= NPCROUTE_MAX_POINTS)
    {
        gNPCRouteRecording = false;
        SendClientMessage(owner, NPCROUTE_COLOR,
            "Лимит 128 точек. Закрывайте переходы до достижения лимита.");
    }
    return 1;
}

// A single native path prevents a new NPC_Move call at every short
// waypoint; use the path completion callback rather than OnNPCFinishMove.
stock NPCRoute_OnNPCFinishMove(npcid)
{
    return npcid == gNPCRoutePreview && NPC_IsValid(gNPCRoutePreview);
}

stock NPCRoute_OnNPCFinishMovePath(npcid, pathid)
{
    if (npcid != gNPCRoutePreview || pathid != gNPCRoutePreviewPath ||
        !NPC_IsValid(gNPCRoutePreview)) return 0;
    if (gNPCRouteOwner != INVALID_PLAYER_ID && IsPlayerConnected(gNPCRouteOwner))
        SendClientMessage(gNPCRouteOwner, NPCROUTE_COLOR,
            "Тест завершён: NPC дошёл до конца маршрута.");
    NPCRoute_StopTest();
    return 1;
}

stock NPCRoute_Test(playerid)
{
    if (gNPCRouteDraftCount < 2)
        return SendClientMessage(playerid, NPCROUTE_COLOR, "Нужно минимум 2 точки.");
    NPCRoute_StopTest();
    gNPCRoutePreviewPath = NPC_CreatePath();
    if (!NPC_IsValidPath(gNPCRoutePreviewPath))
    {
        gNPCRoutePreviewPath = INVALID_PATH_ID;
        return SendClientMessage(playerid, NPCROUTE_COLOR,
            "Не удалось создать путь NPC. Проверьте компонент NPC.");
    }
    for (new i = 0; i < gNPCRouteDraftCount; i++)
    {
        if (!NPC_AddPointToPath(gNPCRoutePreviewPath, gNPCRouteDraftX[i],
            gNPCRouteDraftY[i], gNPCRouteDraftZ[i], 0.35))
        {
            NPCRoute_StopTest();
            return SendClientMessage(playerid, NPCROUTE_COLOR,
                "Не удалось передать точки в путь NPC.");
        }
    }
    gNPCRoutePreview = NPC_Create("Route_Test");
    if (!NPC_IsValid(gNPCRoutePreview))
    {
        NPCRoute_StopTest();
        return SendClientMessage(playerid, NPCROUTE_COLOR,
            "Нет слота NPC. В config.json выставьте max_bots не менее 5.");
    }
    if (!NPC_Spawn(gNPCRoutePreview))
    {
        NPCRoute_StopTest();
        return SendClientMessage(playerid, NPCROUTE_COLOR,
            "Не удалось создать тестового NPC в игровом мире.");
    }
    NPC_SetVirtualWorld(gNPCRoutePreview, 0);
    NPC_SetSkin(gNPCRoutePreview, 60);
    NPC_SetPos(gNPCRoutePreview, gNPCRouteDraftX[0],
        gNPCRouteDraftY[0], gNPCRouteDraftZ[0]);
    if (!NPC_MoveByPath(gNPCRoutePreview, gNPCRoutePreviewPath,
        NPC_MOVE_TYPE_WALK, NPC_MOVE_SPEED_AUTO, false))
    {
        NPCRoute_StopTest();
        return SendClientMessage(playerid, NPCROUTE_COLOR,
            "NPC не смог запустить путь. Проверьте журнал сервера.");
    }
    new message[112];
    format(message, sizeof(message),
        "Тест NPC запущен: %d точек. Ожидайте прохождения до конца.",
        gNPCRouteDraftCount);
    printf("[NPCRoute] Preview started: player=%d, slot=%d, waypoints=%d, path=%d.",
        playerid, gNPCRouteSlot, gNPCRouteDraftCount, gNPCRoutePreviewPath);
    return SendClientMessage(playerid, NPCROUTE_COLOR, message);
}

stock NPCRoute_Help(playerid)
{
    SendClientMessage(playerid, NPCROUTE_COLOR,
        "/npcroute record N — начать запись пути в слот N (0..15), идите пешком.");
    SendClientMessage(playerid, NPCROUTE_COLOR,
        "/npcroute crossing — отметить начало/конец перехода через дорогу.");
    SendClientMessage(playerid, NPCROUTE_COLOR,
        "/npcroute stop — остановить запись; /npcroute undo — убрать последнюю точку.");
    SendClientMessage(playerid, NPCROUTE_COLOR,
        "/npcroute show — показать метки; /npcroute test — проверить путь с NPC.");
    SendClientMessage(playerid, NPCROUTE_COLOR,
        "/npcroute save — сохранить путь; /npcroute apply — перечитать сохранённые пути.");
    SendClientMessage(playerid, NPCROUTE_COLOR,
        "/npcroute status — состояние слотов; /npcroute cancel — отменить черновик.");
    SendClientMessage(playerid, NPCROUTE_COLOR,
        "Порядок: record N -> пройти -> stop -> test -> save -> apply. Пешком, мир 0.");
    SendClientMessage(playerid, NPCROUTE_COLOR,
        "Справка: /npcroute help. Между точками около 2 м, не более 128 точек.");
    return 1;
}

stock NPCRoute_Command(playerid, const params[])
{
    if (!Admin_IsFounder(playerid) || !gAdminDuty[playerid])
        return SendClientMessage(playerid, ADMIN_ERROR,
            "Редактор маршрутов доступен основателю в режиме /aduty.");
    if (isnull(params)) return NPCRoute_Help(playerid);
    // sscanf("s[20]") absorbs the entire remaining string for a final
    // string argument (e.g. "record 0"). Extract just the action token.
    new action[20], pos = 0, length = 0;
    while (params[pos] == ' ') pos++;
    while (params[pos] != EOS && params[pos] != ' ' &&
        length < sizeof(action) - 1)
        action[length++] = params[pos++];
    action[length] = EOS;
    if (!length || !strcmp(action, "help", true))
        return NPCRoute_Help(playerid);
    if (!strcmp(action, "status", true))
    {
        new msg[128];
        for (new i = 0; i < NPCROUTE_COUNT; i++)
        {
            format(msg, sizeof(msg), "Маршрут %d: %s, точек %d, данные %s.",
                i, gNPCRouteEnabled[i] ? ("есть") : ("нет"),
                gNPCRouteCount[i], gNPCRouteInvalid[i] ? ("повреждён") : ("OK"));
            SendClientMessage(playerid, NPCROUTE_COLOR, msg);
        }
        return 1;
    }
    if (!strcmp(action, "record", true))
    {
        new slot;
        if (sscanf(params, "s[20]i", action, slot) || !NPCRoute_ValidSlot(slot))
            return SendClientMessage(playerid, NPCROUTE_COLOR, "Использование: /npcroute record 0..15.");
        if (gNPCRouteOwner != INVALID_PLAYER_ID && gNPCRouteOwner != playerid)
            return SendClientMessage(playerid, NPCROUTE_COLOR, "Редактор уже занят другим игроком.");
        if (gNPCRouteOwner == playerid) NPCRoute_ResetEditor();
        if (GetPlayerState(playerid) != PLAYER_STATE_ONFOOT ||
            GetPlayerVirtualWorld(playerid) != 0 || GetPlayerInterior(playerid) != 0)
            return SendClientMessage(playerid, NPCROUTE_COLOR, "Нужно стоять пешком в мире 0.");
        gNPCRouteOwner = playerid;
        gNPCRouteSlot = slot;
        gNPCRouteDraftCount = 0;
        gNPCRouteCrossMode = false;
        NPCRoute_AddPoint(playerid, true);
        gNPCRouteRecording = true;
        new msg[100];
        format(msg, sizeof(msg), "Запись маршрута %d запущена. Идите по тротуару. /npcroute stop.", slot);
        SendClientMessage(playerid, NPCROUTE_COLOR, msg);
        SendClientMessage(playerid, NPCROUTE_COLOR,
            "Переход: /npcroute crossing перед дорогой и ещё раз после неё.");
        SendClientMessage(playerid, NPCROUTE_COLOR,
            "Подробно о командах: /npcroute help.");
        return 1;
    }
    if (!strcmp(action, "apply", true))
    {
        if (gNPCRouteRecording) return SendClientMessage(playerid, NPCROUTE_COLOR,
            "Остановите запись перед применением маршрутов.");
        NPCRoute_StopTest();
        NPCRoute_LoadAll();
        return SendClientMessage(playerid, NPCROUTE_COLOR,
            "Маршруты перечитаны. Проверка движения: /npcroute test.");
    }
    if (gNPCRouteOwner != playerid || gNPCRouteSlot < 0)
        return SendClientMessage(playerid, NPCROUTE_COLOR, "Сначала /npcroute record 0..15.");
    if (!strcmp(action, "cancel", true))
    {
        NPCRoute_ResetEditor();
        return SendClientMessage(playerid, NPCROUTE_COLOR, "Редактор закрыт, файл не изменён.");
    }
    if (!strcmp(action, "stop", true))
    {
        if (gNPCRouteCrossMode)
            return SendClientMessage(playerid, NPCROUTE_COLOR,
                "Переход не закрыт! Введите /npcroute crossing, затем /npcroute stop.");
        gNPCRouteRecording = false;
        NPCRoute_ShowMarkers();
        new msg[96];
        format(msg, sizeof(msg), "Запись остановлена: %d точек. /npcroute test, save или undo.",
            gNPCRouteDraftCount);
        return SendClientMessage(playerid, NPCROUTE_COLOR, msg);
    }
    if (!strcmp(action, "crossing", true))
    {
        if (!gNPCRouteRecording)
            return SendClientMessage(playerid, NPCROUTE_COLOR, "Разметка перехода только во время записи.");
        if (!NPCRoute_AddPoint(playerid, true))
            return SendClientMessage(playerid, NPCROUTE_COLOR, "Нельзя поставить границу перехода.");
        gNPCRouteCrossMode = !gNPCRouteCrossMode;
        return SendClientMessage(playerid, NPCROUTE_CROSS_COLOR,
            gNPCRouteCrossMode ? ("Переход НАЧАТ. Идите до противоположного бордюра.") :
                ("Переход ЗАКРЫТ. Продолжайте движение по тротуару."));
    }
    if (gNPCRouteRecording)
        return SendClientMessage(playerid, NPCROUTE_COLOR, "Сначала /npcroute stop.");
    if (!strcmp(action, "undo", true))
    {
        if (gNPCRouteDraftCount > 1)
        {
            gNPCRouteDraftCount--;
            gNPCRouteDraftCross[gNPCRouteDraftCount-1] = 0;
        }
        NPCRoute_ShowMarkers();
        return SendClientMessage(playerid, NPCROUTE_COLOR, "Последняя точка удалена.");
    }
    if (!strcmp(action, "show", true))
    {
        NPCRoute_ShowMarkers();
        return SendClientMessage(playerid, NPCROUTE_COLOR, "Точки маршрута показаны только вам.");
    }
    if (!strcmp(action, "test", true)) return NPCRoute_Test(playerid);
    if (!strcmp(action, "save", true))
    {
        if (!NPCRoute_Save())
            return SendClientMessage(playerid, ADMIN_ERROR,
                "Ошибка сохранения: проверьте маршрут/файл и закройте переходы.");
        return SendClientMessage(playerid, NPCROUTE_COLOR,
            "Маршрут сохранён. /npcroute apply - перечитать данные без рестарта.");
    }
    return NPCRoute_Help(playerid);
}

stock NPCRoute_Init()
{
    fcreatedir(NPCROUTE_DIR);
    for (new i = 0; i < NPCROUTE_MAX_POINTS; i++) gNPCRouteMarkers[i] = INVALID_OBJECT_ID;
    NPCRoute_ResetEditor();
    NPCRoute_LoadAll();
    gNPCRouteTimer = SetTimer("NPCRoute_Tick", NPCROUTE_SAMPLE_MS, true);
    print("[NPCRoute] Route editor and saved waypoints initialized.");
    return 1;
}

stock NPCRoute_OnDisconnect(playerid)
{
    if (gNPCRouteOwner == playerid) NPCRoute_ResetEditor();
    return 1;
}

stock NPCRoute_Shutdown()
{
    if (gNPCRouteTimer) KillTimer(gNPCRouteTimer);
    gNPCRouteTimer = 0;
    NPCRoute_ResetEditor();
    return 1;
}
