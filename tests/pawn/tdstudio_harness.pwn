#include "tdstudio_test_host.inc"
#include "tdstudio_movement.inc"
#include "tdstudio_layers.inc"

main() {}
public OnGameModeInit()
{
    // Run after server initialisation; the Python fixture stops this process.
    SetTimer("TDS_RunTests", 100, false);
    return 1;
}
forward TDS_RunTests();
public TDS_RunTests()
{
    Check(TDS_Init() == 1 && TDS_Init() == 1, "idempotent session monitor init");
    Ready(0, true); Ready(1); Ready(2);
    Check(!TDS_Allowed(-1) && !TDS_Allowed(MAX_PLAYERS), "invalid player bounds");
    Check(!TDS_Allowed(1), "authenticated ordinary staff denied without explicit grant");
    TDS_Grant(1, 2, true);
    Check(!TDS_Allowed(2), "non-founder cannot grant");
    tAudit = false; TDS_Grant(0, 1, true);
    Check(!TDS_Allowed(1), "grant fails closed when audit unavailable");
    tAudit = true;
    TDS_Grant(0, 1, true);
    Check(TDS_Allowed(1), "explicit developer grant permits non-admin");
    gAuthState[1] = 0;
    Check(!TDS_Allowed(1), "grant cannot bypass login");
    gAuthState[1] = AUTH_READY;
    gDBPrimaryLoaded[1] = false;
    Check(!TDS_Allowed(1), "grant requires loaded DB account");
    gDBPrimaryLoaded[1] = true;
    gDBPrimaryAccountId[1]++;
    Check(!TDS_Allowed(1), "grant is bound to account ID");
    gDBPrimaryAccountId[1]--;
    new PlayerText:foreign = MockCreate(0, 1.0, 1.0, "other UI");
    Check(_:foreign == 0, "valid handle zero reserved by another UI");
    TDS_Open(0);
    new s = TDS_Session(0);
    Check(s >= 0 && gTDS[s][tsDialog] == TDS_MAIN, "command opens main");
    Answer(0, TDS_MAIN, 1, 0);
    Check(gTDS[s][tsDialog] == TDS_NEW, "main opens project dialog");
    Answer(0, TDS_NEW, 1, -1, "First project");
    Check(gTDS[s][tsProject], "new project created via dialog");
    Check(!TDS_NewProject(0, "overwrite"), "new cannot overwrite current project");
    new text = TDS_AddElement(0, TDS_TEXT_TYPE);
    new rect = TDS_AddElement(0, TDS_RECT_TYPE);
    new sprite = TDS_AddElement(0, TDS_SPRITE_TYPE);
    new model = TDS_AddElement(0, TDS_MODEL_TYPE);
    Check(text == 0 && rect == 1 && sprite == 2 && model == 3, "four element types");
    Check(tFont[0][_:gTDSDraw[s][text]] == 1 && tFont[0][_:gTDSDraw[s][rect]] == 4 &&
        tFont[0][_:gTDSDraw[s][sprite]] == 4 && tFont[0][_:gTDSDraw[s][model]] == 5, "renderer maps fonts");
    new allocations = tCreate, destructions = tDestroy;
    new otherTouches = tTouches[0][_:gTDSDraw[s][rect]];
    gTDS[s][tsSelected] = text;
    Check(TDS_ApplyInput(0, TDS_POSITION, "123.5 234.25"), "exact coordinates");
    Check(tX[0][_:gTDSDraw[s][text]] == 123.5 && tY[0][_:gTDSDraw[s][text]] == 234.25, "renderer receives coordinates");
    Check(TDS_Move(0, 0.1, -10.0), "nudge");
    Check(tCreate == allocations && tDestroy == destructions, "editing reuses handles");
    Check(tTouches[0][_:gTDSDraw[s][rect]] == otherTouches, "editing touches only selected element");
    Check(TDS_ApplyInput(0, TDS_COLOUR, "ff8800aa"), "rgba parser");
    Check(TDS_ApplyInput(0, TDS_ALPHA, "77") && (gTDSElement[s][text][tdColour] & 0xFF) == 77, "alpha preserves RGB");
    Check(TDS_ApplyInput(0, TDS_CONTENT, "100% ~g~TARKENSO~w~"), "literal percent and safe format codes");
    new Float:x = gTDSElement[s][text][tdX];
    Check(!TDS_ApplyInput(0, TDS_POSITION, "10 NaN"), "reject NaN");
    Check(!TDS_ApplyInput(0, TDS_POSITION, "10 20 trailing"), "reject trailing garbage");
    Check(!TDS_ApplyInput(0, TDS_POSITION, "999999999999 20"), "reject overflow");
    Check(!TDS_ApplyInput(0, TDS_POSITION, "-1 20"), "reject negative position");
    Check(!TDS_ApplyInput(0, TDS_POSITION, "1e2 20"), "reject exponent");
    Check(!TDS_ApplyInput(0, TDS_SIZE, "1 0"), "reject zero dimensions");
    Check(gTDSElement[s][text][tdX] == x, "invalid multi-value edits are atomic");
    Check(!TDS_ApplyInput(0, TDS_CONTENT, "~k~~BAD"), "reject unsupported client directives");
    Check(!TDS_ApplyInput(0, TDS_CONTENT, "~r~"), "reject invisible-only text");
    Check(!TDS_ApplyInput(0, TDS_CONTENT, "bad\ntext"), "reject control characters");
    new longText[TDS_TEXT + 8];
    for (new i; i < sizeof(longText) - 1; i++) longText[i] = 'A';
    Check(!TDS_ApplyInput(0, TDS_CONTENT, longText), "reject oversized text without truncation");
    Check(!TDS_ApplyInput(0, TDS_COLOUR, "FFFFFFFFF"), "reject invalid hex length");
    Check(!TDS_ApplyInput(0, TDS_ALPHA, "1.5"), "reject fractional integer");
    Check(!TDS_ApplyInput(0, TDS_FONT, "4"), "text cannot silently become a sprite");
    Check(TDS_ApplyInput(0, TDS_ALIGN, "2"), "center alignment");
    Check(tW[0][_:gTDSDraw[s][text]] == 30.0 && tH[0][_:gTDSDraw[s][text]] == 100.0, "center TextSize axis conversion");
    Check(TDS_Toggle(0, TDS_BOX) && gTDSElement[s][text][tdBox], "text box");
    gTDS[s][tsSelected] = rect;
    Check(!TDS_ApplyInput(0, TDS_FONT, "2"), "inapplicable rectangle property rejected");
    gTDS[s][tsSelected] = model;
    Check(TDS_ApplyInput(0, TDS_ROTATION, "10 -20 30 1.5"), "model rotation and zoom");
    Check(!TDS_ApplyInput(0, TDS_ROTATION, "10 20 30 0"), "invalid model zoom rejected");
    Check(TDS_ApplyInput(0, TDS_VEH_COLOURS, "5 6"), "vehicle preview colours");
    TDS_Open(1);
    new other = TDS_Session(1);
    Check(other >= 0 && other != s, "independent developer session");
    TDS_NewProject(1, "Second"); TDS_AddElement(1, TDS_TEXT_TYPE);
    new otherCount = gTDS[other][tsCount];
    Check(!TDS_DeleteElement(0, -1) && !TDS_DeleteElement(0, 9999), "array bounds");
    new oldId = gTDSElement[s][sprite][tdId];
    Check(TDS_DeleteElement(0, sprite) && !TDS_DeleteElement(0, sprite), "delete once only");
    Check(TDS_AddElement(0, TDS_SPRITE_TYPE) == sprite && gTDSElement[s][sprite][tdId] > oldId, "stable ids never reused");
    Check(gTDS[other][tsCount] == otherCount, "another project unaffected");
    TDS_SetMode(0, TDS_EDIT_MODE);
    tNow += 200;
    TDS_OnClick(0, gTDSDraw[s][text]);
    Check(gTDS[s][tsSelected] == text, "click chooses own element");
    new selected = gTDS[s][tsSelected];
    tNow += 200;
    TDS_OnClick(0, PlayerText:250);
    Check(gTDS[s][tsSelected] == selected, "unknown handle ignored");
    TDS_OnClick(0, foreign);
    Check(gTDS[s][tsSelected] == selected, "foreign interface handle ignored");
    for (new stepIndex; stepIndex < sizeof(gTDSSteps); stepIndex++)
    {
        TDS_StepMenu(0); Answer(0, TDS_STEP, 1, stepIndex);
        tNow += 200;
        x = gTDSElement[s][text][tdX];
        TDS_OnClick(0, gTDSTool[s][4]);
        Check(floatabs(gTDSElement[s][text][tdX] - x - gTDSSteps[stepIndex]) < 0.005, "toolbar uses configured step");
    }
    tNow += 200;
    TDS_OnClick(0, gTDSTool[s][4]);
    x = gTDSElement[s][text][tdX];
    TDS_OnClick(0, gTDSTool[s][4]);
    Check(gTDSElement[s][text][tdX] == x, "click spam bounded");
    TDS_SetMode(0, TDS_PREVIEW_MODE);
    Check(!tShown[0][_:gTDSTool[s][0]] && !tShown[0][_:gTDSTool[s][8]] && tShown[0][_:gTDSDraw[s][text]], "preview hides tools only");
    Check(!gTDS[s][tsMouse] && !tSelectable[0][_:gTDSDraw[s][text]], "preview uses project selectability");
    tNow += 600; TDS_Open(0);
    Check(gTDS[s][tsMode] == TDS_EDIT_MODE && gTDS[s][tsMouse], "preview returns to edit");
    TDS_OnCancel(0, Text:INVALID_TEXT_DRAW);
    Check(gTDS[s][tsDialog] == TDS_MAIN && !gTDS[s][tsMouse] && tCancel < 10, "ESC safely returns to menu");
    new count = gTDS[s][tsCount];
    Answer(0, TDS_ADD, 1, 0);
    Check(gTDS[s][tsCount] == count, "unsolicited dialog rejected");
    Answer(0, TDS_MAIN, 1, 7);
    Answer(0, TDS_EDIT_MENU, 1, 2);
    Answer(0, TDS_ADD, 1, 99999);
    Check(gTDS[s][tsCount] == count, "forged list index rejected");
    while (gTDS[s][tsCount] < TDS_ELEMENTS) TDS_AddElement(0, TDS_RECT_TYPE);
    Check(TDS_AddElement(0, TDS_TEXT_TYPE) == -1, "element limit");
    Check(gTDS[s][tsCount] == 64, "bounded element count");
    TDS_Close(0); TDS_Close(0);
    Check(MockValid(0, foreign), "close preserves unrelated UI");
    Check(TDS_Session(1) == other && gTDS[other][tsCount] == 1, "close preserves other session");
    tNow += 600; TDS_Open(0); s = TDS_Session(0);
    Check(s >= 0 && !gTDS[s][tsProject], "reopen cleanly");
    TDS_NewProject(0, "Failure"); tFailAfter = 0;
    Check(TDS_AddElement(0, TDS_TEXT_TYPE) == -1 && gTDS[s][tsCount] == 0, "allocation failure rollback");
    tFailAfter = -1;
    TDS_Grant(0, 1, false);
    Check(TDS_Session(1) < 0 && !TDS_Allowed(1), "revocation frees resources immediately");
    TDS_Grant(0, 1, true); tNow += 600; TDS_Open(1);
    gAuthToken[1]++; TDS_Poll();
    Check(TDS_Session(1) < 0 && !TDS_Allowed(1), "reused player id cannot inherit grant or session");
    TDS_Close(0); tFailAfter = 3; tNow += 600; TDS_Open(0);
    Check(!TDS_Active(0), "partial toolbar allocation rollback"); tFailAfter = -1;
    for (new p; p < 9; p++)
    {
        Ready(p, true); tNow += 600; TDS_Open(p);
    }
    Check(!TDS_Active(8), "session pool exhausted safely");
    TDS_Reset(0, false); Ready(0, true); tNow += 600; TDS_Open(0);
    Check(TDS_Session(0) >= 0, "disconnect frees session pool slot");
    TDS_Shutdown();
    Check(tCreate - tDestroy == 1 && MockValid(0, foreign), "shutdown destroys every owned handle");
    RunMovementTests();
    RunLayerTests();
    Check(tCreate - tDestroy == 1 && MockValid(0, foreign), "movement cleanup preserves foreign handles");
    Check(tBadHandle == 0, "no invalid native handle access");
    Check(!tDialogOverflow, "native dialog body/caption limits respected");
    printf("TDS TEST RESULT: checks=%d failures=%d", tChecks, tFailures);
    return 1;
}
