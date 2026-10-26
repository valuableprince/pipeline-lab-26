#if defined _tarkenso_tds_codec
    #endinput
#endif
#define _tarkenso_tds_codec

// Version 1: scalar columns followed by CP1251 text encoded as ASCII HEX.
#define TDS_WIRE_MAX (48000)
new gTDSWire[TDS_WIRE_MAX];
new const E_TDS_ELEMENT:gTDSFields[] = {tdId, tdOrder, tdType, tdX, tdY, tdW, tdH, tdLetterX, tdLetterY, tdColour, tdBoxColour, tdBackground, tdFont, tdAlign, tdOutline, tdShadow, tdBox, tdProportional, tdSelectable, tdSprite, tdModel, tdRotX, tdRotY, tdRotZ, tdZoom, tdVehColour1, tdVehColour2};

stock bool:TDS_FloatField(E_TDS_ELEMENT:field)
{
    return field == tdX || field == tdY || field == tdW || field == tdH ||
        field == tdLetterX || field == tdLetterY || field == tdRotX ||
        field == tdRotY || field == tdRotZ || field == tdZoom;
}

stock bool:TDS_ValidName(const name[])
{
    if (!TDS_SafeText(name, TDS_NAME - 1) || name[0] == ' ' || name[strlen(name)-1] == ' ') return false;
    new bool:letter;
    for (new i; name[i]; i++)
    {
        new c = name[i];
        if (c == '/' || c == '\\' || c == ':' || c == '*' || c == '?' || c == '"' || c == '<' || c == '>' || c == '|') return false;
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c >= 128) letter = true;
    }
    return letter;
}

stock bool:TDS_Hex(const source[], dest[], capacity)
{
    new n = strlen(source);
    if (n * 2 + 1 > capacity) return false;
    new const digits[] = "0123456789ABCDEF";
    for (new i; i < n; i++)
    {
        if (source[i] < 1 || source[i] > 255) return false;
        dest[i*2] = digits[(source[i] >>> 4) & 15]; dest[i*2+1] = digits[source[i] & 15];
    }
    dest[n*2] = EOS;
    return true;
}

stock bool:TDS_Unhex(const source[], dest[], capacity)
{
    new n = strlen(source);
    if ((n % 2) || n / 2 >= capacity) return false;
    for (new i; i < n; i+=2)
    {
        new value;
        for (new j; j < 2; j++)
        {
            new c = tolower(source[i+j]), digit;
            if (c >= '0' && c <= '9') digit = c-'0';
            else if (c >= 'a' && c <= 'f') digit = c-'a'+10;
            else return false;
            value = value*16+digit;
        }
        if (!value) return false;
        dest[i/2] = value;
    }
    dest[n/2] = EOS;
    return true;
}

stock bool:TDS_DataValid(const data[E_TDS_ELEMENT])
{
    if (data[tdId] <= 0 || data[tdId] >= 2147483646 || data[tdOrder] < 0 ||
        data[tdType] < TDS_TEXT_TYPE || data[tdType] > TDS_MODEL_TYPE ||
        !TDS_InRange(data[tdX],0.0,640.0) || !TDS_InRange(data[tdY],0.0,448.0) ||
        !TDS_InRange(data[tdW],0.1,640.0) || !TDS_InRange(data[tdH],0.1,448.0) ||
        !TDS_InRange(data[tdLetterX],0.01,5.0) || !TDS_InRange(data[tdLetterY],0.01,10.0) ||
        data[tdFont] < 0 || data[tdFont] > 3 || data[tdAlign] < 1 || data[tdAlign] > 3 ||
        data[tdOutline] < 0 || data[tdOutline] > 4 || data[tdShadow] < 0 || data[tdShadow] > 4 ||
        _:data[tdBox] < 0 || _:data[tdBox] > 1 || _:data[tdProportional] < 0 || _:data[tdProportional] > 1 ||
        _:data[tdSelectable] < 0 || _:data[tdSelectable] > 1 ||
        data[tdSprite] < 0 || data[tdSprite] >= sizeof(gTDSSprites) ||
        !TDS_InRange(data[tdRotX],-360.0,360.0) || !TDS_InRange(data[tdRotY],-360.0,360.0) ||
        !TDS_InRange(data[tdRotZ],-360.0,360.0) || !TDS_InRange(data[tdZoom],0.1,10.0) ||
        data[tdVehColour1] < 0 || data[tdVehColour1] > 255 || data[tdVehColour2] < 0 || data[tdVehColour2] > 255 ||
        !TDS_SafeText(data[tdText],TDS_TEXT-1,true)) return false;
    for (new i; i < sizeof(gTDSModels); i++) if (data[tdModel] == gTDSModels[i]) return true;
    return false;
}

stock TDS_Order(session, slots[])
{
    new count;
    for (new i; i < TDS_ELEMENTS; i++)
    {
        if (!TDS_ElementValid(session,i)) continue;
        new j = count;
        while (j > 0 && gTDSElement[session][slots[j-1]][tdOrder] > gTDSElement[session][i][tdOrder])
        { slots[j] = slots[j-1]; j--; }
        slots[j] = i; count++;
    }
    return count;
}

stock bool:TDS_ProjectValid(session)
{
    if (!gTDS[session][tsProject] || !TDS_ValidName(gTDS[session][tsName]) ||
        gTDS[session][tsFormat] != TDS_FORMAT || gTDS[session][tsInterface] < 0 || gTDS[session][tsInterface] > 1 ||
        (gTDS[session][tsDescription][0] && !TDS_SafeText(gTDS[session][tsDescription],TDS_DESCRIPTION-1))) return false;
    new count;
    for (new i; i < TDS_ELEMENTS; i++)
    {
        if (!TDS_ElementValid(session,i)) continue;
        if (!TDS_DataValid(gTDSElement[session][i]) || gTDSElement[session][i][tdId] >= gTDS[session][tsNextId]) return false;
        for (new j; j < i; j++) if (TDS_ElementValid(session,j) &&
            (gTDSElement[session][j][tdId] == gTDSElement[session][i][tdId] ||
             gTDSElement[session][j][tdOrder] == gTDSElement[session][i][tdOrder])) return false;
        count++;
    }
    return count == gTDS[session][tsCount];
}

stock bool:TDS_Append(dest[], capacity, const text[])
{
    if (strlen(dest)+strlen(text) >= capacity) return false;
    strcat(dest,text,capacity);
    return true;
}

stock bool:TDS_Encode(session)
{
    if (!TDS_ProjectValid(session)) return false;
    new slots[TDS_ELEMENTS], count = TDS_Order(session,slots), part[300];
    format(gTDSWire,sizeof(gTDSWire),"[");
    for (new row; row < count; row++)
    {
        new e = slots[row];
        if (!TDS_Append(gTDSWire,sizeof(gTDSWire),row ? (",[") : ("["))) return false;
        for (new f; f < sizeof(gTDSFields); f++)
        {
            new E_TDS_ELEMENT:field = gTDSFields[f];
            if (TDS_FloatField(field)) format(part,sizeof(part),"%.9f,",Float:gTDSElement[session][e][field]);
            else format(part,sizeof(part),"%d,",gTDSElement[session][e][field]);
            if (!TDS_Append(gTDSWire,sizeof(gTDSWire),part)) return false;
        }
        new hex[TDS_TEXT*2];
        if (!TDS_Hex(gTDSElement[session][e][tdText],hex,sizeof(hex))) return false;
        format(part,sizeof(part),"\"%s\"]",hex);
        if (!TDS_Append(gTDSWire,sizeof(gTDSWire),part)) return false;
    }
    return TDS_Append(gTDSWire,sizeof(gTDSWire),"]");
}
