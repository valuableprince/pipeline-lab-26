#if defined _tarkenso_ship_sf_pier_map_included
    #endinput
#endif
#define _tarkenso_ship_sf_pier_map_included

// SHIP-MAP-01 / #126. Local scene; NO surveyed SF origin has been supplied.
// Override these BEFORE including this file. See docs/SHIP_SF_PIER_MAP_01.md.
// 0 = off, 1 = test-server preview, 2 = surveyed normal startup.
#if !defined SF_PIER_MAP_MODE
    #define SF_PIER_MAP_MODE (0)
#endif
#if !defined SF_PIER_SITE_CONFIGURED
    #define SF_PIER_SITE_CONFIGURED (0)
#endif
#if !defined SF_PIER_SITE_VERIFIED
    #define SF_PIER_SITE_VERIFIED (0)
#endif
// A configuration flag alone must not silently select the zero fallback origin.
#if SF_PIER_MAP_MODE > 0 && SF_PIER_SITE_CONFIGURED
    #if !defined SF_PIER_BASE_X || !defined SF_PIER_BASE_Y || !defined SF_PIER_BASE_Z || !defined SF_PIER_BASE_ROT
        #error Explicit SF_PIER_BASE_X/Y/Z/ROT are required for preview or live mode.
    #endif
#endif
#if !defined SF_PIER_BASE_X
    #define SF_PIER_BASE_X (0.0)
#endif
#if !defined SF_PIER_BASE_Y
    #define SF_PIER_BASE_Y (0.0)
#endif
#if !defined SF_PIER_BASE_Z
    #define SF_PIER_BASE_Z (0.0)
#endif
#if !defined SF_PIER_BASE_ROT
    #define SF_PIER_BASE_ROT (0.0)
#endif
#if SF_PIER_MAP_MODE < 0 || SF_PIER_MAP_MODE > 2
    #error Invalid SF_PIER_MAP_MODE (use 0, 1 or 2).
#endif
#if SF_PIER_MAP_MODE > 0 && !SF_PIER_SITE_CONFIGURED
    #error Configure SF_PIER_BASE_X/Y/Z/ROT and SF_PIER_SITE_CONFIGURED first.
#endif
#if SF_PIER_MAP_MODE == 2 && !SF_PIER_SITE_VERIFIED
    #error NEEDS_IN_GAME_SURVEY: normal startup requires SF_PIER_SITE_VERIFIED.
#endif

#define SF_PIER_MAX_OBJECTS       (512)
#define SF_PIER_STREAM_DISTANCE   (450.0)
#define SF_PIER_DRAW_DISTANCE     (350.0)
#define SF_PIER_WORLD             (0)
#define SF_PIER_INTERIOR          (0)
#define SF_PIER_TILE              (9.0)
#define SF_PIER_COLUMNS           (32)
#define SF_PIER_ROWS              (7)
#define SF_PIER_APPROACH_COLUMNS  (3)
#if !defined SF_PIER_APPROACH_ROWS
    #define SF_PIER_APPROACH_ROWS (8)
#endif
#define SF_PIER_LENGTH           (288.0)
#define SF_PIER_WIDTH            (63.0)
#define SF_PIER_EXPORT_DIR       "ship_sf_pier"

// Bounds relative to the ORIGINAL object pivot, not half mesh sizes.
// Crayder/Model-Sizes-Plus @ 0f5efe7d85b10bcd8b12e10143b446ec0c87c388
// (ColAndreas export); checked against Prineside/CIT model names and pictures.
// Mesh bounds differ slightly from collision bounds, especially on 3620/1278.
// Exception: non-colliding 3076 uses measured visual DFF bounds (GTA Stuff).
enum E_SFPM_MODEL
{
    sfmID,
    Float:sfmMinX, Float:sfmMinY, Float:sfmMinZ,
    Float:sfmMaxX, Float:sfmMaxY, Float:sfmMaxZ
};
enum
{
    SFPM_DECK, SFPM_CRANE, SFPM_CONTAINER_A, SFPM_CONTAINER_B,
    SFPM_CONTAINER_C, SFPM_HUT, SFPM_LIGHT, SFPM_BOLLARD,
    SFPM_FENDER, SFPM_BARRIER, SFPM_MARKING
};
new const gSFPierModels[][E_SFPM_MODEL] = {
    {3095, -4.492614, -4.494384, 0.003365, 4.507385, 4.505615, 0.555365},
    {3620, -3.422485, -15.195312, -13.095438, 3.405151, 16.238769, 12.500000},
    {3570, -4.029794, -1.299793, -1.347940, 4.057408, 1.291645, 1.349384},
    {3571, -4.029794, -1.303699, -1.347940, 4.057408, 1.287739, 1.349384},
    {3572, -4.029794, -1.304431, -1.347940, 4.057409, 1.287007, 1.349384},
    {3626, -4.494343, -2.406250, -1.570312, 4.531941, 2.428727, 1.578098},
    {1278, -0.308859, -0.294371, -14.144999, 0.491140, 0.505628, 14.015000},
    {1214, -0.287642, -0.288119, 0.007829, 0.312888, 0.312411, 1.359167},
    {1327, -0.665036, -0.935439, -0.945221, 0.034967, 0.944570, 0.943306},
    {3578, -5.148437, -0.632812, -0.781250, 5.156250, 0.648437, 0.781250},
    {3076, -8.156521, -38.073689, -0.007813, 8.167217, 38.079441, 0.008524}
};

enum E_SFPM_OBJECT
{
    sfpModelSlot,
    Float:sfpX, Float:sfpY, Float:sfpZ,
    Float:sfpRX, Float:sfpRZ,
    sfpMaterial // 0 = original; 1 = concrete
};
new gSFPierPlan[SF_PIER_MAX_OBJECTS][E_SFPM_OBJECT];
new gSFPierPlanCount;
new gSFPierObjects[SF_PIER_MAX_OBJECTS];
new gSFPierObjectCount;

// XY rotation is a mesh yaw: 0 = local +X east, local +Y towards the water.
stock ShipSFPierMap_World(Float:lx, Float:ly, Float:lz,
    &Float:x, &Float:y, &Float:z)
{
    new Float:c = floatcos(SF_PIER_BASE_ROT, degrees);
    new Float:s = floatsin(SF_PIER_BASE_ROT, degrees);
    x = SF_PIER_BASE_X + lx * c - ly * s;
    y = SF_PIER_BASE_Y + lx * s + ly * c;
    z = SF_PIER_BASE_Z + lz;
    return 1;
}

// Place the bounds centre at x/y/z, correcting the asymmetric pivot.
// Only flat slabs, vertical slab fascias (RX=90) and upright props are used.
stock bool:SFPM_Centre(slot, Float:x, Float:y, Float:z,
    Float:rx = 0.0, Float:rz = 0.0, material = 0)
{
    if (slot < 0 || slot >= sizeof(gSFPierModels) ||
        gSFPierPlanCount >= SF_PIER_MAX_OBJECTS) return false;
    new Float:cx = (gSFPierModels[slot][sfmMinX] + gSFPierModels[slot][sfmMaxX]) * 0.5;
    new Float:cy = (gSFPierModels[slot][sfmMinY] + gSFPierModels[slot][sfmMaxY]) * 0.5;
    new Float:cz = (gSFPierModels[slot][sfmMinZ] + gSFPierModels[slot][sfmMaxZ]) * 0.5;
    new Float:oy = cy * floatcos(rx, degrees) - cz * floatsin(rx, degrees);
    new Float:oz = cy * floatsin(rx, degrees) + cz * floatcos(rx, degrees);
    new i = gSFPierPlanCount++;
    gSFPierPlan[i][sfpModelSlot] = slot;
    gSFPierPlan[i][sfpX] = x - cx * floatcos(rz, degrees) + oy * floatsin(rz, degrees);
    gSFPierPlan[i][sfpY] = y - cx * floatsin(rz, degrees) - oy * floatcos(rz, degrees);
    gSFPierPlan[i][sfpZ] = z - oz;
    gSFPierPlan[i][sfpRX] = rx;
    gSFPierPlan[i][sfpRZ] = rz;
    gSFPierPlan[i][sfpMaterial] = material;
    return true;
}

stock bool:SFPM_OnFloor(slot, Float:x, Float:y, Float:bottom = 0.0, Float:rz = 0.0)
{
    new Float:height = gSFPierModels[slot][sfmMaxZ] - gSFPierModels[slot][sfmMinZ];
    return SFPM_Centre(slot, x, y, bottom + height * 0.5, 0.0, rz);
}

stock bool:SFPM_Tile(column, row)
{
    // 3095 is 9 x 9 x .552; its pivot is near the BOTTOM, not the middle.
    return SFPM_Centre(SFPM_DECK, (float(column) + 0.5) * SF_PIER_TILE,
        (float(row) + 0.5) * SF_PIER_TILE, -0.276, 0.0, 0.0, 1);
}

stock bool:SFPM_BuildBase()
{
    // Disjoint grid cells: no overlaid foundation tiles or coplanar seams.
    for (new x; x < SF_PIER_COLUMNS; x++)
        for (new y; y < SF_PIER_ROWS; y++)
            if (!SFPM_Tile(x, y)) return false;
    for (new x; x < SF_PIER_APPROACH_COLUMNS; x++)
        for (new y = -SF_PIER_APPROACH_ROWS; y < 0; y++)
            if (!SFPM_Tile(x, y)) return false;

    // Vertical 9m fascias butt against the underside of the .552m deck.
    // Side faces extend down into water; corner joints are hidden structural
    // overlaps, never overlapping coplanar driving surfaces.
    new Float:wallZ = -0.552 - 4.5;
    for (new x; x < SF_PIER_COLUMNS; x++)
    {
        new Float:cx = (float(x) + 0.5) * SF_PIER_TILE;
        if (!SFPM_Centre(SFPM_DECK, cx, SF_PIER_WIDTH - 0.276,
            wallZ, 90.0, 0.0, 1)) return false;
        // The 27m west-side entrance stays open to the shore approach.
        if (x >= SF_PIER_APPROACH_COLUMNS && !SFPM_Centre(SFPM_DECK,
            cx, 0.276, wallZ, 90.0, 0.0, 1)) return false;
    }
    for (new y = -SF_PIER_APPROACH_ROWS; y < SF_PIER_ROWS; y++)
    {
        new Float:cy = (float(y) + 0.5) * SF_PIER_TILE;
        if (!SFPM_Centre(SFPM_DECK, 0.276, cy, wallZ, 90.0, 90.0, 1)) return false;
        new Float:east = y < 0 ? 27.0 : SF_PIER_LENGTH;
        if (!SFPM_Centre(SFPM_DECK, east - 0.276, cy,
            wallZ, 90.0, 90.0, 1)) return false;
    }
    return true;
}

stock bool:SFPM_BuildCargo()
{
    new Float:containerHeight = gSFPierModels[SFPM_CONTAINER_A][sfmMaxZ] -
        gSFPierModels[SFPM_CONTAINER_A][sfmMinZ];
    // North block: 7 columns x 3 rows, two tiers; 9.9m longitudinal aisles.
    for (new x; x < 7; x++)
        for (new y; y < 3; y++)
            for (new layer; layer < 2; layer++)
                if (!SFPM_OnFloor(SFPM_CONTAINER_A + (x + y + layer) % 3,
                    54.0 + float(x) * 18.0, 42.0 + float(y) * 4.0,
                    float(layer) * containerHeight)) return false;
    // South block: 12 on the ground, six correctly supported upper boxes.
    for (new x; x < 6; x++)
        for (new y; y < 2; y++)
        {
            new slot = SFPM_CONTAINER_A + (x + y) % 3;
            new Float:cx = 54.0 + float(x) * 18.0;
            new Float:cy = 12.0 + float(y) * 4.0;
            if (!SFPM_OnFloor(slot, cx, cy)) return false;
            if (x % 2 == 0 && !SFPM_OnFloor(slot, cx, cy, containerHeight)) return false;
        }
    // Two genuine dock work huts: technical/store room and port office.
    if (!SFPM_OnFloor(SFPM_HUT, 185.0, 12.0, 0.0, 180.0)) return false;
    if (!SFPM_OnFloor(SFPM_HUT, 200.0, 12.0, 0.0, 180.0)) return false;
    // DFF inspection: the boom tip is at model -Y, the feet at +Y=8.13..16.24.
    // RZ=180 puts the boom over water and ALL feet inside the 63m deck.
    // The whole crane stays east of the 230m trial hull; still needs GTA UAT.
    return SFPM_OnFloor(SFPM_CRANE, 274.0, 51.0, 0.0, 180.0);
}

stock bool:SFPM_BuildFurniture()
{
    // Original driving-school paint geometry, no collision; 2cm above the
    // intact floor. Never use an alpha-only line texture AS the actual floor.
    if (!SFPM_OnFloor(SFPM_MARKING, 200.0, 32.0, 0.02, 90.0)) return false;
    for (new i; i < 12; i++)
    {
        // Last station moves past the crane's complete collision envelope.
        new Float:x = i == 11 ? 282.0 : 12.0 + float(i) * 24.0;
        // Paired real metal bollards: visual mooring equivalents, no ropes/API.
        if (!SFPM_OnFloor(SFPM_BOLLARD, x - 0.5, 61.8)) return false;
        if (!SFPM_OnFloor(SFPM_BOLLARD, x + 0.5, 61.8)) return false;
        // Upright rubber tyres face the water, below the top of the quay.
        if (!SFPM_OnFloor(SFPM_FENDER, x - 1.3, 63.35, -2.3, 90.0)) return false;
        if (!SFPM_OnFloor(SFPM_FENDER, x + 1.3, 63.35, -2.3, 90.0)) return false;
    }
    // Tall original floodlights; meshes/coronas must be inspected at night.
    for (new i; i < 7; i++)
        if (!SFPM_OnFloor(SFPM_LIGHT, 8.0 + float(i) * 40.0, 57.0)) return false;
    for (new i; i < 6; i++)
        if (!SFPM_OnFloor(SFPM_LIGHT, 44.0 + float(i) * 40.0, 4.0)) return false;
    if (!SFPM_OnFloor(SFPM_LIGHT, 3.0, -36.0)) return false;
    if (!SFPM_OnFloor(SFPM_LIGHT, 24.0, -36.0)) return false;
    // Black/yellow dock barriers protect landward/side edges, not the berth.
    for (new i; i < 20; i++)
        if (!SFPM_OnFloor(SFPM_BARRIER, 40.0 + float(i) * 12.0, 1.5)) return false;
    for (new i; i < 5; i++)
    {
        new Float:y = 9.0 + float(i) * 11.0;
        if (!SFPM_OnFloor(SFPM_BARRIER, 1.5, y, 0.0, 90.0)) return false;
        if (!SFPM_OnFloor(SFPM_BARRIER, 286.5, y, 0.0, 90.0)) return false;
    }
    return true;
}

stock bool:ShipSFPierMap_Build()
{
    gSFPierPlanCount = 0;
    if (SF_PIER_APPROACH_ROWS < 1 || !SFPM_BuildBase() ||
        !SFPM_BuildCargo() || !SFPM_BuildFurniture())
    {
        gSFPierPlanCount = 0;
        return false;
    }
    return true;
}

stock ShipSFPierMap_Shutdown()
{
    for (new i; i < gSFPierObjectCount; i++)
    {
        if (IsValidDynamicObject(gSFPierObjects[i]))
            DestroyDynamicObject(gSFPierObjects[i]);
        gSFPierObjects[i] = INVALID_STREAMER_ID;
    }
    gSFPierObjectCount = 0;
    return 1;
}

stock bool:SFPM_Material(objectid, material)
{
    if (material == 0) return true;
    // Real GTA SA TXD donor 9514 / 711_sfw. No custom model or texture pack.
    if (!SetDynamicObjectMaterial(objectid, 1, 9514, "711_sfw", "ws_carpark2")) return false;
    return SetDynamicObjectMaterial(objectid, 0, 9514, "711_sfw", "ws_carpark2") != 0;
}

stock bool:ShipSFPierMap_CreateGeometry()
{
    // Idempotent startup/re-entry: never allocate another copy for a player.
    if (gSFPierObjectCount > 0) return gSFPierObjectCount == gSFPierPlanCount;
    if (gSFPierPlanCount == 0) return false;
    for (new i; i < gSFPierPlanCount; i++)
    {
        new Float:x, Float:y, Float:z;
        ShipSFPierMap_World(gSFPierPlan[i][sfpX], gSFPierPlan[i][sfpY],
            gSFPierPlan[i][sfpZ], x, y, z);
        new slot = gSFPierPlan[i][sfpModelSlot];
        new objectid = CreateDynamicObject(gSFPierModels[slot][sfmID], x, y, z,
            gSFPierPlan[i][sfpRX], 0.0, SF_PIER_BASE_ROT + gSFPierPlan[i][sfpRZ],
            SF_PIER_WORLD, SF_PIER_INTERIOR, -1, SF_PIER_STREAM_DISTANCE,
            SF_PIER_DRAW_DISTANCE, -1, slot == SFPM_DECK ? 20 : 0);
        if (!IsValidDynamicObject(objectid))
        {
            ShipSFPierMap_Shutdown();
            return false;
        }
        // Own the handle BEFORE material operations, so a failed one rolls back.
        gSFPierObjects[gSFPierObjectCount++] = objectid;
        if (!SFPM_Material(objectid, gSFPierPlan[i][sfpMaterial]))
        {
            ShipSFPierMap_Shutdown();
            return false;
        }
    }
    return true;
}

stock bool:SFPM_WriteEditorHeader(File:out)
{
    if (fwrite(out, "#include <open.mp>\r\n// Map Editor import: literal world coordinates, original GTA SA objects.\r\n// Standalone test filterscript ONLY; do not load together with the pier module.\r\n#define SF_PIER_EDITOR_PREVIEW_ENABLED (0)\r\n") <= 0) return false;
    if (fwrite(out, "new gSFPierEditorObjects[512], gSFPierEditorCount;\r\nstock SFPierEditor_Clear()\r\n{\r\n    while (gSFPierEditorCount > 0)\r\n    {\r\n        new objectid = gSFPierEditorObjects[--gSFPierEditorCount];\r\n        if (IsValidObject(objectid)) DestroyObject(objectid);\r\n    }\r\n    return 1;\r\n}\r\n") <= 0) return false;
    if (fwrite(out, "stock bool:SFPierEditor_Keep(objectid)\r\n{\r\n    if (!IsValidObject(objectid) || gSFPierEditorCount >= sizeof(gSFPierEditorObjects))\r\n    {\r\n        if (IsValidObject(objectid)) DestroyObject(objectid);\r\n        SFPierEditor_Clear();\r\n        return false;\r\n    }\r\n    gSFPierEditorObjects[gSFPierEditorCount++] = objectid;\r\n    return true;\r\n}\r\n") <= 0) return false;
    return fwrite(out, "main() {}\r\npublic OnFilterScriptInit()\r\n{\r\n    SFPierEditor_Clear();\r\n#if SF_PIER_EDITOR_PREVIEW_ENABLED\r\n    new sfPierObject;\r\n") > 0;
}

// Explicit developer export only. Not called at production startup or by players.
// false: streamer init fragment; true: literal CreateObject Map Editor import.
// The standalone filterscript is OFF by default and owns/cleans its own handles.
stock bool:ShipSFPierMap_Export(bool:mapEditor = false)
{
    if (gSFPierPlanCount == 0) return false;
    fcreatedir(SF_PIER_EXPORT_DIR);
    new path[96];
    format(path, sizeof(path), "%s/%s", SF_PIER_EXPORT_DIR,
        mapEditor ? "map_editor.pwn" : "preview_objects.inc");
    new File:out = fopen(path, io_write);
    if (!out) return false;
#if SF_PIER_SITE_VERIFIED
    new bool:ok = fwrite(out, "// SHIP-MAP-01: export from a survey-confirmed site configuration.\r\n") > 0;
#else
    new bool:ok = fwrite(out, "// SHIP-MAP-01: NEEDS_IN_GAME_SURVEY. Test preview, not approved SF coordinates.\r\n") > 0;
#endif
    new line[384];
    format(line, sizeof(line), "// BASE: %.6f %.6f %.6f ROT %.6f; objects %d; site verified: %d.\r\n",
        SF_PIER_BASE_X, SF_PIER_BASE_Y, SF_PIER_BASE_Z, SF_PIER_BASE_ROT,
        gSFPierPlanCount, SF_PIER_SITE_VERIFIED);
    if (ok) ok = fwrite(out, line) > 0;
    if (ok)
    {
        if (mapEditor) ok = SFPM_WriteEditorHeader(out);
        else ok = fwrite(out, "// Paste inside a separate test init ONLY.\r\nnew sfPierObject;\r\n") > 0;
    }
    for (new i; i < gSFPierPlanCount && ok; i++)
    {
        new Float:x, Float:y, Float:z;
        ShipSFPierMap_World(gSFPierPlan[i][sfpX], gSFPierPlan[i][sfpY],
            gSFPierPlan[i][sfpZ], x, y, z);
        new slot = gSFPierPlan[i][sfpModelSlot];
        if (mapEditor)
            format(line, sizeof(line), "    sfPierObject = CreateObject(%d, %.6f, %.6f, %.6f, %.6f, 0.0, %.6f, 350.0);\r\n",
                gSFPierModels[slot][sfmID], x, y, z, gSFPierPlan[i][sfpRX],
                SF_PIER_BASE_ROT + gSFPierPlan[i][sfpRZ]);
        else
            format(line, sizeof(line), "%sCreateDynamicObject(%d, %.6f, %.6f, %.6f, %.6f, 0.0, %.6f, 0, 0, -1, 450.0, 350.0, -1, %d);\r\n",
                gSFPierPlan[i][sfpMaterial] == 0 ? "" : "sfPierObject = ",
                gSFPierModels[slot][sfmID], x, y, z, gSFPierPlan[i][sfpRX],
                SF_PIER_BASE_ROT + gSFPierPlan[i][sfpRZ], slot == SFPM_DECK ? 20 : 0);
        ok = fwrite(out, line) > 0;
        if (ok && mapEditor)
            ok = fwrite(out, "    if (!SFPierEditor_Keep(sfPierObject)) return 0;\r\n") > 0;
        if (!ok || gSFPierPlan[i][sfpMaterial] == 0) continue;
        for (new material; material < 2 && ok; material++)
        {
            format(line, sizeof(line), "%s(sfPierObject, %d, 9514, \"711_sfw\", \"ws_carpark2\");\r\n",
                mapEditor ? "    SetObjectMaterial" : "SetDynamicObjectMaterial", material);
            ok = fwrite(out, line) > 0;
        }
    }
    if (ok && mapEditor)
        ok = fwrite(out, "#endif\r\n    return 1;\r\n}\r\npublic OnFilterScriptExit()\r\n{\r\n    SFPierEditor_Clear();\r\n    return 1;\r\n}\r\n") > 0;
    fclose(out);
    return ok;
}

stock ShipSFPierMap_Init()
{
#if SF_PIER_MAP_MODE == 0
    ConsoleRU_Log("[SHIP-MAP-01] Пирс SF отключен: требуется привязка и проверка в GTA.");
    return 1;
#else
    if (gSFPierObjectCount > 0) return 1;
    if (!ShipSFPierMap_Build() || !ShipSFPierMap_CreateGeometry())
    {
        ShipSFPierMap_Shutdown();
        ConsoleRU_Log("[SHIP-MAP-01] Ошибка создания пирса SF; созданные объекты удалены.");
        return 0;
    }
    new message[200];
#if SF_PIER_MAP_MODE == 1
    format(message, sizeof(message), "[SHIP-MAP-01] Пирс SF: %d объектов, мир 0. Статус: %s.",
        gSFPierObjectCount, "NEEDS_IN_GAME_SURVEY");
#else
    format(message, sizeof(message), "[SHIP-MAP-01] Пирс SF: %d объектов, мир 0. Проверенная привязка.",
        gSFPierObjectCount);
#endif
    ConsoleRU_Log(message);
    return 1;
#endif
}
