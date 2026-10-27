#if defined _tarkenso_tds_layout
    #endinput
#endif
#define _tarkenso_tds_layout

enum E_TDS_LAYER { TDS_LAYER_UP, TDS_LAYER_DOWN, TDS_LAYER_FRONT, TDS_LAYER_BACK };

stock TDS_LayoutSession(playerid)
{
    new s = TDS_RequireSession(playerid);
    if (s < 0 || !gTDS[s][tsProject] || gTDS[s][tsBusy] || gTDS[s][tsMode] == TDS_MOVE_MODE) return -1;
    return s;
}

stock TDS_LayoutReset(s)
{
    for (new e; e < TDS_ELEMENTS; e++)
    {
        gTDSPicked[s][e] = false; gTDSMoving[s][e] = 0;
        for (new f; f < _:E_TDS_GEOMETRY; f++) gTDSOrigin[s][e][E_TDS_GEOMETRY:f] = 0.0;
    }
    for (new g; g < TDS_GROUPS; g++) { gTDSGroup[s][g][tgId] = 0; gTDSGroup[s][g][tgName][0] = EOS; }
    gTDS[s][tsNextGroupId] = 1; gTDS[s][tsGroupCount] = 0;
    gTDS[s][tsGroupFocus] = 0; gTDS[s][tsSelectedGroup] = 0; gTDS[s][tsMultiPick] = false;
    gTDS[s][tsMoveCount] = 0;
    return 1;
}

stock TDS_PickCount(s)
{
    if (s < 0 || s >= TDS_SESSIONS) return 0;
    new count;
    for (new e; e < TDS_ELEMENTS; e++) if (gTDSPicked[s][e] && TDS_ElementValid(s, e)) count++;
    return count;
}

// Internal focus setter; callers have already checked session ownership.
stock TDS_PickOne(s, e)
{
    if (!TDS_ElementValid(s, e)) return 0;
    for (new i; i < TDS_ELEMENTS; i++) gTDSPicked[s][i] = i == e;
    gTDS[s][tsSelected] = e; gTDS[s][tsSelectedGroup] = 0;
    return 1;
}

stock TDS_Select(playerid, e, bool:toggle = false)
{
    new s = TDS_LayoutSession(playerid);
    if (!TDS_ElementValid(s, e)) return 0;
    if (!toggle) TDS_PickOne(s, e);
    else
    {
        gTDSPicked[s][e] = !gTDSPicked[s][e]; gTDS[s][tsSelectedGroup] = 0;
        if (gTDSPicked[s][e]) gTDS[s][tsSelected] = e;
        else if (gTDS[s][tsSelected] == e)
        {
            gTDS[s][tsSelected] = -1;
            for (new i; i < TDS_ELEMENTS; i++) if (gTDSPicked[s][i] && TDS_ElementValid(s, i)) { gTDS[s][tsSelected] = i; break; }
        }
    }
    TDS_RenderStatus(s);
    return 1;
}

stock TDS_SelectAll(playerid, bool:select)
{
    new s = TDS_LayoutSession(playerid);
    if (s < 0) return 0;
    gTDS[s][tsSelected] = -1; gTDS[s][tsSelectedGroup] = 0;
    for (new e; e < TDS_ELEMENTS; e++)
    {
        gTDSPicked[s][e] = select && TDS_ElementValid(s, e);
        if (gTDSPicked[s][e] && gTDS[s][tsSelected] < 0) gTDS[s][tsSelected] = e;
    }
    TDS_RenderStatus(s);
    return 1;
}

stock TDS_Targets(s, slots[])
{
    if (s < 0 || s >= TDS_SESSIONS || !gTDS[s][tsUsed]) return 0;
    new count, primary = gTDS[s][tsSelected];
    if (!TDS_ElementValid(s, primary)) return 0;
    // Keep compatibility with trusted host code setting only tsSelected.
    if (gTDS[s][tsMode] != TDS_MOVE_MODE && !gTDSPicked[s][primary]) { slots[0] = primary; return 1; }
    for (new e; e < TDS_ELEMENTS; e++)
        if ((gTDS[s][tsMode] == TDS_MOVE_MODE ? (gTDSMoving[s][e] != 0) : gTDSPicked[s][e]) && TDS_ElementValid(s, e))
            slots[count++] = e;
    return count;
}

stock TDS_ClampMove(s, &Float:dx, &Float:dy)
{
    new slots[TDS_ELEMENTS], count = TDS_Targets(s, slots);
    new Float:left = -640.0, Float:right = 640.0, Float:top = -448.0, Float:bottom = 448.0;
    for (new i; i < count; i++)
    {
        new e = slots[i], Float:x = gTDSElement[s][e][tdX], Float:y = gTDSElement[s][e][tdY];
        if (-x > left) left = -x;
        if (640.0-x < right) right = 640.0-x;
        if (-y > top) top = -y;
        if (448.0-y < bottom) bottom = 448.0-y;
    }
    if (count > 1)
    {
        // A group stops as one vector at the first limiting anchor; it must
        // not slide some axes along an edge while another axis is blocked.
        new Float:factor = 1.0, Float:limit;
        if (dx < left) { limit = left/dx; if (limit < factor) factor = limit; }
        if (dx > right) { limit = right/dx; if (limit < factor) factor = limit; }
        if (dy < top) { limit = top/dy; if (limit < factor) factor = limit; }
        if (dy > bottom) { limit = bottom/dy; if (limit < factor) factor = limit; }
        dx *= factor; dy *= factor;
        return count;
    }
    if (dx < left) dx = left;
    if (dx > right) dx = right;
    if (dy < top) dy = top;
    if (dy > bottom) dy = bottom;
    return count;
}

stock TDS_GroupSlot(s, id)
{
    if (s < 0 || s >= TDS_SESSIONS || !gTDS[s][tsUsed] || !gTDS[s][tsProject] || id <= 0) return -1;
    for (new g; g < TDS_GROUPS; g++) if (gTDSGroup[s][g][tgId] == id) return g;
    return -1;
}

stock TDS_GroupMembers(s, id)
{
    if (TDS_GroupSlot(s, id) < 0) return 0;
    new count;
    for (new e; e < TDS_ELEMENTS; e++) if (TDS_ElementValid(s, e) && gTDSElement[s][e][tdGroup] == id) count++;
    return count;
}

stock TDS_GroupPrune(s, id)
{
    new g = TDS_GroupSlot(s, id);
    if (g < 0 || TDS_GroupMembers(s, id)) return 0;
    gTDSGroup[s][g][tgId] = 0; gTDSGroup[s][g][tgName][0] = EOS; gTDS[s][tsGroupCount]--;
    if (gTDS[s][tsSelectedGroup] == id) gTDS[s][tsSelectedGroup] = 0;
    if (gTDS[s][tsGroupFocus] == id) gTDS[s][tsGroupFocus] = 0;
    return 1;
}

stock bool:TDS_GroupNameAvailable(s, const name[], except = 0)
{
    if (!TDS_ValidName(name)) return false;
    for (new g; g < TDS_GROUPS; g++) if (gTDSGroup[s][g][tgId] && gTDSGroup[s][g][tgId] != except &&
        !strcmp(gTDSGroup[s][g][tgName], name, false)) return false;
    return true;
}

stock TDS_GroupSelect(playerid, id)
{
    new s = TDS_LayoutSession(playerid);
    if (TDS_GroupSlot(s, id) < 0 || !TDS_GroupMembers(s, id)) return 0;
    TDS_SelectAll(playerid, false);
    new slots[TDS_ELEMENTS], count = TDS_Order(s, slots);
    for (new i; i < count; i++) if (gTDSElement[s][slots[i]][tdGroup] == id)
    {
        gTDSPicked[s][slots[i]] = true;
        if (gTDS[s][tsSelected] < 0) gTDS[s][tsSelected] = slots[i];
    }
    gTDS[s][tsSelectedGroup] = id; gTDS[s][tsGroupFocus] = id;
    TDS_RenderStatus(s);
    return 1;
}

stock TDS_GroupCreate(playerid, const name[])
{
    new s = TDS_LayoutSession(playerid);
    if (s < 0 || gTDS[s][tsGroupCount] >= TDS_GROUPS || gTDS[s][tsNextGroupId] >= 2147483646 ||
        TDS_PickCount(s) < 2 || !TDS_GroupNameAvailable(s, name)) return 0;
    for (new e; e < TDS_ELEMENTS; e++) if (gTDSPicked[s][e] && TDS_ElementValid(s, e) && gTDSElement[s][e][tdGroup]) return 0;
    for (new g; g < TDS_GROUPS; g++) if (!gTDSGroup[s][g][tgId])
    {
        new id = gTDS[s][tsNextGroupId]++;
        gTDSGroup[s][g][tgId] = id; format(gTDSGroup[s][g][tgName], TDS_NAME, "%s", name);
        for (new e; e < TDS_ELEMENTS; e++) if (gTDSPicked[s][e] && TDS_ElementValid(s, e)) gTDSElement[s][e][tdGroup] = id;
        gTDS[s][tsGroupCount]++; gTDS[s][tsDirty] = true;
        TDS_GroupSelect(playerid, id);
        TDS_HRecord(s, TDS_H_GROUP_CREATE, id);
        return id;
    }
    return 0;
}

stock bool:TDS_GroupRename(playerid, id, const name[])
{
    new s = TDS_LayoutSession(playerid), g = TDS_GroupSlot(s, id);
    if (g < 0 || !TDS_GroupNameAvailable(s, name, id)) return false;
    if (!strcmp(gTDSGroup[s][g][tgName], name, false)) return true;
    format(gTDSGroup[s][g][tgName], TDS_NAME, "%s", name); gTDS[s][tsDirty] = true;
    TDS_HRecord(s, TDS_H_GROUP_RENAME, id);
    TDS_RenderStatus(s);
    return true;
}

stock bool:TDS_GroupAssign(playerid, id, e, bool:add)
{
    new s = TDS_LayoutSession(playerid);
    if (TDS_GroupSlot(s, id) < 0 || !TDS_ElementValid(s, e)) return false;
    new old = gTDSElement[s][e][tdGroup];
    if ((add && old && old != id) || (!add && old != id)) return false;
    if (add && old == id) return true;
    gTDSElement[s][e][tdGroup] = add ? id : 0;
    // Composition edits invalidate the "whole group" selection context.
    gTDS[s][tsSelectedGroup] = 0;
    if (!add) TDS_GroupPrune(s, id);
    gTDS[s][tsDirty] = true; TDS_RenderStatus(s);
    TDS_HRecord(s, TDS_H_GROUP_ASSIGN, id);
    return true;
}

stock bool:TDS_GroupDissolve(playerid, id)
{
    new s = TDS_LayoutSession(playerid);
    if (TDS_GroupSlot(s, id) < 0) return false;
    for (new e; e < TDS_ELEMENTS; e++) if (TDS_ElementValid(s, e) && gTDSElement[s][e][tdGroup] == id) gTDSElement[s][e][tdGroup] = 0;
    TDS_GroupPrune(s, id); gTDS[s][tsDirty] = true; TDS_RenderStatus(s);
    TDS_HRecord(s, TDS_H_GROUP_DISSOLVE, id);
    return true;
}

stock TDS_SaveOrders(s, orders[])
{
    for (new e; e < TDS_ELEMENTS; e++) orders[e] = gTDSElement[s][e][tdOrder];
    return 1;
}

stock TDS_RestoreOrders(s, const orders[])
{
    for (new e; e < TDS_ELEMENTS; e++) gTDSElement[s][e][tdOrder] = orders[e];
    return 1;
}

stock TDS_Reindex(s, const slots[], count)
{
    for (new i; i < count; i++) gTDSElement[s][slots[i]][tdOrder] = i;
    return 1;
}

stock bool:TDS_Layer(playerid, E_TDS_LAYER:action)
{
    new s = TDS_LayoutSession(playerid);
    if (s < 0 || action < TDS_LAYER_UP || action > TDS_LAYER_BACK) return false;
    new targets[TDS_ELEMENTS], n = TDS_Targets(s, targets);
    if (!n) return false;
    new bool:picked[TDS_ELEMENTS], slots[TDS_ELEMENTS], before[TDS_ELEMENTS], count = TDS_Order(s, slots);
    for (new i; i < n; i++) picked[targets[i]] = true;
    for (new i; i < count; i++) before[i] = slots[i];
    switch (action)
    {
        // Each selected run crosses one adjacent unselected element. Stable
        // for scattered groups as well as contiguous blocks.
        case TDS_LAYER_UP:
            for (new i = count-2; i >= 0; i--) if (picked[slots[i]] && !picked[slots[i+1]])
            { new swap = slots[i]; slots[i] = slots[i+1]; slots[i+1] = swap; }
        case TDS_LAYER_DOWN:
            for (new i; i < count-1; i++) if (!picked[slots[i]] && picked[slots[i+1]])
            { new swap = slots[i]; slots[i] = slots[i+1]; slots[i+1] = swap; }
        case TDS_LAYER_FRONT, TDS_LAYER_BACK:
        {
            new at;
            for (new pass; pass < 2; pass++) for (new i; i < count; i++)
                if (picked[before[i]] == (action == TDS_LAYER_FRONT ? (pass == 1) : (pass == 0))) slots[at++] = before[i];
        }
    }
    new bool:changed;
    for (new i; i < count; i++) if (slots[i] != before[i]) changed = true;
    if (!changed) return true; // no allocation or dirty flag at the boundary
    new orders[TDS_ELEMENTS]; TDS_SaveOrders(s, orders); TDS_Reindex(s, slots, count);
    if (!TDS_RenderRebuild(s)) { TDS_RestoreOrders(s, orders); return false; }
    gTDS[s][tsDirty] = true; TDS_RenderStatus(s);
    TDS_HRecord(s, TDS_H_LAYER);
    return true;
}

stock TDS_Duplicate(playerid)
{
    new s = TDS_LayoutSession(playerid);
    if (s < 0 || gTDS[s][tsCount] >= TDS_ELEMENTS || gTDS[s][tsNextId] >= 2147483646) return -1;
    new source = gTDS[s][tsSelected];
    if (!TDS_ElementValid(s, source) || !TDS_DataValid(gTDSElement[s][source])) return -1;
    new copy = -1;
    for (new e; e < TDS_ELEMENTS; e++) if (!gTDSElement[s][e][tdId]) { copy = e; break; }
    if (copy < 0) return -1;
    new slots[TDS_ELEMENTS], orders[TDS_ELEMENTS], count = TDS_Order(s, slots), at;
    TDS_SaveOrders(s, orders);
    while (at < count && slots[at] != source) at++;
    if (at == count) return -1;
    for (new i = count; i > at+1; i--) slots[i] = slots[i-1];
    slots[at+1] = copy;
    for (new E_TDS_ELEMENT:f; f < E_TDS_ELEMENT; f++) gTDSElement[s][copy][f] = gTDSElement[s][source][f];
    gTDSElement[s][copy][tdId] = gTDS[s][tsNextId]; gTDSElement[s][copy][tdGroup] = 0;
    gTDSElement[s][copy][tdX] += 10.0; gTDSElement[s][copy][tdY] += 10.0;
    if (gTDSElement[s][copy][tdX] > 640.0) gTDSElement[s][copy][tdX] = 640.0;
    if (gTDSElement[s][copy][tdY] > 448.0) gTDSElement[s][copy][tdY] = 448.0;
    TDS_Reindex(s, slots, count+1);
    if (!TDS_RenderRebuild(s)) { gTDSElement[s][copy][tdId] = 0; TDS_RestoreOrders(s, orders); return -1; }
    gTDS[s][tsNextId]++; gTDS[s][tsCount]++; gTDS[s][tsDirty] = true;
    TDS_PickOne(s, copy); TDS_RenderStatus(s);
    TDS_HRecord(s, TDS_H_COPY, gTDSElement[s][copy][tdId]);
    return copy;
}
