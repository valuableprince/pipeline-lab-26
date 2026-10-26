#if defined _tarkenso_tds_input
    #endinput
#endif
#define _tarkenso_tds_input

#define TDS_INPUT_MS (50)
#define TDS_INPUT_STALE_MS (250)
#define TDS_INPUT_BUTTONS (KEY_SECONDARY_ATTACK | KEY_YES | KEY_NO | KEY_SPRINT)

stock bool:TDSInput_Target(session)
{
    if (session < 0 || session >= TDS_SESSIONS || !gTDS[session][tsUsed] ||
        gTDS[session][tsMode] != TDS_MOVE_MODE) return false;
    new e = gTDS[session][tsMoveSlot];
    if (!TDS_ElementValid(session, e) || gTDS[session][tsSelected] != e ||
        gTDSElement[session][e][tdId] != gTDS[session][tsMoveId] ||
        gTDS[session][tsStep] < 0 || gTDS[session][tsStep] >= sizeof(gTDSSteps)) return false;
    new count;
    for (new i; i < TDS_ELEMENTS; i++) if (gTDSMoving[session][i])
    {
        if (!TDS_ElementValid(session, i) || gTDSElement[session][i][tdId] != gTDSMoving[session][i] ||
            !TDS_InRange(gTDSElement[session][i][tdX], 0.0, 640.0) ||
            !TDS_InRange(gTDSElement[session][i][tdY], 0.0, 448.0)) return false;
        count++;
    }
    return count > 0 && count == gTDS[session][tsMoveCount] && gTDSMoving[session][e] == gTDS[session][tsMoveId];
}

stock bool:TDSInput_Usable(session)
{
    if (!TDSInput_Target(session) || gTDS[session][tsBusy]) return false;
    new p = gTDS[session][tsOwner];
    return TDS_Session(p) == session && TDS_Allowed(p) && TDSHost_ControlAllowed(p) &&
        gTDS[session][tsMoveFreeze] && !IsPlayerControllable(p) &&
        GetPlayerDialogID(p) == INVALID_DIALOG_ID && !gTDS[session][tsMouse];
}

// No authentication requirement here: revocation/disconnect must also roll back.
// Release only a freeze acquired by this lease, while host policy permits it.
stock TDSInput_End(session, bool:accept, bool:connected = true, bool:redraw = true)
{
    if (session < 0 || session >= TDS_SESSIONS || !gTDS[session][tsUsed] ||
        gTDS[session][tsMode] != TDS_MOVE_MODE) return 0;
    new p = gTDS[session][tsOwner], bool:changed;
    // Restore all model coordinates first, then update their existing handles.
    for (new e; e < TDS_ELEMENTS; e++) if (gTDSMoving[session][e] && TDS_ElementValid(session, e) &&
        gTDSElement[session][e][tdId] == gTDSMoving[session][e])
    {
        if (gTDSElement[session][e][tdX] != gTDSMoveX[session][e] ||
            gTDSElement[session][e][tdY] != gTDSMoveY[session][e]) changed = true;
        if (!accept)
        {
            gTDSElement[session][e][tdX] = gTDSMoveX[session][e];
            gTDSElement[session][e][tdY] = gTDSMoveY[session][e];
        }
    }
    if (changed && accept) gTDS[session][tsDirty] = true;
    new bool:release = gTDS[session][tsMoveFreeze];
    gTDS[session][tsMode] = TDS_MENU_MODE;
    gTDS[session][tsMoveSlot] = -1; gTDS[session][tsMoveId] = 0;
    gTDS[session][tsMoveTick] = 0; gTDS[session][tsMoveKeys] = KEY:0;
    gTDS[session][tsMoveReady] = false; gTDS[session][tsMoveFreeze] = false;
    if (release && connected && IsPlayerConnected(p) && TDS_Session(p) == session &&
        TDSHost_ControlAllowed(p)) TogglePlayerControllable(p, true);
    for (new e; e < TDS_ELEMENTS; e++)
    {
        if (redraw && connected && changed && !accept && gTDSMoving[session][e] &&
            TDS_ElementValid(session, e) && gTDSElement[session][e][tdId] == gTDSMoving[session][e]) TDS_RenderPosition(session, e);
        gTDSMoving[session][e] = 0; gTDSMoveX[session][e] = 0.0; gTDSMoveY[session][e] = 0.0;
    }
    gTDS[session][tsMoveCount] = 0;
    return 1;
}

stock bool:TDS_SetStep(playerid, index)
{
    new s = TDS_RequireSession(playerid);
    if (s < 0 || gTDS[s][tsBusy] || index < 0 || index >= sizeof(gTDSSteps)) return false;
    gTDS[s][tsStep] = index;
    // Changing speed never changes the position or the project's dirty state.
    gTDS[s][tsMoveTick] = GetTickCount();
    TDS_RenderStatus(s);
    return true;
}

stock TDSInput_Start(playerid)
{
    new s = TDS_RequireSession(playerid);
    if (s < 0 || gTDS[s][tsBusy] || gTDS[s][tsMode] == TDS_MOVE_MODE) return 0;
    new e = gTDS[s][tsSelected];
    if (!TDS_ElementValid(s, e))
        return SendClientMessage(playerid, TDS_COLOR, "Сначала выберите элемент мышью или через список.");
    if (!TDS_DataValid(gTDSElement[s][e]) || gTDS[s][tsStep] < 0 || gTDS[s][tsStep] >= sizeof(gTDSSteps)) return 0;
    if (!TDSHost_ControlAllowed(playerid) || !IsPlayerControllable(playerid))
        return SendClientMessage(playerid, ADMIN_ERROR, "Перемещение доступно пешком, вне гонки/тюрьмы и без чужой блокировки управления.");
    new slots[TDS_ELEMENTS], count = TDS_Targets(s, slots);
    if (!count) return 0;
    for (new i; i < count; i++) if (!TDS_DataValid(gTDSElement[s][slots[i]])) return 0;
    new dialog = GetPlayerDialogID(playerid);
    if (dialog != INVALID_DIALOG_ID && dialog != gTDS[s][tsDialog]) return 0;
    // Cursor cancellation can dispatch a callback; relinquish it before freezing.
    TDS_StopMouse(s);
    if (dialog != INVALID_DIALOG_ID) HidePlayerDialog(playerid);
    gTDS[s][tsDialog] = 0;
    if (!TogglePlayerControllable(playerid, false))
    {
        TDS_SetMode(playerid, TDS_EDIT_MODE);
        SendClientMessage(playerid, ADMIN_ERROR, "Не удалось заблокировать персонажа. Режим перемещения не включён.");
        return 0;
    }
    for (new i; i < TDS_ELEMENTS; i++) gTDSMoving[s][i] = 0;
    for (new i; i < count; i++)
    {
        new target = slots[i];
        gTDSMoving[s][target] = gTDSElement[s][target][tdId];
        gTDSMoveX[s][target] = gTDSElement[s][target][tdX]; gTDSMoveY[s][target] = gTDSElement[s][target][tdY];
    }
    gTDS[s][tsMoveCount] = count;
    gTDS[s][tsMode] = TDS_MOVE_MODE;
    gTDS[s][tsMoveSlot] = e; gTDS[s][tsMoveId] = gTDSElement[s][e][tdId];
    gTDS[s][tsMoveTick] = GetTickCount(); gTDS[s][tsMoveKeys] = KEY:0;
    gTDS[s][tsMoveReady] = false; gTDS[s][tsMoveFreeze] = true;
    TDS_RenderStatus(s); TDS_RenderToolsVisible(s, true);
    SendClientMessage(playerid, TDS_COLOR, "Перемещение: отпустите клавиши. Направления — действия движения GTA; Enter/Y — принять; N — отменить; Space — шаг.");
    return 1;
}

stock TDSInput_Finish(playerid, bool:accept)
{
    new s = TDS_RequireSession(playerid);
    if (s < 0 || !TDSInput_Usable(s)) { TDS_Close(playerid); return 0; }
    TDSInput_End(s, accept);
    return TDS_SetMode(playerid, TDS_EDIT_MODE);
}

stock TDSInput_Buttons(session, KEY:keys)
{
    new KEY:pressed = keys & ~gTDS[session][tsMoveKeys];
    gTDS[session][tsMoveKeys] = keys;
    if (!gTDS[session][tsMoveReady]) return 1;
    new p = gTDS[session][tsOwner];
    // Cancel wins when multiple actions arrive in the same packet.
    if (pressed & KEY_NO) return TDSInput_Finish(p, false);
    if (pressed & (KEY_SECONDARY_ATTACK | KEY_YES)) return TDSInput_Finish(p, true);
    if ((pressed & KEY_SPRINT) && TDS_Action(session))
        TDS_SetStep(p, (gTDS[session][tsStep] + 1) % sizeof(gTDSSteps));
    return 1;
}

stock TDSInput_OnKeys(playerid, KEY:keys)
{
    new s = TDS_Session(playerid);
    if (s < 0 || gTDS[s][tsMode] != TDS_MOVE_MODE) return 0;
    if (!TDSInput_Usable(s)) return TDS_Close(playerid);
    return TDSInput_Buttons(s, keys);
}

// Called by the host's existing client update stream. No timer per player/element.
// GetPlayerKeys supplies actions/axes, not physical key codes or cursor pixels.
stock TDSInput_OnUpdate(playerid)
{
    new s = TDS_Session(playerid);
    if (s < 0 || gTDS[s][tsMode] != TDS_MOVE_MODE) return 1;
    if (!TDSInput_Usable(s)) { TDS_Close(playerid); return 1; }
    new KEY:keys, ud, lr;
    if (!GetPlayerKeys(playerid, keys, ud, lr)) return 1;
    new now = GetTickCount(), elapsed = now - gTDS[s][tsMoveTick];
    // Pauses/network stalls cannot cause catch-up jumps or resume stale input.
    if (elapsed < 0 || elapsed > TDS_INPUT_STALE_MS) gTDS[s][tsMoveReady] = false;
    if (!gTDS[s][tsMoveReady])
    {
        gTDS[s][tsMoveTick] = now; gTDS[s][tsMoveKeys] = keys;
        if (!ud && !lr && !(keys & TDS_INPUT_BUTTONS)) gTDS[s][tsMoveReady] = true;
        return 1;
    }
    TDSInput_Buttons(s, keys);
    if (TDS_Session(playerid) != s || gTDS[s][tsMode] != TDS_MOVE_MODE) return 1;
    if (elapsed < TDS_INPUT_MS || gTDS[s][tsMoveTick] == now) return 1;
    gTDS[s][tsMoveTick] = now;
    if (ud < -128 || ud > 128 || lr < -128 || lr > 128 || (!ud && !lr)) return 1;
    // Time-based velocity, capped at two quanta per packet. Typical 50 ms input
    // moves one configured step; no accumulated work is replayed after a stall.
    if (elapsed > TDS_INPUT_MS * 2) elapsed = TDS_INPUT_MS * 2;
    new Float:scale = gTDSSteps[gTDS[s][tsStep]] * float(elapsed) / float(TDS_INPUT_MS);
    new Float:dx = float(lr) / 128.0, Float:dy = float(ud) / 128.0;
    new Float:length = floatsqroot(dx * dx + dy * dy);
    if (length > 1.0) { dx /= length; dy /= length; }
    dx *= scale; dy *= scale;
    TDS_ClampMove(s, dx, dy);
    TDS_Move(playerid, dx, dy);
    return 1; // never suppress unrelated player synchronization
}
