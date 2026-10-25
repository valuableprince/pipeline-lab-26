#if defined _tarkenso_rp_chat_included
    #endinput
#endif
#define _tarkenso_rp_chat_included

// RP-CHAT-01. Local in-character actions and speech, without dialog popups.
#define RP_TEXT_MAX          (90)
#define RP_RADIUS_ACTION     (20.0)
#define RP_RADIUS_SPEECH     (20.0)
#define RP_RADIUS_WHISPER    (4.0)
#define RP_RADIUS_LOW        (8.0)
#define RP_RADIUS_SHOUT      (45.0)
#define RP_ACTION_DURATION   (8000)

#define RP_COLOR_ACTION      (0xCCA3E7FF)
#define RP_COLOR_DO          (0xE0D6B5FF)
#define RP_COLOR_OOC         (0xB7BEC8FF)
#define RP_COLOR_SUCCESS     (0x92C9A0FF)
#define RP_COLOR_FAILURE     (0xE99696FF)

new Text3D:gRPActionLabel[MAX_PLAYERS];
new gRPActionSerial[MAX_PLAYERS];

forward RP_ActionExpire(playerid, serial);

stock RP_Init()
{
    for (new i = 0; i < MAX_PLAYERS; i++)
    {
        gRPActionLabel[i] = Text3D:INVALID_3DTEXT_ID;
        gRPActionSerial[i] = 0;
    }
    return 1;
}

stock RP_Reset(playerid)
{
    // Keep the serial monotonic so callbacks from previous sessions cannot clear a new label.
    gRPActionSerial[playerid]++;
    if (gRPActionLabel[playerid] != Text3D:INVALID_3DTEXT_ID)
        Delete3DTextLabel(gRPActionLabel[playerid]);
    gRPActionLabel[playerid] = Text3D:INVALID_3DTEXT_ID;
    return 1;
}

stock bool:RP_ReadText(const params[], output[], capacity)
{
    new start = 0, length = strlen(params);
    while (start < length && params[start] <= ' ') start++;
    while (length > start && params[length - 1] <= ' ') length--;
    if (start == length || length - start > RP_TEXT_MAX) return false;

    new out = 0;
    for (new i = start; i < length && out < capacity - 1; i++)
    {
        new ch = params[i];
        // Prevent client-message color spoofing and embedded line breaks.
        if (ch == '{') ch = '(';
        else if (ch == '}') ch = ')';
        else if (ch < 32) ch = ' ';
        output[out++] = ch;
    }
    output[out] = EOS;
    return true;
}

stock bool:RP_CanSpeak(playerid)
{
    if (gAuthState[playerid] != AUTH_READY || !gAccountSpawned[playerid])
    {
        SendClientMessage(playerid, COLOR_RED, "Сначала войдите в игру.");
        return false;
    }
    if (gAdminMuteUntil[playerid] > gettime())
    {
        SendClientMessage(playerid, ADMIN_ERROR, "Ваш чат временно заблокирован.");
        return false;
    }
    return Guard_AllowChat(playerid);
}

stock RP_SendLocal(playerid, color, const message[], Float:radius)
{
    new Float:x, Float:y, Float:z, Float:px, Float:py, Float:pz;
    GetPlayerPos(playerid, x, y, z);
    new world = GetPlayerVirtualWorld(playerid);
    new interior = GetPlayerInterior(playerid);
    new Float:maxDistSquared = radius * radius;
    for (new i = 0; i < MAX_PLAYERS; i++)
    {
        if (!IsPlayerConnected(i) || gAuthState[i] != AUTH_READY) continue;
        if (GetPlayerVirtualWorld(i) != world || GetPlayerInterior(i) != interior) continue;
        GetPlayerPos(i, px, py, pz);
        if ((px - x) * (px - x) + (py - y) * (py - y) + (pz - z) * (pz - z) > maxDistSquared)
            continue;
        SendClientMessage(i, color, message);
    }
    return 1;
}

stock RP_ShowActionLabel(playerid, const message[])
{
    RP_Reset(playerid);
    new Float:x, Float:y, Float:z;
    GetPlayerPos(playerid, x, y, z);
    gRPActionLabel[playerid] = Create3DTextLabel(message, RP_COLOR_ACTION, x, y, z,
        RP_RADIUS_ACTION, GetPlayerVirtualWorld(playerid), false);
    if (gRPActionLabel[playerid] == Text3D:INVALID_3DTEXT_ID)
        return 0;
    Attach3DTextLabelToPlayer(gRPActionLabel[playerid], playerid, 0.0, 0.0, 0.5);
    SetTimerEx("RP_ActionExpire", RP_ACTION_DURATION, false, "ii", playerid, gRPActionSerial[playerid]);
    return 1;
}

public RP_ActionExpire(playerid, serial)
{
    if (IsPlayerConnected(playerid) && gRPActionSerial[playerid] == serial)
        RP_Reset(playerid);
    return 1;
}

// type: 0=me, 1=do, 2=try, 3=todo, 4=ame, 5=b, 6=s, 7=w, 8=low.
stock RP_Command(playerid, type, const params[])
{
    new input[RP_TEXT_MAX + 1], line[144], nickname[MAX_PLAYER_NAME];
    if (!RP_ReadText(params, input, sizeof(input)))
    {
        switch (type)
        {
            case 0: return SendClientMessage(playerid, COLOR_AMBER, "Синтаксис: /me действие (до 90 символов).");
            case 1: return SendClientMessage(playerid, COLOR_AMBER, "Синтаксис: /do описание (до 90 символов).");
            case 2: return SendClientMessage(playerid, COLOR_AMBER, "Синтаксис: /try попытка (до 90 символов).");
            case 3: return SendClientMessage(playerid, COLOR_AMBER, "Синтаксис: /todo речь*действие (всего до 90 символов).");
            case 4: return SendClientMessage(playerid, COLOR_AMBER, "Синтаксис: /ame действие (до 90 символов).");
            case 5: return SendClientMessage(playerid, COLOR_AMBER, "Синтаксис: /b сообщение (локальный OOC).");
            case 6: return SendClientMessage(playerid, COLOR_AMBER, "Синтаксис: /s сообщение (крик, 45 м).");
            case 7: return SendClientMessage(playerid, COLOR_AMBER, "Синтаксис: /w сообщение (шёпот, 4 м).");
            case 8: return SendClientMessage(playerid, COLOR_AMBER, "Синтаксис: /low сообщение (тихо, 8 м).");
        }
        return 1;
    }
    new speech[RP_TEXT_MAX + 1], action[RP_TEXT_MAX + 1], tail[RP_TEXT_MAX + 1];
    if (type == 3)
    {
        new split = -1;
        for (new i = 0; input[i] != EOS; i++)
        {
            if (input[i] == '*') { split = i; break; }
        }
        if (split < 1 || !input[split + 1])
            return SendClientMessage(playerid, COLOR_AMBER, "Пример: /todo Привет!*протянув руку");
        new originalLength = strlen(input);
        strmid(tail, input, split + 1, originalLength, sizeof(tail));
        input[split] = EOS;
        if (!RP_ReadText(input, speech, sizeof(speech)) ||
            !RP_ReadText(tail, action, sizeof(action)))
            return SendClientMessage(playerid, COLOR_AMBER, "Пример: /todo Привет!*протянув руку");
    }
    if (!RP_CanSpeak(playerid)) return 1;
    GetPlayerName(playerid, nickname, sizeof(nickname));
    switch (type)
    {
        case 0:
        {
            format(line, sizeof(line), "* %s %s", nickname, input);
            return RP_SendLocal(playerid, RP_COLOR_ACTION, line, RP_RADIUS_ACTION);
        }
        case 1:
        {
            format(line, sizeof(line), "* %s (( %s ))", input, nickname);
            return RP_SendLocal(playerid, RP_COLOR_DO, line, RP_RADIUS_ACTION);
        }
        case 2:
        {
            new bool:success = random(2) == 1;
            if (success)
                format(line, sizeof(line), "* %s пытается %s — удачно", nickname, input);
            else
                format(line, sizeof(line), "* %s пытается %s — неудачно", nickname, input);
            return RP_SendLocal(playerid, success ? RP_COLOR_SUCCESS : RP_COLOR_FAILURE,
                line, RP_RADIUS_ACTION);
        }
        case 3:
        {
            format(line, sizeof(line), "* %s: «%s» — %s", nickname, speech, action);
            return RP_SendLocal(playerid, RP_COLOR_ACTION, line, RP_RADIUS_ACTION);
        }
        case 4:
        {
            format(line, sizeof(line), "* %s %s", nickname, input);
            if (!RP_ShowActionLabel(playerid, line))
                return SendClientMessage(playerid, COLOR_RED, "Не удалось отобразить действие над персонажем.");
            return RP_SendLocal(playerid, RP_COLOR_ACTION, line, RP_RADIUS_ACTION);
        }
        case 5:
        {
            format(line, sizeof(line), "(( %s(%d): %s ))", nickname, playerid, input);
            return RP_SendLocal(playerid, RP_COLOR_OOC, line, RP_RADIUS_ACTION);
        }
        case 6:
        {
            format(line, sizeof(line), "%s(%d) кричит: %s!", nickname, playerid, input);
            return RP_SendLocal(playerid, COLOR_WHITE, line, RP_RADIUS_SHOUT);
        }
        case 7:
        {
            format(line, sizeof(line), "%s(%d) шепчет: %s", nickname, playerid, input);
            return RP_SendLocal(playerid, RP_COLOR_OOC, line, RP_RADIUS_WHISPER);
        }
        case 8:
        {
            format(line, sizeof(line), "%s(%d) тихо говорит: %s", nickname, playerid, input);
            return RP_SendLocal(playerid, COLOR_WHITE, line, RP_RADIUS_LOW);
        }
    }
    return 1;
}
