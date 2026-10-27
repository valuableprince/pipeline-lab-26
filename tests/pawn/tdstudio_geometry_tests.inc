// Real Pawn arithmetic/production editor; synthetic players, network and TD API.
new tGeoBefore[TDS_ELEMENTS][E_TDS_ELEMENT];
new PlayerText:tGeoHandles[TDS_ELEMENTS];

stock bool:Near(Float:a, Float:b, Float:epsilon = 0.0001) { return floatabs(a-b) <= epsilon; }

stock GeoFixture(bool:grouped = false)
{
    new s = LayoutFixture();
    TDS_Select(0, 0); TDS_GeoExact(0, "100 100 40 20 0.2 1");
    TDS_Select(0, 1); TDS_GeoExact(0, "160 100 30 20 0.3 1.2");
    TDS_Select(0, 2); TDS_GeoExact(0, "100 140 40 30 0.3 1.2");
    TDS_Select(0, 3); TDS_GeoExact(0, "160 140 30 40 0.3 1.2");
    if (grouped) { TDS_SelectAll(0, true); TDS_GroupCreate(0, "TARKENSO_LOGO"); }
    gTDS[s][tsDirty] = false;
    return s;
}

stock GeoSnapshot(s)
{
    for (new e; e < TDS_ELEMENTS; e++)
    {
        for (new E_TDS_ELEMENT:f; f < E_TDS_ELEMENT; f++) tGeoBefore[e][f] = gTDSElement[s][e][f];
        tGeoHandles[e] = gTDSDraw[s][e];
    }
    return 1;
}

stock CheckGeoSnapshot(s, bool:geometry = true)
{
    new bool:same = true;
    for (new e; e < TDS_ELEMENTS; e++)
    {
        if (tGeoHandles[e] != gTDSDraw[s][e]) same = false;
        for (new E_TDS_ELEMENT:f; f < E_TDS_ELEMENT; f++)
        {
            if (!geometry && f >= tdX && f <= tdLetterY) continue;
            if (tGeoBefore[e][f] != gTDSElement[s][e][f]) same = false;
        }
    }
    Check(same, geometry ? ("all model cells and layer handles match snapshot") : ("IDs/layers/groups/content/style/handles unchanged"));
    return 1;
}

stock RunGeometryTests()
{
    new checks = tChecks; TDS_Init();
    new s = GeoFixture(), creates = tCreate, destroys = tDestroy;
    TDS_Select(0, 2); GeoSnapshot(s);
    Check(TDSInput_Start(0, TDS_TRANS_RESIZE) == 1, "A start sprite resize"); MovePacket(0);
    MovePacket(0, 0, KEY_RIGHT); MovePacket(0, KEY_DOWN);
    Check(gTDSElement[s][2][tdW] == 41.0 && gTDSElement[s][2][tdH] == 31.0 &&
        tW[0][_:gTDSDraw[s][2]] == 41.0 && tH[0][_:gTDSDraw[s][2]] == 31.0, "A native width/height receive live sizes");
    Check(!gTDS[s][tsDirty] && tCreate == creates && tDestroy == destroys, "A live resize keeps clean revision and handles");
    Check(!TDS_GeoScale(0, 2.0, 2.0) && !TDS_GeoPosition(0, 1.0, 1.0) && !TDS_GridSet(0, true), "conflicting geometry/settings blocked during transaction");
    TDSInput_Finish(0, false); CheckGeoSnapshot(s);
    Check(!gTDS[s][tsDirty] && tControl[0], "E cancel restores clean state and player control");
    for (new step; step < sizeof(gTDSSteps); step++)
    {
        gTDS[s][tsResizeStep] = step; TDSInput_Start(0, TDS_TRANS_RESIZE); MovePacket(0); MovePacket(0, 0, KEY_RIGHT);
        Check(Near(gTDSElement[s][2][tdW], 40.0+gTDSSteps[step]), "A all five resize steps use shared keyboard controller");
        TDSInput_Finish(0, false);
    }
    gTDS[s][tsResizeStep] = 0; gTDS[s][tsKeepRatio] = true;
    TDSInput_Start(0, TDS_TRANS_RESIZE); MovePacket(0);
    for (new i; i < 50; i++)
    {
        MovePacket(0, 0, KEY_RIGHT);
        Check(Near(gTDSElement[s][2][tdH], 30.0*(gTDSElement[s][2][tdW]/40.0)), "B ratio uses original dimensions every frame");
    }
    MovePacket(0, KEY_DOWN);
    Check(Near(gTDSElement[s][2][tdW], 40.0*(gTDSElement[s][2][tdH]/30.0)), "B height drives original ratio too");
    TDSInput_OnKeys(0, KEY_YES); Check(gTDS[s][tsDirty], "B only confirmed actual size marks dirty");
    gTDS[s][tsDirty] = false; TDSInput_Start(0, TDS_TRANS_RESIZE); MovePacket(0); TDSInput_OnKeys(0, KEY_YES);
    Check(!gTDS[s][tsDirty], "unchanged confirmation is not an edit");

    s = GeoFixture(); TDS_Select(0, 0); creates = tCreate;
    TDSInput_Start(0, TDS_TRANS_RESIZE); MovePacket(0); MovePacket(0, 0, KEY_RIGHT); TDSInput_Finish(0, true);
    Check(gTDSElement[s][0][tdW] == 41.0 && gTDSElement[s][0][tdLetterX] == 0.2 &&
        tW[0][_:gTDSDraw[s][0]] == 141.0, "C text area resize leaves glyph scale and uses absolute left endpoint");
    TDSInput_Start(0, TDS_TRANS_RESIZE, true); MovePacket(0); MovePacket(0, 0, KEY_RIGHT); TDSInput_Finish(0, true);
    Check(Near(gTDSElement[s][0][tdLetterX], 1.2) && gTDSElement[s][0][tdW] == 41.0 &&
        Near(tLetterX[0][_:gTDSDraw[s][0]], 1.2), "C glyph resize leaves area and updates LetterSize");
    TDS_ApplyInput(0, TDS_ALIGN, "2"); TDS_GeoExact(0, "100 100 40 20 0.2 1");
    Check(tW[0][_:gTDSDraw[s][0]] == 20.0 && tH[0][_:gTDSDraw[s][0]] == 40.0, "C center text native axes swapped");
    new Float:data[E_TDS_GEOMETRY], Float:l, Float:t, Float:r, Float:b;
    TDS_GeoRead(s, 0, data); TDS_GeoRect(data, TDS_TEXT_TYPE, 2, l, t, r, b);
    Check(l == 80.0 && r == 120.0 && t == 100.0 && b == 120.0, "C center logical rectangle uses half width");
    TDS_ApplyInput(0, TDS_ALIGN, "3"); TDS_GeoExact(0, "100 100 40 20 0.2 1");
    TDS_GeoRead(s, 0, data); TDS_GeoRect(data, TDS_TEXT_TYPE, 3, l, t, r, b);
    Check(l == 60.0 && r == 100.0 && tW[0][_:gTDSDraw[s][0]] == 60.0 && tCreate == creates, "C right text endpoint and bounds agree without allocation");

    s = GeoFixture(true); GeoSnapshot(s); creates = tCreate;
    TDSInput_Start(0, TDS_TRANS_SCALE); MovePacket(0); MovePacket(0, 0, KEY_RIGHT);
    Check(Near(gTDSElement[s][0][tdX], 145.0+(100.0-145.0)*(91.0/90.0)) &&
        Near(gTDSElement[s][0][tdLetterX], 0.2*(91.0/90.0)) && gTDSElement[s][0][tdLetterY] == 1.0,
        "D four-type center scaling transforms positions/area/text letters from snapshot");
    CheckGeoSnapshot(s, false); Check(!gTDS[s][tsDirty] && tCreate == creates, "D group scaling never allocates or dirties early");
    TDSInput_OnKeys(0, KEY_NO); CheckGeoSnapshot(s); Check(!gTDS[s][tsDirty], "E group cancel restores every geometry cell");
    gTDS[s][tsAnchorCenter] = false;
    Check(TDS_GeoScale(0, 2.0, 1.5), "D exact non-uniform scale top-left");
    Check(gTDSElement[s][0][tdX] == 100.0 && gTDSElement[s][1][tdX] == 220.0 &&
        gTDSElement[s][2][tdY] == 160.0 && gTDSElement[s][3][tdH] == 60.0 &&
        Near(gTDSElement[s][0][tdLetterX], 0.4) && Near(gTDSElement[s][0][tdLetterY], 1.5), "D all four types scaled with common pivot");
    CheckGeoSnapshot(s, false); GeoSnapshot(s); gTDS[s][tsDirty] = false;
    Check(!TDS_GeoScale(0, 100.0, 100.0) && !gTDS[s][tsDirty], "O one invalid member rejects entire scale"); CheckGeoSnapshot(s);
    TDS_GroupSelect(0, 1); TDSInput_Start(0, TDS_TRANS_RESIZE);
    Check(gTDS[s][tsMoveCount] == 1, "area resize targets primary even when the entire group is selected");
    MovePacket(0); MovePacket(0, 0, KEY_RIGHT); TDSInput_Finish(0, true);
    Check(gTDSElement[s][0][tdW] == tGeoBefore[0][tdW]+1.0 && gTDSElement[s][1][tdW] == tGeoBefore[1][tdW], "individual member resize does not scale its group");

    for (new action; action < 6; action++)
    {
        s = GeoFixture(); TDS_Select(0, 0); TDS_Select(0, 2, true); GeoSnapshot(s);
        Check(TDS_GeoAlign(0, E_TDS_ALIGN_OP:action, true), "F align to active element all six directions");
        TDS_GeoRead(s, 0, data); TDS_GeoRect(data, TDS_TEXT_TYPE, 1, l, t, r, b);
        new Float:dx, Float:dy;
        TDS_GeoAlignDelta(E_TDS_ALIGN_OP:action, l, t, r, b, 100.0, 140.0, 140.0, 170.0, dx, dy);
        Check(Near(dx, 0.0) && Near(dy, 0.0) && gTDSElement[s][2][tdX] == tGeoBefore[2][tdX] &&
            gTDSElement[s][2][tdY] == tGeoBefore[2][tdY], "F alignment target matches and active is fixed");
    }
    s = GeoFixture(); TDS_SelectAll(0, true); Check(TDS_GeoAlign(0, TDS_ALIGN_RIGHT, false), "F selection rectangle reference");
    Check(gTDSElement[s][0][tdX] == 150.0 && gTDSElement[s][2][tdX] == 150.0 &&
        gTDSElement[s][1][tdX] == 160.0, "F selection bbox edge used instead of active anchor");
    s = GeoFixture(true); new extra = TDS_AddElement(0, TDS_RECT_TYPE);
    TDS_GeoExact(0, "300 200 50 50 0.3 1.2"); GeoSnapshot(s);
    Check(TDS_GeoAlign(0, TDS_ALIGN_RIGHT, true, 1) && gTDSElement[s][0][tdX] == 260.0 &&
        gTDSElement[s][1][tdX] == 320.0 && gTDSElement[s][extra][tdX] == 300.0,
        "F whole remembered group translates rigidly to external active reference");
    CheckGeoSnapshot(s, false); TDS_GroupSelect(0, 1);
    Check(!TDS_GeoAlign(0, TDS_ALIGN_LEFT, true, 1) && !TDS_GeoAlign(0, TDS_ALIGN_LEFT, true, 999), "F invalid/self group reference rejected");
    Check(TDS_GeoScreen(0, 0), "G group centers on actual virtual screen");
    new slots[TDS_ELEMENTS], count = TDS_Targets(s, slots); TDS_GeoBounds(s, slots, count, l, t, r, b);
    Check((l+r)/2.0 == 320.0 && (t+b)/2.0 == 240.0, "G center is 320x240, not half legacy anchor limits");
    for (new action = 1; action < 7; action++) Check(TDS_GeoScreen(0, action), "G all screen edge/corner positions for group");
    TDS_Select(0, extra); TDS_GeoExact(0, "200 200 10 10 0.3 1.2"); GeoSnapshot(s);
    Check(!TDS_GeoScreen(0, 4), "G incompatible bottom anchor beyond format-2 Y448 is explicit rejection"); CheckGeoSnapshot(s);
    gTDS[s][tsMargin] = 40.0; Check(TDS_GeoScreen(0, 6) && gTDSElement[s][extra][tdY] == 430.0, "G configurable margin makes bottom placement valid");

    s = GeoFixture(); TDS_Select(0, 1); TDS_GeoExact(0, "10 10 10 10 0.3 1.2");
    TDS_Select(0, 2); TDS_GeoExact(0, "40 20 20 20 0.3 1.2");
    TDS_Select(0, 3); TDS_GeoExact(0, "100 100 30 30 0.3 1.2");
    TDS_Select(0, 1); TDS_Select(0, 2, true); TDS_Select(0, 3, true);
    Check(TDS_GeoDistribute(0, true) && gTDSElement[s][2][tdX] == 50.0, "H variable widths use equal free gaps (30/30)");
    Check(TDS_GeoDistribute(0, false) && gTDSElement[s][2][tdY] == 50.0, "H variable heights use equal free gaps");
    TDS_Select(0, 2); TDS_GeoExact(0, "20 20 200 200 0.3 1.2");
    TDS_Select(0, 1); TDS_Select(0, 2, true); TDS_Select(0, 3, true); GeoSnapshot(s);
    Check(!TDS_GeoDistribute(0, true), "H insufficient free space rejects implicit overlap"); CheckGeoSnapshot(s);
    TDS_Select(0, 1); Check(!TDS_GeoDistribute(0, true), "H at least three selected elements required");

    s = GeoFixture(); TDS_Select(0, 2); GeoSnapshot(s); creates = tCreate; destroys = tDestroy;
    Check(TDS_GridSet(0, true) && tCreate == creates+6 && gTDSGridVisible[s], "I visual grid costs exactly six handles");
    new bool:nonselectable = true, touches;
    for (new i; i < TDS_GRID_GUIDES; i++)
    {
        if (tSelectable[0][_:gTDSGrid[s][i]]) nonselectable = false;
        touches += tTouches[0][_:gTDSGrid[s][i]];
    }
    Check(nonselectable, "I guide handles never intercept canvas selection");
    TDS_RenderStatus(s); new after;
    for (new i; i < TDS_GRID_GUIDES; i++) after += tTouches[0][_:gTDSGrid[s][i]];
    Check(after == touches, "I cached unchanged grid sends no guide updates");
    for (new i; i < 4; i++) Check(TDS_GridStep(0, i), "I all grid steps available");
    Check(!TDS_GridStep(0, -1) && !TDS_GridStep(0, 4), "I invalid grid indices rejected");
    TDS_SetMode(0, TDS_PREVIEW_MODE); Check(!gTDSGridVisible[s], "I preview hides local guides");
    TDS_SetMode(0, TDS_EDIT_MODE); Check(gTDSGridVisible[s] && tCreate == creates+6, "I editor restores existing guides");
    CheckGeoSnapshot(s); Check(!gTDS[s][tsDirty], "I grid settings are not project edits");
    TDS_GridSet(0, false); Check(tDestroy == destroys+6, "I disable releases all guides");
    tFailAfter = 3; Check(!TDS_GridSet(0, true) && !gTDS[s][tsGridShow] && tCreate-tDestroy == creates-destroys, "O partial grid allocation failure frees only temporary guides");
    tFailAfter = -1;
    TDS_GeoExact(0, "123 187 40 30 0.3 1.2"); gTDS[s][tsDirty] = false;
    TDS_GridStep(0, 2); gTDS[s][tsSnap] = true;
    Check(gTDSElement[s][2][tdX] == 123.0 && !gTDS[s][tsDirty], "J enabling snap changes no coordinates");
    Check(TDS_GeoPosition(0, 123.0, 187.0) && gTDSElement[s][2][tdX] == 120.0 && gTDSElement[s][2][tdY] == 190.0, "J nearest mathematical grid points");
    Check(TDS_GeoExact(0, "640 448 40 30 0.3 1.2") && gTDSElement[s][2][tdX] == 640.0 &&
        gTDSElement[s][2][tdY] == 440.0, "J exact Snap chooses nearest legal grid point at legacy upper boundary");
    Check(TDS_ApplyInput(0, TDS_POSITION, "123 187") && gTDSElement[s][2][tdX] == 120.0 &&
        gTDSElement[s][2][tdY] == 190.0, "J existing property dialog obeys the same snap rule");
    gTDS[s][tsDirty] = false; TDSInput_Start(0); MovePacket(0);
    for (new i; i < 6; i++) MovePacket(0, 0, KEY_RIGHT);
    Check(gTDSElement[s][2][tdX] == 130.0 && !gTDS[s][tsDirty], "J held sub-grid steps accumulate unsnapped intent");
    TDSInput_Finish(0, false); Check(gTDSElement[s][2][tdX] == 120.0, "J cancellation restores exact unsnapped snapshot");
    gTDS[s][tsSnap] = false; Check(TDS_GeoPosition(0, 123.125, 187.25) && gTDSElement[s][2][tdX] == 123.125, "J disabled snap allows exact fractional coordinates");
    s = GeoFixture(true); gTDS[s][tsSnap] = true; TDS_GridStep(0, 2); GeoSnapshot(s);
    Check(TDS_GeoPosition(0, 173.0, 187.0), "K group pivot position snaps as one object");
    count = TDS_Targets(s, slots); TDS_GeoBounds(s, slots, count, l, t, r, b);
    Check((l+r)/2.0 == 170.0 && (t+b)/2.0 == 190.0 &&
        gTDSElement[s][1][tdX]-gTDSElement[s][0][tdX] == 60.0 &&
        gTDSElement[s][2][tdY]-gTDSElement[s][0][tdY] == 40.0, "K common rounded pivot preserves relative offsets");
    CheckGeoSnapshot(s, false);

    s = GeoFixture(); TDS_Select(0, 2); GeoSnapshot(s); gTDS[s][tsDirty] = false;
    Check(!TDS_GeoExact(0, "10 10 0 30 0.3 1.2") && !TDS_GeoExact(0, "10 10 -1 30 0.3 1.2") &&
        !TDS_GeoExact(0, "10 NaN 40 30 0.3 1.2") && !TDS_GeoExact(0, "1e3 10 40 30 0.3 1.2") &&
        !TDS_GeoExact(0, "10 10 40 449 0.3 1.2"), "O strict exact geometry rejects zero/negative/NaN/exponent/range");
    Check(!TDS_GeoScale(0, Float:0x7FC00000, 1.0) && !TDS_GeoScale(0, Float:0x7F800000, 1.0) &&
        !TDS_GeoScale(0, 0.0, 1.0) && !TDS_GeoScale(0, -1.0, 1.0), "O direct numeric APIs reject NaN/infinity/zero/negative");
    CheckGeoSnapshot(s); Check(!gTDS[s][tsDirty], "O rejected edits keep clean model");
    Check(!TDS_GeoPosition(-1, 10.0, 10.0) && !TDS_GeoScreen(MAX_PLAYERS, 0) &&
        !TDS_GeoAlign(-1, TDS_ALIGN_LEFT) && !TDS_GridSet(MAX_PLAYERS, true), "O invalid owners cannot address any geometry/session resources");
    MockDestroy(0, gTDSDraw[s][2]);
    Check(!TDS_ApplyInput(0, TDS_SIZE, "50 40") && !gTDS[s][tsDirty], "O property sizes preflight native handle before changing working model");
    CheckGeoSnapshot(s);
    s = GeoFixture(); TDS_Select(0, 2);
    gTDS[s][tsBusy] = 1;
    Check(!TDS_GeoPosition(0, 200.0, 200.0) && !TDS_GeoScale(0, 1.5, 1.5) && !TDS_GeoExact(0, "10 10 40 30 0.3 1.2"), "O pending DB work excludes all geometry entry points");
    gTDS[s][tsBusy] = 0; gTDS[s][tsResizeStep] = 999;
    Check(!TDSInput_Start(0, TDS_TRANS_RESIZE), "O corrupt resize step rejected before native freeze"); gTDS[s][tsResizeStep] = 2;
    TDSInput_Start(0, TDS_TRANS_SCALE); MovePacket(0); MovePacket(0, 0, KEY_RIGHT);
    MockDestroy(0, gTDSDraw[s][2]); MovePacket(0, 0, KEY_RIGHT);
    Check(!TDS_Active(0) && tControl[0], "O disappeared native handle aborts transaction safely");
    s = GeoFixture(true); TDSInput_Start(0, TDS_TRANS_SCALE); MovePacket(0); MovePacket(0, KEY_DOWN);
    tConnected[0] = false; new releases = tUnfreeze; TDS_Reset(0, false);
    Check(!TDS_Active(0) && releases == tUnfreeze && !gTDS[s][tsMoveCount], "O disconnect drops scale snapshots without stale native unfreeze");

    s = GeoFixture(); TDS_Select(0, 2); TDS_EditMenu(0); Answer(0, TDS_EDIT_MENU, 1, 13);
    Check(gTDS[s][tsDialog] == TDS_GEOMETRY, "geometry UI extends existing editor menu");
    Answer(0, TDS_GEOMETRY, 1, 0); Check(gTDS[s][tsDialog] == TDS_GEO_INFO, "geometry numeric inspector is reachable");
    Answer(0, TDS_GEO_INFO, 1, 0); Answer(0, TDS_GEOMETRY, 1, 4);
    Answer(0, TDS_GEO_INPUT, 1, -1, "123.125 187.25 55.5 25.5 0.3 1.2");
    Check(gTDSElement[s][2][tdX] == 123.125 && gTDSElement[s][2][tdW] == 55.5, "exact six-value UI applies production geometry");
    Answer(0, TDS_GEOMETRY, 1, 14); Answer(0, TDS_GEO_STEP, 1, 1);
    Check(gTDS[s][tsResizeStep] == 1, "resize step UI independent of move step");
    Answer(0, TDS_GEOMETRY, 1, 1); MovePacket(0); MovePacket(0, 0, 0, 200, KEY_CROUCH);
    Check(gTDS[s][tsGeoAxis] == 1, "crouch action edge switches resize axis");
    MovePacket(0); MovePacket(0, KEY_DOWN);
    Check(gTDSElement[s][2][tdW] == 56.0 && gTDSElement[s][2][tdH] == 25.5, "axis W redirects vertical action to width only");
    MovePacket(0, 0, 0, 200, KEY_SPRINT); Check(gTDS[s][tsResizeStep] == 2 && gTDS[s][tsStep] == 2, "sprint edge cycles resize step");
    TDSInput_Finish(0, false);

    s = LayoutFixture(TDS_ELEMENTS); TDS_Select(0, 0); TDS_GridSet(0, true); GeoSnapshot(s);
    new PlayerText:foreignDraws[256], foreignCount;
    for (new i; i < sizeof(foreignDraws); i++)
    {
        new PlayerText:td = MockCreate(0, 0.0, 0.0, "foreign UI");
        if (td == INVALID_PLAYER_TEXT_DRAW) break;
        foreignDraws[foreignCount++] = td;
    }
    creates = tCreate; destroys = tDestroy; gTDS[s][tsDirty] = false;
    TDSInput_Start(0, TDS_TRANS_RESIZE); MovePacket(0); MovePacket(0, 0, KEY_RIGHT);
    Check(gTDSElement[s][0][tdW] == 101.0 && tCreate == creates && tDestroy == destroys,
        "O resize works at the 64-element limit even with zero free player TD handles");
    TDSInput_Finish(0, false); CheckGeoSnapshot(s);
    Check(!TDS_Layer(0, TDS_LAYER_FRONT) && !gTDS[s][tsDirty] && gTDSGridVisible[s],
        "O layer allocation failure at full pool keeps model, grid and existing layer handles");
    CheckGeoSnapshot(s);
    for (new i; i < foreignCount; i++) MockDestroy(0, foreignDraws[i]);

    // P: 8 sessions, 1000 updates/sec/session. Only each 50ms quantum renders.
    TDS_Shutdown(); TDS_Init();
    for (new p; p < TDS_SESSIONS; p++)
    {
        Ready(p, true); tNow += 600; TDS_Open(p); TDS_NewProject(p, "Concurrent geometry");
        for (new e; e < 4; e++) TDS_AddElement(p, E_TDS_TYPE:e);
        new q = TDS_Session(p); gTDS[q][tsResizeStep] = 0;
        TDS_SelectAll(p, true); TDS_GroupCreate(p, "Group"); gTDS[q][tsDirty] = false;
        TDSInput_Start(p, TDS_TRANS_SCALE);
    }
    // Synchronize epochs after fixture creation, then neutral re-arm all leases.
    for (new p; p < TDS_SESSIONS; p++) { new q = TDS_Session(p); gTDS[q][tsMoveTick] = tNow; TDSInput_OnUpdate(p); }
    creates = tCreate; destroys = tDestroy;
    new beforeTouch[TDS_SESSIONS];
    for (new p; p < TDS_SESSIONS; p++) beforeTouch[p] = tTouches[p][_:gTDSDraw[TDS_Session(p)][0]];
    for (new ms; ms < 1000; ms++)
    {
        tNow++;
        for (new p; p < TDS_SESSIONS; p++) { tLR[p] = KEY_RIGHT; TDSInput_OnUpdate(p); }
    }
    for (new p; p < TDS_SESSIONS; p++)
    {
        new q = TDS_Session(p), td = _:gTDSDraw[q][0];
        Check(tTouches[p][td]-beforeTouch[p] == 20*5, "P 1000 update packets produce exactly 20 geometry renders (five native mutations) per text");
        Check(!gTDS[q][tsDirty] && Near(gTDSElement[q][0][tdW], 102.0), "P every session retains independent snapshot and bounded live scale");
        TDSInput_Finish(p, false);
    }
    Check(tCreate == creates && tDestroy == destroys, "P eight simultaneous group transforms allocate/recreate zero handles");
    TDS_Shutdown();
    printf("TDS GEOMETRY RESULT: checks=%d (Pawn arithmetic, mocked input/UI; no GTA client)", tChecks-checks);
    return 1;
}
