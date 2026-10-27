// Executes production layout/group code with synthetic player/TextDraw/input APIs.
stock CheckLayers(s, const ids[], count)
{
    new slots[TDS_ELEMENTS], bool:same = TDS_Order(s, slots) == count, bool:handles = true;
    for (new i; i < count; i++)
    {
        if (gTDSElement[s][slots[i]][tdId] != ids[i]) same = false;
        if (i && _:gTDSDraw[s][slots[i]] <= _:gTDSDraw[s][slots[i-1]]) handles = false;
    }
    Check(same, "expected back-to-front stable ID order");
    Check(handles, "native handles follow layer order");
    Check(TDS_ProjectValid(s), "layer/group model remains serializable");
    return 1;
}

stock LayoutFixture(count = 4)
{
    TDS_Close(0); Ready(0, true); tNow += 600; TDS_Open(0);
    new s = TDS_Session(0); TDS_NewProject(0, "Layer fixture");
    for (new i; i < count; i++) TDS_AddElement(0, E_TDS_TYPE:(i%4));
    Answer(0, TDS_MAIN, 1, 7); Answer(0, TDS_EDIT_MENU, 1, 1);
    gTDS[s][tsDirty] = false;
    return s;
}

stock RunLayerTests()
{
    new checks = tChecks; TDS_Init();
    new s = LayoutFixture(), creates, destroys;
    TDS_Select(0, 1); TDS_Layer(0, TDS_LAYER_UP);
    new order1[] = {1,3,2,4}; CheckLayers(s, order1, 4);
    Check(gTDS[s][tsSelected] == 1 && gTDSPicked[s][1] && gTDS[s][tsDirty], "layer rebuild preserves focus and selection");
    TDS_Layer(0, TDS_LAYER_DOWN);
    new order2[] = {1,2,3,4}; CheckLayers(s, order2, 4);
    TDS_Layer(0, TDS_LAYER_FRONT);
    new order3[] = {1,3,4,2}; CheckLayers(s, order3, 4);
    gTDS[s][tsDirty] = false; creates = tCreate;
    Check(TDS_Layer(0, TDS_LAYER_FRONT) && tCreate == creates && !gTDS[s][tsDirty], "layer boundary is a true no-op");
    TDS_Layer(0, TDS_LAYER_BACK);
    new order4[] = {2,1,3,4}; CheckLayers(s, order4, 4);
    Check(!TDS_Layer(0, E_TDS_LAYER:999), "invalid layer action rejected");
    new handles[TDS_ELEMENTS], orders[TDS_ELEMENTS];
    for (new e; e < 4; e++) handles[e] = _:gTDSDraw[s][e]; TDS_SaveOrders(s, orders);
    tFailAfter = 1; gTDS[s][tsDirty] = false;
    Check(!TDS_Layer(0, TDS_LAYER_FRONT) && !gTDS[s][tsDirty], "partial layer allocation failure rolls back dirty/model");
    tFailAfter = -1;
    new bool:kept = true;
    for (new e; e < 4; e++) if (_:gTDSDraw[s][e] != handles[e] || gTDSElement[s][e][tdOrder] != orders[e] ||
        !MockValid(0, gTDSDraw[s][e])) kept = false;
    Check(kept, "allocation failure keeps every old handle and order");

    TDS_Select(0, 2); TDS_ApplyInput(0, TDS_COLOUR, "12ABCDEF");
    new original[E_TDS_ELEMENT];
    for (new E_TDS_ELEMENT:f; f < E_TDS_ELEMENT; f++) original[f] = gTDSElement[s][2][f];
    new next = gTDS[s][tsNextId], copy = TDS_Duplicate(0);
    Check(copy >= 0 && gTDSElement[s][copy][tdId] == next && TDS_PickCount(s) == 1 && gTDS[s][tsSelected] == copy,
        "duplicate receives unique ID and sole focus");
    Check(gTDSElement[s][copy][tdX] == original[tdX]+10.0 && gTDSElement[s][copy][tdY] == original[tdY]+10.0,
        "duplicate offsets X/Y");
    new bool:equal = true;
    for (new E_TDS_ELEMENT:f; f < E_TDS_ELEMENT; f++)
        if (f != tdId && f != tdOrder && f != tdX && f != tdY && f != tdGroup && gTDSElement[s][copy][f] != original[f]) equal = false;
    Check(equal, "duplicate retains every applicable scalar/text property");
    new order5[] = {2,1,3,5,4}; CheckLayers(s, order5, 5);
    TDS_ApplyInput(0, TDS_COLOUR, "FFFFFFFF"); TDS_ApplyInput(0, TDS_POSITION, "640 448");
    Check(gTDSElement[s][2][tdColour] == original[tdColour] && gTDSElement[s][2][tdX] == original[tdX], "copy edits are independent");
    new edge = TDS_Duplicate(0);
    Check(gTDSElement[s][edge][tdX] == 640.0 && gTDSElement[s][edge][tdY] == 448.0, "duplicate clamps at canvas boundary");
    next = gTDS[s][tsNextId]; new count = gTDS[s][tsCount]; tFailAfter = 2; gTDS[s][tsDirty] = false;
    Check(TDS_Duplicate(0) < 0 && gTDS[s][tsCount] == count && gTDS[s][tsNextId] == next &&
        !gTDS[s][tsDirty] && TDS_ProjectValid(s), "duplicate resource failure has no partial model or consumed ID");
    tFailAfter = -1;

    TDS_SelectAll(0, false); Check(TDS_PickCount(s) == 0 && gTDS[s][tsSelected] == -1, "clear selection");
    TDS_Select(0, 0); TDS_Select(0, 1, true); TDS_Select(0, 2, true);
    Check(TDS_PickCount(s) == 3 && gTDS[s][tsSelected] == 2 && !gTDS[s][tsDirty], "multi selection is transient and keeps active focus");
    TDS_Select(0, 2, true);
    Check(TDS_PickCount(s) == 2 && gTDS[s][tsSelected] != 2, "removing primary chooses another picked element");
    Check(!TDS_Select(0, -1) && !TDS_Select(0, TDS_ELEMENTS), "selection indices bounded");
    TDS_SelectAll(0, true); Check(TDS_PickCount(s) == count, "select all existing elements");
    TDS_DeleteElement(0, edge); Check(TDS_PickCount(s) == count-1 && !gTDSPicked[s][edge], "deleted slot removed from selection");

    // Group four distinct types through the real dialog dispatcher.
    s = LayoutFixture(); TDS_SelectAll(0, true); TDS_GroupList(0);
    Answer(0, TDS_GROUP_LIST, 1, 0); Answer(0, TDS_GROUP_NAME, 1, -1, "Логотип TARKENSO");
    new group = gTDS[s][tsGroupFocus];
    Check(group == 1 && gTDS[s][tsGroupCount] == 1 && TDS_GroupMembers(s, group) == 4 &&
        gTDS[s][tsSelectedGroup] == group, "dialog creates/selects four-type group");
    Check(!TDS_GroupRename(0, group, "../bad") && !TDS_GroupRename(0, 999, "bad"), "group names and IDs validated");
    Check(TDS_GroupRename(0, group, "TARKENSO_LOGO"), "group rename");
    new groupSlot = TDS_GroupSlot(s, group);
    TDS_Select(0, 0); copy = TDS_Duplicate(0);
    Check(gTDSElement[s][copy][tdGroup] == 0 && TDS_GroupMembers(s, group) == 4, "duplicate is independent of original group");
    TDS_SelectAll(0, false); TDS_Select(0, 0); TDS_Select(0, copy, true);
    Check(!TDS_GroupCreate(0, "Mixed"), "group creation cannot silently reassign an existing member");
    TDS_GroupSelect(0, group); TDS_GroupAssign(0, group, 0, false);
    Check(TDS_GroupMembers(s, group) == 3 && !gTDSElement[s][0][tdGroup] && !gTDS[s][tsSelectedGroup], "exclude member invalidates whole-group context");
    TDS_GroupAssign(0, group, 0, true); TDS_GroupSelect(0, group);
    Check(TDS_GroupMembers(s, group) == 4 && TDS_PickCount(s) == 4, "add back and select whole group");
    gTDS[s][tsDirty] = false; creates = tCreate; destroys = tDestroy;
    for (new step; step < sizeof(gTDSSteps); step++)
    {
        TDS_SetStep(0, step); TDSInput_Start(0); MovePacket(0); MovePacket(0, 0, KEY_RIGHT);
        new bool:moved = true;
        for (new e; e < 4; e++) if (floatabs(gTDSElement[s][e][tdX] - 270.0 - gTDSSteps[step]) > 0.0001) moved = false;
        Check(moved && !gTDS[s][tsDirty], "group moves equally at each of five steps without premature dirty");
        Check(!TDS_GroupDissolve(0, group) && !TDS_Layer(0, TDS_LAYER_FRONT) && TDS_Duplicate(0) < 0 &&
            !TDS_SelectAll(0, false), "conflicting layout mutations blocked during move");
        TDSInput_Finish(0, false);
        new bool:restored = true;
        for (new e; e < 4; e++) if (gTDSElement[s][e][tdX] != 270.0 || gTDSElement[s][e][tdY] != 200.0) restored = false;
        Check(restored && !gTDS[s][tsDirty], "cancel restores every group member and clean state");
    }
    Check(tCreate == creates && tDestroy == destroys, "group motion/cancel never recreates handles");
    TDS_SetStep(0, 2); TDSInput_Start(0); MovePacket(0); MovePacket(0, KEY_UP); TDSInput_OnKeys(0, KEY_YES);
    Check(gTDS[s][tsDirty] && gTDSElement[s][0][tdY] == 199.0 && gTDSElement[s][3][tdY] == 199.0 &&
        gTDSElement[s][copy][tdY] == 210.0, "confirm changes exactly the group, excluding its ungrouped copy");
    // Common boundary correction must preserve offsets and never move a subset.
    TDS_Select(0, 0); TDS_ApplyInput(0, TDS_POSITION, "639.5 100"); TDS_GroupSelect(0, group);
    gTDS[s][tsDirty] = false; TDSInput_Start(0); MovePacket(0); MovePacket(0, 0, KEY_RIGHT);
    Check(gTDSElement[s][0][tdX] == 640.0 && gTDSElement[s][1][tdX] == 270.5, "limiting member clamps the entire group's delta");
    new touches = tTouches[0][_:gTDSDraw[s][1]]; MovePacket(0, 0, KEY_RIGHT);
    Check(gTDSElement[s][1][tdX] == 270.5 && tTouches[0][_:gTDSDraw[s][1]] == touches, "group stops at the first member's boundary");
    MovePacket(0, KEY_UP, KEY_RIGHT);
    Check(gTDSElement[s][0][tdY] == 100.0 && gTDSElement[s][1][tdY] == 199.0, "blocked group diagonal stops the whole vector");
    TDSInput_Finish(0, false);
    Check(gTDSElement[s][0][tdX] == 639.5 && gTDSElement[s][1][tdX] == 270.0 && !gTDS[s][tsDirty], "boundary cancellation is atomic");
    Check(!TDS_Move(0, 1.0, 0.0) && gTDSElement[s][1][tdX] == 270.0, "out-of-range toolbar move cannot move only a subset");
    Check(TDS_Encode(s) && strfind(gTDSGroupWire, "[1,", false) >= 0, "group codec encodes stable group ID/member IDs");
    new badGroup = gTDSElement[s][0][tdGroup]; gTDSElement[s][0][tdGroup] = 999;
    Check(!TDS_ProjectValid(s), "orphaned group reference cannot be saved/exported"); gTDSElement[s][0][tdGroup] = badGroup;
    TDS_Select(0, copy); TDS_DeleteElement(0, copy);
    TDS_GroupDissolve(0, group);
    Check(gTDS[s][tsGroupCount] == 0 && gTDSGroup[s][groupSlot][tgId] == 0 && gTDS[s][tsCount] == 4,
        "dissolve retains all TextDraw elements");
    TDS_SelectAll(0, true); new later = TDS_GroupCreate(0, "Later");
    Check(later > group, "group IDs never recycled after dissolve");
    for (new e; e < 4; e++) TDS_DeleteElement(0, e);
    Check(gTDS[s][tsGroupCount] == 0 && TDS_PickCount(s) == 0 && !gTDS[s][tsSelectedGroup] && TDS_ProjectValid(s),
        "last member deletion removes empty group without dangling membership");

    s = LayoutFixture(); TDS_Select(0, 0); TDS_Select(0, 2, true); group = TDS_GroupCreate(0, "Scattered");
    TDS_Layer(0, TDS_LAYER_UP); new scattered1[] = {2,1,4,3}; CheckLayers(s, scattered1, 4);
    TDS_Layer(0, TDS_LAYER_DOWN); CheckLayers(s, order2, 4);
    TDS_Layer(0, TDS_LAYER_FRONT); new scattered2[] = {2,4,1,3}; CheckLayers(s, scattered2, 4);
    TDS_Layer(0, TDS_LAYER_BACK); new scattered3[] = {1,3,2,4}; CheckLayers(s, scattered3, 4);
    Check(TDS_GroupMembers(s, group) == 2 && gTDSElement[s][0][tdGroup] == group && gTDSElement[s][2][tdGroup] == group,
        "scattered group layer operations preserve membership/internal relative order");
    TDS_SelectionMenu(0); Answer(0, TDS_SELECTION, 1, 1); tNow += 200;
    TDS_OnClick(0, gTDSDraw[s][1]);
    Check(TDS_PickCount(s) == 3 && gTDS[s][tsMultiPick] && !gTDS[s][tsSelectedGroup], "mouse multi mode toggles into existing selection");
    tNow += 200; TDS_OnClick(0, gTDSDraw[s][1]); Check(TDS_PickCount(s) == 2, "mouse multi mode toggles out");
    TDS_LayersMenu(0); Answer(0, TDS_LAYERS, 1, 0);
    Check(gTDS[s][tsRows][0] == 0 && gTDS[s][tsRows][1] == 2, "UI element list follows layers rather than storage slots");
    Answer(0, TDS_LIST, 1, 2);
    Check(gTDS[s][tsSelected] == 1 && TDS_PickCount(s) == 1, "ordinary list selection remains single-focus property editing");
    Ready(1, true); tNow += 600; TDS_Open(1); TDS_NewProject(1, "Other layout"); TDS_AddElement(1, TDS_TEXT_TYPE);
    new other = TDS_Session(1); TDS_GroupSelect(0, group); TDS_SelectAll(1, false);
    Check(TDS_PickCount(s) == 2 && !TDS_PickCount(other) && !TDS_GroupSelect(1, group), "group IDs and selection are project/session local");
    TDS_Close(1);

    s = LayoutFixture();
    for (new type; type < 4; type++)
    {
        TDS_Select(0, type);
        if (!type) TDS_ApplyInput(0, TDS_CONTENT, "Копия \"текста\" C:\\ui 100%");
        if (type == 3) { TDS_ApplyInput(0, TDS_ROTATION, "10 -20 30 1.5"); TDS_ApplyInput(0, TDS_VEH_COLOURS, "7 42"); }
        for (new E_TDS_ELEMENT:f; f < E_TDS_ELEMENT; f++) original[f] = gTDSElement[s][type][f];
        copy = TDS_Duplicate(0); equal = copy >= 0;
        if (copy >= 0) for (new E_TDS_ELEMENT:f; f < E_TDS_ELEMENT; f++)
            if (f != tdId && f != tdOrder && f != tdX && f != tdY && f != tdGroup && gTDSElement[s][copy][f] != original[f]) equal = false;
        Check(equal, "all four duplicate types preserve complete properties including strings/model preview");
    }
    s = LayoutFixture(64); next = gTDS[s][tsNextId]; creates = tCreate;
    Check(TDS_Duplicate(0) < 0 && gTDS[s][tsCount] == 64 && gTDS[s][tsNextId] == next && creates == tCreate,
        "element limit rejected before mutation/allocation");
    for (new g; g < TDS_GROUPS; g++)
    {
        new name[32]; format(name, sizeof(name), "Group_%d", g);
        TDS_Select(0, g*2); TDS_Select(0, g*2+1, true); Check(TDS_GroupCreate(0, name) > 0, "group capacity member pair");
    }
    TDS_Select(0, 32); TDS_Select(0, 33, true);
    Check(!TDS_GroupCreate(0, "Overflow") && gTDS[s][tsGroupCount] == TDS_GROUPS && !gTDSElement[s][32][tdGroup], "group limit has no partial membership");
    Check(TDS_ProjectValid(s) && TDS_Encode(s), "maximum project/group count fits bounded codec");
    // Exercise all list views with maximum-length IDs and group captions.
    for (new e; e < TDS_ELEMENTS; e++)
    {
        gTDSElement[s][e][tdId] = 2147483500+e;
        if (gTDSElement[s][e][tdGroup]) gTDSElement[s][e][tdGroup] += 2147483600;
    }
    for (new g; g < TDS_GROUPS; g++)
    {
        gTDSGroup[s][g][tgId] += 2147483600;
        format(gTDSGroup[s][g][tgName], TDS_NAME, "Long_group_name_12345678901234%02d", g);
    }
    gTDS[s][tsNextId] = 2147483646; gTDS[s][tsNextGroupId] = 2147483646;
    TDS_List(0); TDS_PickList(0); TDS_GroupList(0); gTDS[s][tsGroupFocus] = 2147483601;
    TDS_GroupMenu(0); TDS_PickList(0, true);
    Check(!tDialogOverflow && TDS_ProjectValid(s), "64-element/16-group lists fit client limits even with long IDs/names");
    // A real response has dismissed the native dialog before movement starts.
    HidePlayerDialog(0); gTDS[s][tsDialog] = 0;
    TDS_GroupSelect(0, 2147483601); TDSInput_Start(0); MovePacket(0); MovePacket(0, 0, KEY_RIGHT);
    TDS_Reset(0, false); Ready(0, true); tNow += 600; TDS_Open(0); s = TDS_Session(0);
    Check(!gTDS[s][tsGroupCount] && !TDS_PickCount(s) && !gTDS[s][tsMoveCount] && !gTDS[s][tsProject], "reconnect cannot inherit groups/selection/move snapshot");
    TDS_Shutdown();
    printf("TDS LAYOUT RESULT: checks=%d (mocked UI/handles/input)", tChecks-checks);
    return 1;
}
