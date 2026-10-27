#if defined _tarkenso_ship_studio_included
    #endinput
#endif
#define _tarkenso_ship_studio_included

// SHIP-STUDIO-01: one editable cargo vessel, composed of ordinary server objects.
// Standard object attachments are used deliberately: this is NOT a vehicle.
// This is a development tool. No automatic map collision or passenger guarantee.
#define SHIP_DIR           "shipstudio"
#define SHIP_DATA          SHIP_DIR "/cargo_01.dat"
#define SHIP_EXPORT        SHIP_DIR "/cargo_01_route.inc"
#define SHIP_MAX_PARTS     (48)
#define SHIP_MAX_POINTS    (80)
#define SHIP_COLOR         (0x87D9E5FF)
#define SHIP_CRUISE_SPEED  (4.0)
#define SHIP_CARGO_ROOT_Z   (7.0)
#define SHIP_CARGO_ROOT_ID  (9585)
// Freighter mesh points along local +X, 90 degrees from navigation +Y.
#define SHIP_CARGO_HEADING_OFFSET (90.0)
#define SHIP_CARGO_START_X  (2640.0)
#define SHIP_CARGO_START_Y  (-2740.0)

new gShipRoot = INVALID_OBJECT_ID;
new gShipModel;
new gShipOwner = INVALID_PLAYER_ID;
new gShipPartCount;
new gShipPartObject[SHIP_MAX_PARTS];
new gShipPartModel[SHIP_MAX_PARTS];
new Float:gShipPartLocal[SHIP_MAX_PARTS][6];
new gShipPointCount;
new Float:gShipPoint[SHIP_MAX_POINTS][4];
new bool:gShipRunning;
new gShipNextPoint;
new gShipTimer;
new gShipEditOwner = INVALID_PLAYER_ID;
forward ShipEditor_Command(playerid, const params[]);
new gShipPortDeparture = -1;
forward ShipPort_Tick();
forward ShipPort_Command(playerid, const params[]);

forward Ship_Tick();

stock bool:Ship_ValidCoord(Float:x, Float:y, Float:z)
{
    return x >= -3000.0 && x <= 3000.0 &&
        y >= -3000.0 && y <= 3000.0 && z >= -100.0 && z <= 1000.0;
}

stock bool:Ship_ValidModel(model)
{
    return model >= 600 && model <= 19999;
}

stock bool:Ship_Ready()
{
    return gShipRoot != INVALID_OBJECT_ID && IsValidObject(gShipRoot);
}

stock Ship_Stop()
{
    gShipPortDeparture = -1;
    gShipRunning = false;
    if (Ship_Ready()) StopObject(gShipRoot);
    return 1;
}

stock Ship_Reset()
{
    Ship_Stop();
    for (new i = 0; i < gShipPartCount; i++)
    {
        if (IsValidObject(gShipPartObject[i])) DestroyObject(gShipPartObject[i]);
        gShipPartObject[i] = INVALID_OBJECT_ID;
    }
    if (Ship_Ready()) DestroyObject(gShipRoot);
    gShipRoot = INVALID_OBJECT_ID;
    gShipModel = 0;
    gShipPartCount = 0;
    gShipPointCount = 0;
    gShipNextPoint = 0;
    gShipOwner = INVALID_PLAYER_ID;
    return 1;
}

stock bool:Ship_Create(Float:x, Float:y, Float:z, Float:angle, model)
{
    if (!Ship_ValidModel(model) || !Ship_ValidCoord(x, y, z)) return false;
    new id = CreateObject(model, x, y, z, 0.0, 0.0, angle, 350.0);
    if (id == INVALID_OBJECT_ID) return false;
    gShipRoot = id;
    gShipModel = model;
    return true;
}

stock bool:Ship_AddPart(model, Float:ox, Float:oy, Float:oz,
    Float:rx, Float:ry, Float:rz)
{
    if (!Ship_Ready() || gShipPartCount >= SHIP_MAX_PARTS ||
        !Ship_ValidModel(model) || floatabs(ox) > 150.0 ||
        floatabs(oy) > 150.0 || floatabs(oz) > 60.0) return false;

    new Float:x, Float:y, Float:z;
    GetObjectPos(gShipRoot, x, y, z);
    new id = CreateObject(model, x, y, z, 0.0, 0.0, 0.0, 350.0);
    if (id == INVALID_OBJECT_ID) return false;
    if (!AttachObjectToObject(id, gShipRoot, ox, oy, oz, rx, ry, rz, true))
    {
        DestroyObject(id);
        return false;
    }
    new slot = gShipPartCount++;
    gShipPartObject[slot] = id;
    gShipPartModel[slot] = model;
    gShipPartLocal[slot][0] = ox;
    gShipPartLocal[slot][1] = oy;
    gShipPartLocal[slot][2] = oz;
    gShipPartLocal[slot][3] = rx;
    gShipPartLocal[slot][4] = ry;
    gShipPartLocal[slot][5] = rz;
    return true;
}

stock bool:Ship_Mark()
{
    if (!Ship_Ready() || gShipPointCount >= SHIP_MAX_POINTS) return false;
    new Float:x, Float:y, Float:z, Float:rx, Float:ry, Float:angle;
    GetObjectPos(gShipRoot, x, y, z);
    GetObjectRot(gShipRoot, rx, ry, angle);
    if (!Ship_ValidCoord(x, y, z)) return false;
    new slot = gShipPointCount++;
    gShipPoint[slot][0] = x;
    gShipPoint[slot][1] = y;
    gShipPoint[slot][2] = z;
    gShipPoint[slot][3] = angle;
    return true;
}

// File is first written to .tmp and published by rename. Existing revision -> .bak.
stock bool:Ship_Save()
{
    if (!Ship_Ready()) return false;
    new Float:x, Float:y, Float:z, Float:rx, Float:ry, Float:rz;
    GetObjectPos(gShipRoot, x, y, z);
    GetObjectRot(gShipRoot, rx, ry, rz);
    if (!Ship_ValidCoord(x, y, z)) return false;

    new temp[80], backup[80], line[160];
    format(temp, sizeof(temp), "%s.tmp", SHIP_DATA);
    format(backup, sizeof(backup), "%s.bak", SHIP_DATA);
    if (fexist(temp) && !fremove(temp)) return false;
    new File:file = fopen(temp, io_write);
    if (!file) return false;
    new bool:ok = fwrite(file, "TARKENSO_SHIP|1\n") > 0;
    format(line, sizeof(line), "%d %.5f %.5f %.5f %.5f %d %d\n",
        gShipModel, x, y, z, rz, gShipPartCount, gShipPointCount);
    if (fwrite(file, line) <= 0) ok = false;
    for (new i = 0; i < gShipPartCount; i++)
    {
        format(line, sizeof(line), "%d %.5f %.5f %.5f %.5f %.5f %.5f\n",
            gShipPartModel[i], gShipPartLocal[i][0], gShipPartLocal[i][1],
            gShipPartLocal[i][2], gShipPartLocal[i][3], gShipPartLocal[i][4],
            gShipPartLocal[i][5]);
        if (fwrite(file, line) <= 0) ok = false;
    }
    for (new i = 0; i < gShipPointCount; i++)
    {
        format(line, sizeof(line), "%.5f %.5f %.5f %.5f\n",
            gShipPoint[i][0], gShipPoint[i][1],
            gShipPoint[i][2], gShipPoint[i][3]);
        if (fwrite(file, line) <= 0) ok = false;
    }
    fclose(file);
    if (!ok) { fremove(temp); return false; }
    if (fexist(SHIP_DATA))
    {
        if (fexist(backup) && !fremove(backup)) { fremove(temp); return false; }
        if (!frename(SHIP_DATA, backup)) { fremove(temp); return false; }
    }
    if (!frename(temp, SHIP_DATA))
    {
        if (fexist(backup) && !fexist(SHIP_DATA)) frename(backup, SHIP_DATA);
        fremove(temp);
        return false;
    }
    return true;
}

// Loads only into an empty slot, preventing partial file reads from replacing
// an already active ship. On any invalid record the incomplete ship is removed.
stock bool:Ship_Load()
{
    if (Ship_Ready() || !fexist(SHIP_DATA)) return false;
    new File:file = fopen(SHIP_DATA, io_read);
    if (!file) return false;
    new line[160], header[48], extra[4];
    new model, partCount, pointCount;
    new Float:x, Float:y, Float:z, Float:angle;
    new bool:ok = fread(file, header, sizeof(header)) != 0;
    Auth_StripLine(header);
    if (strcmp(header, "TARKENSO_SHIP|1", false)) ok = false;
    if (ok && !fread(file, line, sizeof(line))) ok = false;
    if (ok && sscanf(line, "iffffii", model, x, y, z, angle, partCount, pointCount))
        ok = false;
    if (ok && (!Ship_ValidModel(model) || !Ship_ValidCoord(x, y, z) ||
        partCount < 0 || partCount > SHIP_MAX_PARTS ||
        pointCount < 0 || pointCount > SHIP_MAX_POINTS)) ok = false;
    if (ok) ok = Ship_Create(x, y, z, angle, model);
    for (new i = 0; ok && i < partCount; i++)
    {
        new piece;
        new Float:ox, Float:oy, Float:oz, Float:rx, Float:ry, Float:rz;
        if (!fread(file, line, sizeof(line)) ||
            sscanf(line, "iffffff", piece, ox, oy, oz, rx, ry, rz) ||
            !Ship_AddPart(piece, ox, oy, oz, rx, ry, rz)) ok = false;
    }
    for (new i = 0; ok && i < pointCount; i++)
    {
        new Float:px, Float:py, Float:pz, Float:heading;
        if (!fread(file, line, sizeof(line)) ||
            sscanf(line, "ffff", px, py, pz, heading) ||
            !Ship_ValidCoord(px, py, pz)) { ok = false; continue; }
        new slot = gShipPointCount++;
        gShipPoint[slot][0] = px;
        gShipPoint[slot][1] = py;
        gShipPoint[slot][2] = pz;
        gShipPoint[slot][3] = heading;
    }
    if (ok && fread(file, extra, sizeof(extra))) ok = false;
    fclose(file);
    if (!ok) Ship_Reset();
    return ok;
}

stock bool:Ship_Export()
{
    if (!Ship_Ready() || gShipPointCount < 2) return false;
    new File:file = fopen(SHIP_EXPORT, io_write);
    if (!file) return false;
    new bool:ok = fwrite(file, "// Generated by Tarkenso RPG Ship Studio. Review before including.\n") > 0;
    new line[160];
    if (fwrite(file, "new const Float:gCargoShipRoute[][4] = {\n") <= 0) ok = false;
    for (new i = 0; i < gShipPointCount; i++)
    {
        format(line, sizeof(line), "    {%.5f, %.5f, %.5f, %.5f}%s\n",
            gShipPoint[i][0], gShipPoint[i][1], gShipPoint[i][2],
            gShipPoint[i][3], i == gShipPointCount - 1 ? ("") : (","));
        if (fwrite(file, line) <= 0) ok = false;
    }
    if (fwrite(file, "};\n") <= 0) ok = false;
    fclose(file);
    if (!ok) fremove(SHIP_EXPORT);
    return ok;
}

stock Ship_Advance()
{
    if (!gShipRunning || !Ship_Ready()) return 0;
    while (gShipNextPoint < gShipPointCount)
    {
        new i = gShipNextPoint++;
        new Float:x, Float:y, Float:z;
        GetObjectPos(gShipRoot, x, y, z);
        new Float:dx = x - gShipPoint[i][0];
        new Float:dy = y - gShipPoint[i][1];
        new Float:dz = z - gShipPoint[i][2];
        if (dx * dx + dy * dy + dz * dz < 0.16)
        {
            SetObjectRot(gShipRoot, 0.0, 0.0, gShipPoint[i][3]);
            continue;
        }
        MoveObject(gShipRoot, gShipPoint[i][0], gShipPoint[i][1],
            gShipPoint[i][2], SHIP_CRUISE_SPEED,
            0.0, 0.0, gShipPoint[i][3]);
        return 1;
    }
    gShipRunning = false;
    return 1;
}

public Ship_Tick()
{
    if (gShipPortDeparture >= 0) return ShipPort_Tick();
    if (!gShipRunning || !Ship_Ready()) return 1;
    if (gShipNextPoint < 1) return Ship_Advance();
    new i = gShipNextPoint - 1;
    new Float:x, Float:y, Float:z;
    GetObjectPos(gShipRoot, x, y, z);
    new Float:dx = x - gShipPoint[i][0];
    new Float:dy = y - gShipPoint[i][1];
    new Float:dz = z - gShipPoint[i][2];
    if (dx * dx + dy * dy + dz * dz <= 0.16) Ship_Advance();
    return 1;
}

// SHIP-ASSEMBLY-01: reconstructed GTA SA San Fierro cargo ship at the LS start.
// IPL origin: hull 9585 @ -2409.84,1544.95,7.0000, zero rotation.
// Parts preserve relative *original* object world offsets; rotating only the root
// applies those offsets in its local frame. Do not add extra free-standing containers
// to already modelled freight box groups: that would cause clipping/overdraw.
// The 9585 model is about 230 units long. Dock clearance is unverified.
stock bool:Ship_CreateCargoAssembly()
{
    if (Ship_Ready()) return false;
    if (!Ship_Create(SHIP_CARGO_START_X, SHIP_CARGO_START_Y,
        SHIP_CARGO_ROOT_Z, 330.95, SHIP_CARGO_ROOT_ID)) return false;
    // deck: original IPL world (-2412.12, 1544.95, 17.05)
    if (!Ship_AddPart(9586, -2.2800, 0.0000,
        10.0500, 0.0, 0.0, 0.0))
    {
        Ship_Reset();
        return false;
    }
    // upper structure alpha: original IPL world (-2411.39, 1544.95, 27.0781)
    if (!Ship_AddPart(9761, -1.5500, 0.0000,
        20.0781, 0.0, 0.0, 0.0))
    {
        Ship_Reset();
        return false;
    }
    // stern superstructure: original IPL world (-2485.08, 1544.95, 26.1953)
    if (!Ship_AddPart(9584, -75.2400, 0.0000,
        19.1953, 0.0, 0.0, 0.0))
    {
        Ship_Reset();
        return false;
    }
    // internal section: original IPL world (-2403.51, 1544.95, 8.7188)
    if (!Ship_AddPart(9590, 6.3300, 0.0000,
        1.7188, 0.0, 0.0, 0.0))
    {
        Ship_Reset();
        return false;
    }
    // cargo container group 1: original IPL world (-2401.14, 1544.94, 23.5938)
    if (!Ship_AddPart(9587, 8.7000, -0.0100,
        16.5938, 0.0, 0.0, 0.0))
    {
        Ship_Reset();
        return false;
    }
    // inside cargo: original IPL world (-2404.21, 1544.94, 7.5859)
    if (!Ship_AddPart(9588, 5.6300, -0.0100,
        0.5859, 0.0, 0.0, 0.0))
    {
        Ship_Reset();
        return false;
    }
    // cargo container group 2: original IPL world (-2389.4, 1544.88, 4.03)
    if (!Ship_AddPart(9589, 20.4400, -0.0700,
        -2.9700, 0.0, 0.0, 0.0))
    {
        Ship_Reset();
        return false;
    }
    // cargo container group 3: original IPL world (-2392.59, 1545.91, 24.8594)
    if (!Ship_AddPart(9604, 17.2500, 0.9600,
        17.8594, 0.0, 0.0, 0.0))
    {
        Ship_Reset();
        return false;
    }
    // wheelhouse platform: original IPL world (-2473.59, 1543.77, 29.0781)
    if (!Ship_AddPart(9698, -63.7500, -1.1800,
        22.0781, 0.0, 0.0, 0.0))
    {
        Ship_Reset();
        return false;
    }
    // bridge detail 02: original IPL world (-2470.45, 1551.12, 33.1406)
    if (!Ship_AddPart(9819, -60.6100, 6.1700,
        26.1406, 0.0, 0.0, 0.0))
    {
        Ship_Reset();
        return false;
    }
    // bridge detail 03: original IPL world (-2474.36, 1547.24, 24.75)
    if (!Ship_AddPart(9821, -64.5200, 2.2900,
        17.7500, 0.0, 0.0, 0.0))
    {
        Ship_Reset();
        return false;
    }
    // bridge detail 08: original IPL world (-2470.94, 1550.75, 32.9062)
    if (!Ship_AddPart(9822, -61.1000, 5.8000,
        25.9062, 0.0, 0.0, 0.0))
    {
        Ship_Reset();
        return false;
    }
    return true;
}

// SHIP-ROUTE-01: provisional LS Ocean Docks -> SF Easter Basin approach.
// Verified only against overall GTA SA map / known port areas, NOT in-game.
// The terminal is an offshore holding point, NOT a pier berthing position.
// Do NOT run without checking object-envelope clearance, sea bounds and bridges.
// LS south coast v2: add an eastward offshore arc and shift southern sea lanes
// south by 15..55 units where the 3000-unit map limit permits it.
// NOT a collision-certified corridor; even a 230-unit hull may cross map bounds.
new const Float:gShipPresetLsSf[][4] = {
    {2640.0, -2740.0, 0.0, 240.95},
    {2730.0, -2790.0, 0.0, 233.13},
    {2810.0, -2850.0, 0.0, 195.26},
    {2825.0, -2905.0, 0.0, 131.63},
    {2780.0, -2945.0, 0.0, 106.26},
    {2660.0, -2980.0, 0.0, 91.59},
    {2480.0, -2985.0, 0.0, 90.00},
    {2100.0, -2985.0, 0.0, 90.00},
    {1580.0, -2985.0, 0.0, 90.00},
    {980.0, -2985.0, 0.0, 90.00},
    {380.0, -2985.0, 0.0, 90.00},
    {-220.0, -2985.0, 0.0, 90.00},
    {-820.0, -2985.0, 0.0, 89.09},
    {-1450.0, -2975.0, 0.0, 86.42},
    {-2090.0, -2935.0, 0.0, 75.43},
    {-2590.0, -2805.0, 0.0, 38.20},
    {-2830.0, -2500.0, 0.0, 13.63},
    {-2910.0, -2170.0, 0.0, 0.99},
    {-2920.0, -1590.0, 0.0, 0.00},
    {-2920.0, -1010.0, 0.0, 0.00},
    {-2920.0, -430.0, 0.0, 0.00},
    {-2920.0, 150.0, 0.0, 0.00},
    {-2920.0, 730.0, 0.0, 359.03},
    {-2910.0, 1320.0, 0.0, 354.05},
    {-2860.0, 1800.0, 0.0, 335.70},
    {-2720.0, 2110.0, 0.0, 292.62},
    {-2480.0, 2210.0, 0.0, 272.86},
    {-2080.0, 2230.0, 0.0, 264.43},
    {-1670.0, 2190.0, 0.0, 244.80},
    {-1330.0, 2030.0, 0.0, 223.73},
    {-1110.0, 1800.0, 0.0, 202.17},
    {-1000.0, 1530.0, 0.0, 190.12},
    {-950.0, 1250.0, 0.0, 180.00},
    {-950.0, 990.0, 0.0, 167.47},
    {-990.0, 810.0, 0.0, 137.73},
    {-1090.0, 700.0, 0.0, 137.73}
};

stock bool:Ship_SetRoutePreset(bool:reverse)
{
    new count = sizeof(gShipPresetLsSf);
    if (count > SHIP_MAX_POINTS) return false;
    gShipPointCount = count;
    for (new i = 0; i < count; i++)
    {
        new j = reverse ? count - 1 - i : i;
        for (new k = 0; k < 3; k++)
            gShipPoint[i][k] = gShipPresetLsSf[j][k];
        if (gShipModel == SHIP_CARGO_ROOT_ID)
            gShipPoint[i][2] = SHIP_CARGO_ROOT_Z;
        // Reverse heading must face the next target, not the previous direction.
        if (reverse && j > 0)
        {
            gShipPoint[i][3] = gShipPresetLsSf[j - 1][3] + 180.0;
            if (gShipPoint[i][3] >= 360.0)
                gShipPoint[i][3] -= 360.0;
        }
        else if (reverse)
            gShipPoint[i][3] = gShipPoint[i-1][3];
        else
            gShipPoint[i][3] = gShipPresetLsSf[j][3];
    }
    if (gShipModel == SHIP_CARGO_ROOT_ID)
    {
        for (new i = 0; i < count; i++)
        {
            gShipPoint[i][3] += SHIP_CARGO_HEADING_OFFSET;
            if (gShipPoint[i][3] >= 360.0)
                gShipPoint[i][3] -= 360.0;
        }
    }
    gShipNextPoint = 0;

    return true;
}

stock Ship_Preset(playerid, bool:reverse)
{
    if (!Ship_Ready())
        return SendClientMessage(playerid, ADMIN_ERROR,
            "[Tarkenso RPG] Сначала создайте или загрузите корабль.");
    if (gShipRunning)
        return SendClientMessage(playerid, ADMIN_ERROR,
            "[Tarkenso RPG] Остановите автопилот командой /ship stop.");
    if (!Ship_SetRoutePreset(reverse))
        return SendClientMessage(playerid, ADMIN_ERROR,
            "[Tarkenso RPG] Ошибка загрузки точек маршрута.");
    // Keep the command-level replay cursor reset explicit for existing contracts.
    gShipNextPoint = 0;
    SendClientMessage(playerid, SHIP_COLOR,
        reverse ? "Маршрут SF -> LS загружен как черновик." :
                  "Маршрут LS -> SF загружен как черновик.");
    SendClientMessage(playerid, SHIP_COLOR,
        "ВНИМАНИЕ: точки НЕ проверены по коллизиям; берег/мосты могут мешать.");
    SendClientMessage(playerid, SHIP_COLOR,
        "Сначала /ship export и проверка карты. /ship run стартует с первой точки.");
    return Ship_Status(playerid);
}

stock Ship_Help(playerid)
{
    SendClientMessage(playerid, SHIP_COLOR,
        "[Tarkenso RPG] Ship Studio: /ship cargo — создать грузовой корабль; /ship create [hull model].");
    SendClientMessage(playerid, SHIP_COLOR,
        "[Tarkenso RPG] /ship add [model dx dy dz rx ry rz] — добавить деталь к корпусу.");
    SendClientMessage(playerid, SHIP_COLOR,
        "/ship move [meters], /ship turn [degrees], /ship set [x y z angle], /ship mark.");
    SendClientMessage(playerid, SHIP_COLOR,
        "/ship run, /ship stop, /ship undo, /ship save, /ship load, /ship export.");
    SendClientMessage(playerid, SHIP_COLOR,
        "/ship preset lssf|sfls — готовый черновик морского маршрута (без проверки коллизий).");
    SendClientMessage(playerid, SHIP_COLOR,
        "[Tarkenso RPG] /ship edit — вид сверху, управление мышью, сохранение причалов.");
    SendClientMessage(playerid, SHIP_COLOR,
        "[Tarkenso RPG] /ship port ls|sf berth|exit|depart — портовая настройка и отход.");
    SendClientMessage(playerid, SHIP_COLOR,
        "/ship status, /ship release, /ship clear. EDITOR ONLY; collision is NOT checked.");
    return 1;
}

stock Ship_Status(playerid)
{
    new line[144], Float:x, Float:y, Float:z;
    if (!Ship_Ready()) return SendClientMessage(playerid, SHIP_COLOR,
        "Ship Studio: корабль не загружен. /ship create [model] или /ship load.");
    GetObjectPos(gShipRoot, x, y, z);
    format(line, sizeof(line), "Ship | hull %d | parts %d/%d | route %d/%d | %s",
        gShipModel, gShipPartCount, SHIP_MAX_PARTS, gShipPointCount,
        SHIP_MAX_POINTS, gShipRunning ? ("AUTO") : ("STOP"));
    SendClientMessage(playerid, SHIP_COLOR, line);
    format(line, sizeof(line), "Position: %.2f %.2f %.2f | editor ID %d",
        x, y, z, gShipOwner);
    SendClientMessage(playerid, SHIP_COLOR, line);
    return 1;
}

stock bool:Ship_Lock(playerid)
{
    if (gShipOwner != INVALID_PLAYER_ID && gShipOwner != playerid)
    {
        SendClientMessage(playerid, ADMIN_ERROR, "Ship Studio занят другим разработчиком.");
        return false;
    }
    gShipOwner = playerid;
    return true;
}

stock Ship_Command(playerid, const params[])
{
    if (!Studio_Require(playerid)) return 1;
    if (isnull(params)) return Ship_Help(playerid);
    new action[24];
    if (sscanf(params, "s[24]", action)) return Ship_Help(playerid);
    if (!strcmp(action, "help", true)) return Ship_Help(playerid);
    if (!strcmp(action, "status", true)) return Ship_Status(playerid);
    if (!strcmp(action, "edit", true))
    {
        new editParams[32];
        if (strlen(params) <= 5) return ShipEditor_Command(playerid, "");
        strmid(editParams, params, 5, strlen(params), sizeof(editParams));
        return ShipEditor_Command(playerid, editParams);
    }
    if (gShipEditOwner != INVALID_PLAYER_ID)
        return SendClientMessage(playerid, ADMIN_ERROR,
            "[Tarkenso RPG] Позиционирование активно. Используйте /ship edit save|cancel.");
    if (!strcmp(action, "load", true))
    {
        if (!Ship_Lock(playerid)) return 1;
        if (!Ship_Load()) return SendClientMessage(playerid, ADMIN_ERROR,
            "Загрузка не удалась. Очистите корабль или проверьте cargo_01.dat.");
        return Ship_Status(playerid);
    }
    if (!strcmp(action, "release", true))
    {
        if (gShipOwner != playerid) return 1;
        Ship_Stop();
        gShipOwner = INVALID_PLAYER_ID;
        return SendClientMessage(playerid, SHIP_COLOR, "Ship Studio: блокировка снята.");
    }
    if (!Ship_Lock(playerid)) return 1;
    if (!strcmp(action, "cargo", true))
    {
        if (Ship_Ready()) return SendClientMessage(playerid, ADMIN_ERROR,
            "[Tarkenso RPG] Корабль уже существует. Для замены сохраните и выполните /ship clear.");
        if (!Ship_CreateCargoAssembly())
            return SendClientMessage(playerid, ADMIN_ERROR,
                "[Tarkenso RPG] Не удалось собрать грузовой корабль: проверьте лимит объектов.");
        if (!Ship_SetRoutePreset(false))
        {
            Ship_Reset();
            return SendClientMessage(playerid, ADMIN_ERROR,
                "[Tarkenso RPG] Ошибка установки маршрута для сборки.");
        }
        SendClientMessage(playerid, SHIP_COLOR,
            "[Tarkenso RPG] Грузовой корабль собран! Маршрут LS -> SF задан, рейс НЕ запущен.");
        return Ship_Status(playerid);
    }
    if (!strcmp(action, "port", true))
    {
        new portParams[64];
        if (strlen(params) <= 5) return ShipPort_Command(playerid, "");
        strmid(portParams, params, 5, strlen(params), sizeof(portParams));
        return ShipPort_Command(playerid, portParams);
    }
    if (!strcmp(action, "create", true))
    {
        new model;
        if (sscanf(params, "s[24]i", action, model) || !Ship_ValidModel(model))
            return SendClientMessage(playerid, ADMIN_ERROR,
                "Usage: /ship create [hull model ID].");
        if (Ship_Ready()) return SendClientMessage(playerid, ADMIN_ERROR,
            "Сначала /ship save и /ship clear. На этапе 1 поддерживается одно судно.");
        if (GetPlayerVirtualWorld(playerid) != 0 || GetPlayerInterior(playerid) != 0)
            return SendClientMessage(playerid, ADMIN_ERROR, "Создание только в основном мире.");
        new Float:x, Float:y, Float:z, Float:a;
        GetPlayerPos(playerid, x, y, z);
        GetPlayerFacingAngle(playerid, a);
        if (!Ship_Create(x, y, z, a, model))
            return SendClientMessage(playerid, ADMIN_ERROR, "Создание объекта не удалось.");
        return Ship_Status(playerid);
    }
    if (!strcmp(action, "clear", true))
    {
        Ship_Reset();
        return SendClientMessage(playerid, SHIP_COLOR,
            "Корабль удалён из памяти. Файл сохранения не затронут.");
    }
    if (!strcmp(action, "preset", true))
    {
        new direction[16];
        if (sscanf(params, "s[24]s[16]", action, direction))
            return SendClientMessage(playerid, ADMIN_ERROR,
                "Использование: /ship preset lssf|sfls.");
        if (!strcmp(direction, "lssf", true)) return Ship_Preset(playerid, false);
        if (!strcmp(direction, "sfls", true)) return Ship_Preset(playerid, true);
        return SendClientMessage(playerid, ADMIN_ERROR,
            "Направление: lssf (LS -> SF) или sfls (SF -> LS).");
    }
    if (!Ship_Ready()) return SendClientMessage(playerid, ADMIN_ERROR,
        "Нет корабля. /ship create [model] или /ship load.");
    if (!strcmp(action, "stop", true))
    {
        Ship_Stop();
        return SendClientMessage(playerid, SHIP_COLOR, "Корабль остановлен.");
    }
    if (!strcmp(action, "save", true))
    {
        if (!Ship_Save()) return SendClientMessage(playerid, ADMIN_ERROR,
            "Не удалось сохранить cargo_01.dat.");
        return SendClientMessage(playerid, SHIP_COLOR, "Сохранено: shipstudio/cargo_01.dat.");
    }
    if (!strcmp(action, "export", true))
    {
        if (!Ship_Export()) return SendClientMessage(playerid, ADMIN_ERROR,
            "Экспорт не удался: нужно минимум две точки маршрута.");
        return SendClientMessage(playerid, SHIP_COLOR,
            "Экспорт: shipstudio/cargo_01_route.inc.");
    }
    if (!strcmp(action, "add", true))
    {
        new model;
        new Float:dx, Float:dy, Float:dz, Float:rx, Float:ry, Float:rz;
        if (sscanf(params, "s[24]iffffff", action, model, dx, dy, dz, rx, ry, rz))
            return SendClientMessage(playerid, ADMIN_ERROR,
                "Usage: /ship add [model dx dy dz rx ry rz].");
        if (gShipRunning || !Ship_AddPart(model, dx, dy, dz, rx, ry, rz))
            return SendClientMessage(playerid, ADMIN_ERROR,
                "Не удалось добавить часть (модель, лимит или движение).");
        return Ship_Status(playerid);
    }
    if (!strcmp(action, "move", true))
    {
        new Float:meters, Float:x, Float:y, Float:z, Float:rx, Float:ry, Float:angle;
        if (sscanf(params, "s[24]f", action, meters) || floatabs(meters) > 100.0 ||
            floatabs(meters) < 0.01)
            return SendClientMessage(playerid, ADMIN_ERROR, "Usage: /ship move [-100..100].");
        if (gShipRunning) return SendClientMessage(playerid, ADMIN_ERROR,
            "Остановите автопилот: /ship stop.");
        StopObject(gShipRoot);
        GetObjectPos(gShipRoot, x, y, z);
        GetObjectRot(gShipRoot, rx, ry, angle);
        // Use the model's longitudinal heading, not its mesh yaw.
        if (gShipModel == SHIP_CARGO_ROOT_ID)
            angle -= SHIP_CARGO_HEADING_OFFSET;
        new Float:tx = x - floatsin(angle, degrees) * meters;
        new Float:ty = y + floatcos(angle, degrees) * meters;
        if (!Ship_ValidCoord(tx, ty, z))
            return SendClientMessage(playerid, ADMIN_ERROR, "За пределами карты.");
        MoveObject(gShipRoot, tx, ty, z, SHIP_CRUISE_SPEED);
        return SendClientMessage(playerid, SHIP_COLOR,
            "Ручной ход начат. /ship stop — остановка; /ship mark — запись.");
    }
    if (!strcmp(action, "turn", true))
    {
        new Float:degreesValue, Float:rx, Float:ry, Float:angle;
        if (sscanf(params, "s[24]f", action, degreesValue) ||
            floatabs(degreesValue) > 180.0)
            return SendClientMessage(playerid, ADMIN_ERROR, "Usage: /ship turn [-180..180].");
        if (gShipRunning) return SendClientMessage(playerid, ADMIN_ERROR,
            "Остановите автопилот.");
        StopObject(gShipRoot);
        GetObjectRot(gShipRoot, rx, ry, angle);
        SetObjectRot(gShipRoot, 0.0, 0.0, angle + degreesValue);
        return Ship_Status(playerid);
    }
    if (!strcmp(action, "set", true))
    {
        new Float:x, Float:y, Float:z, Float:angle;
        if (sscanf(params, "s[24]ffff", action, x, y, z, angle) ||
            !Ship_ValidCoord(x, y, z))
            return SendClientMessage(playerid, ADMIN_ERROR,
                "Usage: /ship set [x y z angle].");
        Ship_Stop();
        SetObjectPos(gShipRoot, x, y, z);
        SetObjectRot(gShipRoot, 0.0, 0.0, angle);
        return Ship_Status(playerid);
    }
    if (!strcmp(action, "mark", true))
    {
        if (!Ship_Mark()) return SendClientMessage(playerid, ADMIN_ERROR,
            "Не удалось записать точку (предел 80 или координаты).");
        new line[72];
        format(line, sizeof(line), "Записана точка маршрута #%d.", gShipPointCount);
        return SendClientMessage(playerid, SHIP_COLOR, line);
    }
    if (!strcmp(action, "undo", true))
    {
        if (gShipRunning || !gShipPointCount) return 1;
        gShipPointCount--;
        return SendClientMessage(playerid, SHIP_COLOR, "Последняя точка удалена.");
    }
    if (!strcmp(action, "run", true))
    {
        if (gShipPointCount < 2)
            return SendClientMessage(playerid, ADMIN_ERROR, "Запишите хотя бы две точки.");
        Ship_Stop();
        SetObjectPos(gShipRoot, gShipPoint[0][0],
            gShipPoint[0][1], gShipPoint[0][2]);
        SetObjectRot(gShipRoot, 0.0, 0.0, gShipPoint[0][3]);
        gShipNextPoint = 1;
        gShipRunning = true;
        Ship_Advance();
        return SendClientMessage(playerid, SHIP_COLOR,
            "Пробный рейс запущен. Проверяйте вручную препятствия и палубу.");
    }
    return Ship_Help(playerid);
}

stock Ship_Init()
{
    fcreatedir(SHIP_DIR);
    for (new i = 0; i < SHIP_MAX_PARTS; i++) gShipPartObject[i] = INVALID_OBJECT_ID;
    if (fexist(SHIP_DATA))
    {
        if (!Ship_Load())
            print("[Ship Studio] Saved ship failed to load; auto cargo spawn blocked.");
    }
    else
    {
        if (!Ship_CreateCargoAssembly() || !Ship_SetRoutePreset(false))
        {
            Ship_Reset();
            print("[Ship Studio] Default cargo ship creation FAILED.");
        }
        else
            print("[Ship Studio] GTA SA freighter assembled at LS route start.");
    }
    gShipTimer = SetTimer("Ship_Tick", 250, true);
    print("[Ship Studio] Stage 1 initialized.");
    return 1;
}

stock Ship_OnDisconnect(playerid)
{
    if (gShipOwner == playerid)
    {
        Ship_Stop();
        gShipOwner = INVALID_PLAYER_ID;
    }
    return 1;
}

stock Ship_Shutdown()
{
    if (gShipTimer) KillTimer(gShipTimer);
    gShipTimer = 0;
    Ship_Reset();
    return 1;
}
