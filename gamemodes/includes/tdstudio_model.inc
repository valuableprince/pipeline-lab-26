#if defined _tarkenso_tds_model
    #endinput
#endif
#define _tarkenso_tds_model

// Serializable data only. PlayerText handles live in the renderer, not the model.
#define TDS_SESSIONS (8)
#define TDS_ELEMENTS (64)
#define TDS_TEXT (128)
#define TDS_NAME (33)
#define TDS_DESCRIPTION (129)
#define TDS_FORMAT (2)
#define TDS_GROUPS (16)
#define TDS_PROJECT_PAGE (20)
#define TDS_TOOLS (11)
#define TDS_COLOR (0xF1C263FF)
#define TDS_FIRST_DIALOG (7200)
#define TDS_LAST_DIALOG (7247)
#define TDS_MAIN (7200)
#define TDS_NEW (7201)
#define TDS_ADD (7202)
#define TDS_LIST (7203)
#define TDS_PROPS (7204)
#define TDS_INPUT (7205)
#define TDS_STEP (7206)
#define TDS_SPRITES (7207)
#define TDS_DELETE (7208)
#define TDS_CLOSE (7209)
#define TDS_MODELS (7210)
#define TDS_EDIT_MENU (7211)
#define TDS_PROJECTS (7212)
#define TDS_RENAME (7213)
#define TDS_SETTINGS (7214)
#define TDS_DESC_DIALOG (7215)
#define TDS_PROJECT_DELETE (7216)
#define TDS_DISCARD (7217)
#define TDS_LAYERS (7218)
#define TDS_SELECTION (7219)
#define TDS_PICK_LIST (7220)
#define TDS_GROUP_LIST (7221)
#define TDS_GROUP_MENU (7222)
#define TDS_GROUP_NAME (7223)
#define TDS_GROUP_DELETE (7224)
#define TDS_GROUP_MEMBERS (7225)
#define TDS_GEOMETRY (7226)
#define TDS_GEO_INPUT (7227)
#define TDS_GEO_ALIGN (7228)
#define TDS_GEO_SCREEN (7229)
#define TDS_GEO_GRID (7230)
#define TDS_GEO_STEP (7231)
#define TDS_GEO_INFO (7232)
#define TDS_HISTORY_MENU (7233)
#define TDS_HISTORY_LIST (7234)
#define TDS_HISTORY_CLEAR (7235)
#define TDS_TEMPLATE_MENU (7236)
#define TDS_TEMPLATE_CAT (7237)
#define TDS_TEMPLATE_LIST (7238)
#define TDS_TEMPLATE_DETAIL (7239)
#define TDS_TEMPLATE_NAME (7240)
#define TDS_TEMPLATE_DESC (7241)
#define TDS_TEMPLATE_NEW_CAT (7242)
#define TDS_TEMPLATE_CONFIRM (7243)
#define TDS_TEMPLATE_DELETE (7244)
#define TDS_TEMPLATE_PLACE (7245)
#define TDS_TEMPLATE_POS (7246)
#define TDS_TEMPLATE_VIS (7247)

enum E_TDS_TYPE { TDS_TEXT_TYPE, TDS_RECT_TYPE, TDS_SPRITE_TYPE, TDS_MODEL_TYPE };
enum E_TDS_MODE { TDS_MENU_MODE, TDS_EDIT_MODE, TDS_PREVIEW_MODE, TDS_MOVE_MODE };
// MOVE_MODE owns all interactive transactions, including resize/scale.
enum E_TDS_TRANSFORM { TDS_TRANS_MOVE, TDS_TRANS_RESIZE, TDS_TRANS_SCALE };
enum E_TDS_GEOMETRY { Float:geoX, Float:geoY, Float:geoW, Float:geoH, Float:geoLetterX, Float:geoLetterY };
enum E_TDS_ALIGN_OP { TDS_ALIGN_LEFT, TDS_ALIGN_RIGHT, TDS_ALIGN_TOP, TDS_ALIGN_BOTTOM, TDS_ALIGN_HCENTER, TDS_ALIGN_VCENTER };
enum E_TDS_FIELD
{
    TDS_POSITION, TDS_SIZE, TDS_LETTERS, TDS_CONTENT, TDS_COLOUR,
    TDS_ALPHA, TDS_FONT, TDS_ALIGN, TDS_BOX, TDS_BOX_COLOUR,
    TDS_BACKGROUND, TDS_OUTLINE, TDS_SHADOW, TDS_PROPORTIONAL,
    TDS_SELECTABLE, TDS_MODEL, TDS_ROTATION, TDS_VEH_COLOURS
};
enum E_TDS_ELEMENT
{
    tdId, tdOrder, E_TDS_TYPE:tdType,
    Float:tdX, Float:tdY, Float:tdW, Float:tdH,
    Float:tdLetterX, Float:tdLetterY,
    tdColour, tdBoxColour, tdBackground,
    tdFont, tdAlign, tdOutline, tdShadow,
    bool:tdBox, bool:tdProportional, bool:tdSelectable,
    tdSprite, tdModel, Float:tdRotX, Float:tdRotY, Float:tdRotZ, Float:tdZoom,
    tdVehColour1, tdVehColour2, tdGroup, tdText[TDS_TEXT]
};
enum E_TDS_GROUP { tgId, tgName[TDS_NAME] };
enum E_TDS_SESSION
{
    bool:tsUsed, tsOwner, tsToken, tsAccount,
    bool:tsProject, tsName[TDS_NAME], tsNextId, tsCount, tsSelected,
    E_TDS_MODE:tsMode, tsStep, tsDialog, E_TDS_FIELD:tsField,
    tsRows[TDS_ELEMENTS], tsRowCount, tsLastAction, bool:tsThrottled,
    bool:tsMouse,
    tsProjectId, tsCreator, tsRevision, tsFormat, tsInterface,
    tsDescription[TDS_DESCRIPTION], tsCreated[20], tsUpdated[20], bool:tsDirty,
    tsLease, tsBusy, tsPage, tsProjectRows[TDS_PROJECT_PAGE], tsProjectRowCount,
    bool:tsMore, tsPending, tsPendingProject,
    // Transient move transaction; tdX/tdY remain the only working coordinates.
    tsMoveSlot, tsMoveId,
    tsMoveTick, KEY:tsMoveKeys, bool:tsMoveReady, bool:tsMoveFreeze,
    tsNextGroupId, tsGroupCount, tsGroupFocus, tsSelectedGroup,
    bool:tsMultiPick, bool:tsRenameGroup, tsMoveCount,
    E_TDS_TRANSFORM:tsTransform, tsResizeStep, tsGeoAxis, bool:tsKeepRatio, bool:tsGlyphs,
    bool:tsAnchorCenter, bool:tsAlignActive, bool:tsAlignGroup,
    tsGridStep, bool:tsGridShow, bool:tsSnap, Float:tsMargin, tsGeoField,
    Float:tsGeoPivotX, Float:tsGeoPivotY, Float:tsGeoWidth, Float:tsGeoHeight,
    Float:tsGeoValueW, Float:tsGeoValueH, Float:tsGeoRawX, Float:tsGeoRawY,
    tsTplId, tsTplOwner, tsTplRevision, tsTplCategory, tsTplVisibility,
    tsTplName[TDS_NAME], tsTplDescription[TDS_DESCRIPTION], tsTplCreated[20], tsTplUpdated[20],
    tsTplFilter, tsTplPage, tsTplRows[TDS_PROJECT_PAGE], tsTplRowCount, bool:tsTplMore,
    tsTplEdit, tsTplPlace, Float:tsTplX, Float:tsTplY
};

// Pool allocation avoids a large element array for every possible player.
new gTDS[TDS_SESSIONS][E_TDS_SESSION];
new gTDSElement[TDS_SESSIONS][TDS_ELEMENTS][E_TDS_ELEMENT];
new gTDSGroup[TDS_SESSIONS][TDS_GROUPS][E_TDS_GROUP];
// Editor selection and move snapshots never enter the serialized element data.
new bool:gTDSPicked[TDS_SESSIONS][TDS_ELEMENTS];
new gTDSMoving[TDS_SESSIONS][TDS_ELEMENTS]; // original ID, zero when excluded
new Float:gTDSOrigin[TDS_SESSIONS][TDS_ELEMENTS][E_TDS_GEOMETRY];
new const E_TDS_ELEMENT:gTDSGeoFields[] = {tdX, tdY, tdW, tdH, tdLetterX, tdLetterY};
new const Float:gTDSGridSteps[] = {2.0, 5.0, 10.0, 20.0};
new gTDSPlayerSlot[MAX_PLAYERS]; // slot + 1; zero means no session
new bool:gTDSGranted[MAX_PLAYERS];
new gTDSGrantToken[MAX_PLAYERS];
new gTDSGrantAccount[MAX_PLAYERS];
new gTDSOpenTick[MAX_PLAYERS];
new bool:gTDSOpenLimited[MAX_PLAYERS];
new gTDSLeaseSerial;

new const Float:gTDSSteps[] = {0.1, 0.5, 1.0, 5.0, 10.0};
new const gTDSTypes[][] = {"Текст", "Прямоугольник", "Спрайт", "Модель"};
new const gTDSTypeLabels[][] = {"TEXT", "RECT", "SPRITE", "MODEL"};
// Known stock TXD names only; arbitrary client texture references are not accepted.
new const gTDSSprites[][] =
{
    "LD_SPAC:white", "LD_BEAT:chit", "LD_BEAT:cross", "LD_BEAT:triang",
    "LD_BEAT:circle", "LD_BEAT:square"
};
new const gTDSModels[] = {411, 560, 26, 2852, 1220, 18633};
new const gTDSModelNames[][] =
{
    "411 | Infernus", "560 | Sultan", "26 | Персонаж",
    "2852 | Журналы", "1220 | Коробка", "18633 | Гаечный ключ"
};

stock bool:TDS_PlayerValid(playerid)
{
    return playerid >= 0 && playerid < MAX_PLAYERS;
}

stock bool:TDS_InRange(Float:value, Float:low, Float:high)
{
    // Positive checks reject NaN as well as infinities.
    return value >= low && value <= high;
}

// Strict decimal grammar, bounded before conversion; no exponent/NaN/overflow.
stock bool:TDS_Number(const input[], &at, &Float:value)
{
    while (input[at] == ' ') at++;
    new sign = 1, digits, fraction, divisor = 1, whole;
    if (input[at] == '-') { sign = -1; at++; }
    else if (input[at] == '+') at++;
    while (input[at] >= '0' && input[at] <= '9')
    {
        if (++digits > 5) return false;
        whole = whole * 10 + input[at++] - '0';
    }
    if (!digits) return false;
    if (input[at] == '.')
    {
        at++;
        new decimals;
        while (input[at] >= '0' && input[at] <= '9')
        {
            if (++decimals > 3) return false;
            fraction = fraction * 10 + input[at++] - '0';
            divisor *= 10;
        }
        if (!decimals) return false;
    }
    if (input[at] != EOS && input[at] != ' ') return false;
    value = float(sign) * (float(whole) + float(fraction) / float(divisor));
    return true;
}

stock bool:TDS_Numbers(const input[], Float:values[], count)
{
    if (strlen(input) > 96) return false;
    new at;
    for (new i; i < count; i++) if (!TDS_Number(input, at, values[i])) return false;
    while (input[at] == ' ') at++;
    return input[at] == EOS;
}

stock bool:TDS_Integer(const input[], &value, minimum, maximum)
{
    new Float:values[1];
    if (!TDS_Numbers(input, values, 1) ||
        !TDS_InRange(values[0], float(minimum), float(maximum))) return false;
    value = floatround(values[0]);
    return float(value) == values[0];
}

stock bool:TDS_RGBA(const input[], &value)
{
    if (strlen(input) != 8) return false;
    new result;
    for (new i; i < 8; i++)
    {
        new c = tolower(input[i]), nibble;
        if (c >= '0' && c <= '9') nibble = c - '0';
        else if (c >= 'a' && c <= 'f') nibble = c - 'a' + 10;
        else return false;
        result = (result << 4) | nibble;
    }
    value = result;
    return true;
}

stock bool:TDS_SafeText(const input[], maximum, bool:formatting = false)
{
    new length = strlen(input), bool:visible;
    if (length < 1 || length > maximum) return false;
    for (new i; i < length; i++)
    {
        new c = input[i];
        if (c < 32 || c > 255 || c == 127 || c == '{' || c == '}') return false;
        if (c == '~')
        {
            if (!formatting || i + 2 >= length || input[i + 2] != '~') return false;
            c = tolower(input[i + 1]);
            if (c != 'r' && c != 'g' && c != 'b' && c != 'w' && c != 'y' &&
                c != 'p' && c != 'l' && c != 'h' && c != 'n') return false;
            i += 2;
        }
        else if (c != ' ') visible = true;
    }
    return visible;
}

stock bool:TDS_ElementValid(session, slot)
{
    return session >= 0 && session < TDS_SESSIONS &&
        gTDS[session][tsUsed] && gTDS[session][tsProject] &&
        slot >= 0 && slot < TDS_ELEMENTS && gTDSElement[session][slot][tdId] > 0;
}

stock TDS_DefaultElement(session, slot, E_TDS_TYPE:type, id)
{
    for (new E_TDS_ELEMENT:i; i < E_TDS_ELEMENT; i++) gTDSElement[session][slot][i] = 0;
    gTDSElement[session][slot][tdId] = id;
    gTDSElement[session][slot][tdOrder] = id;
    gTDSElement[session][slot][tdType] = type;
    gTDSElement[session][slot][tdX] = 270.0;
    gTDSElement[session][slot][tdY] = 200.0;
    gTDSElement[session][slot][tdW] = 100.0;
    gTDSElement[session][slot][tdH] = 30.0;
    gTDSElement[session][slot][tdLetterX] = 0.3;
    gTDSElement[session][slot][tdLetterY] = 1.2;
    gTDSElement[session][slot][tdColour] = 0xFFFFFFFF;
    gTDSElement[session][slot][tdBoxColour] = 0x20252DCC;
    gTDSElement[session][slot][tdBackground] = 0x00000000;
    gTDSElement[session][slot][tdFont] = 1;
    gTDSElement[session][slot][tdAlign] = 1;
    gTDSElement[session][slot][tdProportional] = true;
    gTDSElement[session][slot][tdModel] = gTDSModels[0];
    gTDSElement[session][slot][tdZoom] = 1.0;
    format(gTDSElement[session][slot][tdText], TDS_TEXT, "TARKENSO");
    if (type == TDS_RECT_TYPE) gTDSElement[session][slot][tdColour] = 0x20252DDD;
    if (type == TDS_MODEL_TYPE) gTDSElement[session][slot][tdH] = 100.0;
    return 1;
}
