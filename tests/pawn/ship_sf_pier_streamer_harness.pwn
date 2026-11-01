// Integration with unmodified streamer v2.9.6; no native replacements or players.
#include <open.mp>
#pragma dynamic 16384
#include "streamer.inc"
#define SF_PIER_MAP_MODE (1)
#define SF_PIER_SITE_CONFIGURED (1)
// Synthetic test origin, NOT a proposed site in SF.
#define SF_PIER_BASE_X (100.0)
#define SF_PIER_BASE_Y (200.0)
#define SF_PIER_BASE_Z (30.0)
#define SF_PIER_BASE_ROT (90.0)

new tChecks, tFailures;
stock ConsoleRU_Log(const text[])
{
    #pragma unused text
    return 1;
}
#include "ship_sf_pier_map.inc"

stock Check(bool:ok, const name[])
{
    tChecks++;
    if (!ok) { tFailures++; printf("PIER FAIL: %s", name); }
    return 1;
}
stock bool:Near(Float:a, Float:b)
{
    return floatabs(a - b) < 0.0001;
}
stock bool:DumpAndInspectNativeScene()
{
    new File:out = fopen("ship_sf_pier_scene.csv", io_write);
    if (!out) return false;
    fwrite(out, "model,x,y,z,rx,rz,material,world_x,world_y,world_z,world_rz\n");
    new line[320];
    for (new i; i < gSFPierObjectCount; i++)
    {
        new objectid = gSFPierObjects[i];
        new slot = gSFPierPlan[i][sfpModelSlot];
        Check(IsValidDynamicObject(objectid) != 0, "native handle validity");
        Check(Streamer_GetIntData(STREAMER_TYPE_OBJECT, objectid, E_STREAMER_MODEL_ID) ==
            gSFPierModels[slot][sfmID], "native model");
        new Float:x, Float:y, Float:z, Float:rx, Float:ry, Float:rz;
        Check(GetDynamicObjectPos(objectid, x, y, z) != 0, "native position read");
        Check(GetDynamicObjectRot(objectid, rx, ry, rz) != 0, "native rotation read");
        new Float:ex, Float:ey, Float:ez;
        ShipSFPierMap_World(gSFPierPlan[i][sfpX], gSFPierPlan[i][sfpY],
            gSFPierPlan[i][sfpZ], ex, ey, ez);
        Check(Near(x, ex) && Near(y, ey) && Near(z, ez), "native world transform");
        Check(Near(rx, gSFPierPlan[i][sfpRX]) && Near(ry, 0.0) &&
            Near(rz, SF_PIER_BASE_ROT + gSFPierPlan[i][sfpRZ]), "native angles");
        Check(Streamer_GetArrayDataLength(STREAMER_TYPE_OBJECT, objectid, E_STREAMER_WORLD_ID) == 1 &&
            Streamer_IsInArrayData(STREAMER_TYPE_OBJECT, objectid, E_STREAMER_WORLD_ID, 0) != 0,
            "only world zero");
        Check(Streamer_GetArrayDataLength(STREAMER_TYPE_OBJECT, objectid, E_STREAMER_INTERIOR_ID) == 1 &&
            Streamer_IsInArrayData(STREAMER_TYPE_OBJECT, objectid, E_STREAMER_INTERIOR_ID, 0) != 0,
            "only interior zero");
        Check(Streamer_IsInArrayData(STREAMER_TYPE_OBJECT, objectid, E_STREAMER_PLAYER_ID, 0) != 0 &&
            Streamer_IsInArrayData(STREAMER_TYPE_OBJECT, objectid, E_STREAMER_PLAYER_ID, 1) != 0,
            "shared player visibility filter");
        new Float:distance;
        Streamer_GetFloatData(STREAMER_TYPE_OBJECT, objectid, E_STREAMER_STREAM_DISTANCE, distance);
        Check(Near(distance, 450.0), "native stream distance");
        Streamer_GetFloatData(STREAMER_TYPE_OBJECT, objectid, E_STREAMER_DRAW_DISTANCE, distance);
        Check(Near(distance, 350.0), "native draw distance");
        Check(Streamer_GetIntData(STREAMER_TYPE_OBJECT, objectid, E_STREAMER_PRIORITY) ==
            (slot == SFPM_DECK ? 20 : 0), "native priority");
        if (gSFPierPlan[i][sfpMaterial])
            for (new material; material < 2; material++)
            {
                new model, color, txd[32], texture[64];
                Check(GetDynamicObjectMaterial(objectid, material, model, txd, texture, color) != 0 &&
                    model == 9514 && !strcmp(txd, "711_sfw") && !strcmp(texture, "ws_carpark2") &&
                    color == 0, "native material roundtrip");
            }
        // Local geometry is from the plan; world coordinates/angles below are
        // read back from the real plugin, not reconstructed from the plan.
        format(line, sizeof(line), "%d,%.6f,%.6f,%.6f,%.6f,%.6f,%d,%.6f,%.6f,%.6f,%.6f\n",
            Streamer_GetIntData(STREAMER_TYPE_OBJECT, objectid, E_STREAMER_MODEL_ID),
            gSFPierPlan[i][sfpX], gSFPierPlan[i][sfpY], gSFPierPlan[i][sfpZ],
            rx, rz - SF_PIER_BASE_ROT, gSFPierPlan[i][sfpMaterial], x, y, z, rz);
        fwrite(out, line);
    }
    fclose(out);
    return true;
}

main() {}
public OnGameModeInit()
{
    Check(CountDynamicObjects() == 0, "fresh plugin fixture");
    new sentinel = CreateDynamicObject(1214, 20.0, 20.0, 10.0,
        0.0, 0.0, 0.0, 77, 3, -1);
    Check(IsValidDynamicObject(sentinel) != 0, "unrelated object created");
    Check(ShipSFPierMap_Init() == 1 && gSFPierObjectCount == 496 &&
        CountDynamicObjects() == 497, "full native scene");
    new first = gSFPierObjects[0];
    Check(ShipSFPierMap_Init() == 1 && ShipSFPierMap_CreateGeometry() &&
        CountDynamicObjects() == 497 && gSFPierObjects[0] == first, "native idempotence");
    Check(DumpAndInspectNativeScene(), "native CSV");
    Check(ShipSFPierMap_Export() && ShipSFPierMap_Export(true), "both exports");
    ShipSFPierMap_Shutdown();
    ShipSFPierMap_Shutdown();
    Check(CountDynamicObjects() == 1 && IsValidDynamicObject(sentinel) != 0,
        "cleanup preserves unrelated geometry");
    // Exercise real plugin allocation refusal, not a replacement Create native.
    new previousLimit = Streamer_GetMaxItems(STREAMER_TYPE_OBJECT);
    Check(Streamer_SetMaxItems(STREAMER_TYPE_OBJECT, 11) != 0, "temporary fixture limit");
    Check(ShipSFPierMap_Init() == 0 && gSFPierObjectCount == 0 &&
        CountDynamicObjects() == 1 && IsValidDynamicObject(sentinel) != 0, "real allocation rollback");
    Check(Streamer_SetMaxItems(STREAMER_TYPE_OBJECT, previousLimit) != 0, "restore plugin limit");
    Check(ShipSFPierMap_Init() == 1 && CountDynamicObjects() == 497, "native retry");
    ShipSFPierMap_Shutdown();
    Check(CountDynamicObjects() == 1 && IsValidDynamicObject(sentinel) != 0, "native restart cleanup");
    DestroyDynamicObject(sentinel);
    Check(CountDynamicObjects() == 0, "all fixture handles released");
    printf("PIER TEST RESULT: checks=%d failures=%d", tChecks, tFailures);
    return 1;
}
