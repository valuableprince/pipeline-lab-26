#if defined _tarkenso_broadcast_included
    #endinput
#endif
#define _tarkenso_broadcast_included

// BROADCAST-01. Game-side control for a trusted externally published live MP3 stream.
// The Pawn server cannot capture a microphone or verify that an Icecast source is live.
// Player clients require no voice plugin; the publisher uses a separate browser gateway.
#define BROADCAST_CONFIG       "broadcast.cfg"
#define BROADCAST_URL_LEN      (192)
#define BROADCAST_DURATION     (120)
#define BROADCAST_COOLDOWN     (300)
#define BROADCAST_TICK_MS      (1000)
#define BROADCAST_DIALOG       (6970)
#define BROADCAST_COLOR        (0xF2CB87FF)

new gBroadcastUrl[BROADCAST_URL_LEN];
new gBroadcastOwner = INVALID_PLAYER_ID;
new gBroadcastOwnerToken;
new gBroadcastEndsAt;
new gBroadcastNextAllowedAt;
new gBroadcastTimer;
new bool:gBroadcastPlaying[MAX_PLAYERS];
new bool:gBroadcastAttempted[MAX_PLAYERS];
new gBroadcastPreviousRadio[MAX_PLAYERS];

stock bool:Broadcast_IsActive()
{
    return gBroadcastOwner != INVALID_PLAYER_ID;
}

stock bool:Broadcast_IsActiveForPlayer(playerid)
{
    return Broadcast_IsActive() && playerid >= 0 && playerid < MAX_PLAYERS &&
        gBroadcastAttempted[playerid];
}

stock bool:Broadcast_AllowedUrl(const url[])
{
    if (strfind(url, "https://audio.tarkenso.ru/", true) != 0)
        return false;
    new len = strlen(url);
    if (len <= strlen("https://audio.tarkenso.ru/") || len >= BROADCAST_URL_LEN)
        return false;
    for (new i = 0; i < len; i++)
    {
        if (url[i] < 33 || url[i] > 126 ||
            url[i] == '"' || url[i] == '\\' || url[i] == '<' ||
            url[i] == '>' || url[i] == '?' || url[i] == '#' ||
            url[i] == '@' || url[i] == '|')
            return false;
    }
    if (strfind(url, "..") != -1) return false;
    return true;
}

stock Broadcast_LoadConfig()
{
    gBroadcastUrl[0] = EOS;
    new File:config = fopen(BROADCAST_CONFIG, io_read);
    if (!config)
    {
        print("[BROADCAST] Disabled: scriptfiles/broadcast.cfg is missing.");
        return 0;
    }
    new line[BROADCAST_URL_LEN], extra[2];
    new bool:valid = bool:fread(config, line, sizeof(line));
    if (valid)
    {
        new len = strlen(line);
        while (len && (line[len - 1] == '\n' || line[len - 1] == '\r'))
            line[--len] = EOS;
        valid = Broadcast_AllowedUrl(line);
        if (fread(config, extra, sizeof(extra))) valid = false;
    }
    fclose(config);
    if (!valid)
    {
        print("[BROADCAST] Disabled: invalid stream URL or extra configuration lines.");
        return 0;
    }
    format(gBroadcastUrl, sizeof(gBroadcastUrl), "%s", line);
    print("[BROADCAST] Icecast URL configured; source availability requires manual verification.");
    return 1;
}

stock Broadcast_OnConnect(playerid)
{
    gBroadcastPlaying[playerid] = false;
    gBroadcastAttempted[playerid] = false;
    gBroadcastPreviousRadio[playerid] = -1;
    return 1;
}

stock Broadcast_Apply(playerid)
{
    if (!Broadcast_IsActive() || !IsPlayerConnected(playerid) ||
        gAuthState[playerid] != AUTH_READY || gBroadcastAttempted[playerid]) return 0;
    gBroadcastAttempted[playerid] = true;
    gBroadcastPreviousRadio[playerid] = gRadioStation[playerid];
    // One client-side URL slot: the broadcast temporarily owns it.
    if (gRadioStation[playerid] != -1 || gRaceAudioPlaying[playerid] ||
        gStoryTapePlaying[playerid])
        StopAudioStreamForPlayer(playerid);
    // A cassette cannot be resumed at its previous playback position.
    gStoryTapePlaying[playerid] = false;
    if (!PlayAudioStreamForPlayer(playerid, gBroadcastUrl))
    {
        SendClientMessage(playerid, COLOR_RED,
            "Не удалось запустить поток объявления. Проверьте настройки аудио.");
        return 0;
    }
    gBroadcastPlaying[playerid] = true;
    GameTextForPlayer(playerid, "~y~TARKENSO~w~: LIVE BROADCAST", 6000, 3);
    SendClientMessage(playerid, BROADCAST_COLOR,
        "[ЭФИР] Объявление администрации. Интернет-радио временно приостановлено.");
    return 1;
}

stock Broadcast_Restore(playerid)
{
    if (!gBroadcastAttempted[playerid])
    {
        gBroadcastPreviousRadio[playerid] = -1;
        return 0;
    }
    new bool:wasPlaying = gBroadcastPlaying[playerid];
    gBroadcastPlaying[playerid] = false;
    gBroadcastAttempted[playerid] = false;
    if (!IsPlayerConnected(playerid))
    {
        gBroadcastPreviousRadio[playerid] = -1;
        return 0;
    }
    if (wasPlaying) StopAudioStreamForPlayer(playerid);
    if (gAuthState[playerid] != AUTH_READY)
    {
        gBroadcastPreviousRadio[playerid] = -1;
        return 1;
    }
    if (Race_IsParticipant(playerid) && gRaceMode == RACE_IN_PROGRESS &&
        gRaceAudioEnabled[playerid])
    {
        new stage = gRaceAudioStage[playerid];
        if (stage < 1 || stage > RACE_PLAYLIST_COUNT)
            stage = gRacePlaylistReady ? RacePlaylist_StartingStage(gRaceAudioRound) : 1;
        if (RaceAudio_Play(playerid, stage))
            gRaceAudioNextAt[playerid] = gettime() - gRaceStartAt +
                (gRacePlaylistReady ? RacePlaylist_StageDuration(stage) : RACE_AUDIO_TRACK_A_SEC);
    }
    else if (gBroadcastPreviousRadio[playerid] >= 0)
        Radio_Play(playerid, gBroadcastPreviousRadio[playerid]);
    gBroadcastPreviousRadio[playerid] = -1;
    return 1;
}

stock Broadcast_Stop(const reason[])
{
    if (!Broadcast_IsActive()) return 0;
    new owner = gBroadcastOwner;
    gBroadcastOwner = INVALID_PLAYER_ID;
    gBroadcastEndsAt = 0;
    for (new i = 0; i < MAX_PLAYERS; i++)
        Broadcast_Restore(i);
    new line[144];
    format(line, sizeof(line), "[ЭФИР] Объявление завершено (%s).", reason);
    SendClientMessageToAll(BROADCAST_COLOR, line);
    printf("[BROADCAST] End owner=%d reason=%s", owner, reason);
    return 1;
}

stock Broadcast_Start(playerid)
{
    if (!Admin_Require(playerid, PERM_MANAGE_STAFF)) return 1;
    if (!gBroadcastUrl[0])
        return SendClientMessage(playerid, ADMIN_ERROR,
            "Эфир не настроен: нужен Icecast и scriptfiles/broadcast.cfg.");
    if (Broadcast_IsActive())
    {
        if (gBroadcastOwner != playerid && Admin_IsFounder(playerid))
            Broadcast_Stop("приоритет основателя");
        else
            return SendClientMessage(playerid, ADMIN_ERROR,
                "Эфир уже занят. /broadcast status.");
    }
    if (!Admin_IsFounder(playerid) && gettime() < gBroadcastNextAllowedAt)
        return SendClientMessage(playerid, ADMIN_ERROR,
            "Между объявлениями требуется перерыв 5 минут.");
    if (!Admin_LogPlayer(playerid, "BROADCAST_START", "live audio (120s)"))
        return SendClientMessage(playerid, ADMIN_ERROR,
            "Журнал администрации недоступен. Эфир не начат.");
    gBroadcastOwner = playerid;
    gBroadcastOwnerToken = gAuthToken[playerid];
    gBroadcastEndsAt = gettime() + BROADCAST_DURATION;
    gBroadcastNextAllowedAt = gettime() + BROADCAST_COOLDOWN;
    new line[144];
    format(line, sizeof(line),
        "[ЭФИР] Администратор %s ведёт голосовое объявление.", gAccountName[playerid]);
    SendClientMessageToAll(BROADCAST_COLOR, line);
    for (new i = 0; i < MAX_PLAYERS; i++)
        Broadcast_Apply(i);
    SendClientMessage(playerid, ADMIN_OK,
        "Эфир запрошен на 120 секунд. Сначала включите микрофон в веб-студии.");
    return 1;
}

stock Broadcast_Command(playerid, const params[])
{
    if (!Admin_Require(playerid, PERM_MANAGE_STAFF)) return 1;
    if (isnull(params))
        return ShowPlayerDialog(playerid, BROADCAST_DIALOG, DIALOG_STYLE_LIST,
            "TARKENSO | Прямой эфир",
            "Начать вещание (до 120 секунд)\nЗавершить вещание\nСтатус эфира",
            "Выбрать", "Закрыть");
    if (!strcmp(params, "start", true)) return Broadcast_Start(playerid);
    if (!strcmp(params, "stop", true))
    {
        if (!Broadcast_IsActive())
            return SendClientMessage(playerid, ADMIN_ERROR, "Сейчас нет активного эфира.");
        if (gBroadcastOwner != playerid && !Admin_IsFounder(playerid))
            return SendClientMessage(playerid, ADMIN_ERROR, "Остановить чужой эфир может только основатель.");
        Admin_LogPlayer(playerid, "BROADCAST_STOP", "operator stop");
        Broadcast_Stop("остановлено администратором");
        return 1;
    }
    if (!strcmp(params, "status", true))
    {
        if (!Broadcast_IsActive())
            return SendClientMessage(playerid, BROADCAST_COLOR,
                gBroadcastUrl[0] ? "Эфир свободен. /broadcast start." :
                "Эфир отключён: отсутствует broadcast.cfg.");
        new line[144];
        format(line, sizeof(line), "Эфир: %s, осталось %d секунд.",
            gAccountName[gBroadcastOwner], gBroadcastEndsAt - gettime());
        return SendClientMessage(playerid, BROADCAST_COLOR, line);
    }
    return SendClientMessage(playerid, BROADCAST_COLOR,
        "Использование: /broadcast [start|stop|status].");
}

stock Broadcast_OnDialog(playerid, dialogid, response, listitem)
{
    if (dialogid != BROADCAST_DIALOG) return 0;
    if (!response) return 1;
    if (!Admin_Require(playerid, PERM_MANAGE_STAFF)) return 1;
    switch (listitem)
    {
        case 0: return Broadcast_Start(playerid);
        case 1: return Broadcast_Command(playerid, "stop");
        case 2: return Broadcast_Command(playerid, "status");
    }
    return 1;
}

stock Broadcast_OnReady(playerid)
{
    return Broadcast_Apply(playerid);
}

stock Broadcast_OnDisconnect(playerid)
{
    if (Broadcast_IsActive() && gBroadcastOwner == playerid)
        Broadcast_Stop("ведущий отключился");
    gBroadcastPlaying[playerid] = false;
    gBroadcastAttempted[playerid] = false;
    gBroadcastPreviousRadio[playerid] = -1;
    return 1;
}

stock Broadcast_OnDutyDisabled(playerid)
{
    if (Broadcast_IsActive() && gBroadcastOwner == playerid)
        Broadcast_Stop("ведущий вышел из Duty");
    return 1;
}

forward Broadcast_Tick();
public Broadcast_Tick()
{
    if (!Broadcast_IsActive()) return 1;
    new playerid = gBroadcastOwner;
    if (!IsPlayerConnected(playerid) || gAuthToken[playerid] != gBroadcastOwnerToken ||
        gAuthState[playerid] != AUTH_READY || gAdminLevel[playerid] < 5 ||
        !gAdminDuty[playerid] || gettime() >= gBroadcastEndsAt)
    {
        Broadcast_Stop("лимит времени или потеря полномочий");
        return 1;
    }
    for (new i = 0; i < MAX_PLAYERS; i++)
        Broadcast_Apply(i);
    return 1;
}

stock Broadcast_Init()
{
    gBroadcastOwner = INVALID_PLAYER_ID;
    gBroadcastNextAllowedAt = 0;
    Broadcast_LoadConfig();
    gBroadcastTimer = SetTimer("Broadcast_Tick", BROADCAST_TICK_MS, true);
    if (!gBroadcastTimer)
        print("[BROADCAST] ERROR: broadcast watchdog timer unavailable.");
    return 1;
}

stock Broadcast_Shutdown()
{
    Broadcast_Stop("перезапуск сервера");
    if (gBroadcastTimer) KillTimer(gBroadcastTimer);
    gBroadcastTimer = 0;
    return 1;
}
