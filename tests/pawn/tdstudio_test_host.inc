// Execute the production includes in open.mp with deterministic host/UI doubles.
// This exercises Pawn logic and ownership, not GTA rendering or network clicks.
#define MAX_PLAYERS (16)
#include <open.mp>
#pragma dynamic 16384
#define TARKENSO_DB_PRIMARY
#if !defined TDS_TEST_MYSQL
    #define TDS_TEST_NO_DB
#endif
#define ADMIN_ERROR (0xE27E82FF)
#define AUTH_READY (1)

new gAuthToken[MAX_PLAYERS], gAuthState[MAX_PLAYERS];
new gDBPrimaryAccountId[MAX_PLAYERS];
new bool:gDBPrimaryLoaded[MAX_PLAYERS], bool:gAccountSpawned[MAX_PLAYERS];
new bool:gAccountDead[MAX_PLAYERS], bool:gSortOpen[MAX_PLAYERS];
new bool:tConnected[MAX_PLAYERS], bool:tFounder[MAX_PLAYERS];
new bool:gAdminFrozen[MAX_PLAYERS], bool:tControl[MAX_PLAYERS], bool:tRace[MAX_PLAYERS], bool:tJailed[MAX_PLAYERS];
new PLAYER_STATE:tState[MAX_PLAYERS], KEY:tKeys[MAX_PLAYERS], tUD[MAX_PLAYERS], tLR[MAX_PLAYERS];
new tFreeze, tUnfreeze, bool:tFreezeFail;
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
    return tRace[playerid]; }
stock bool:AdminPunish_IsJailed(playerid) {
    return tJailed[playerid]; }

stock bool:MockConnected(playerid) { return playerid >= 0 && playerid < MAX_PLAYERS && tConnected[playerid]; }
stock PLAYER_STATE:MockState(playerid) {
    return tState[playerid]; }
stock bool:MockControl(playerid) { return tControl[playerid]; }
stock bool:MockToggleControl(playerid, bool:enable)
{
    if (tFreezeFail && !enable) return false;
    tControl[playerid] = enable;
    if (enable) tUnfreeze++; else tFreeze++;
    return true;
}
stock bool:MockKeys(playerid, &KEY:keys, &ud, &lr)
{ keys = tKeys[playerid]; ud = tUD[playerid]; lr = tLR[playerid]; return true; }
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
#define IsPlayerControllable MockControl
#define TogglePlayerControllable MockToggleControl
#define GetPlayerKeys MockKeys
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


#if defined TDS_TEST_MYSQL
native MySQL:mysql_connect_file(const file_name[] = "mysql.ini");
native mysql_close(MySQL:handle);
native Cache:mysql_query(MySQL:handle,const query[],bool:use_cache=true);
native mysql_tquery(MySQL:handle,const query[],const callback[]="",const format[]="",{MySQL,Float,_}:...);
native mysql_format(MySQL:handle,output[],max_len,const format[],{MySQL,Float,_}:...);
native cache_get_row_count(&destination);
native cache_get_value_name_int(row,const column[],&destination);
native cache_get_value_name_float(row,const column[],&Float:destination);
native cache_get_value_name(row,const column[],destination[],max_len=sizeof(destination));
native cache_delete(Cache:cache_id);
new MySQL:gDBPrimary;
stock bool:DBPrimary_IsReady() { return gDBPrimary != MySQL:0; }
#endif

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
    tState[playerid] = PLAYER_STATE_ONFOOT; tControl[playerid] = true;
    tKeys[playerid] = KEY:0; tUD[playerid] = 0; tLR[playerid] = 0;
    return 1;
}
stock Answer(playerid, dialogid, response, row, const text[] = "")
{
    tNow += 600;
    tDialog[playerid] = INVALID_DIALOG_ID;
    return TDS_OnDialog(playerid, dialogid, response, row, text);
}
