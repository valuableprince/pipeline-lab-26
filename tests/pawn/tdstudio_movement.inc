// Synthetic input only. These assertions do NOT test physical GTA key bindings.
stock MovePacket(playerid, ud = 0, lr = 0, milliseconds = TDS_INPUT_MS, KEY:keys = KEY:0)
{
    tNow += milliseconds; tUD[playerid] = ud; tLR[playerid] = lr; tKeys[playerid] = keys;
    return TDSInput_OnUpdate(playerid);
}

stock MoveFixture()
{
    TDS_Close(0); Ready(0, true); tNow += 600; TDS_Open(0);
    new s = TDS_Session(0);
    TDS_NewProject(0, "Movement"); TDS_AddElement(0, TDS_SPRITE_TYPE);
    Answer(0, TDS_MAIN, 1, 7); Answer(0, TDS_EDIT_MENU, 1, 1);
    gTDS[s][tsDirty] = false;
    return s;
}

stock CheckMovementProperties(s, e, const before[E_TDS_ELEMENT])
{
    new bool:equal = true;
    for (new E_TDS_ELEMENT:f; f < E_TDS_ELEMENT; f++)
        if (f != tdX && f != tdY && gTDSElement[s][e][f] != before[f]) equal = false;
    Check(equal, "movement preserves every non-position property");
    return 1;
}

stock RunMovementTests()
{
    new startChecks = tChecks;
    TDS_Init();
    new s = MoveFixture(), e = gTDS[s][tsSelected];
    Check(!TDSInput_Start(-1) && !TDSInput_Start(MAX_PLAYERS), "move invalid player bounds");
    gTDS[s][tsSelected] = -1; TDSInput_Start(0);
    Check(gTDS[s][tsMode] != TDS_MOVE_MODE && tControl[0], "move requires selection");
    gTDS[s][tsSelected] = TDS_ELEMENTS; TDSInput_Start(0);
    Check(gTDS[s][tsMode] != TDS_MOVE_MODE, "move rejects out-of-range selection");
    gTDS[s][tsSelected] = e;
    tControl[0] = false; TDSInput_Start(0);
    Check(!gTDS[s][tsMoveFreeze] && !tControl[0], "preexisting freeze is never acquired");
    tControl[0] = true; tFreezeFail = true;
    Check(!TDSInput_Start(0) && tControl[0] && !gTDS[s][tsMoveFreeze], "failed freeze leaves keyboard mode disabled");
    tFreezeFail = false;
    tDialog[0] = 6000;
    Check(!TDSInput_Start(0) && tDialog[0] == 6000, "foreign dialog cannot be stolen");
    tDialog[0] = INVALID_DIALOG_ID;
    gTDS[s][tsBusy] = 1;
    Check(!TDSInput_Start(0) && tControl[0], "pending DB work excludes input"); gTDS[s][tsBusy] = 0;
    gTDS[s][tsStep] = 999;
    Check(!TDSInput_Start(0), "move rejects invalid step before array access"); TDS_SetStep(0, 2);
    Check(!TDS_SetStep(0, -1) && !TDS_SetStep(0, 5), "step indices are bounded");

    TDS_SetMode(0, TDS_EDIT_MODE); tNow += 200;
    TDS_OnClick(0, gTDSTool[s][0]);
    Check(gTDS[s][tsMode] == TDS_MOVE_MODE && !gTDS[s][tsMouse] && !tControl[0], "status click swaps cursor for frozen keyboard mode");
    Check(tShown[0][_:gTDSTool[s][0]] && tShown[0][_:gTDSTool[s][1]] &&
        !tShown[0][_:gTDSTool[s][8]], "keyboard reuses status and bottom hint strip");
    new Float:x = gTDSElement[s][e][tdX], Float:y = gTDSElement[s][e][tdY];
    MovePacket(0, 0, KEY_RIGHT); TDSInput_OnKeys(0, KEY_YES);
    Check(gTDSElement[s][e][tdX] == x && gTDS[s][tsMode] == TDS_MOVE_MODE, "entry waits for neutral input including held accept");
    MovePacket(0);
    new creates = tCreate, destroys = tDestroy, touches = tTouches[0][_:gTDSDraw[s][e]];
    for (new i; i < 49; i++) MovePacket(0, 0, KEY_RIGHT, 1);
    Check(gTDSElement[s][e][tdX] == x && tTouches[0][_:gTDSDraw[s][e]] == touches, "flood below 50ms causes no render");
    MovePacket(0, 0, KEY_RIGHT, 1);
    Check(gTDSElement[s][e][tdX] == x + 1.0, "first 50ms quantum moves one configured step");
    MovePacket(0, 0, KEY_LEFT); MovePacket(0, KEY_UP); MovePacket(0, KEY_DOWN);
    Check(gTDSElement[s][e][tdX] == x && gTDSElement[s][e][tdY] == y, "all four directions affect only selected coordinates");
    for (new i; i < 20; i++) MovePacket(0, 0, KEY_RIGHT);
    Check(gTDSElement[s][e][tdX] == x + 20.0, "held action repeats at predictable 20 steps per second");
    MovePacket(0); MovePacket(0);
    Check(gTDSElement[s][e][tdX] == x + 20.0, "release stops without an extra move");
    MovePacket(0, KEY_UP, KEY_RIGHT);
    Check(floatabs(gTDSElement[s][e][tdX] - x - 20.707106) < 0.0001 &&
        floatabs(gTDSElement[s][e][tdY] - y + 0.707106) < 0.0001, "diagonal velocity is normalized");
    Check(tCreate == creates && tDestroy == destroys, "continuous movement allocates no handles");
    Check(!gTDS[s][tsDirty], "unconfirmed movement does not dirty clean project");
    Check(!TDS_ApplyInput(0, TDS_COLOUR, "00000000") && !TDS_Toggle(0, TDS_SELECTABLE) &&
        !TDS_DeleteElement(0, e) && TDS_AddElement(0, TDS_TEXT_TYPE) == -1 && !TDS_Export(0), "other mutations/export excluded during move transaction");
    new selected = gTDS[s][tsSelected];
    TDS_OnClick(0, gTDSTool[s][4]);
    Check(gTDS[s][tsSelected] == selected && gTDS[s][tsMode] == TDS_MOVE_MODE, "stale cursor clicks consumed during keyboard input");
    TDSInput_OnKeys(0, KEY_NO);
    Check(gTDSElement[s][e][tdX] == x && gTDSElement[s][e][tdY] == y && !gTDS[s][tsDirty], "N cancels exact position and dirty state");
    Check(tControl[0] && gTDS[s][tsMode] == TDS_EDIT_MODE && gTDS[s][tsMouse] &&
        tSelectable[0][_:gTDSTool[s][1]] && tShown[0][_:gTDSTool[s][8]], "cancel restores control and original toolbar");

    for (new step; step < sizeof(gTDSSteps); step++)
    {
        Check(TDS_SetStep(0, step) && gTDSElement[s][e][tdX] == x, "each step can change without moving");
        TDSInput_Start(0); MovePacket(0); MovePacket(0, 0, KEY_RIGHT);
        Check(floatabs(gTDSElement[s][e][tdX] - x - gTDSSteps[step]) < 0.0001, "all five step magnitudes used by real controller");
        TDSInput_Finish(0, false);
    }
    TDS_SetStep(0, 2); TDSInput_Start(0); MovePacket(0);
    x = gTDSElement[s][e][tdX];
    MovePacket(0, 0, 0, 100, KEY_SPRINT);
    Check(gTDS[s][tsStep] == 3 && gTDSElement[s][e][tdX] == x, "Space cycles step without displacement");
    for (new i; i < 4; i++) MovePacket(0, 0, 0, 50, KEY_SPRINT);
    Check(gTDS[s][tsStep] == 3, "held Space does not cycle repeatedly");
    MovePacket(0); MovePacket(0, 0, 0, 100, KEY_SPRINT);
    Check(gTDS[s][tsStep] == 4, "next Space edge advances step");
    TDSInput_Finish(0, false);

    TDS_SetStep(0, 2); TDSInput_Start(0); MovePacket(0); MovePacket(0, 0, KEY_RIGHT, 75);
    Check(floatabs(gTDSElement[s][e][tdX] - x - 1.5) < 0.0001, "velocity scales with normal packet interval");
    MovePacket(0, 0, KEY_RIGHT, 200);
    Check(floatabs(gTDSElement[s][e][tdX] - x - 3.5) < 0.0001, "slow packet caps catch-up at two quanta");
    MovePacket(0, 0, KEY_RIGHT, 1000);
    Check(floatabs(gTDSElement[s][e][tdX] - x - 3.5) < 0.0001 && !gTDS[s][tsMoveReady], "pause cannot replay stale direction");
    MovePacket(0, 0, KEY_RIGHT);
    Check(!gTDS[s][tsMoveReady], "resumed held input must be released");
    MovePacket(0); MovePacket(0, 0, KEY_RIGHT);
    Check(floatabs(gTDSElement[s][e][tdX] - x - 4.5) < 0.0001, "neutral packet rearms movement after stall");
    MovePacket(0, 999, -999);
    Check(floatabs(gTDSElement[s][e][tdX] - x - 4.5) < 0.0001, "invalid axes ignored");
    TDSInput_OnKeys(0, KEY_SECONDARY_ATTACK);
    Check(gTDS[s][tsDirty] && gTDS[s][tsMode] == TDS_EDIT_MODE, "Enter action confirms into working model");

    gTDS[s][tsDirty] = false;
    TDS_ApplyInput(0, TDS_POSITION, "639.9 447.9"); gTDS[s][tsDirty] = false;
    TDS_SetStep(0, 4); TDSInput_Start(0); MovePacket(0); MovePacket(0, KEY_DOWN, KEY_RIGHT);
    Check(gTDSElement[s][e][tdX] == 640.0 && gTDSElement[s][e][tdY] == 448.0, "coordinates clamp at upper bounds");
    touches = tTouches[0][_:gTDSDraw[s][e]]; MovePacket(0, KEY_DOWN, KEY_RIGHT);
    Check(tTouches[0][_:gTDSDraw[s][e]] == touches, "staying at boundary sends no redundant draw update");
    MovePacket(0, KEY_UP, KEY_RIGHT);
    Check(gTDSElement[s][e][tdX] == 640.0 && gTDSElement[s][e][tdY] < 448.0, "one blocked axis does not block the other");
    TDSInput_Finish(0, false);
    TDS_ApplyInput(0, TDS_POSITION, "0.1 0.1"); gTDS[s][tsDirty] = false;
    TDSInput_Start(0); MovePacket(0); MovePacket(0, KEY_UP, KEY_LEFT);
    Check(gTDSElement[s][e][tdX] == 0.0 && gTDSElement[s][e][tdY] == 0.0, "coordinates clamp at lower bounds");
    TDSInput_Finish(0, false);

    // Each type shares the same coordinates; text alignment endpoints move too.
    TDS_ClearProject(s); TDS_NewProject(0, "Four types");
    for (new type; type < 4; type++)
    {
        e = TDS_AddElement(0, E_TDS_TYPE:type);
        new before[E_TDS_ELEMENT];
        for (new E_TDS_ELEMENT:f; f < E_TDS_ELEMENT; f++) before[f] = gTDSElement[s][e][f];
        gTDS[s][tsDirty] = false; TDS_SetStep(0, 2);
        TDSInput_Start(0); MovePacket(0); MovePacket(0, 0, KEY_RIGHT);
        Check(gTDSElement[s][e][tdX] == before[tdX] + 1.0 &&
            tX[0][_:gTDSDraw[s][e]] == gTDSElement[s][e][tdX], "all element types move in working model and renderer");
        if (!type) Check(tW[0][_:gTDSDraw[s][e]] == before[tdX] + 1.0 + before[tdW], "moving text updates hitbox endpoint");
        CheckMovementProperties(s, e, before);
        TDSInput_OnKeys(0, KEY_YES);
        Check(gTDS[s][tsDirty] && gTDS[s][tsMode] == TDS_EDIT_MODE, "Y confirms each element type");
    }
    gTDS[s][tsDirty] = false; TDSInput_Start(0); MovePacket(0); TDSInput_OnKeys(0, KEY_YES);
    Check(!gTDS[s][tsDirty], "confirming an unchanged position does not dirty project");
    gTDS[s][tsDirty] = true; TDSInput_Start(0); MovePacket(0); MovePacket(0, KEY_UP); TDSInput_OnKeys(0, KEY_NO);
    Check(gTDS[s][tsDirty], "cancel preserves earlier unsaved edits");

    x = gTDSElement[s][e][tdX]; y = gTDSElement[s][e][tdY];
    TDSInput_Start(0); MovePacket(0); MovePacket(0, 0, KEY_RIGHT); tNow += 600; TDS_Open(0);
    Check(gTDS[s][tsMode] == TDS_EDIT_MODE && tControl[0] && gTDSElement[s][e][tdX] == x, "tdstudio command is a cancel escape hatch");
    TDSInput_Start(0); MovePacket(0); MovePacket(0, KEY_UP); TDS_Main(0);
    Check(gTDS[s][tsMode] == TDS_MENU_MODE && gTDSElement[s][e][tdY] == y && tControl[0], "opening a menu rolls back and releases input");

    Ready(1, true); tNow += 600; TDS_Open(1); TDS_NewProject(1, "Independent");
    new other = TDS_Session(1), second = TDS_AddElement(1, TDS_MODEL_TYPE);
    TDS_SetStep(0, 2); TDS_SetStep(1, 0); TDSInput_Start(0); TDSInput_Start(1);
    MovePacket(0); MovePacket(1); MovePacket(0, 0, KEY_RIGHT); MovePacket(1, KEY_UP);
    Check(gTDSElement[s][e][tdX] > x && gTDSElement[other][second][tdX] == 270.0 &&
        gTDSElement[other][second][tdY] < 200.0, "simultaneous sessions have independent direction and step");
    TDSInput_Finish(0, false);
    Check(gTDS[other][tsMode] == TDS_MOVE_MODE && !tControl[1], "finishing one session does not release another");
    TDS_Close(1);
    Check(tControl[1] && !TDS_Active(1), "close releases only owned freeze");

    TDSInput_Start(0); MovePacket(0); MovePacket(0, KEY_UP); gAdminFrozen[0] = true;
    TDSInput_OnUpdate(0);
    Check(!TDS_Active(0) && !tControl[0], "admin freeze during move is preserved on cleanup");
    gAdminFrozen[0] = false; MoveFixture(); TDSInput_Start(0); MovePacket(0);
    gAuthToken[0]++; new releases = tUnfreeze; TDS_Poll();
    Check(!TDS_Active(0) && tUnfreeze == releases, "expired lease never unfreezes a replacement identity");
    MoveFixture(); TDSInput_Start(0); MovePacket(0); tFounder[0] = false; TDSInput_OnUpdate(0);
    Check(!TDS_Active(0) && tControl[0], "revocation closes and releases this account's own freeze");
    MoveFixture(); TDSInput_Start(0); MovePacket(0); tConnected[0] = false;
    releases = tUnfreeze; TDS_Reset(0, false);
    Check(!TDS_Active(0) && releases == tUnfreeze, "disconnect cleans temporary state without native unfreeze");
    s = MoveFixture(); TDSInput_Start(0); MovePacket(0); gTDS[s][tsSelected] = TDS_ELEMENTS;
    TDSInput_OnUpdate(0);
    Check(!TDS_Active(0) && tControl[0], "stale selected index closes safely");
    s = MoveFixture(); TDSInput_Start(0); MovePacket(0); gTDSElement[s][0][tdId]++;
    TDSInput_OnUpdate(0);
    Check(!TDS_Active(0) && tControl[0], "stable element ID rejects replaced slot");
    MoveFixture(); TDSInput_Start(0); MovePacket(0); tState[0] = PLAYER_STATE_DRIVER;
    TDSInput_OnUpdate(0);
    Check(!TDS_Active(0) && !tControl[0], "state change does not release control in a vehicle");
    s = MoveFixture(); TDSInput_Start(0); MovePacket(0); MovePacket(0, KEY_UP);
    TDS_ClearProject(s);
    Check(!gTDS[s][tsProject] && gTDS[s][tsMode] == TDS_MENU_MODE && tControl[0], "project clear discards move transaction and resources");
    s = MoveFixture(); TDSInput_Start(0); MovePacket(0); MockDestroy(0, gTDSDraw[s][0]);
    MovePacket(0, 0, KEY_RIGHT);
    Check(!TDS_Active(0) && tControl[0], "missing native handle fails safely and releases control");
    s = MoveFixture(); TDSInput_Start(0); MovePacket(0); TDS_Shutdown();
    Check(!TDS_Active(0) && tControl[0] && !gTDS[s][tsMoveFreeze], "shutdown releases keyboard ownership");
    printf("TDS MOVEMENT RESULT: checks=%d (synthetic input; no GTA client)", tChecks - startChecks);
    return 1;
}
