// Run the actual generated native-object filterscript callbacks in open.mp.
// No streamer dependency or GTA client is needed for object ownership checks.
#include <open.mp>
#pragma dynamic 16384
#include "map_editor_enabled.pwn"

new tChecks, tFailures;
new tOtherObjects[MAX_OBJECTS], tOtherCount;

stock Check(bool:ok, const name[])
{
    tChecks++;
    if (!ok) { tFailures++; printf("PIER FAIL: %s", name); }
    return 1;
}
stock NativeCount()
{
    new count;
    for (new i; i < MAX_OBJECTS; i++) if (IsValidObject(i)) count++;
    return count;
}

public OnGameModeInit()
{
    Check(NativeCount() == 0, "empty native fixture");
    new sentinel = CreateObject(1214, 20.0, 20.0, 10.0, 0.0, 0.0, 0.0);
    Check(IsValidObject(sentinel), "unrelated native object");
    Check(OnFilterScriptInit() == 1 && gSFPierEditorCount == 496 && NativeCount() == 497,
        "generated native init");
    for (new i; i < gSFPierEditorCount; i++)
    {
        new objectid = gSFPierEditorObjects[i];
        Check(IsValidObject(objectid), "exported native handle");
        Check(GetObjectModel(objectid) > 0, "exported native model");
        new Float:x, Float:y, Float:z;
        Check(GetObjectPos(objectid, x, y, z), "exported native position");
        if (GetObjectModel(objectid) == 3095)
            for (new material; material < 2; material++)
            {
                new model, color, txd[32], texture[64];
                Check(GetObjectMaterial(objectid, material, model, txd, sizeof(txd),
                    texture, sizeof(texture), color) && model == 9514 &&
                    !strcmp(txd, "711_sfw") && !strcmp(texture, "ws_carpark2") && color == 0,
                    "exported native material");
            }
    }
    Check(OnFilterScriptInit() == 1 && NativeCount() == 497 && IsValidObject(sentinel),
        "repeated exported init replaces only owned objects");
    Check(OnFilterScriptExit() == 1 && NativeCount() == 1 && IsValidObject(sentinel),
        "exported exit preserves unrelated object");
    Check(OnFilterScriptExit() == 1 && NativeCount() == 1, "repeated exported exit");
    // Fill the real engine pool, then free five slots. The sixth pier object
    // must fail and roll back those five, preserving ALL unrelated handles.
    for (new i; i < MAX_OBJECTS; i++)
    {
        new objectid = CreateObject(1214, 20.0, 20.0, 10.0, 0.0, 0.0, 0.0);
        if (!IsValidObject(objectid)) break;
        tOtherObjects[tOtherCount++] = objectid;
    }
    Check(tOtherCount > 5, "engine object pool filled");
    for (new i; i < 5; i++) DestroyObject(tOtherObjects[--tOtherCount]);
    Check(OnFilterScriptInit() == 0 && gSFPierEditorCount == 0 &&
        NativeCount() == tOtherCount + 1 && IsValidObject(sentinel), "exported allocation rollback");
    while (tOtherCount > 0)
    {
        new objectid = tOtherObjects[--tOtherCount];
        Check(IsValidObject(objectid), "unrelated object survives rollback");
        DestroyObject(objectid);
    }
    Check(OnFilterScriptInit() == 1 && NativeCount() == 497, "exported retry");
    OnFilterScriptExit();
    DestroyObject(sentinel);
    Check(NativeCount() == 0, "native fixture cleanup");
    printf("PIER TEST RESULT: checks=%d failures=%d", tChecks, tFailures);
    return 1;
}
