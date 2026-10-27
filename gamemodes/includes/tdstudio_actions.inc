#if defined _tarkenso_tds_actions
    #endinput
#endif
#define _tarkenso_tds_actions

stock bool:TDS_FieldAllowed(E_TDS_TYPE:type, E_TDS_FIELD:field)
{
    switch (field)
    {
        case TDS_POSITION, TDS_SIZE, TDS_COLOUR, TDS_ALPHA, TDS_SELECTABLE: return true;
        case TDS_CONTENT: return type == TDS_TEXT_TYPE || type == TDS_SPRITE_TYPE;
        case TDS_BACKGROUND: return type == TDS_TEXT_TYPE || type == TDS_MODEL_TYPE;
        case TDS_MODEL, TDS_ROTATION, TDS_VEH_COLOURS: return type == TDS_MODEL_TYPE;
        default: return type == TDS_TEXT_TYPE;
    }
}

// Validate every component before changing the model. A failed parse is atomic.
stock bool:TDS_ApplyInput(playerid, E_TDS_FIELD:field, const input[])
{
    new s = TDS_RequireSession(playerid);
    if (s < 0 || gTDS[s][tsBusy] || gTDS[s][tsMode] == TDS_MOVE_MODE) return false;
    new e = gTDS[s][tsSelected];
    if (!TDS_ElementValid(s, e) || !TDS_FieldAllowed(gTDSElement[s][e][tdType], field)) return false;
    new Float:v[4], number;
    switch (field)
    {
        case TDS_POSITION, TDS_SIZE, TDS_LETTERS:
        {
            if (!TDS_Numbers(input, v, 2)) return false;
            return TDS_GeoProperty(playerid, field, v);
        }
        case TDS_CONTENT:
        {
            if (gTDSElement[s][e][tdType] != TDS_TEXT_TYPE || !TDS_SafeText(input, TDS_TEXT - 1, true)) return false;
            format(gTDSElement[s][e][tdText], TDS_TEXT, "%s", input);
        }
        case TDS_COLOUR, TDS_BOX_COLOUR, TDS_BACKGROUND:
        {
            if (!TDS_RGBA(input, number)) return false;
            switch (field)
            {
                case TDS_COLOUR: gTDSElement[s][e][tdColour] = number;
                case TDS_BOX_COLOUR: gTDSElement[s][e][tdBoxColour] = number;
                case TDS_BACKGROUND: gTDSElement[s][e][tdBackground] = number;
            }
        }
        case TDS_ALPHA:
        {
            if (!TDS_Integer(input, number, 0, 255)) return false;
            gTDSElement[s][e][tdColour] = (gTDSElement[s][e][tdColour] & 0xFFFFFF00) | number;
        }
        case TDS_FONT, TDS_ALIGN, TDS_OUTLINE, TDS_SHADOW:
        {
            new low = 0, high = 4;
            if (field == TDS_FONT) high = 3;
            if (field == TDS_ALIGN) { low = 1; high = 3; }
            if (!TDS_Integer(input, number, low, high)) return false;
            switch (field)
            {
                case TDS_FONT: gTDSElement[s][e][tdFont] = number;
                case TDS_ALIGN: gTDSElement[s][e][tdAlign] = number;
                case TDS_OUTLINE: gTDSElement[s][e][tdOutline] = number;
                case TDS_SHADOW: gTDSElement[s][e][tdShadow] = number;
            }
        }
        case TDS_ROTATION:
        {
            if (!TDS_Numbers(input, v, 4) || !TDS_InRange(v[0], -360.0, 360.0) ||
                !TDS_InRange(v[1], -360.0, 360.0) || !TDS_InRange(v[2], -360.0, 360.0) ||
                !TDS_InRange(v[3], 0.1, 10.0)) return false;
            gTDSElement[s][e][tdRotX] = v[0]; gTDSElement[s][e][tdRotY] = v[1];
            gTDSElement[s][e][tdRotZ] = v[2]; gTDSElement[s][e][tdZoom] = v[3];
        }
        case TDS_VEH_COLOURS:
        {
            if (!TDS_Numbers(input, v, 2)) return false;
            for (new i; i < 2; i++)
                if (!TDS_InRange(v[i], 0.0, 255.0) || float(floatround(v[i])) != v[i]) return false;
            gTDSElement[s][e][tdVehColour1] = floatround(v[0]);
            gTDSElement[s][e][tdVehColour2] = floatround(v[1]);
        }
        default: return false;
    }
    if (!TDS_RenderElement(s, e)) { TDS_Close(playerid); return false; }
    TDS_RenderStatus(s);
    gTDS[s][tsDirty] = true;
    return true;
}

stock bool:TDS_Toggle(playerid, E_TDS_FIELD:field)
{
    new s = TDS_RequireSession(playerid);
    if (s < 0 || gTDS[s][tsBusy] || gTDS[s][tsMode] == TDS_MOVE_MODE) return false;
    new e = gTDS[s][tsSelected];
    if (!TDS_ElementValid(s, e) || !TDS_FieldAllowed(gTDSElement[s][e][tdType], field)) return false;
    switch (field)
    {
        case TDS_BOX: gTDSElement[s][e][tdBox] = !gTDSElement[s][e][tdBox];
        case TDS_PROPORTIONAL: gTDSElement[s][e][tdProportional] = !gTDSElement[s][e][tdProportional];
        case TDS_SELECTABLE: gTDSElement[s][e][tdSelectable] = !gTDSElement[s][e][tdSelectable];
        default: return false;
    }
    if (!TDS_RenderElement(s, e)) { TDS_Close(playerid); return false; }
    gTDS[s][tsDirty] = true;
    return true;
}
