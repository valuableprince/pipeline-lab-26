// Execute the production includes in open.mp with deterministic host/UI doubles.
// This exercises Pawn logic and ownership, not GTA rendering or network clicks.
#define MAX_PLAYERS (16)
#include <open.mp>
#pragma dynamic 16384
#define TARKENSO_DB_PRIMARY
#define ADMIN_ERROR (0xE27E82FF)
#define AUTH_READY (1)

new gAuthToken[MAX_PLAYERS], gAuthState[MAX_PLAYERS];
new gDBPrimaryAccountId[MAX_PLAYERS];
new bool:gDBPrimaryLoaded[MAX_PLAYERS], bool:gAccountSpawned[MAX_PLAYERS];
new bool:gAccountDead[MAX_PLAYERS], bool:gSortOpen[MAX_PLAYERS];
new bool:tConnected[MAX_PLAYERS], bool:tFounder[MAX_PLAYERS];
new bool:tAudit = true;
new bool:tExists[MAX_PLAYERS][256], bool:tShown[MAX_PLAYERS][256];
new bool:tSelectable[MAX_PLAYERS][256];
new tDialog[MAX_PLAYERS], tCreate, tDestroy, tBadHandle, tCancel, tFailAfter = -1, tNow;
new Float:tX[MAX_PLAYERS][256], Float:tY[MAX_PLAYERS][256];
new Float:tW[MAX_PLAYERS][256], Float:tH[MAX_PLAYERS][256];
new tFont[MAX_PLAYERS][256], tTouches[MAX_PLAYERS][256];
new tFailures, tChecks;

stock bool:Admin_Ready(playerid) { return tConnected[playerid] && gAuthState[playerid] == AUTH_READY; }
stock bool:Studio_Allowed(playerid) { return Admin_Ready(playerid) && tFounder[playerid]; }
stock bool:Admin_LogPlayer(playerid, const action[], const detail[]) {
    #pragma unused playerid, action, detail
    return tAudit; }
stock bool:Race_IsParticipant(playerid) {
    #pragma unused playerid
    return false; }
stock bool:AdminPunish_IsJailed(playerid) {
    #pragma unused playerid
    return false; }

stock bool:MockConnected(playerid) { return playerid >= 0 && playerid < MAX_PLAYERS && tConnected[playerid]; }
stock PLAYER_STATE:MockState(playerid) {
    #pragma unused playerid
    return PLAYER_STATE_ONFOOT; }
stock MockMessage(playerid, colour, const text[]) {
    #pragma unused playerid, colour, text
    return 1; }
stock MockTick() { return tNow; }
stock MockHideDialog(playerid) { tDialog[playerid] = INVALID_DIALOG_ID; return 1; }
stock MockDialogId(playerid) { return tDialog[playerid]; }
stock bool:MockDialog(playerid, dialogid, DIALOG_STYLE:style, const title[], const body[], const accept[], const cancel[], OPEN_MP_TAGS:...)
{
    #pragma unused style, title, body, accept, cancel
    tDialog[playerid] = dialogid; return true; }
stock MockSelect(playerid, colour) {
    #pragma unused playerid, colour
    return 1; }
forward TDS_OnCancel(playerid, Text:clicked);
stock MockCancel(playerid)
{
    tCancel++;
    if (tCancel > 100) { print("TDS FAIL: recursive cancel"); return 0; }
    TDS_OnCancel(playerid, Text:INVALID_TEXT_DRAW);
    return 1;
}
stock bool:MockValid(playerid, PlayerText:td)
{
    return playerid >= 0 && playerid < MAX_PLAYERS && _:td >= 0 && _:td < 256 && tExists[playerid][_:td];
}
stock MockTouch(playerid, PlayerText:td)
{
    if (!MockValid(playerid, td)) tBadHandle++;
    else tTouches[playerid][_:td]++;
    return 1;
}
stock PlayerText:MockCreate(playerid, Float:x, Float:y, const text[])
{
    #pragma unused text
    if (tFailAfter == 0) return INVALID_PLAYER_TEXT_DRAW;
    if (tFailAfter > 0) tFailAfter--;
    for (new i; i < 256; i++) if (!tExists[playerid][i])
    {
        tExists[playerid][i] = true; tShown[playerid][i] = false;
        tX[playerid][i] = x; tY[playerid][i] = y; tCreate++;
        return PlayerText:i;
    }
    return INVALID_PLAYER_TEXT_DRAW;
}
stock MockDestroy(playerid, PlayerText:td)
{
    MockTouch(playerid, td);
    if (!MockValid(playerid, td)) return 0;
    tExists[playerid][_:td] = false; tShown[playerid][_:td] = false; tDestroy++;
    return 1;
}
stock MockShow(playerid, PlayerText:td) { MockTouch(playerid, td); tShown[playerid][_:td] = true; return 1; }
stock MockHide(playerid, PlayerText:td) { MockTouch(playerid, td); tShown[playerid][_:td] = false; return 1; }
stock MockPos(playerid, PlayerText:td, Float:x, Float:y)
{ MockTouch(playerid, td); tX[playerid][_:td] = x; tY[playerid][_:td] = y; return 1; }
stock MockSize(playerid, PlayerText:td, Float:x, Float:y)
{ MockTouch(playerid, td); tW[playerid][_:td] = x; tH[playerid][_:td] = y; return 1; }
stock MockFont(playerid, PlayerText:td, TEXT_DRAW_FONT:font)
{ MockTouch(playerid, td); tFont[playerid][_:td] = _:font; return 1; }
stock MockAlign(playerid, PlayerText:td, TEXT_DRAW_ALIGN:alignment)
{
    #pragma unused alignment
    return MockTouch(playerid, td); }
stock MockValue(playerid, PlayerText:td, value)
{
    #pragma unused value
    return MockTouch(playerid, td); }
stock MockFlag(playerid, PlayerText:td, bool:flag)
{
    #pragma unused flag
    return MockTouch(playerid, td); }
stock MockSelectable(playerid, PlayerText:td, bool:flag)
{ MockTouch(playerid, td); tSelectable[playerid][_:td] = flag; return 1; }
stock MockLetters(playerid, PlayerText:td, Float:x, Float:y)
{
    #pragma unused x, y
    return MockTouch(playerid, td); }
stock MockString(playerid, PlayerText:td, const fmt[], OPEN_MP_TAGS:...)
{
    #pragma unused fmt
    return MockTouch(playerid, td); }
stock MockRot(playerid, PlayerText:td, Float:x, Float:y, Float:z, Float:zoom)
{
    #pragma unused x, y, z, zoom
    return MockTouch(playerid, td); }
stock MockVeh(playerid, PlayerText:td, a, b)
{
    #pragma unused a, b
    return MockTouch(playerid, td); }

#define IsPlayerConnected MockConnected
#define GetPlayerState MockState
#define SendClientMessage MockMessage
#define GetTickCount MockTick
#define HidePlayerDialog MockHideDialog
#define GetPlayerDialogID MockDialogId
#define ShowPlayerDialog MockDialog
#define SelectTextDraw MockSelect
#define CancelSelectTextDraw MockCancel
#define IsValidPlayerTextDraw MockValid
#define CreatePlayerTextDraw MockCreate
#define PlayerTextDrawDestroy MockDestroy
#define PlayerTextDrawShow MockShow
#define PlayerTextDrawHide MockHide
#define PlayerTextDrawSetPos MockPos
#define PlayerTextDrawTextSize MockSize
#define PlayerTextDrawFont MockFont
#define PlayerTextDrawAlignment MockAlign
#define PlayerTextDrawColour MockValue
#define PlayerTextDrawBoxColour MockValue
#define PlayerTextDrawBackgroundColour MockValue
#define PlayerTextDrawSetOutline MockValue
#define PlayerTextDrawSetShadow MockValue
#define PlayerTextDrawSetPreviewModel MockValue
#define PlayerTextDrawUseBox MockFlag
#define PlayerTextDrawSetProportional MockFlag
#define PlayerTextDrawSetSelectable MockSelectable
#define PlayerTextDrawLetterSize MockLetters
#define PlayerTextDrawSetString MockString
#define PlayerTextDrawSetPreviewRot MockRot
#define PlayerTextDrawSetPreviewVehicleColours MockVeh

#include "tdstudio.inc"

stock Check(bool:ok, const name[])
{
    tChecks++;
    if (!ok) { tFailures++; printf("TDS FAIL: %s", name); }
    return 1;
}
stock Ready(playerid, bool:founder = false)
{
    tConnected[playerid] = true; gAuthToken[playerid]++;
    gAuthState[playerid] = AUTH_READY; tFounder[playerid] = founder;
    gDBPrimaryAccountId[playerid] = playerid + 100; gDBPrimaryLoaded[playerid] = true;
    gAccountSpawned[playerid] = true; tDialog[playerid] = INVALID_DIALOG_ID;
    return 1;
}
stock Answer(playerid, dialogid, response, row, const text[] = "")
{
    tNow += 600;
    tDialog[playerid] = INVALID_DIALOG_ID;
    return TDS_OnDialog(playerid, dialogid, response, row, text);
}

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
    for (new stepIndex; stepIndex < 4; stepIndex++)
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
    Answer(0, TDS_MAIN, 1, 2);
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
    Check(tBadHandle == 0, "no invalid native handle access");
    printf("TDS TEST RESULT: checks=%d failures=%d", tChecks, tFailures);
    return 1;
}
