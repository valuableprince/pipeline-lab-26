#if defined _tarkenso_tds_geometry
    #endinput
#endif
#define _tarkenso_tds_geometry

// Scratch plan only: no second working model and no asynchronous lifetime.
new Float:gTDSGeoPlan[TDS_ELEMENTS][E_TDS_GEOMETRY];

stock TDS_GeoInit(s)
{
    gTDS[s][tsResizeStep] = 2; gTDS[s][tsGridStep] = 2;
    gTDS[s][tsAnchorCenter] = true; gTDS[s][tsAlignActive] = true;
    return 1;
}

stock TDS_GeoRead(s, e, Float:data[E_TDS_GEOMETRY])
{
    for (new f; f < sizeof(gTDSGeoFields); f++) data[E_TDS_GEOMETRY:f] = Float:gTDSElement[s][e][gTDSGeoFields[f]];
    return 1;
}

// Logical placement rectangle, not an estimate of rendered glyph ink/padding.
stock TDS_GeoRect(const Float:data[E_TDS_GEOMETRY], E_TDS_TYPE:type, align,
    &Float:left, &Float:top, &Float:right, &Float:bottom)
{
    left = data[geoX]; top = data[geoY];
    if (type == TDS_TEXT_TYPE)
    {
        if (align == 2) left -= data[geoW]/2.0;
        else if (align == 3) left -= data[geoW];
    }
    right = left + data[geoW]; bottom = top + data[geoH];
    return 1;
}

stock bool:TDS_GeoBounds(s, const slots[], count, &Float:left, &Float:top, &Float:right, &Float:bottom, bool:origin = false)
{
    if (s < 0 || s >= TDS_SESSIONS || count < 1 || count > TDS_ELEMENTS) return false;
    for (new i; i < count; i++)
    {
        new e = slots[i], Float:data[E_TDS_GEOMETRY], Float:l, Float:t, Float:r, Float:b;
        if (!TDS_ElementValid(s, e)) return false;
        if (origin) for (new f; f < _:E_TDS_GEOMETRY; f++) data[E_TDS_GEOMETRY:f] = gTDSOrigin[s][e][E_TDS_GEOMETRY:f];
        else TDS_GeoRead(s, e, data);
        if (!TDS_InRange(data[geoW], 0.1, 640.0) || !TDS_InRange(data[geoH], 0.1, 448.0)) return false;
        TDS_GeoRect(data, gTDSElement[s][e][tdType], gTDSElement[s][e][tdAlign], l, t, r, b);
        if (!i || l < left) left = l;
        if (!i || t < top) top = t;
        if (!i || r > right) right = r;
        if (!i || b > bottom) bottom = b;
    }
    return true;
}

stock TDS_GeoPlan(s, const slots[], count, bool:origin = false)
{
    for (new i; i < count; i++)
    {
        new e = slots[i];
        if (origin) for (new f; f < _:E_TDS_GEOMETRY; f++) gTDSGeoPlan[e][E_TDS_GEOMETRY:f] = gTDSOrigin[s][e][E_TDS_GEOMETRY:f];
        else TDS_GeoRead(s, e, gTDSGeoPlan[e]);
    }
    return 1;
}

stock bool:TDS_GeoCommit(s, const slots[], count, bool:live = false)
{
    if (s < 0 || s >= TDS_SESSIONS || count < 1 || count > TDS_ELEMENTS) return false;
    new bool:changed[TDS_ELEMENTS], bool:resized[TDS_ELEMENTS], bool:any, p = gTDS[s][tsOwner];
    // Validate the entire plan and all native handles before the first write.
    for (new i; i < count; i++)
    {
        new e = slots[i];
        if (!TDS_ElementValid(s, e) || !IsValidPlayerTextDraw(p, gTDSDraw[s][e]) ||
            !TDS_InRange(gTDSGeoPlan[e][geoX], 0.0, 640.0) || !TDS_InRange(gTDSGeoPlan[e][geoY], 0.0, 448.0) ||
            !TDS_InRange(gTDSGeoPlan[e][geoW], 0.1, 640.0) || !TDS_InRange(gTDSGeoPlan[e][geoH], 0.1, 448.0) ||
            !TDS_InRange(gTDSGeoPlan[e][geoLetterX], 0.01, 5.0) || !TDS_InRange(gTDSGeoPlan[e][geoLetterY], 0.01, 10.0)) return false;
        for (new f; f < _:E_TDS_GEOMETRY; f++) if (gTDSGeoPlan[e][E_TDS_GEOMETRY:f] != Float:gTDSElement[s][e][gTDSGeoFields[f]])
        { changed[i] = true; any = true; if (f >= _:geoW) resized[i] = true; }
    }
    if (!any) return true;
    for (new i; i < count; i++) if (changed[i]) for (new f; f < _:E_TDS_GEOMETRY; f++)
        gTDSElement[s][slots[i]][gTDSGeoFields[f]] = _:gTDSGeoPlan[slots[i]][E_TDS_GEOMETRY:f];
    // No native here allocates or dispatches a Pawn callback between validation
    // and update. Layer handles and every non-geometry field remain unchanged.
    for (new i; i < count; i++) if (changed[i])
    {
        if (resized[i]) TDS_RenderGeometry(s, slots[i]);
        else TDS_RenderPosition(s, slots[i]);
    }
    if (!live) gTDS[s][tsDirty] = true;
    TDS_RenderStatus(s);
    return true;
}

stock TDS_GeoPivot(s, const slots[], count, &Float:x, &Float:y, bool:origin = false)
{
    if (count == 1 && !gTDS[s][tsSelectedGroup])
    {
        x = origin ? gTDSOrigin[s][slots[0]][geoX] : gTDSElement[s][slots[0]][tdX];
        y = origin ? gTDSOrigin[s][slots[0]][geoY] : gTDSElement[s][slots[0]][tdY];
        return 1;
    }
    new Float:l, Float:t, Float:r, Float:b;
    if (!TDS_GeoBounds(s, slots, count, l, t, r, b, origin)) return 0;
    x = gTDS[s][tsAnchorCenter] ? (l+r)/2.0 : l;
    y = gTDS[s][tsAnchorCenter] ? (t+b)/2.0 : t;
    return 1;
}

stock bool:TDS_GeoSnapDelta(s, const slots[], count, &Float:dx, &Float:dy, bool:origin = false)
{
    if (gTDS[s][tsGridStep] < 0 || gTDS[s][tsGridStep] >= sizeof(gTDSGridSteps)) return false;
    new Float:x, Float:y; if (!TDS_GeoPivot(s, slots, count, x, y, origin)) return false;
    new Float:minX = -640.0, Float:maxX = 640.0, Float:minY = -448.0, Float:maxY = 448.0;
    for (new i; i < count; i++)
    {
        new e = slots[i];
        new Float:ox = origin ? gTDSOrigin[s][e][geoX] : gTDSElement[s][e][tdX];
        new Float:oy = origin ? gTDSOrigin[s][e][geoY] : gTDSElement[s][e][tdY];
        if (-ox > minX) minX = -ox;
        if (640.0-ox < maxX) maxX = 640.0-ox;
        if (-oy > minY) minY = -oy;
        if (448.0-oy < maxY) maxY = 448.0-oy;
    }
    new Float:step = gTDSGridSteps[gTDS[s][tsGridStep]];
    new Float:loX = float(floatround((x+minX)/step, floatround_ceil))*step;
    new Float:hiX = float(floatround((x+maxX)/step, floatround_floor))*step;
    new Float:loY = float(floatround((y+minY)/step, floatround_ceil))*step;
    new Float:hiY = float(floatround((y+maxY)/step, floatround_floor))*step;
    if (loX > hiX || loY > hiY) return false;
    new Float:sx = float(floatround((x+dx)/step))*step, Float:sy = float(floatround((y+dy)/step))*step;
    if (sx < loX) sx = loX;
    if (sx > hiX) sx = hiX;
    if (sy < loY) sy = loY;
    if (sy > hiY) sy = hiY;
    dx = sx-x; dy = sy-y;
    return true;
}

stock bool:TDS_GeoTranslate(s, const slots[], count, Float:dx, Float:dy, bool:snap = false, bool:origin = false, bool:live = false)
{
    if (!TDS_InRange(dx, -4096.0, 4096.0) || !TDS_InRange(dy, -4096.0, 4096.0)) return false;
    if (snap && !TDS_GeoSnapDelta(s, slots, count, dx, dy, origin)) return false;
    TDS_GeoPlan(s, slots, count, origin);
    for (new i; i < count; i++) { gTDSGeoPlan[slots[i]][geoX] += dx; gTDSGeoPlan[slots[i]][geoY] += dy; }
    return TDS_GeoCommit(s, slots, count, live);
}

stock bool:TDS_GeoNudge(s, Float:dx, Float:dy)
{
    new slots[TDS_ELEMENTS], count = TDS_Targets(s, slots);
    return count > 0 && TDS_GeoTranslate(s, slots, count, dx, dy, true);
}

stock bool:TDS_GeoPosition(playerid, Float:x, Float:y)
{
    new s = TDS_LayoutSession(playerid), slots[TDS_ELEMENTS];
    if (s < 0 || !TDS_InRange(x, -1920.0, 1920.0) || !TDS_InRange(y, -960.0, 960.0)) return false;
    new count = TDS_Targets(s, slots), Float:px, Float:py;
    if (!count || !TDS_GeoPivot(s, slots, count, px, py)) return false;
    return TDS_GeoTranslate(s, slots, count, x-px, y-py, gTDS[s][tsSnap]);
}

stock bool:TDS_GeoSnapAnchor(s, e)
{
    if (gTDS[s][tsGridStep] < 0 || gTDS[s][tsGridStep] >= sizeof(gTDSGridSteps)) return false;
    new Float:step = gTDSGridSteps[gTDS[s][tsGridStep]];
    gTDSGeoPlan[e][geoX] = float(floatround(gTDSGeoPlan[e][geoX]/step))*step;
    gTDSGeoPlan[e][geoY] = float(floatround(gTDSGeoPlan[e][geoY]/step))*step;
    new Float:maximum = float(floatround(448.0/step, floatround_floor))*step;
    if (gTDSGeoPlan[e][geoY] > maximum) gTDSGeoPlan[e][geoY] = maximum;
    return true;
}

stock bool:TDS_GeoExact(playerid, const input[])
{
    new s = TDS_LayoutSession(playerid), Float:v[6];
    if (s < 0 || !TDS_Numbers(input, v, 6) ||
        !TDS_InRange(v[0], 0.0, 640.0) || !TDS_InRange(v[1], 0.0, 448.0)) return false;
    new e = gTDS[s][tsSelected], slots[1]; slots[0] = e;
    if (!TDS_ElementValid(s, e)) return false;
    for (new f; f < _:E_TDS_GEOMETRY; f++) gTDSGeoPlan[e][E_TDS_GEOMETRY:f] = v[f];
    // Non-text LetterSize is not a meaningful geometry control.
    if (gTDSElement[s][e][tdType] != TDS_TEXT_TYPE)
    { gTDSGeoPlan[e][geoLetterX] = gTDSElement[s][e][tdLetterX]; gTDSGeoPlan[e][geoLetterY] = gTDSElement[s][e][tdLetterY]; }
    // Exact element input snaps its own anchor, not the containing group.
    if (gTDS[s][tsSnap] && !TDS_GeoSnapAnchor(s, e)) return false;
    return TDS_GeoCommit(s, slots, 1);
}

// Existing property dialogs share the same preflight/commit and Snap rules.
stock bool:TDS_GeoProperty(playerid, E_TDS_FIELD:field, const Float:v[])
{
    new s = TDS_LayoutSession(playerid), slots[1];
    if (s < 0) return false;
    new e = gTDS[s][tsSelected]; slots[0] = e;
    if (!TDS_ElementValid(s, e)) return false;
    TDS_GeoPlan(s, slots, 1);
    switch (field)
    {
        case TDS_POSITION:
        {
            if (!TDS_InRange(v[0], 0.0, 640.0) || !TDS_InRange(v[1], 0.0, 448.0)) return false;
            gTDSGeoPlan[e][geoX] = v[0]; gTDSGeoPlan[e][geoY] = v[1];
            if (gTDS[s][tsSnap] && !TDS_GeoSnapAnchor(s, e)) return false;
        }
        case TDS_SIZE: { gTDSGeoPlan[e][geoW] = v[0]; gTDSGeoPlan[e][geoH] = v[1]; }
        case TDS_LETTERS:
        {
            if (gTDSElement[s][e][tdType] != TDS_TEXT_TYPE) return false;
            gTDSGeoPlan[e][geoLetterX] = v[0]; gTDSGeoPlan[e][geoLetterY] = v[1];
        }
        default: return false;
    }
    return TDS_GeoCommit(s, slots, 1);
}

stock bool:TDS_GeoScalePlan(s, const slots[], count, Float:sx, Float:sy, Float:px, Float:py, bool:origin, bool:live)
{
    if (!TDS_InRange(sx, 0.001, 100.0) || !TDS_InRange(sy, 0.001, 100.0)) return false;
    TDS_GeoPlan(s, slots, count, origin);
    for (new i; i < count; i++)
    {
        new e = slots[i];
        gTDSGeoPlan[e][geoX] = px+(gTDSGeoPlan[e][geoX]-px)*sx;
        gTDSGeoPlan[e][geoY] = py+(gTDSGeoPlan[e][geoY]-py)*sy;
        gTDSGeoPlan[e][geoW] *= sx; gTDSGeoPlan[e][geoH] *= sy;
        if (gTDSElement[s][e][tdType] == TDS_TEXT_TYPE)
        { gTDSGeoPlan[e][geoLetterX] *= sx; gTDSGeoPlan[e][geoLetterY] *= sy; }
    }
    return TDS_GeoCommit(s, slots, count, live);
}

stock bool:TDS_GeoScale(playerid, Float:sx, Float:sy)
{
    new s = TDS_LayoutSession(playerid), slots[TDS_ELEMENTS];
    if (s < 0) return false;
    new count = TDS_Targets(s, slots), Float:l, Float:t, Float:r, Float:b;
    if (!TDS_GeoBounds(s, slots, count, l, t, r, b)) return false;
    if (gTDS[s][tsKeepRatio]) sy = sx;
    return TDS_GeoScalePlan(s, slots, count, sx, sy,
        gTDS[s][tsAnchorCenter] ? (l+r)/2.0 : l, gTDS[s][tsAnchorCenter] ? (t+b)/2.0 : t, false, false);
}

stock TDS_GeoAlignDelta(E_TDS_ALIGN_OP:action, Float:l, Float:t, Float:r, Float:b,
    Float:rl, Float:rt, Float:rr, Float:rb, &Float:dx, &Float:dy)
{
    dx = 0.0; dy = 0.0;
    switch (action)
    {
        case TDS_ALIGN_LEFT: dx = rl-l;
        case TDS_ALIGN_RIGHT: dx = rr-r;
        case TDS_ALIGN_TOP: dy = rt-t;
        case TDS_ALIGN_BOTTOM: dy = rb-b;
        case TDS_ALIGN_HCENTER: dx = (rl+rr-l-r)/2.0;
        case TDS_ALIGN_VCENTER: dy = (rt+rb-t-b)/2.0;
    }
    return 1;
}

stock bool:TDS_GeoAlign(playerid, E_TDS_ALIGN_OP:action, bool:active = true, group = 0)
{
    new s = TDS_LayoutSession(playerid), slots[TDS_ELEMENTS], count;
    if (s < 0 || action < TDS_ALIGN_LEFT || action > TDS_ALIGN_VCENTER) return false;
    new primary = gTDS[s][tsSelected];
    if (!TDS_ElementValid(s, primary)) return false;
    if (group)
    {
        if (TDS_GroupSlot(s, group) < 0 || gTDSElement[s][primary][tdGroup] == group) return false;
        for (new e; e < TDS_ELEMENTS; e++) if (TDS_ElementValid(s, e) && gTDSElement[s][e][tdGroup] == group) slots[count++] = e;
    }
    else count = TDS_Targets(s, slots);
    if (!count || (!group && count < 2)) return false;
    new Float:rl, Float:rt, Float:rr, Float:rb, focus[1]; focus[0] = primary;
    if (active || group)
    { if (!TDS_GeoBounds(s, focus, 1, rl, rt, rr, rb)) return false; }
    else if (!TDS_GeoBounds(s, slots, count, rl, rt, rr, rb)) return false;
    if (group)
    {
        new Float:l, Float:t, Float:r, Float:b, Float:dx, Float:dy;
        if (!TDS_GeoBounds(s, slots, count, l, t, r, b)) return false;
        TDS_GeoAlignDelta(action, l, t, r, b, rl, rt, rr, rb, dx, dy);
        return TDS_GeoTranslate(s, slots, count, dx, dy);
    }
    TDS_GeoPlan(s, slots, count);
    for (new i; i < count; i++)
    {
        new e = slots[i], Float:l, Float:t, Float:r, Float:b, Float:dx, Float:dy;
        if (active && e == primary) continue;
        TDS_GeoRect(gTDSGeoPlan[e], gTDSElement[s][e][tdType], gTDSElement[s][e][tdAlign], l, t, r, b);
        TDS_GeoAlignDelta(action, l, t, r, b, rl, rt, rr, rb, dx, dy);
        gTDSGeoPlan[e][geoX] += dx; gTDSGeoPlan[e][geoY] += dy;
    }
    return TDS_GeoCommit(s, slots, count);
}

// Screen is API virtual 640x480. Existing format-2 anchor limits remain in force.
stock bool:TDS_GeoScreen(playerid, action)
{
    new s = TDS_LayoutSession(playerid), slots[TDS_ELEMENTS];
    if (s < 0 || action < 0 || action > 6 || !TDS_InRange(gTDS[s][tsMargin], 0.0, 224.0)) return false;
    new count = TDS_Targets(s, slots), Float:l, Float:t, Float:r, Float:b, Float:dx, Float:dy;
    if (!TDS_GeoBounds(s, slots, count, l, t, r, b)) return false;
    new Float:m = gTDS[s][tsMargin];
    if (r-l > 640.0-2.0*m || b-t > 480.0-2.0*m) return false;
    switch (action)
    {
        case 0: { dx = 320.0-(l+r)/2.0; dy = 240.0-(t+b)/2.0; }
        case 1: dx = m-l;
        case 2: dx = 640.0-m-r;
        case 3: dy = m-t;
        case 4: dy = 480.0-m-b;
        case 5: { dx = 320.0-(l+r)/2.0; dy = m-t; }
        case 6: { dx = 320.0-(l+r)/2.0; dy = 480.0-m-b; }
    }
    return TDS_GeoTranslate(s, slots, count, dx, dy);
}

stock bool:TDS_GeoDistribute(playerid, bool:horizontal)
{
    new s = TDS_LayoutSession(playerid), slots[TDS_ELEMENTS];
    if (s < 0) return false;
    new count = TDS_Targets(s, slots);
    if (count < 3) return false;
    TDS_GeoPlan(s, slots, count);
    new Float:start[TDS_ELEMENTS], Float:extent[TDS_ELEMENTS], Float:total;
    // Stable coordinate sort; ID tie-break prevents dependence on storage reuse.
    for (new i; i < count; i++)
    {
        new e = slots[i], Float:l, Float:t, Float:r, Float:b;
        TDS_GeoRect(gTDSGeoPlan[e], gTDSElement[s][e][tdType], gTDSElement[s][e][tdAlign], l, t, r, b);
        start[e] = horizontal ? l : t; extent[e] = horizontal ? (r-l) : (b-t); total += extent[e];
        new j = i;
        while (j > 0 && (start[slots[j-1]] > start[e] || (start[slots[j-1]] == start[e] &&
            gTDSElement[s][slots[j-1]][tdId] > gTDSElement[s][e][tdId]))) { slots[j] = slots[j-1]; j--; }
        slots[j] = e;
    }
    new Float:first = start[slots[0]], Float:last = start[slots[count-1]]+extent[slots[count-1]];
    new Float:gap = (last-first-total)/float(count-1), Float:cursor = first;
    if (gap < 0.0) return false; // no implicit overlap mode
    for (new i; i < count; i++)
    {
        new e = slots[i];
        if (i > 0 && i < count-1)
        {
            if (horizontal) gTDSGeoPlan[e][geoX] += cursor-start[e];
            else gTDSGeoPlan[e][geoY] += cursor-start[e];
        }
        cursor += extent[e]+gap;
    }
    return TDS_GeoCommit(s, slots, count);
}

stock bool:TDS_GeoLive(s, Float:dx, Float:dy)
{
    if (s < 0 || s >= TDS_SESSIONS || !gTDS[s][tsUsed] || gTDS[s][tsMode] != TDS_MOVE_MODE ||
        !TDS_InRange(dx, -20.0, 20.0) || !TDS_InRange(dy, -20.0, 20.0)) return false;
    new slots[TDS_ELEMENTS], count = TDS_Targets(s, slots);
    if (!count) return false;
    if (gTDS[s][tsTransform] == TDS_TRANS_MOVE)
    {
        new Float:x = gTDS[s][tsGeoRawX]+dx, Float:y = gTDS[s][tsGeoRawY]+dy;
        // Keep unsnapped intent bounded; sub-grid increments accumulate instead
        // of repeatedly rounding the already-snapped model back to itself.
        for (new i; i < count; i++)
        {
            new e = slots[i], Float:ox = gTDSOrigin[s][e][geoX], Float:oy = gTDSOrigin[s][e][geoY];
            if (x < -ox) x = -ox;
            if (x > 640.0-ox) x = 640.0-ox;
            if (y < -oy) y = -oy;
            if (y > 448.0-oy) y = 448.0-oy;
        }
        if (!TDS_InRange(x, -640.0, 640.0) || !TDS_InRange(y, -448.0, 448.0)) return false;
        if (!TDS_GeoTranslate(s, slots, count, x, y, true, true, true)) return false;
        gTDS[s][tsGeoRawX] = x; gTDS[s][tsGeoRawY] = y;
        return true;
    }
    if (!TDS_InRange(gTDS[s][tsGeoWidth], 0.001, 1920.0) ||
        !TDS_InRange(gTDS[s][tsGeoHeight], 0.001, 960.0)) return false;
    if (gTDS[s][tsGeoAxis] == 1) { if (dx == 0.0) dx = dy; dy = 0.0; }
    else if (gTDS[s][tsGeoAxis] == 2) { if (dy == 0.0) dy = dx; dx = 0.0; }
    new Float:w = gTDS[s][tsGeoValueW]+dx, Float:h = gTDS[s][tsGeoValueH]+dy;
    if (gTDS[s][tsKeepRatio])
    {
        if (dx != 0.0) h = gTDS[s][tsGeoHeight]*(w/gTDS[s][tsGeoWidth]);
        else w = gTDS[s][tsGeoWidth]*(h/gTDS[s][tsGeoHeight]);
    }
    new bool:ok;
    if (gTDS[s][tsTransform] == TDS_TRANS_SCALE)
        ok = TDS_GeoScalePlan(s, slots, count, w/gTDS[s][tsGeoWidth], h/gTDS[s][tsGeoHeight],
            gTDS[s][tsGeoPivotX], gTDS[s][tsGeoPivotY], true, true);
    else
    {
        TDS_GeoPlan(s, slots, count, true);
        new e = slots[0];
        if (gTDS[s][tsGlyphs]) { gTDSGeoPlan[e][geoLetterX] = w; gTDSGeoPlan[e][geoLetterY] = h; }
        else { gTDSGeoPlan[e][geoW] = w; gTDSGeoPlan[e][geoH] = h; }
        ok = TDS_GeoCommit(s, slots, count, true);
    }
    if (ok) { gTDS[s][tsGeoValueW] = w; gTDS[s][tsGeoValueH] = h; }
    return ok;
}

stock TDS_GeoStatus(s, label[], capacity)
{
    new e = gTDS[s][tsSelected], Float:l, Float:t, Float:r, Float:b, slots[TDS_ELEMENTS];
    new count = TDS_Targets(s, slots);
    if (!count || !TDS_GeoBounds(s, slots, count, l, t, r, b)) return 0;
    new group = TDS_GroupSlot(s, gTDS[s][tsSelectedGroup]);
    if (group >= 0 && gTDS[s][tsTransform] == TDS_TRANS_SCALE)
    {
        format(label, capacity, "SCALE G%d %s | N%d | %.2fx%.2f | P %.1f %.1f %s | A%d R%d S%.1f",
            gTDSGroup[s][group][tgId], gTDSGroup[s][group][tgName], count, r-l, b-t,
            gTDS[s][tsGeoPivotX], gTDS[s][tsGeoPivotY], gTDS[s][tsAnchorCenter] ? ("C") : ("TL"),
            gTDS[s][tsGeoAxis], _:gTDS[s][tsKeepRatio], gTDSSteps[gTDS[s][tsResizeStep]]);
        return 1;
    }
    format(label, capacity, "%s ID%d | %d | %s %.3f x %.3f | AXIS %d | RATIO %d | STEP %.1f",
        gTDS[s][tsTransform] == TDS_TRANS_SCALE ? ("SCALE") : ("RESIZE"), gTDSElement[s][e][tdId], count,
        gTDS[s][tsGlyphs] ? ("GLYPHS") : ("AREA"),
        gTDS[s][tsTransform] == TDS_TRANS_SCALE ? (r-l) : (gTDS[s][tsGlyphs] ? gTDSElement[s][e][tdLetterX] : gTDSElement[s][e][tdW]),
        gTDS[s][tsTransform] == TDS_TRANS_SCALE ? (b-t) : (gTDS[s][tsGlyphs] ? gTDSElement[s][e][tdLetterY] : gTDSElement[s][e][tdH]),
        gTDS[s][tsGeoAxis], _:gTDS[s][tsKeepRatio], gTDSSteps[gTDS[s][tsResizeStep]]);
    return 1;
}
