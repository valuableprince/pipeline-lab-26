new tHistoryImage[TDS_H_CELLS];

stock HistoryFixture(bool:grouped = false)
{
    new s = GeoFixture(grouped); TDS_HClear(0); TDS_HCheckpoint(s);
    return s;
}

stock CheckHistoryImage(s)
{
    TDS_HCapture(s, gTDSHistoryImage);
    Check(TDS_HSame(tHistoryImage,gTDSHistoryImage) == 1, "canonical logical state restored exactly");
    Check(TDS_ProjectValid(s), "restored state satisfies project/group/ID invariants");
    return 1;
}

stock RunHistoryTests()
{
    new checks = tChecks; TDS_Init();
    new s = LayoutFixture(0); TDS_HClear(0); TDS_HCheckpoint(s);
    new e = TDS_AddElement(0,TDS_SPRITE_TYPE), id = gTDSElement[s][e][tdId];
    TDS_HCapture(s,tHistoryImage);
    Check(TDS_HCount(s) == 1 && !TDS_HCount(s,true), "A create produces one history operation");
    Check(TDS_HStep(0) == 1 && gTDS[s][tsCount] == 0 && gTDS[s][tsNextId] == id+1 && !gTDS[s][tsDirty], "A undo creation keeps allocator high-water and checkpoint");
    Check(TDS_HStep(0,true) == 1 && gTDS[s][tsCount] == 1 && TDS_StateFind(gTDSElement[s],id) >= 0, "A redo retains original logical ID"); CheckHistoryImage(s);
    TDS_DeleteElement(0,TDS_StateFind(gTDSElement[s],id));
    Check(TDS_HStep(0) == 1, "B undo deleted element"); CheckHistoryImage(s);
    TDS_Select(0,TDS_StateFind(gTDSElement[s],id)); TDS_HClear(0); TDS_HCheckpoint(s);
    new PlayerText:handle = gTDSDraw[s][gTDS[s][tsSelected]];
    Check(TDS_Move(0,100.0,0.0) && TDS_HCount(s) == 1 && TDS_HStep(0) == 1, "C 100-unit completed move undoes as one action");
    Check(gTDSDraw[s][gTDS[s][tsSelected]] == handle && !gTDS[s][tsDirty], "C nonstructural undo reuses native handle and reaches checkpoint");

    s = HistoryFixture(true); TDS_HCapture(s,tHistoryImage);
    TDSInput_Start(0,TDS_TRANS_SCALE); MovePacket(0);
    for (new i; i < 20; i++) MovePacket(0,0,KEY_RIGHT);
    Check(!TDS_HCount(s) && !gTDS[s][tsDirty], "D live frames do not capture history or dirty checkpoint");
    TDSInput_Finish(0,false); CheckHistoryImage(s); Check(!TDS_HCount(s), "cancelled transform adds no history");
    TDSInput_Start(0,TDS_TRANS_SCALE); MovePacket(0); MovePacket(0,0,KEY_RIGHT); TDSInput_Finish(0,true);
    Check(TDS_HCount(s) == 1 && TDS_HStep(0) == 1, "D confirmed group scale is one atomic operation"); CheckHistoryImage(s);
    Check(TDS_HStep(0,true) == 1 && gTDSElement[s][0][tdW] > 40.0, "D redo restores scaled group geometry");
    TDS_HStep(0); TDS_Select(0,0); TDS_HClear(0); TDS_HCapture(s,tHistoryImage);
    TDS_Layer(0,TDS_LAYER_FRONT); Check(TDS_HStep(0) == 1, "E undo layer rebuild"); CheckHistoryImage(s);
    s = HistoryFixture(); TDS_SelectAll(0,true); TDS_HCapture(s,tHistoryImage);
    new group = TDS_GroupCreate(0,"История TARKENSO");
    Check(group > 0 && TDS_HCount(s) == 1 && TDS_HStep(0) == 1 && !gTDS[s][tsGroupCount], "F undo group creation keeps elements"); CheckHistoryImage(s);
    Check(TDS_HStep(0,true) == 1 && TDS_GroupMembers(s,group) == 4, "F redo restores group ID/name/members");
    TDS_HClear(0); TDS_HCapture(s,tHistoryImage);
    TDS_GroupRename(0,group,"Новая группа"); TDS_GroupAssign(0,group,0,false); TDS_GroupDissolve(0,group);
    Check(TDS_HCount(s) == 3 && TDS_HStep(0) == 1 && TDS_GroupMembers(s,group) == 3, "F dissolve restores surviving membership");
    TDS_HStep(0); TDS_HStep(0); CheckHistoryImage(s);
    TDS_GroupSelect(0,group); TDS_HClear(0); TDS_HCapture(s,tHistoryImage);
    TDS_DeleteElement(0,0); TDS_HStep(0); CheckHistoryImage(s);

    s = HistoryFixture(); TDS_Select(0,0); TDS_HCapture(s,tHistoryImage);
    for (new i; i < TDS_HISTORY_DEPTH; i++)
    {
        new input[16]; format(input,sizeof(input),"%d",i+1); Check(TDS_ApplyInput(0,TDS_ALPHA,input), "G completed property operation");
    }
    Check(TDS_HCount(s) == 20, "G required depth is available");
    for (new i; i < 20; i++) Check(TDS_HStep(0) == 1, "G sequential undo"); CheckHistoryImage(s);
    for (new i; i < 20; i++) Check(TDS_HStep(0,true) == 1, "G sequential redo");
    Check((gTDSElement[s][0][tdColour]&255) == 20 && !TDS_HCount(s,true), "G all twenty operations reapplied");
    TDS_HStep(0); TDS_ApplyInput(0,TDS_CONTENT,"Русский \"текст\" C:\\ui 100%");
    Check(!TDS_HCount(s,true) && !TDS_HStep(0,true), "H new change discards the old Redo branch");
    TDS_HStep(0); TDS_HStep(0,true);
    Check(!strcmp(gTDSElement[s][0][tdText],"Русский \"текст\" C:\\ui 100%",false), "packed CP1251 text survives Undo/Redo");
    TDS_HClear(0);
    for (new i; i < 25; i++) { new input[8]; format(input,sizeof(input),"%d",i+1); TDS_ApplyInput(0,TDS_ALPHA,input); }
    Check(TDS_HCount(s) == 20, "I history ring remains bounded after overflow");
    for (new i; i < 20; i++) TDS_HStep(0);
    Check((gTDSElement[s][0][tdColour]&255) == 5 && !TDS_HStep(0), "I only the oldest operations are evicted");
    for (new i; i < 20; i++) TDS_HStep(0,true);
    Check((gTDSElement[s][0][tdColour]&255) == 25, "I overflow Redo endpoints remain correct");
    s = LayoutFixture(0); TDS_AddElement(0,TDS_RECT_TYPE); TDS_HStep(0);
    new fresh = TDS_AddElement(0,TDS_RECT_TYPE);
    Check(gTDSElement[s][fresh][tdId] == 2 && !TDS_HCount(s,true), "H forked creation never reuses an ID from discarded Redo");

    s = HistoryFixture(); TDS_DeleteElement(0,3); TDS_HCapture(s,tHistoryImage);
    new cursor = gTDSHistoryAt[s], next = gTDS[s][tsNextId], creates = tCreate, destroys = tDestroy;
    tFailAfter = 1;
    Check(!TDS_HStep(0) && gTDSHistoryAt[s] == cursor && gTDS[s][tsNextId] == next &&
        tCreate-creates == tDestroy-destroys, "J partial allocation failure leaves pointer/IDs/resources unchanged");
    tFailAfter = -1; CheckHistoryImage(s); Check(TDS_HStep(0) == 1, "J retry restores deleted element after resources return");
    TDS_DeleteElement(0,TDS_StateFind(gTDSElement[s],4)); TDS_HCapture(s,tHistoryImage); cursor = gTDSHistoryAt[s];
    new PlayerText:held[256], heldCount;
    for (new i; i < sizeof(held); i++)
    {
        new PlayerText:td = MockCreate(0,0.0,0.0,"other interface"); if (td == INVALID_PLAYER_TEXT_DRAW) break;
        held[heldCount++] = td;
    }
    Check(!TDS_HStep(0) && gTDSHistoryAt[s] == cursor,"J zero free handles cannot partially restore a deleted element"); CheckHistoryImage(s);
    for (new i; i < heldCount; i++) MockDestroy(0,held[i]);
    Check(TDS_HStep(0) == 1,"J restore succeeds after foreign UI releases handles");
    TDS_Select(0,0); TDS_HClear(0); TDS_HCheckpoint(s);
    TDS_ApplyInput(0,TDS_COLOUR,"12345678");
    Check(gTDS[s][tsDirty] && TDS_HStep(0) == 1 && !gTDS[s][tsDirty], "K Undo returns to saved semantic state");
    Check(TDS_HStep(0,true) == 1 && gTDS[s][tsDirty], "K Redo leaves checkpoint dirty");
    TDS_HCheckpoint(s); TDS_HStep(0); Check(gTDS[s][tsDirty], "K Undo after save differs from newly saved revision");
    TDS_HStep(0,true); Check(!gTDS[s][tsDirty], "K Redo returns to newly saved revision");
    TDS_HClear(0); TDS_ApplyInput(0,TDS_ALPHA,"5"); TDS_HStep(0); Check(!gTDS[s][tsDirty], "K clearing history retains checkpoint");
    heldCount = 0;
    for (new i; i < sizeof(held); i++)
    {
        new PlayerText:td = MockCreate(0,0.0,0.0,"other interface"); if (td == INVALID_PLAYER_TEXT_DRAW) break;
        held[heldCount++] = td;
    }
    creates = tCreate; TDS_ApplyInput(0,TDS_COLOUR,"ABCDEF77");
    Check(TDS_HStep(0) == 1 && tCreate == creates && !gTDS[s][tsDirty],"J property Undo succeeds without allocating in a full native pool");
    for (new i; i < heldCount; i++) MockDestroy(0,held[i]);
    TDS_SetMode(0,TDS_EDIT_MODE); TDS_HClear(0);
    TDS_Select(0,0); TDS_GridStep(0,1); TDS_GridSet(0,true); TDS_GridSet(0,false); TDS_GeoMenu(0);
    Check(!TDS_HCount(s) && !gTDS[s][tsDirty], "selection/grid/menus never pollute history");
    TDS_ClearProject(s); TDS_NewProject(0,"Other history"); Check(!TDS_HCount(s) && !TDS_HStep(0), "new project cannot restore previous project history");
    Check(!TDS_HStep(-1) && !TDS_HStep(MAX_PLAYERS), "invalid owner cannot access history");
    TDS_AddElement(0,TDS_TEXT_TYPE); gTDS[s][tsCreator] = 101; TDS_Grant(0,0,true); tFounder[0] = false;
    Check(!TDS_HStep(0) && !TDS_Active(0),"lost founder privilege cannot retain a foreign project's history through an ordinary editor grant");
    s = HistoryFixture(); TDS_Select(0,0); TDS_ApplyInput(0,TDS_COLOUR,"12345678");
    TDS_HMenu(0); Answer(0,TDS_HISTORY_MENU,1,3); Answer(0,TDS_HISTORY_CLEAR,0,0);
    Check(TDS_HCount(s) == 1,"history clear needs affirmative confirmation");
    TDS_HMenu(0); Answer(0,TDS_HISTORY_MENU,1,0); TDS_SetMode(0,TDS_EDIT_MODE); tNow += 200;
    Check(tSelectable[0][_:gTDSTool[s][10]] && !tSelectable[0][_:gTDSTool[s][9]],"toolbar availability follows Undo/Redo cursor");
    TDS_OnClick(0,gTDSTool[s][10]);
    Check(TDS_HCount(s) == 1 && !TDS_HCount(s,true),"compact REDO button routes to production history");
    tNow += 200; TDS_OnClick(0,gTDSTool[s][9]); Check(!gTDS[s][tsDirty],"compact UNDO button restores checkpoint");
    TDS_HMenu(0); Answer(0,TDS_HISTORY_MENU,1,3); Answer(0,TDS_HISTORY_CLEAR,1,0);
    Check(!TDS_HCount(s,true) && !TDS_HCount(s) && !gTDS[s][tsDirty],"confirmed history clear preserves saved-state checkpoint");
    Answer(0,TDS_HISTORY_MENU,0,0); Check(gTDS[s][tsDialog] == TDS_MAIN,"history Back returns to project menu");

    // L/O/P: reuse the production component capture and insertion engine.
    s = GeoFixture(true); TDS_Select(0,0); TDS_ApplyInput(0,TDS_CONTENT,"Компонент TARKENSO"); TDS_GroupSelect(0,1);
    Check(TDS_TplCapture(0), "L capture selected four-type component");
    new count, groups; Check(TDS_TplValid(count,groups) && count == 4 && groups == 1, "L component preserves complete group structure");
    TDS_ClearProject(s); TDS_NewProject(0,"Components"); TDS_HCheckpoint(s);
    gTDS[s][tsSnap] = true;
    Check(TDS_TplInsert(0,0) && gTDS[s][tsCount] == 4 && gTDS[s][tsGroupCount] == 1 && TDS_HCount(s) == 1, "N centered insertion is one history operation");
    Check(gTDSElement[s][0][tdX] == 275.0 && gTDSElement[s][0][tdY] == 200.0,"center placement preserves virtual screen center while Snap is enabled");
    gTDS[s][tsSnap] = false;
    TDS_HCapture(s,tHistoryImage);
    Check(TDS_TplInsert(0,1,100.0,100.0) && gTDS[s][tsCount] == 8 && gTDS[s][tsGroupCount] == 2, "O repeated insertion assigns independent element/group IDs");
    new g0 = TDS_GroupSlot(s,1), g1 = TDS_GroupSlot(s,2);
    Check(g0 >= 0 && g1 >= 0 && strcmp(gTDSGroup[s][g0][tgName],gTDSGroup[s][g1][tgName],false) != 0,
        "P colliding group names are renamed deterministically");
    Check(TDS_HStep(0) == 1 && gTDS[s][tsCount] == 4, "N Undo removes entire inserted component"); CheckHistoryImage(s);
    Check(TDS_HStep(0,true) == 1 && gTDS[s][tsCount] == 8 && TDS_GroupMembers(s,2) == 4, "N Redo restores original remapped IDs and membership");
    Check(gTDSTemplate[0][tdId] == 1 && !strcmp(gTDSTemplate[0][tdText],"Компонент TARKENSO",false), "O insertions never mutate source component");
    TDS_Select(0,TDS_StateFind(gTDSElement[s],1)); TDS_ApplyInput(0,TDS_CONTENT,"First instance only");
    Check(!strcmp(gTDSElement[s][TDS_StateFind(gTDSElement[s],5)][tdText],"Компонент TARKENSO",false), "O copies have independent strings");
    Check(TDS_TplInsert(0,2,10.0,15.0) && gTDSElement[s][TDS_StateFind(gTDSElement[s],9)][tdX] == 285.0 &&
        gTDSElement[s][TDS_StateFind(gTDSElement[s],9)][tdY] == 215.0,"relative insertion uses chosen offset from the active element anchor");
    gTDS[s][tsSnap] = true;
    Check(TDS_TplInsert(0,1,123.0,187.0) && gTDSElement[s][TDS_StateFind(gTDSElement[s],13)][tdX] == 120.0 &&
        gTDSElement[s][TDS_StateFind(gTDSElement[s],13)][tdY] == 190.0,"template point placement snaps one common origin without distorting members");
    gTDS[s][tsSnap] = false;
    TDS_HCapture(s,tHistoryImage); cursor = gTDSHistoryAt[s]; next = gTDS[s][tsNextId]; tFailAfter = 2;
    Check(!TDS_TplInsert(0,1,100.0,100.0) && gTDSHistoryAt[s] == cursor && gTDS[s][tsNextId] == next, "Q partial insertion resource failure does not consume IDs or history");
    tFailAfter = -1; CheckHistoryImage(s);
    Check(!TDS_TplInsert(0,1,-1000.0,1000.0) && !TDS_TplInsert(0,1,Float:0x7FC00000,100.0), "Q invalid placement and NaN reject atomically");
    new oldGroup = gTDSTemplate[0][tdGroup]; gTDSTemplate[0][tdGroup] = 999;
    Check(!TDS_TplInsert(0,0), "Q orphaned template membership rejected"); gTDSTemplate[0][tdGroup] = oldGroup;
    s = LayoutFixture(64); cursor = gTDSHistoryAt[s]; next = gTDS[s][tsNextId]; creates = tCreate;
    Check(!TDS_TplInsert(0,0) && gTDS[s][tsNextId] == next && gTDSHistoryAt[s] == cursor && tCreate == creates, "Q element limit is checked before mutation/allocation");
    s = LayoutFixture(32);
    for (new g; g < TDS_GROUPS; g++) { new name[32]; format(name,sizeof(name),"Pair_%d",g); TDS_Select(0,g*2); TDS_Select(0,g*2+1,true); TDS_GroupCreate(0,name); }
    Check(!TDS_TplInsert(0,0) && gTDS[s][tsCount] == 32 && gTDS[s][tsGroupCount] == 16, "Q group limit refuses entire component");

    // Max history depth and full models in eight independent editor leases.
    print("TDS HISTORY: start eight-session capacity check");
    TDS_Shutdown(); TDS_Init();
    for (new p; p < TDS_SESSIONS; p++)
    {
        Ready(p,true); tNow += 600; TDS_Open(p); TDS_NewProject(p,"History capacity");
        for (new i; i < TDS_ELEMENTS; i++) TDS_AddElement(p,E_TDS_TYPE:(i%4));
        new q = TDS_Session(p);
        for (new g; g < TDS_GROUPS; g++) { new name[32]; format(name,sizeof(name),"Capacity_%d",g); TDS_Select(p,g*2); TDS_Select(p,g*2+1,true); TDS_GroupCreate(p,name); }
        TDS_Select(p,0); TDS_HClear(p); TDS_HCheckpoint(q);
        for (new i; i < 20; i++) { new v[8]; format(v,sizeof(v),"%d",i); TDS_ApplyInput(p,TDS_ALPHA,v); }
        Check(TDS_HCount(q) == 20 && gTDS[q][tsCount] == 64 && gTDS[q][tsGroupCount] == 16, "eight sessions hold full models and maximum independent history");
    }
    creates = tCreate; destroys = tDestroy;
    for (new p; p < TDS_SESSIONS; p++)
    {
        new q = TDS_Session(p);
        for (new i; i < 20; i++) Check(TDS_HStep(p) == 1, "maximum-session Undo");
        Check(!gTDS[q][tsDirty] && TDS_ProjectValid(q), "full model reaches its own checkpoint");
        for (new i; i < 20; i++) Check(TDS_HStep(p,true) == 1, "maximum-session Redo");
    }
    Check(tCreate == creates && tDestroy == destroys, "repeated property history across eight full projects allocates zero handles");
    TDS_Shutdown();
    printf("TDS HISTORY RESULT: checks=%d cells_per_image=%d history_and_checkpoint_bytes=%d",tChecks-checks,TDS_H_CELLS,
        TDS_SESSIONS*(TDS_H_STATES*TDS_H_CELLS+TDS_H_CELLS+TDS_H_STATES*65+5)*4+TDS_H_CELLS*4);
    return 1;
}
