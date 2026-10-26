#if defined _tarkenso_tds_core
    #endinput
#endif
#define _tarkenso_tds_core

static gTDSTimer;
forward TDS_Poll();
forward bool:TDSInput_Target(session);
forward bool:TDSInput_Usable(session);

stock bool:TDS_Allowed(playerid)
{
    if (!gTDSTimer) return false;
    if (!TDSHost_Ready(playerid)) return false;
    if (TDSHost_Founder(playerid)) return true;
    if (!gTDSGranted[playerid] || gTDSGrantToken[playerid] != TDSHost_Token(playerid) ||
        gTDSGrantAccount[playerid] != TDSHost_Account(playerid)) return false;
    return true;
}

stock TDS_Session(playerid)
{
    if (!TDS_PlayerValid(playerid)) return -1;
    new session = gTDSPlayerSlot[playerid] - 1;
    if (session < 0 || session >= TDS_SESSIONS || !gTDS[session][tsUsed] ||
        gTDS[session][tsOwner] != playerid ||
        gTDS[session][tsToken] != TDSHost_Token(playerid) ||
        gTDS[session][tsAccount] != TDSHost_Account(playerid)) return -1;
    return session;
}

stock bool:TDS_Active(playerid)
{
    return TDS_PlayerValid(playerid) && gTDSPlayerSlot[playerid] != 0;
}

stock TDS_StopMouse(session, bool:cancel = true)
{
    if (!gTDS[session][tsMouse]) return 1;
    // CancelSelectTextDraw dispatches INVALID_TEXT_DRAW; clear ownership first.
    gTDS[session][tsMouse] = false;
    if (cancel && IsPlayerConnected(gTDS[session][tsOwner]))
        CancelSelectTextDraw(gTDS[session][tsOwner]);
    return 1;
}

stock TDS_Close(playerid, bool:connected = true)
{
    if (!TDS_PlayerValid(playerid)) return 0;
    new session = gTDSPlayerSlot[playerid] - 1;
    if (session < 0 || session >= TDS_SESSIONS || !gTDS[session][tsUsed] ||
        gTDS[session][tsOwner] != playerid) return 0;
    TDSExp_Cancel(session);
    TDSInput_End(session, false, connected, false);
    gTDSPlayerSlot[playerid] = 0;
    new dialog = gTDS[session][tsDialog];
    gTDS[session][tsDialog] = 0;
    TDS_StopMouse(session, connected);
    // Never dismiss a dialog owned by another system.
    if (connected && dialog != 0 && GetPlayerDialogID(playerid) == dialog)
        HidePlayerDialog(playerid);
    TDS_RenderDestroy(session);
    for (new i; i < TDS_ELEMENTS; i++) gTDSElement[session][i][tdId] = 0;
    gTDS[session][tsUsed] = false;
    gTDS[session][tsProject] = false;
    gTDS[session][tsName][0] = EOS;
    gTDS[session][tsOwner] = INVALID_PLAYER_ID;
    return 1;
}

stock TDS_Reset(playerid, bool:connected = true)
{
    if (!TDS_PlayerValid(playerid)) return 0;
    TDS_Close(playerid, connected);
    gTDSGranted[playerid] = false;
    gTDSGrantToken[playerid] = 0;
    gTDSGrantAccount[playerid] = 0;
    gTDSOpenLimited[playerid] = false;
    gTDSOpenTick[playerid] = 0;
    return 1;
}

stock TDS_RequireSession(playerid)
{
    new session = TDS_Session(playerid);
    if (session < 0 || !TDS_Allowed(playerid))
    {
        TDS_Close(playerid);
        return -1;
    }
    return session;
}

// One shared action budget for forged dialogs, toolbar clicks and commands.
stock bool:TDS_Action(session)
{
    new now = GetTickCount();
    if (gTDS[session][tsThrottled] && now - gTDS[session][tsLastAction] < 100) return false;
    gTDS[session][tsThrottled] = true;
    gTDS[session][tsLastAction] = now;
    return true;
}

stock TDS_Allocate(playerid)
{
    if (!TDS_Allowed(playerid) || gTDSLeaseSerial >= 2147483646) return -1;
    for (new s; s < TDS_SESSIONS; s++)
    {
        if (gTDS[s][tsUsed]) continue;
        for (new E_TDS_SESSION:i; i < E_TDS_SESSION; i++) gTDS[s][i] = 0;
        gTDS[s][tsUsed] = true;
        gTDS[s][tsLease] = ++gTDSLeaseSerial;
        gTDS[s][tsInterface] = 1;
        gTDS[s][tsFormat] = TDS_FORMAT;
        gTDS[s][tsOwner] = playerid;
        gTDS[s][tsToken] = TDSHost_Token(playerid);
        gTDS[s][tsAccount] = TDSHost_Account(playerid);
        gTDS[s][tsSelected] = -1;
        gTDS[s][tsNextId] = 1;
        gTDS[s][tsStep] = 2; // 1.0
        gTDSPlayerSlot[playerid] = s + 1;
        for (new i; i < TDS_ELEMENTS; i++) gTDSElement[s][i][tdId] = 0;
        TDS_RenderInit(s);
        if (!TDS_RenderTools(s)) { TDS_Close(playerid); return -1; }
        return s;
    }
    return -1;
}

stock bool:TDS_NewProject(playerid, const name[])
{
    new s = TDS_RequireSession(playerid);
    if (s < 0 || gTDS[s][tsBusy] || gTDS[s][tsProject] || !TDS_ValidName(name)) return false;
    format(gTDS[s][tsName], TDS_NAME, "%s", name);
    gTDS[s][tsProject] = true;
    gTDS[s][tsCreator] = TDSHost_Account(playerid);
    gTDS[s][tsDirty] = true;
    return true;
}

stock TDS_AddElement(playerid, E_TDS_TYPE:type)
{
    new s = TDS_RequireSession(playerid);
    if (s < 0 || gTDS[s][tsBusy] || gTDS[s][tsMode] == TDS_MOVE_MODE || !gTDS[s][tsProject] || type < TDS_TEXT_TYPE || type > TDS_MODEL_TYPE ||
        gTDS[s][tsCount] >= TDS_ELEMENTS || gTDS[s][tsNextId] >= 2147483646) return -1;
    for (new i; i < TDS_ELEMENTS; i++)
    {
        if (gTDSElement[s][i][tdId]) continue;
        TDS_DefaultElement(s, i, type, gTDS[s][tsNextId]);
        if (!TDS_RenderRebuild(s))
        {
            TDS_RenderDestroyElement(s, i);
            gTDSElement[s][i][tdId] = 0;
            return -1;
        }
        gTDS[s][tsNextId]++;
        gTDS[s][tsCount]++;
        gTDS[s][tsSelected] = i;
        TDS_RenderStatus(s);
        gTDS[s][tsDirty] = true;
        return i;
    }
    return -1;
}

stock bool:TDS_DeleteElement(playerid, slot)
{
    new s = TDS_RequireSession(playerid);
    if (!TDS_ElementValid(s, slot) || gTDS[s][tsBusy] || gTDS[s][tsMode] == TDS_MOVE_MODE) return false;
    TDS_RenderDestroyElement(s, slot);
    gTDSElement[s][slot][tdId] = 0;
    gTDS[s][tsCount]--;
    if (gTDS[s][tsSelected] == slot) gTDS[s][tsSelected] = -1;
    TDS_RenderStatus(s);
    gTDS[s][tsDirty] = true;
    return true;
}

stock bool:TDS_Move(playerid, Float:dx, Float:dy)
{
    new s = TDS_RequireSession(playerid);
    if (s < 0 || gTDS[s][tsBusy]) return false;
    new slot = gTDS[s][tsSelected];
    if (!TDS_ElementValid(s, slot)) return false;
    if (gTDS[s][tsMode] == TDS_MOVE_MODE && !TDSInput_Target(s)) return false;
    new Float:x = gTDSElement[s][slot][tdX] + dx;
    new Float:y = gTDSElement[s][slot][tdY] + dy;
    if (!TDS_InRange(x, 0.0, 640.0) || !TDS_InRange(y, 0.0, 448.0)) return false;
    if (x == gTDSElement[s][slot][tdX] && y == gTDSElement[s][slot][tdY]) return true;
    gTDSElement[s][slot][tdX] = x;
    gTDSElement[s][slot][tdY] = y;
    if (!TDS_RenderPosition(s, slot)) { TDS_Close(playerid); return false; }
    TDS_RenderStatus(s);
    if (gTDS[s][tsMode] != TDS_MOVE_MODE) gTDS[s][tsDirty] = true;
    return true;
}

stock TDS_SetMode(playerid, E_TDS_MODE:mode)
{
    new s = TDS_RequireSession(playerid);
    if (s < 0 || gTDS[s][tsBusy] || mode < TDS_MENU_MODE || mode > TDS_PREVIEW_MODE) return 0;
    if (mode != TDS_MENU_MODE && !gTDS[s][tsProject]) return 0;
    TDSInput_End(s, false);
    TDS_StopMouse(s);
    gTDS[s][tsDialog] = 0;
    gTDS[s][tsMode] = mode;
    TDS_RenderToolsVisible(s, false);
    if (mode == TDS_EDIT_MODE)
    {
        gTDS[s][tsMouse] = true;
        SelectTextDraw(playerid, TDS_COLOR);
    }
    for (new i; i < TDS_ELEMENTS; i++)
        if (TDS_ElementValid(s, i) && !TDS_RenderElement(s, i))
        {
            TDS_Close(playerid);
            return 0;
        }
    if (mode == TDS_EDIT_MODE)
    {
        TDS_RenderStatus(s);
        TDS_RenderToolsVisible(s, true);
    }
    if (mode == TDS_PREVIEW_MODE)
        SendClientMessage(playerid, TDS_COLOR, "Предпросмотр. /tdstudio — вернуться к редактированию.");
    return 1;
}

// Commands that may open foreign dialogs/selection cannot steal the UI session.
stock bool:TDS_CommandAllowed(playerid, const cmd[])
{
    if (!TDS_Active(playerid) || !strcmp(cmd, "tdstudio", true) ||
        !strcmp(cmd, "tdgrant", true) || !strcmp(cmd, "aduty", true)) return true;
    SendClientMessage(playerid, TDS_COLOR, "Сначала закройте проект через /tdstudio.");
    return false;
}

stock TDS_Grant(playerid, target, bool:allow)
{
    if (!TDSHost_Ready(playerid) || !TDSHost_Founder(playerid))
        return SendClientMessage(playerid, ADMIN_ERROR, "Выдавать доступ может подтверждённый основатель в /aduty.");
    if (!TDSHost_Ready(target))
        return SendClientMessage(playerid, ADMIN_ERROR, "Разработчик должен войти в аккаунт.");
    if (!TDSHost_LogGrant(playerid, target, allow))
        return SendClientMessage(playerid, ADMIN_ERROR, "Журнал недоступен: доступ не изменён.");
    gTDSGranted[target] = allow;
    gTDSGrantToken[target] = TDSHost_Token(target);
    gTDSGrantAccount[target] = TDSHost_Account(target);
    if (!allow) TDS_Close(target);
    SendClientMessage(target, TDS_COLOR, allow ?
        ("Выдан доступ к /tdstudio до отключения от сервера.") : ("Доступ к TextDraw Studio отозван."));
    return SendClientMessage(playerid, TDS_COLOR, "Права TextDraw Studio обновлены.");
}

stock TDS_Init()
{
    if (gTDSTimer) return 1;
    gTDSTimer = SetTimer("TDS_Poll", 1000, true);
    if (!gTDSTimer)
    {
        print("[TDSTUDIO] Session monitor unavailable; editor access disabled.");
        return 0;
    }
    TDSDB_Init();
    return 1;
}

public TDS_Poll()
{
    TDSDB_Expire();
    for (new s; s < TDS_SESSIONS; s++)
    {
        if (!gTDS[s][tsUsed]) continue;
        new playerid = gTDS[s][tsOwner];
        if (!TDS_Allowed(playerid) || TDS_Session(playerid) != s)
        {
            TDS_Close(playerid, IsPlayerConnected(playerid));
            continue;
        }
        if (gTDS[s][tsMode] == TDS_MOVE_MODE && !TDSInput_Usable(s))
        {
            TDS_Close(playerid);
            continue;
        }
        TDSExp_Tick(s);
    }
    return 1;
}

stock TDS_Shutdown()
{
    if (gTDSTimer) KillTimer(gTDSTimer);
    gTDSTimer = 0;
    for (new s; s < TDS_SESSIONS; s++)
        if (gTDS[s][tsUsed]) TDS_Close(gTDS[s][tsOwner]);
    return 1;
}
