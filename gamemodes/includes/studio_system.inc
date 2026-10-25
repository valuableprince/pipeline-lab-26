#if defined _tarkenso_studio_included
    #endinput
#endif
#define _tarkenso_studio_included

// STUDIO-CORE-01: серверные права, игровой вход и опубликованные круглые зоны.
// Зоны не используют SetPlayerCheckpoint: это сохраняет чекпоинты профессий.
#define STUDIO_DIALOG_MAIN      (6300)
#define STUDIO_ZONE_DIR        "studio_zones"
#define STUDIO_ZONE_COUNT      (16)
#define STUDIO_ZONE_MARKERS    (9)
#define STUDIO_ZONE_COLOR      (0x91D4C5FF)
#define STUDIO_ZONE_MAX_RADIUS (50.0)

new bool:gStudioZoneExists[STUDIO_ZONE_COUNT];
new bool:gStudioZoneActive[STUDIO_ZONE_COUNT];
new bool:gStudioZoneInvalid[STUDIO_ZONE_COUNT];
new Float:gStudioZoneX[STUDIO_ZONE_COUNT];
new Float:gStudioZoneY[STUDIO_ZONE_COUNT];
new Float:gStudioZoneZ[STUDIO_ZONE_COUNT];
new Float:gStudioZoneRadius[STUDIO_ZONE_COUNT];
new gStudioZoneWorld[STUDIO_ZONE_COUNT];
new gStudioZoneInterior[STUDIO_ZONE_COUNT];
new gStudioZoneMode[STUDIO_ZONE_COUNT]; // 0: зона, 1: логическая контрольная точка
new bool:gStudioZoneInside[MAX_PLAYERS][STUDIO_ZONE_COUNT];

new gStudioZoneOwner = INVALID_PLAYER_ID;
new gStudioZoneDraftSlot = -1;
new Float:gStudioZoneDraftX;
new Float:gStudioZoneDraftY;
new Float:gStudioZoneDraftZ;
new Float:gStudioZoneDraftRadius;
new gStudioZoneDraftWorld;
new gStudioZoneDraftInterior;
new gStudioZoneDraftMode;
new gStudioZoneMarkers[STUDIO_ZONE_MARKERS];
new gStudioZoneTimer;

forward StudioZone_Poll();

stock bool:Studio_Allowed(playerid)
{
    return Admin_IsFounder(playerid) && gAdminDuty[playerid];
}

stock bool:Studio_Require(playerid)
{
    if (Studio_Allowed(playerid)) return true;
    SendClientMessage(playerid, ADMIN_ERROR,
        "Tarkenso Studio: требуется подтверждённый аккаунт основателя и /aduty.");
    return false;
}

stock StudioZone_Path(slot, dest[], size)
{
    format(dest, size, STUDIO_ZONE_DIR "/zone_%d.dat", slot);
    return 1;
}

stock bool:StudioZone_Valid(Float:x, Float:y, Float:z, Float:radius, world, interior)
{
    return x >= -3000.0 && x <= 3000.0 && y >= -3000.0 && y <= 3000.0 &&
        z >= -150.0 && z <= 2000.0 && radius >= 1.0 &&
        radius <= STUDIO_ZONE_MAX_RADIUS && world >= 0 && world <= 100000 &&
        interior >= 0 && interior <= 255;
}

stock bool:StudioZone_Load(slot)
{
#if defined TARKENSO_DB_PRIMARY
    return DBWorld_ZoneLoad(slot);
#else
    new path[96], backup[112], header[40], values[160], extra[8];
    StudioZone_Path(slot, path, sizeof(path));
    format(backup, sizeof(backup), "%s.bak", path);
    gStudioZoneExists[slot] = false;
    gStudioZoneActive[slot] = false;
    gStudioZoneMode[slot] = 0;
    gStudioZoneInvalid[slot] = false;

    if (!fexist(path) && fexist(backup) && !frename(backup, path))
    {
        gStudioZoneInvalid[slot] = true;
        return false;
    }
    if (!fexist(path)) return true;
    new File:file = fopen(path, io_read);
    if (!file)
    {
        gStudioZoneInvalid[slot] = true;
        return false;
    }
    new bool:ok = fread(file, header, sizeof(header)) != 0;
    if (!fread(file, values, sizeof(values))) ok = false;
    Auth_StripLine(header);
    new bool:legacy = !strcmp(header, "TARKENSO_ZONE|1", false);
    if (!legacy && strcmp(header, "TARKENSO_ZONE|2", false)) ok = false;
    new active, world, interior, mode = 0;
    new Float:x, Float:y, Float:z, Float:radius;
    if (ok && legacy && sscanf(values, "iffffii", active, x, y, z, radius, world, interior)) ok = false;
    if (ok && !legacy && sscanf(values, "iffffiii", active, x, y, z, radius, world, interior, mode)) ok = false;
    if (ok && (active < 0 || active > 1 || mode < 0 || mode > 1 ||
        !StudioZone_Valid(x, y, z, radius, world, interior))) ok = false;
    if (fread(file, extra, sizeof(extra))) ok = false;
    fclose(file);
    if (!ok)
    {
        gStudioZoneInvalid[slot] = true;
        printf("[Studio] Zone %d is corrupt; publishing blocked.", slot);
        return false;
    }
    gStudioZoneExists[slot] = true;
    gStudioZoneActive[slot] = (active == 1);
    gStudioZoneX[slot] = x;
    gStudioZoneY[slot] = y;
    gStudioZoneZ[slot] = z;
    gStudioZoneRadius[slot] = radius;
    gStudioZoneWorld[slot] = world;
    gStudioZoneInterior[slot] = interior;
    gStudioZoneMode[slot] = mode;
    return true;
#endif
}

// Сохраняем один предыдущий релиз в .bak. Новая версия видна после успешного rename.
stock bool:StudioZone_Write(slot, bool:active, Float:x, Float:y, Float:z,
    Float:radius, world, interior, mode = 0)
{
    if (slot < 0 || slot >= STUDIO_ZONE_COUNT || gStudioZoneInvalid[slot] ||
        (mode != 0 && mode != 1) ||
        !StudioZone_Valid(x, y, z, radius, world, interior)) return false;
    #if defined TARKENSO_DB_PRIMARY
    if (!DBWorld_ZoneSave(slot, active, x, y, z, radius, world, interior, mode)) return false;
    #else
    new path[96], temp[112], backup[112], line[160];
    StudioZone_Path(slot, path, sizeof(path));
    format(temp, sizeof(temp), "%s.tmp", path);
    format(backup, sizeof(backup), "%s.bak", path);
    if (fexist(temp) && !fremove(temp)) return false;
    new File:file = fopen(temp, io_write);
    if (!file) return false;
    new bool:ok = fwrite(file, "TARKENSO_ZONE|2\n") > 0;
    format(line, sizeof(line), "%d %.4f %.4f %.4f %.3f %d %d %d\n",
        _:active, x, y, z, radius, world, interior, mode);
    if (fwrite(file, line) <= 0) ok = false;
    fclose(file);
    if (!ok) { fremove(temp); return false; }
    new bool:old = fexist(path) != 0;
    if (old)
    {
        if (fexist(backup) && !fremove(backup)) { fremove(temp); return false; }
        if (!frename(path, backup)) { fremove(temp); return false; }
    }
    if (!frename(temp, path))
    {
        if (old && !frename(backup, path))
            printf("[Studio] Restore zone %d manually from its .bak backup.", slot);
        fremove(temp);
        return false;
    }
    #endif
    gStudioZoneExists[slot] = true;
    gStudioZoneActive[slot] = active;
    gStudioZoneX[slot] = x;
    gStudioZoneY[slot] = y;
    gStudioZoneZ[slot] = z;
    gStudioZoneRadius[slot] = radius;
    gStudioZoneWorld[slot] = world;
    gStudioZoneInterior[slot] = interior;
    gStudioZoneMode[slot] = mode;
    // Пересчёт входов после публикации: старые отметки не переносятся.
    for (new p = 0; p < MAX_PLAYERS; p++) gStudioZoneInside[p][slot] = false;
    return true;
}

stock StudioZone_ClearMarkers()
{
    for (new i = 0; i < STUDIO_ZONE_MARKERS; i++)
    {
        if (gStudioZoneMarkers[i] == INVALID_OBJECT_ID) continue;
        if (gStudioZoneOwner != INVALID_PLAYER_ID && IsPlayerConnected(gStudioZoneOwner))
            DestroyPlayerObject(gStudioZoneOwner, gStudioZoneMarkers[i]);
        gStudioZoneMarkers[i] = INVALID_OBJECT_ID;
    }
    return 1;
}

stock StudioZone_Cancel()
{
    StudioZone_ClearMarkers();
    gStudioZoneOwner = INVALID_PLAYER_ID;
    gStudioZoneDraftSlot = -1;
    gStudioZoneDraftMode = 0;
    return 1;
}

stock StudioZone_Show(playerid)
{
    StudioZone_ClearMarkers();
    if (gStudioZoneDraftSlot < 0 || gStudioZoneOwner != playerid) return 0;
    new Float:x = gStudioZoneDraftX, Float:y = gStudioZoneDraftY;
    new Float:z = gStudioZoneDraftZ + 0.45, Float:r = gStudioZoneDraftRadius;
    gStudioZoneMarkers[0] = CreatePlayerObject(playerid, 19131, x, y, z, 0.0, 0.0, 0.0);
    gStudioZoneMarkers[1] = CreatePlayerObject(playerid, 19131, x+r, y, z, 0.0, 0.0, 0.0);
    gStudioZoneMarkers[2] = CreatePlayerObject(playerid, 19131, x-r, y, z, 0.0, 0.0, 0.0);
    gStudioZoneMarkers[3] = CreatePlayerObject(playerid, 19131, x, y+r, z, 0.0, 0.0, 0.0);
    gStudioZoneMarkers[4] = CreatePlayerObject(playerid, 19131, x, y-r, z, 0.0, 0.0, 0.0);
    new Float:d = r * 0.70710678;
    gStudioZoneMarkers[5] = CreatePlayerObject(playerid, 19131, x+d, y+d, z, 0.0, 0.0, 0.0);
    gStudioZoneMarkers[6] = CreatePlayerObject(playerid, 19131, x+d, y-d, z, 0.0, 0.0, 0.0);
    gStudioZoneMarkers[7] = CreatePlayerObject(playerid, 19131, x-d, y+d, z, 0.0, 0.0, 0.0);
    gStudioZoneMarkers[8] = CreatePlayerObject(playerid, 19131, x-d, y-d, z, 0.0, 0.0, 0.0);
    return 1;
}

stock bool:StudioZone_Capture(playerid)
{
    if (GetPlayerState(playerid) != PLAYER_STATE_ONFOOT) return false;
    new Float:x, Float:y, Float:z;
    GetPlayerPos(playerid, x, y, z);
    new world = GetPlayerVirtualWorld(playerid);
    new interior = GetPlayerInterior(playerid);
    if (!StudioZone_Valid(x, y, z, gStudioZoneDraftRadius, world, interior)) return false;
    gStudioZoneDraftX = x;
    gStudioZoneDraftY = y;
    gStudioZoneDraftZ = z;
    gStudioZoneDraftWorld = world;
    gStudioZoneDraftInterior = interior;
    return true;
}

stock StudioZone_Status(playerid)
{
    new active, total, corrupt;
    for (new i = 0; i < STUDIO_ZONE_COUNT; i++)
    {
        if (gStudioZoneExists[i]) total++;
        if (gStudioZoneActive[i]) active++;
        if (gStudioZoneInvalid[i]) corrupt++;
    }
    new message[130];
    format(message, sizeof(message),
        "Зоны: опубликовано %d, сохранено %d/%d, повреждено %d.",
        active, total, STUDIO_ZONE_COUNT, corrupt);
    SendClientMessage(playerid, STUDIO_ZONE_COLOR, message);
    if (gStudioZoneDraftSlot >= 0)
    {
        format(message, sizeof(message), "Редактируется слот %d, владелец ID %d.",
            gStudioZoneDraftSlot, gStudioZoneOwner);
        SendClientMessage(playerid, STUDIO_ZONE_COLOR, message);
    }
    return 1;
}

stock StudioZone_Help(playerid)
{
    SendClientMessage(playerid, STUDIO_ZONE_COLOR,
        "/zoneedit create [0..15] [радиус 1..50] — создать черновик на своей позиции.");
    SendClientMessage(playerid, STUDIO_ZONE_COLOR,
        "/zoneedit move, radius [метры], mode, show, hide, publish, cancel.");
    SendClientMessage(playerid, STUDIO_ZONE_COLOR,
        "/zoneedit disable [0..15], status — снять публикацию / посмотреть состояние.");
    return 1;
}

stock StudioZone_Command(playerid, const params[])
{
    if (!Studio_Require(playerid)) return 1;
    if (isnull(params)) return StudioZone_Help(playerid);
    new action[24];
    if (sscanf(params, "s[24]", action)) return StudioZone_Help(playerid);
    if (!strcmp(action, "status", true)) return StudioZone_Status(playerid);
    if (!strcmp(action, "create", true))
    {
        new slot;
        new Float:radius;
        if (sscanf(params, "s[24]if", action, slot, radius) ||
            slot < 0 || slot >= STUDIO_ZONE_COUNT ||
            radius < 1.0 || radius > STUDIO_ZONE_MAX_RADIUS)
            return SendClientMessage(playerid, ADMIN_ERROR,
                "Использование: /zoneedit create [0..15] [радиус 1..50].");
        if (gStudioZoneInvalid[slot])
            return SendClientMessage(playerid, ADMIN_ERROR,
                "Файл слота повреждён. Восстановите вручную из .bak.");
        if (gStudioZoneOwner != INVALID_PLAYER_ID && gStudioZoneOwner != playerid)
            return SendClientMessage(playerid, ADMIN_ERROR, "Другой разработчик редактирует зону.");
        if (gStudioZoneOwner == playerid) StudioZone_Cancel();
        gStudioZoneOwner = playerid;
        gStudioZoneDraftSlot = slot;
        gStudioZoneDraftRadius = radius;
        gStudioZoneDraftMode = 0;
        if (!StudioZone_Capture(playerid))
        {
            StudioZone_Cancel();
            return SendClientMessage(playerid, ADMIN_ERROR,
                "Создать зону можно только пешком в допустимых координатах.");
        }
        StudioZone_Show(playerid);
        return SendClientMessage(playerid, STUDIO_ZONE_COLOR,
            "Черновик зоны открыт. Метки видны только вам; публикация: /zoneedit publish.");
    }
    if (!strcmp(action, "disable", true))
    {
        new slot;
        if (sscanf(params, "s[24]i", action, slot) ||
            slot < 0 || slot >= STUDIO_ZONE_COUNT)
            return SendClientMessage(playerid, ADMIN_ERROR,
                "Использование: /zoneedit disable [0..15].");
        if (gStudioZoneOwner != INVALID_PLAYER_ID)
            return SendClientMessage(playerid, ADMIN_ERROR, "Сначала закройте активный черновик.");
        if (!gStudioZoneExists[slot] || !gStudioZoneActive[slot])
            return SendClientMessage(playerid, ADMIN_ERROR, "Нет опубликованной зоны в этом слоте.");
        if (!StudioZone_Write(slot, false, gStudioZoneX[slot], gStudioZoneY[slot],
            gStudioZoneZ[slot], gStudioZoneRadius[slot],
            gStudioZoneWorld[slot], gStudioZoneInterior[slot], gStudioZoneMode[slot]))
            return SendClientMessage(playerid, ADMIN_ERROR, "Не удалось снять публикацию зоны.");
        new detail[56];
        format(detail, sizeof(detail), "slot=%d", slot);
        if (!Admin_LogPlayer(playerid, "STUDIO_ZONE_DISABLE", detail))
            print("[Studio] Failed to write zone disable audit event.");
        return SendClientMessage(playerid, STUDIO_ZONE_COLOR, "Публикация зоны отключена.");
    }
    if (gStudioZoneOwner != playerid || gStudioZoneDraftSlot < 0)
        return SendClientMessage(playerid, ADMIN_ERROR, "Сначала /zoneedit create.");
    if (!strcmp(action, "cancel", true))
    {
        StudioZone_Cancel();
        return SendClientMessage(playerid, STUDIO_ZONE_COLOR, "Черновик отменён, сервер не изменён.");
    }
    if (!strcmp(action, "hide", true))
    {
        StudioZone_ClearMarkers();
        return SendClientMessage(playerid, STUDIO_ZONE_COLOR, "Метки скрыты.");
    }
    if (!strcmp(action, "show", true))
    {
        StudioZone_Show(playerid);
        return SendClientMessage(playerid, STUDIO_ZONE_COLOR, "Метки зоны показаны только вам.");
    }
    if (!strcmp(action, "move", true))
    {
        if (!StudioZone_Capture(playerid))
            return SendClientMessage(playerid, ADMIN_ERROR, "Нельзя переместить зону из этого положения.");
        StudioZone_Show(playerid);
        return SendClientMessage(playerid, STUDIO_ZONE_COLOR, "Центр зоны перенесён в вашу позицию.");
    }
    if (!strcmp(action, "radius", true))
    {
        new Float:radius;
        if (sscanf(params, "s[24]f", action, radius) ||
            radius < 1.0 || radius > STUDIO_ZONE_MAX_RADIUS)
            return SendClientMessage(playerid, ADMIN_ERROR, "Использование: /zoneedit radius [1..50].");
        gStudioZoneDraftRadius = radius;
        StudioZone_Show(playerid);
        return SendClientMessage(playerid, STUDIO_ZONE_COLOR, "Радиус черновика изменён.");
    }
    if (!strcmp(action, "mode", true))
    {
        gStudioZoneDraftMode = 1 - gStudioZoneDraftMode;
        return SendClientMessage(playerid, STUDIO_ZONE_COLOR,
            gStudioZoneDraftMode == 1 ? ("Режим контрольной точки.") : ("Режим обычной зоны."));
    }
    if (!strcmp(action, "publish", true))
    {
        new slot = gStudioZoneDraftSlot;
        if (!StudioZone_Write(slot, true, gStudioZoneDraftX, gStudioZoneDraftY,
            gStudioZoneDraftZ, gStudioZoneDraftRadius,
            gStudioZoneDraftWorld, gStudioZoneDraftInterior, gStudioZoneDraftMode))
            return SendClientMessage(playerid, ADMIN_ERROR, "Ошибка записи. Публикация отменена.");
        new detail[100];
        format(detail, sizeof(detail), "slot=%d r=%.2f world=%d interior=%d",
            slot, gStudioZoneDraftRadius, gStudioZoneDraftWorld, gStudioZoneDraftInterior);
        if (!Admin_LogPlayer(playerid, "STUDIO_ZONE_PUBLISH", detail))
            print("[Studio] Failed to write zone publish audit event.");
        StudioZone_Cancel();
        return SendClientMessage(playerid, STUDIO_ZONE_COLOR,
            "Зона опубликована без перезапуска. Предыдущий релиз: файл .bak.");
    }
    return StudioZone_Help(playerid);
}

// Публикация создаёт только безопасную информационную зону, не запускает квестов
// и не перехватывает штатные чекпоинты профессий.
public StudioZone_Poll()
{
    for (new playerid = 0; playerid < MAX_PLAYERS; playerid++)
    {
        if (!IsPlayerConnected(playerid) || IsPlayerNPC(playerid) ||
            gAuthState[playerid] != AUTH_READY) continue;
        new Float:x, Float:y, Float:z;
        GetPlayerPos(playerid, x, y, z);
        new world = GetPlayerVirtualWorld(playerid);
        new interior = GetPlayerInterior(playerid);
        for (new slot = 0; slot < STUDIO_ZONE_COUNT; slot++)
        {
            if (!gStudioZoneActive[slot] || world != gStudioZoneWorld[slot] ||
                interior != gStudioZoneInterior[slot])
            {
                gStudioZoneInside[playerid][slot] = false;
                continue;
            }
            new Float:dx = x - gStudioZoneX[slot];
            new Float:dy = y - gStudioZoneY[slot];
            new Float:dz = z - gStudioZoneZ[slot];
            new Float:distanceSq = dx*dx + dy*dy + dz*dz;
            new Float:radius = gStudioZoneRadius[slot];
            if (!gStudioZoneInside[playerid][slot] && distanceSq <= radius*radius)
            {
                gStudioZoneInside[playerid][slot] = true;
                new message[90];
                if (gStudioZoneMode[slot] == 1)
                    format(message, sizeof(message), "Достигнута контрольная точка Tarkenso #%d.", slot);
                else
                    format(message, sizeof(message), "Вы вошли в зону Tarkenso #%d.", slot);
                SendClientMessage(playerid, STUDIO_ZONE_COLOR, message);
            }
            else if (gStudioZoneInside[playerid][slot] &&
                distanceSq > (radius + 1.0) * (radius + 1.0))
                gStudioZoneInside[playerid][slot] = false;
        }
    }
    return 1;
}

stock Studio_Open(playerid)
{
    if (!Studio_Require(playerid)) return 1;
    ShowPlayerDialog(playerid, STUDIO_DIALOG_MAIN, DIALOG_STYLE_LIST, "Tarkenso Studio",
        "Зоны / контрольные точки\nМусорные контейнеры\nСостояние Studio",
        "Открыть", "Закрыть");
    return 1;
}

stock Studio_Command(playerid, const params[])
{
    if (!Studio_Require(playerid)) return 1;
    if (isnull(params)) return Studio_Open(playerid);

    new action[24], tail[116], start = 0, offset = 0;
    while (params[start] == ' ') start++;
    while (params[start] != EOS && params[start] != ' ' &&
        offset < sizeof(action) - 1)
        action[offset++] = params[start++];
    action[offset] = EOS;
    while (params[start] == ' ') start++;
    strmid(tail, params, start, strlen(params), sizeof(tail));

    if (!strcmp(action, "zone", true) || !strcmp(action, "zones", true))
    {
        if (!tail[0]) return StudioUI_ZoneList(playerid);
        return StudioZone_Command(playerid, tail);
    }
    if (!strcmp(action, "containers", true) || !strcmp(action, "bins", true))
        return CleanerEditor_List(playerid);
    if (!strcmp(action, "status", true))
        return StudioZone_Status(playerid);
    if (!strcmp(action, "help", true))
    {
        SendClientMessage(playerid, STUDIO_ZONE_COLOR,
            "Studio: /studio zones — зоны; /studio containers — контейнеры.");
        SendClientMessage(playerid, STUDIO_ZONE_COLOR,
            "/studio status — состояние зон; /zoneedit — прямые команды.");
        return 1;
    }
    return SendClientMessage(playerid, ADMIN_ERROR,
        "Использование: /studio zones|containers|status|help.");
}

stock Studio_OnDialog(playerid, dialogid, response, listitem)
{
    if (dialogid != STUDIO_DIALOG_MAIN) return 0;
    if (!Studio_Require(playerid) || !response) return 1;
    switch (listitem)
    {
        case 0: StudioUI_ZoneList(playerid);
        case 1: CleanerEditor_List(playerid);
        case 2: StudioZone_Status(playerid);
    }
    return 1;
}

stock Studio_Init()
{
    fcreatedir(STUDIO_ZONE_DIR);
    for (new i = 0; i < STUDIO_ZONE_MARKERS; i++)
        gStudioZoneMarkers[i] = INVALID_OBJECT_ID;
    StudioZone_Cancel();
    for (new slot = 0; slot < STUDIO_ZONE_COUNT; slot++) StudioZone_Load(slot);
    gStudioZoneTimer = SetTimer("StudioZone_Poll", 900, true);
    print("[Studio] Core and zone editor initialized.");
    return 1;
}

stock Studio_OnDisconnect(playerid)
{
    if (gStudioZoneOwner == playerid) StudioZone_Cancel();
    if (gCleanerEditorOwner == playerid) CleanerEditor_Cancel();
    for (new slot = 0; slot < STUDIO_ZONE_COUNT; slot++)
        gStudioZoneInside[playerid][slot] = false;
    return 1;
}

stock Studio_Shutdown()
{
    if (gStudioZoneTimer) KillTimer(gStudioZoneTimer);
    gStudioZoneTimer = 0;
    StudioZone_Cancel();
    CleanerEditor_Cancel();
    return 1;
}
