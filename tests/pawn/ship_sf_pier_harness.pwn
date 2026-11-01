// Executes production scene construction with deterministic streamer failures.
#include <open.mp>
#pragma dynamic 16384
#include "streamer.inc"

#define SF_PIER_MAP_MODE (1)
#define SF_PIER_SITE_CONFIGURED (1)
// A test fixture, NOT a proposed SF location. Tests nonzero origin + 90deg yaw.
#define SF_PIER_BASE_X (100.0)
#define SF_PIER_BASE_Y (200.0)
#define SF_PIER_BASE_Z (30.0)
#define SF_PIER_BASE_ROT (90.0)
new tChecks, tFailures, tAlive, tCreated, tDestroyed;
new tFailCreate = -1, tFailMaterial = -1, tMaterialCalls;
new bool:tObjects[513];
new tLastLog[256];

stock ConsoleRU_Log(const text[])
{
    format(tLastLog, sizeof(tLastLog), "%s", text);
    return 1;
}
stock MockCreate(modelid, Float:x, Float:y, Float:z, Float:rx, Float:ry, Float:rz,
    world, interior, player, Float:stream, Float:draw, area, priority)
{
    #pragma unused modelid, x, y, z, rx, ry, rz, priority
    tChecks++;
    if (world != 0 || interior != 0 || player != -1 || area != -1 ||
        stream != 450.0 || draw != 350.0) tFailures++;
    if (tFailCreate == 0) return INVALID_STREAMER_ID;
    if (tFailCreate > 0) tFailCreate--;
    for (new i = 1; i < sizeof(tObjects); i++)
        if (!tObjects[i])
        {
            tObjects[i] = true;
            tAlive++; tCreated++;
            return i;
        }
    return INVALID_STREAMER_ID;
}
stock MockValid(objectid)
{
    return objectid > 0 && objectid < sizeof(tObjects) && tObjects[objectid];
}
stock MockDestroy(objectid)
{
    tChecks++;
    if (!MockValid(objectid)) { tFailures++; return 0; }
    tObjects[objectid] = false;
    tAlive--; tDestroyed++;
    return 1;
}
stock MockMaterial(objectid, index, model, const txd[], const texture[])
{
    #pragma unused texture
    tChecks++; tMaterialCalls++;
    if (!MockValid(objectid) || index < 0 || index > 1 || model != 9514 ||
        strcmp(txd, "711_sfw")) tFailures++;
    if (tFailMaterial == 0) return 0;
    if (tFailMaterial > 0) tFailMaterial--;
    return 1;
}
#define CreateDynamicObject MockCreate
#define IsValidDynamicObject MockValid
#define DestroyDynamicObject MockDestroy
#define SetDynamicObjectMaterial MockMaterial
#include "ship_sf_pier_map.inc"

stock Check(bool:ok, const name[])
{
    tChecks++;
    if (!ok) { tFailures++; printf("PIER FAIL: %s", name); }
    return 1;
}
stock DumpScene()
{
    new File:out = fopen("ship_sf_pier_scene.csv", io_write);
    if (!out) return 0;
    fwrite(out, "model,x,y,z,rx,rz,material,world_x,world_y,world_z,world_rz\n");
    new line[320];
    for (new i; i < gSFPierPlanCount; i++)
    {
        new Float:x, Float:y, Float:z;
        ShipSFPierMap_World(gSFPierPlan[i][sfpX], gSFPierPlan[i][sfpY], gSFPierPlan[i][sfpZ], x, y, z);
        format(line, sizeof(line), "%d,%.6f,%.6f,%.6f,%.6f,%.6f,%d,%.6f,%.6f,%.6f,%.6f\n",
            gSFPierModels[gSFPierPlan[i][sfpModelSlot]][sfmID],
            gSFPierPlan[i][sfpX], gSFPierPlan[i][sfpY], gSFPierPlan[i][sfpZ],
            gSFPierPlan[i][sfpRX], gSFPierPlan[i][sfpRZ], gSFPierPlan[i][sfpMaterial],
            x, y, z, SF_PIER_BASE_ROT + gSFPierPlan[i][sfpRZ]);
        fwrite(out, line);
    }
    fclose(out);
    return 1;
}

main() {}
public OnGameModeInit()
{
    Check(ShipSFPierMap_Init() == 1, "preview init");
    Check(gSFPierPlanCount == 496 && tAlive == 496, "complete scene inventory");
    Check(strfind(tLastLog, "NEEDS_IN_GAME_SURVEY") >= 0, "honest survey status");
    new before = tCreated;
    Check(ShipSFPierMap_Init() == 1 && tCreated == before, "idempotent init");
    Check(ShipSFPierMap_CreateGeometry() && tCreated == before, "idempotent allocation");
    Check(ShipSFPierMap_Export(), "full Pawn export");
    Check(DumpScene() != 0, "actual plan export for independent geometry tests");
    ShipSFPierMap_Shutdown();
    ShipSFPierMap_Shutdown();
    Check(tAlive == 0 && tDestroyed == tCreated, "idempotent shutdown");
    // Fail EACH object allocation, including first and last. Every owned handle
    // must be released; a subsequent startup must remain possible.
    for (new fail; fail < gSFPierPlanCount; fail++)
    {
        tFailCreate = fail;
        Check(!ShipSFPierMap_CreateGeometry(), "injected creation failure");
        Check(tAlive == 0 && gSFPierObjectCount == 0, "creation rollback");
    }
    tFailCreate = -1;
    for (new fail; fail < 678; fail++) // 339 structural objects x 2 materials
    {
        tFailMaterial = fail;
        Check(!ShipSFPierMap_CreateGeometry(), "injected material failure");
        Check(tAlive == 0 && gSFPierObjectCount == 0, "material rollback");
    }
    tFailMaterial = -1;
    Check(ShipSFPierMap_Init() == 1 && tAlive == 496, "retry after failures");
    ShipSFPierMap_Shutdown();
    // Capacity exhaustion cannot leave a partially usable plan.
    gSFPierPlanCount = SF_PIER_MAX_OBJECTS;
    Check(!SFPM_Centre(SFPM_DECK, 0.0, 0.0, 0.0), "capacity check");
    Check(ShipSFPierMap_Build() && gSFPierPlanCount == 496, "rebuild after capacity exhaustion");
    Check(tAlive == 0 && tCreated == tDestroyed, "no leaks");
    printf("PIER TEST RESULT: checks=%d failures=%d", tChecks, tFailures);
    // The host stops after this report. Owned geometry was destroyed above;
    // engine/process teardown is outside this mocked-streamer test's scope.
    return 1;
}
