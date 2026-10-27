#if defined _tarkenso_ship_maneuver_included
    #endinput
#endif
#define _tarkenso_ship_maneuver_included

// SHIP-MANEUVER-01: independently saved departure and arrival paths per port.
// Only a founder in the visual editor can author, preview and persist paths.
// A ship never teleports between waypoints. Geometry requires in-game testing.
#define SHIP_MANEUVER_FILE SHIP_DIR "/maneuvers_01.dat"
#define SHIP_MANEUVER_TEMP SHIP_DIR "/maneuvers_01.tmp"
#define SHIP_MANEUVER_MAX (8)
#define SHIP_MANEUVER_DEPART (0)
#define SHIP_MANEUVER_ARRIVE (1)
#define SHIP_MANEUVER_SPEED (0.9)
#define SHIP_MANEUVER_EPS (0.64)
#define SHIP_MANEUVER_SEGMENT_MAX (600.0)

new gShipManeuverCount[2][2];
new Float:gShipManeuverPoint[2][2][SHIP_MANEUVER_MAX][4];
new gShipManeuverDraftCount[2][2];
new Float:gShipManeuverDraft[2][2][SHIP_MANEUVER_MAX][4];
new gShipManeuverSelectedPort;
new gShipManeuverSelectedKind;
new bool:gShipManeuverPreview;
new gShipManeuverPort;
new gShipManeuverKind;
new gShipManeuverNext;
new PlayerText3D:gShipManeuverLabels[MAX_PLAYERS][SHIP_MANEUVER_MAX];

stock ShipManeuver_DraftLoad()
{
    for (new p = 0; p < 2; p++)
    {
        for (new k = 0; k < 2; k++)
        {
            gShipManeuverDraftCount[p][k] = gShipManeuverCount[p][k];
            for (new i = 0; i < gShipManeuverCount[p][k]; i++)
                for (new a = 0; a < 4; a++)
                    gShipManeuverDraft[p][k][i][a] = gShipManeuverPoint[p][k][i][a];
        }
    }
    return 1;
}

stock bool:ShipManeuver_Save()
{
    new File:f = fopen(SHIP_MANEUVER_TEMP, io_write);
    if (!f) return false;
    new bool:ok = fwrite(f, "TARKENSO_MANEUVER|1\n") > 0;
    new line[144];
    for (new p = 0; p < 2; p++)
    {
        for (new k = 0; k < 2; k++)
        {
            format(line, sizeof(line), "%d %d %d\n", p, k, gShipManeuverDraftCount[p][k]);
            if (fwrite(f, line) <= 0) ok = false;
            for (new i = 0; i < gShipManeuverDraftCount[p][k]; i++)
            {
                format(line, sizeof(line), "%.4f %.4f %.4f %.4f\n",
                    gShipManeuverDraft[p][k][i][0], gShipManeuverDraft[p][k][i][1],
                    gShipManeuverDraft[p][k][i][2], gShipManeuverDraft[p][k][i][3]);
                if (fwrite(f, line) <= 0) ok = false;
            }
        }
    }
    fclose(f);
    if (!ok) { fremove(SHIP_MANEUVER_TEMP); return false; }
    if (fexist(SHIP_MANEUVER_FILE)) fremove(SHIP_MANEUVER_FILE);
    if (!frename(SHIP_MANEUVER_TEMP, SHIP_MANEUVER_FILE)) return false;
    for (new p = 0; p < 2; p++)
    {
        for (new k = 0; k < 2; k++)
        {
            gShipManeuverCount[p][k] = gShipManeuverDraftCount[p][k];
            for (new i = 0; i < gShipManeuverCount[p][k]; i++)
                for (new a = 0; a < 4; a++)
                    gShipManeuverPoint[p][k][i][a] = gShipManeuverDraft[p][k][i][a];
        }
    }
    return true;
}

stock bool:ShipManeuver_Load()
{
    if (!fexist(SHIP_MANEUVER_FILE)) return true;
    new File:f = fopen(SHIP_MANEUVER_FILE, io_read);
    if (!f) return false;
    new header[48], line[144];
    new bool:ok = fread(f, header, sizeof(header)) != 0;
    if (ok) Auth_StripLine(header);
    if (ok && strcmp(header, "TARKENSO_MANEUVER|1", false)) ok = false;
    // Parse to draft first; never publish a partly loaded file.
    for (new p = 0; ok && p < 2; p++)
    {
        for (new k = 0; ok && k < 2; k++)
        {
            new fp, fk, count;
            if (!fread(f, line, sizeof(line)) ||
                sscanf(line, "iii", fp, fk, count) ||
                fp != p || fk != k || count < 0 || count > SHIP_MANEUVER_MAX)
            {
                ok = false;
                continue;
            }
            gShipManeuverDraftCount[p][k] = count;
            for (new i = 0; ok && i < count; i++)
            {
                new Float:x, Float:y, Float:z, Float:yaw;
                if (!fread(f, line, sizeof(line)) ||
                    sscanf(line, "ffff", x, y, z, yaw) ||
                    !Ship_ValidCoord(x, y, z) || yaw < 0.0 || yaw >= 360.0)
                {
                    ok = false;
                    continue;
                }
                gShipManeuverDraft[p][k][i][0] = x;
                gShipManeuverDraft[p][k][i][1] = y;
                gShipManeuverDraft[p][k][i][2] = z;
                gShipManeuverDraft[p][k][i][3] = yaw;
            }
        }
    }
    if (ok && fread(f, line, sizeof(line))) ok = false;
    fclose(f);
    if (!ok) { ShipManeuver_DraftLoad(); return false; }
    for (new p = 0; p < 2; p++)
    {
        for (new k = 0; k < 2; k++)
        {
            gShipManeuverCount[p][k] = gShipManeuverDraftCount[p][k];
            for (new i = 0; i < gShipManeuverCount[p][k]; i++)
                for (new a = 0; a < 4; a++)
                    gShipManeuverPoint[p][k][i][a] = gShipManeuverDraft[p][k][i][a];
        }
    }
    return true;
}

// Stops movement but never changes a player's coordinates.
stock ShipManeuver_Stop()
{
    if (gShipManeuverActive && Ship_Ready()) StopObject(gShipRoot);
    gShipManeuverActive = false;
    gShipManeuverPreview = false;
    gShipManeuverNext = 0;
    gShipManeuverArrivePort = -1;
    return 1;
}

stock bool:ShipManeuver_Validated(playerid, port, kind, bool:draft)
{
    if (!Ship_Ready() || gShipModel != SHIP_CARGO_ROOT_ID ||
        !gShipPortSaved[port][SHIP_PORT_BERTH] ||
        !gShipPortSaved[port][SHIP_PORT_EXIT]) return false;
    new count = draft ? gShipManeuverDraftCount[port][kind] :
        gShipManeuverCount[port][kind];
    if (count < 1 || count > SHIP_MANEUVER_MAX) return false;
    new startType = kind == SHIP_MANEUVER_DEPART ? SHIP_PORT_BERTH : SHIP_PORT_EXIT;
    new endType = kind == SHIP_MANEUVER_DEPART ? SHIP_PORT_EXIT : SHIP_PORT_BERTH;
    new Float:lastX = gShipPortCoord[port][startType][0];
    new Float:lastY = gShipPortCoord[port][startType][1];
    new Float:lastZ = gShipPortCoord[port][startType][2];
    for (new i = 0; i <= count; i++)
    {
        new Float:x, Float:y, Float:z;
        if (i == count)
        {
            x = gShipPortCoord[port][endType][0];
            y = gShipPortCoord[port][endType][1];
            z = gShipPortCoord[port][endType][2];
        }
        else if (draft)
        {
            x = gShipManeuverDraft[port][kind][i][0];
            y = gShipManeuverDraft[port][kind][i][1];
            z = gShipManeuverDraft[port][kind][i][2];
        }
        else
        {
            x = gShipManeuverPoint[port][kind][i][0];
            y = gShipManeuverPoint[port][kind][i][1];
            z = gShipManeuverPoint[port][kind][i][2];
        }
        new Float:dx = x-lastX, Float:dy = y-lastY, Float:dz = z-lastZ;
        new Float:d2 = dx*dx + dy*dy + dz*dz;
        if (d2 < 1.0 || d2 > SHIP_MANEUVER_SEGMENT_MAX*SHIP_MANEUVER_SEGMENT_MAX)
        {
            if (playerid >= 0) SendClientMessage(playerid, ADMIN_ERROR,
                "[Tarkenso RPG] Точки слишком близко или дальше 600 м друг от друга.");
            return false;
        }
        lastX = x;
        lastY = y;
        lastZ = z;
    }
    return true;
}

stock ShipManeuver_MoveNext()
{
    if (!gShipManeuverActive || !Ship_Ready()) return 0;
    new p = gShipManeuverPort, k = gShipManeuverKind;
    new count = gShipManeuverPreview ? gShipManeuverDraftCount[p][k] :
        gShipManeuverCount[p][k];
    if (gShipManeuverNext > count)
    {
        gShipManeuverActive = false;
        if (gShipManeuverPreview)
        {
            gShipManeuverPreview = false;
            gShipManeuverArrivePort = -1;
            return 1;
        }
        if (k == SHIP_MANEUVER_DEPART)
        {
            gShipPortDeparture = -1;
            gShipRunning = true;
            gShipNextPoint = 0;
            return Ship_Advance();
        }
        gShipPortDeparture = -1;
        gShipManeuverArrivePort = -1;
        return 1;
    }
    new Float:x, Float:y, Float:z, Float:yaw;
    if (gShipManeuverNext == count)
    {
        new endType = k == SHIP_MANEUVER_DEPART ? SHIP_PORT_EXIT : SHIP_PORT_BERTH;
        x = gShipPortCoord[p][endType][0];
        y = gShipPortCoord[p][endType][1];
        z = gShipPortCoord[p][endType][2];
        yaw = gShipPortCoord[p][endType][3];
    }
    else
    {
        new idx = gShipManeuverNext;
        if (gShipManeuverPreview)
        {
            x = gShipManeuverDraft[p][k][idx][0];
            y = gShipManeuverDraft[p][k][idx][1];
            z = gShipManeuverDraft[p][k][idx][2];
            yaw = gShipManeuverDraft[p][k][idx][3];
        }
        else
        {
            x = gShipManeuverPoint[p][k][idx][0];
            y = gShipManeuverPoint[p][k][idx][1];
            z = gShipManeuverPoint[p][k][idx][2];
            yaw = gShipManeuverPoint[p][k][idx][3];
        }
    }
    gShipManeuverNext++;
    MoveObject(gShipRoot, x, y, z, SHIP_MANEUVER_SPEED, 0.0, 0.0, yaw);
    return 1;
}

stock ShipManeuver_Tick()
{
    if (!gShipManeuverActive || !Ship_Ready()) return 0;
    new p = gShipManeuverPort, k = gShipManeuverKind;
    if (gShipManeuverNext == -1)
    {
        new Float:ax, Float:ay, Float:az;
        GetObjectPos(gShipRoot, ax, ay, az);
        new Float:ex = ax-gShipPortCoord[p][SHIP_PORT_EXIT][0];
        new Float:ey = ay-gShipPortCoord[p][SHIP_PORT_EXIT][1];
        new Float:ez = az-gShipPortCoord[p][SHIP_PORT_EXIT][2];
        if (ex*ex+ey*ey+ez*ez <= SHIP_MANEUVER_EPS)
        {
            gShipManeuverNext = 0;
            return ShipManeuver_MoveNext();
        }
        return 1;
    }
    new count = gShipManeuverPreview ? gShipManeuverDraftCount[p][k] :
        gShipManeuverCount[p][k];
    new idx = gShipManeuverNext-1;
    if (idx < 0 || idx > count) return ShipManeuver_MoveNext();
    new Float:x, Float:y, Float:z;
    if (idx == count)
    {
        new t = k == SHIP_MANEUVER_DEPART ? SHIP_PORT_EXIT : SHIP_PORT_BERTH;
        x = gShipPortCoord[p][t][0];
        y = gShipPortCoord[p][t][1];
        z = gShipPortCoord[p][t][2];
    }
    else if (gShipManeuverPreview)
    {
        x = gShipManeuverDraft[p][k][idx][0];
        y = gShipManeuverDraft[p][k][idx][1];
        z = gShipManeuverDraft[p][k][idx][2];
    }
    else
    {
        x = gShipManeuverPoint[p][k][idx][0];
        y = gShipManeuverPoint[p][k][idx][1];
        z = gShipManeuverPoint[p][k][idx][2];
    }
    new Float:sx, Float:sy, Float:sz;
    GetObjectPos(gShipRoot, sx, sy, sz);
    new Float:dx = x-sx, Float:dy = y-sy, Float:dz = z-sz;
    if (dx*dx+dy*dy+dz*dz <= SHIP_MANEUVER_EPS)
        return ShipManeuver_MoveNext();
    return 1;
}

stock bool:ShipManeuver_Start(port, kind, bool:preview)
{
    if (gShipManeuverActive || gShipRunning || !Ship_Ready()) return false;
    if (!ShipManeuver_Validated(-1, port, kind, preview)) return false;
    if (preview && ShipEditor_Occupied()) return false;
    new startType = kind == SHIP_MANEUVER_DEPART ? SHIP_PORT_BERTH : SHIP_PORT_EXIT;
    new Float:x, Float:y, Float:z;
    GetObjectPos(gShipRoot, x, y, z);
    new Float:dx = x-gShipPortCoord[port][startType][0];
    new Float:dy = y-gShipPortCoord[port][startType][1];
    new Float:dz = z-gShipPortCoord[port][startType][2];
    new Float:startD2 = dx*dx + dy*dy + dz*dz;
    if (startD2 > 64.0 && (preview || kind != SHIP_MANEUVER_ARRIVE ||
        startD2 > SHIP_PORT_EXIT_NEAR_ROUTE*SHIP_PORT_EXIT_NEAR_ROUTE)) return false;
    gShipManeuverPort = port;
    gShipManeuverKind = kind;
    gShipManeuverPreview = preview;
    gShipManeuverActive = true;
    gShipManeuverNext = 0;
    if (startD2 > 64.0)
    {
        gShipManeuverNext = -1;
        MoveObject(gShipRoot, gShipPortCoord[port][startType][0],
            gShipPortCoord[port][startType][1],
            gShipPortCoord[port][startType][2], SHIP_MANEUVER_SPEED,
            0.0, 0.0, gShipPortCoord[port][startType][3]);
        return true;
    }
    return ShipManeuver_MoveNext() != 0;
}

stock ShipManeuver_MarkersClear(playerid)
{
    for (new i = 0; i < SHIP_MANEUVER_MAX; i++)
    {
        if (gShipManeuverLabels[playerid][i] != PlayerText3D:INVALID_3DTEXT_ID)
            DeletePlayer3DTextLabel(playerid, gShipManeuverLabels[playerid][i]);
        gShipManeuverLabels[playerid][i] = PlayerText3D:INVALID_3DTEXT_ID;
    }
    return 1;
}

stock ShipManeuver_MarkersShow(playerid)
{
    ShipManeuver_MarkersClear(playerid);
    new p = gShipManeuverSelectedPort, k = gShipManeuverSelectedKind;
    for (new i = 0; i < gShipManeuverDraftCount[p][k]; i++)
    {
        new name[24];
        format(name, sizeof(name), "WAYPOINT %d", i+1);
        gShipManeuverLabels[playerid][i] = CreatePlayer3DTextLabel(playerid,
            name, 0x8FE6AFFF, gShipManeuverDraft[p][k][i][0],
            gShipManeuverDraft[p][k][i][1],
            gShipManeuverDraft[p][k][i][2] + 25.0, 400.0);
    }
    return 1;
}

stock ShipManeuver_EditorBegin(playerid)
{
    ShipManeuver_DraftLoad();
    gShipManeuverSelectedPort = SHIP_PORT_LS;
    gShipManeuverSelectedKind = SHIP_MANEUVER_DEPART;
    ShipManeuver_MarkersShow(playerid);
    return 1;
}

stock ShipManeuver_EditorEnd(playerid)
{
    ShipManeuver_MarkersClear(playerid);
    if (gShipManeuverActive && gShipManeuverPreview) ShipManeuver_Stop();
    return 1;
}

stock ShipManeuver_EditorSelect(playerid, port, kind)
{
    gShipManeuverSelectedPort = port;
    gShipManeuverSelectedKind = kind;
    ShipManeuver_MarkersShow(playerid);
    return 1;
}

stock ShipManeuver_EditorAdd(playerid)
{
    if (gShipManeuverActive) return 0;
    new p = gShipManeuverSelectedPort, k = gShipManeuverSelectedKind;
    new count = gShipManeuverDraftCount[p][k];
    if (count >= SHIP_MANEUVER_MAX)
        return SendClientMessage(playerid, ADMIN_ERROR,
            "[Tarkenso RPG] Максимум 8 промежуточных точек на манёвр.");
    new Float:x, Float:y, Float:z, Float:rx, Float:ry, Float:yaw;
    GetObjectPos(gShipRoot, x, y, z);
    GetObjectRot(gShipRoot, rx, ry, yaw);
    // Reject accidental duplicate records and save the hull's real heading.
    if (count)
    {
        new Float:dx = x-gShipManeuverDraft[p][k][count-1][0];
        new Float:dy = y-gShipManeuverDraft[p][k][count-1][1];
        if (dx*dx+dy*dy < 1.0)
            return SendClientMessage(playerid, ADMIN_ERROR,
                "[Tarkenso RPG] Переместите корабль минимум на 1 м от предыдущей точки.");
    }
    for (new i = 0; i < 3; i++)
        gShipManeuverDraft[p][k][count][i] = i == 0 ? x : (i == 1 ? y : z);
    gShipManeuverDraft[p][k][count][3] = yaw;
    gShipManeuverDraftCount[p][k]++;
    ShipManeuver_MarkersShow(playerid);
    new line[80];
    format(line, sizeof(line), "[Tarkenso RPG] Добавлена точка %d/%d.", count+1, SHIP_MANEUVER_MAX);
    return SendClientMessage(playerid, SHIP_COLOR, line);
}

stock ShipManeuver_EditorUndo(playerid)
{
    if (gShipManeuverActive) return 0;
    new p = gShipManeuverSelectedPort, k = gShipManeuverSelectedKind;
    if (gShipManeuverDraftCount[p][k] > 0) gShipManeuverDraftCount[p][k]--;
    ShipManeuver_MarkersShow(playerid);
    return SendClientMessage(playerid, SHIP_COLOR,
        "[Tarkenso RPG] Последняя точка удалена из черновика.");
}

stock ShipManeuver_EditorSave(playerid)
{
    if (gShipManeuverActive) return 0;
    // Validate only configured routes; empty routes are allowed to retain old behavior.
    for (new p = 0; p < 2; p++)
        for (new k = 0; k < 2; k++)
            if (gShipManeuverDraftCount[p][k] &&
                !ShipManeuver_Validated(playerid, p, k, true))
                return 1;
    if (!ShipManeuver_Save())
        return SendClientMessage(playerid, ADMIN_ERROR,
            "[Tarkenso RPG] Файл манёвров не сохранён; проверьте scriptfiles.");
    return SendClientMessage(playerid, SHIP_COLOR,
        "[Tarkenso RPG] Манёвры сохранены для LS/SF (отход и заход).");
}

stock ShipManeuver_EditorLoad(playerid)
{
    if (gShipManeuverActive) return 0;
    ShipManeuver_DraftLoad();
    ShipManeuver_MarkersShow(playerid);
    return SendClientMessage(playerid, SHIP_COLOR,
        "[Tarkenso RPG] Несохранённые изменения маршрутов отменены.");
}

stock ShipManeuver_EditorPreview(playerid)
{
    if (gShipManeuverActive) return 1;
    if (!ShipManeuver_Start(gShipManeuverSelectedPort, gShipManeuverSelectedKind, true))
        return SendClientMessage(playerid, ADMIN_ERROR,
            "[Tarkenso RPG] PREVIEW: верните корабль в berth/exit и проверьте точки.");
    return SendClientMessage(playerid, SHIP_COLOR,
        "[Tarkenso RPG] Проверочный манёвр запущен без пассажиров. STOP остановит его.");
}

stock ShipManeuver_EditorStop(playerid)
{
    if (!gShipManeuverPreview) return 1;
    ShipManeuver_Stop();
    return SendClientMessage(playerid, SHIP_COLOR,
        "[Tarkenso RPG] Предпросмотр остановлен без телепортации.");
}

stock ShipManeuver_Init()
{
    gShipManeuverActive = false;
    gShipManeuverPreview = false;
    gShipManeuverArrivePort = -1;
    for (new p = 0; p < MAX_PLAYERS; p++)
        for (new i = 0; i < SHIP_MANEUVER_MAX; i++)
            gShipManeuverLabels[p][i] = PlayerText3D:INVALID_3DTEXT_ID;
    for (new p = 0; p < 2; p++)
        for (new k = 0; k < 2; k++)
            gShipManeuverCount[p][k] = 0;
    if (!ShipManeuver_Load())
        print("[ShipManeuver] Invalid route file: routes disabled, original preserved.");
    return 1;
}
