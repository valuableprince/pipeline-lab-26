#if defined _tarkenso_tds_state
    #endinput
#endif
#define _tarkenso_tds_state

// Shared synchronous staging area for history and template installation.
// Never retains native handles, a session lease or asynchronous state.
new gTDSStage[TDS_ELEMENTS][E_TDS_ELEMENT];
new gTDSStageGroups[TDS_GROUPS][E_TDS_GROUP];
forward bool:TDS_Allowed(playerid);

stock TDS_StateFind(const data[TDS_ELEMENTS][E_TDS_ELEMENT], id)
{
    if (id <= 0) return -1;
    for (new e; e < TDS_ELEMENTS; e++) if (data[e][tdId] == id) return e;
    return -1;
}

stock TDS_StateOrder(const data[TDS_ELEMENTS][E_TDS_ELEMENT], slots[], bool:byId = false)
{
    new count;
    for (new e; e < TDS_ELEMENTS; e++) if (data[e][tdId])
    {
        new j = count, value = byId ? data[e][tdId] : data[e][tdOrder];
        while (j && (byId ? data[slots[j-1]][tdId] : data[slots[j-1]][tdOrder]) > value)
        { slots[j] = slots[j-1]; j--; }
        slots[j] = e; count++;
    }
    return count;
}

stock bool:TDS_StateValid(const data[TDS_ELEMENTS][E_TDS_ELEMENT], const groups[TDS_GROUPS][E_TDS_GROUP],
    nextId, nextGroup, &count, &groupCount)
{
    count = 0; groupCount = 0;
    if (nextId < 1 || nextId > 2147483646) return false;
    for (new e; e < TDS_ELEMENTS; e++) if (data[e][tdId])
    {
        if (!TDS_DataValid(data[e]) || data[e][tdId] >= nextId) return false;
        for (new j; j < e; j++) if (data[j][tdId] &&
            (data[j][tdId] == data[e][tdId] || data[j][tdOrder] == data[e][tdOrder])) return false;
        count++;
    }
    for (new g; g < TDS_GROUPS; g++) if (groups[g][tgId]) groupCount++;
    return TDS_GroupDataValid(data, groups, nextGroup, groupCount);
}

stock bool:TDS_StateApply(s, const name[], const description[], interfaceType, nextId, nextGroup)
{
    if (s < 0 || s >= TDS_SESSIONS || !gTDS[s][tsUsed] || !gTDS[s][tsProject] ||
        gTDS[s][tsMode] == TDS_MOVE_MODE || !TDS_ValidName(name) ||
        (description[0] && !TDS_SafeText(description, TDS_DESCRIPTION-1)) ||
        interfaceType < 0 || interfaceType > 1) return false;
    new p = gTDS[s][tsOwner], count, groupCount;
    if (TDS_Session(p) != s || !TDS_Allowed(p) ||
        (gTDS[s][tsCreator] != TDSHost_Account(p) && !TDSHost_Founder(p)) ||
        !TDS_StateValid(gTDSStage, gTDSStageGroups, nextId, nextGroup, count, groupCount)) return false;
    new map[TDS_ELEMENTS], bool:changed[TDS_ELEMENTS], bool:rebuild = count != gTDS[s][tsCount];
    new selectedId, picked[TDS_ELEMENTS], pickedCount;
    if (TDS_ElementValid(s, gTDS[s][tsSelected])) selectedId = gTDSElement[s][gTDS[s][tsSelected]][tdId];
    for (new e; e < TDS_ELEMENTS; e++) if (TDS_ElementValid(s, e) && gTDSPicked[s][e]) picked[pickedCount++] = gTDSElement[s][e][tdId];
    for (new e; e < TDS_ELEMENTS; e++) if (gTDSStage[e][tdId])
    {
        map[e] = TDS_StateFind(gTDSElement[s], gTDSStage[e][tdId]);
        if (map[e] < 0 || gTDSStage[e][tdOrder] != gTDSElement[s][map[e]][tdOrder] ||
            !IsValidPlayerTextDraw(p, gTDSDraw[s][map[e]])) { rebuild = true; continue; }
        for (new E_TDS_ELEMENT:f; f < E_TDS_ELEMENT; f++)
            if (gTDSStage[e][f] != gTDSElement[s][map[e]][f]) changed[e] = true;
    }
    new PlayerText:draws[TDS_ELEMENTS], slots[TDS_ELEMENTS];
    for (new e; e < TDS_ELEMENTS; e++) draws[e] = INVALID_PLAYER_TEXT_DRAW;
    if (rebuild)
    {
        TDS_StateOrder(gTDSStage, slots);
        for (new i; i < count; i++)
        {
            new e = slots[i];
            if (TDS_RenderData(p, gTDSStage[e], draws[e], gTDS[s][tsMode] == TDS_EDIT_MODE, false)) continue;
            for (new j; j < TDS_ELEMENTS; j++) if (draws[j] != INVALID_PLAYER_TEXT_DRAW) PlayerTextDrawDestroy(p, draws[j]);
            return false; // no model, pointer, selection or old handle was changed
        }
        for (new e; e < TDS_ELEMENTS; e++) { TDS_RenderDestroyElement(s, e); gTDSElement[s][e][tdId] = 0; }
    }
    // All fallible resource work is complete. No native below allocates/calls back.
    for (new e; e < TDS_ELEMENTS; e++) if (gTDSStage[e][tdId])
    {
        new to = rebuild ? e : map[e];
        if (!rebuild && !changed[e]) continue;
        for (new E_TDS_ELEMENT:f; f < E_TDS_ELEMENT; f++) gTDSElement[s][to][f] = gTDSStage[e][f];
        if (rebuild) gTDSDraw[s][to] = draws[e];
    }
    for (new g; g < TDS_GROUPS; g++) for (new E_TDS_GROUP:f; f < E_TDS_GROUP; f++) gTDSGroup[s][g][f] = gTDSStageGroups[g][f];
    gTDS[s][tsCount] = count; gTDS[s][tsGroupCount] = groupCount;
    // Allocator high-water marks never go backwards, including across Redo forks.
    if (nextId > gTDS[s][tsNextId]) gTDS[s][tsNextId] = nextId;
    if (nextGroup > gTDS[s][tsNextGroupId]) gTDS[s][tsNextGroupId] = nextGroup;
    format(gTDS[s][tsName], TDS_NAME, "%s", name);
    format(gTDS[s][tsDescription], TDS_DESCRIPTION, "%s", description); gTDS[s][tsInterface] = interfaceType;
    gTDS[s][tsSelected] = TDS_StateFind(gTDSElement[s], selectedId);
    for (new e; e < TDS_ELEMENTS; e++)
    {
        gTDSPicked[s][e] = false;
        if (!TDS_ElementValid(s, e)) continue;
        for (new i; i < pickedCount; i++) if (picked[i] == gTDSElement[s][e][tdId]) gTDSPicked[s][e] = true;
    }
    if (gTDS[s][tsSelected] < 0 && count) { TDS_Order(s, slots); TDS_PickOne(s, slots[0]); }
    if (TDS_GroupSlot(s, gTDS[s][tsGroupFocus]) < 0) gTDS[s][tsGroupFocus] = 0;
    gTDS[s][tsSelectedGroup] = 0;
    for (new e; e < TDS_ELEMENTS; e++) if (gTDSStage[e][tdId])
    {
        if (rebuild) PlayerTextDrawShow(p, gTDSDraw[s][e]);
        else if (changed[e]) TDS_RenderElement(s, map[e]);
    }
    return true;
}
