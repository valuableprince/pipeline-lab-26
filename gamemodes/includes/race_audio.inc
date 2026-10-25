#if defined _tarkenso_race_audio_included
    #endinput
#endif
#define _tarkenso_race_audio_included

// RACE-AUDIO-03. Hard acid techno / aggressive techno MP3 BarkTunes (CC BY 4.0).
// Нельзя указывать YouTube/Apple Music HTML вместо прямого музыкального файла.
// В одном SA-MP клиенте одновременно поддерживается только один аудиопоток.
// Roger That, Elbucky (Remastered): 158 BPM, hard acid techno.
// Stab 'em with Your Fork: 146 BPM, aggressive techno/breakbeats.
#define RACE_AUDIO_TRACK_A       "https://barktunes.jp/song/BarkTunes_-_Roger_That,_Elbucky_(Remastered).mp3"
#define RACE_AUDIO_TRACK_B       "https://barktunes.jp/song/BarkTunes_-_Stab_'em_with_Your_Fork.mp3"
#define RACE_AUDIO_TRACK_A_SEC   (150)
#define RACE_AUDIO_PREVIEW_SEC   (15)
#define RACE_AUDIO_COLOR         (0xA6B7E8FF)

new bool:gRaceAudioEnabled[MAX_PLAYERS];
new bool:gRaceAudioPlaying[MAX_PLAYERS];
new gRaceAudioStage[MAX_PLAYERS];
new gRaceAudioPreviewUntil[MAX_PLAYERS];
new gRaceAudioNextAt[MAX_PLAYERS];
new gRaceAudioRound;

stock RaceAudio_Stop(playerid)
{
    // Не останавливаем чужое радио, если наша система не запускала поток.
    if (gRaceAudioPlaying[playerid] && IsPlayerConnected(playerid) &&
        !Broadcast_IsActiveForPlayer(playerid))
        StopAudioStreamForPlayer(playerid);
    gRaceAudioPlaying[playerid] = false;
    gRaceAudioStage[playerid] = 0;
    gRaceAudioPreviewUntil[playerid] = 0;
    gRaceAudioNextAt[playerid] = 0;
    return 1;
}

stock RaceAudio_OnConnect(playerid)
{
    // Настройка действует на сеанс игры: по умолчанию музыка включена.
    gRaceAudioEnabled[playerid] = true;
    gRaceAudioPlaying[playerid] = false;
    gRaceAudioStage[playerid] = 0;
    gRaceAudioPreviewUntil[playerid] = 0;
    gRaceAudioNextAt[playerid] = 0;
    return 1;
}

stock RaceAudio_OnDisconnect(playerid)
{
    RaceAudio_Stop(playerid);
    gRaceAudioEnabled[playerid] = false;
    return 1;
}

// Выполнение PlayAudioStreamForPlayer не подтверждает загрузку/декодирование
// HTTP-файла клиентом; проверка слышимости — обязательный игровой UAT.
stock RaceAudio_OnRaceStart()
{
    if (++gRaceAudioRound > 1000000) gRaceAudioRound = 1;
    return 1;
}

stock bool:RaceAudio_Play(playerid, stage)
{
    if (!IsPlayerConnected(playerid) || !gRaceAudioEnabled[playerid] ||
        Broadcast_IsActiveForPlayer(playerid))
        return false;
    new bool:ok = false;
    if (gRacePlaylistReady)
    {
        if (stage < 1 || stage > RACE_PLAYLIST_COUNT) return false;
        ok = PlayAudioStreamForPlayer(playerid, gRacePlaylistUrls[stage - 1]);
    }
    else if (stage == 1)
        ok = PlayAudioStreamForPlayer(playerid, RACE_AUDIO_TRACK_A);
    else if (stage == 2)
        ok = PlayAudioStreamForPlayer(playerid, RACE_AUDIO_TRACK_B);
    if (!ok) return false;
    gRaceAudioPlaying[playerid] = true;
    gRaceAudioStage[playerid] = stage;
    return true;
}

stock RaceAudio_Announce(playerid, stage)
{
    if (!gRacePlaylistReady) return 0;
    new message[160];
    format(message, sizeof(message), "RACING RADIO: %s - %s.",
        gRacePlaylistTitles[stage - 1], gRacePlaylistArtists[stage - 1]);
    SendClientMessage(playerid, RACE_AUDIO_COLOR, message);
    return 1;
}

stock RaceAudio_Begin(playerid)
{
    RaceAudio_Stop(playerid);
    if (!gRaceAudioEnabled[playerid]) return 0;
    new stage = gRacePlaylistReady ? RacePlaylist_StartingStage(gRaceAudioRound) : 1;
    if (!RaceAudio_Play(playerid, stage))
    {
        SendClientMessage(playerid, COLOR_RED, "Не удалось отправить музыкальный поток.");
        return 0;
    }
    gRaceAudioNextAt[playerid] = gRacePlaylistReady ?
        RacePlaylist_StageDuration(stage) : RACE_AUDIO_TRACK_A_SEC;
    if (gRacePlaylistReady) RaceAudio_Announce(playerid, stage);
    else SendClientMessage(playerid, RACE_AUDIO_COLOR,
        "RACING RADIO: Roger That, Elbucky — BarkTunes. /raceaudio off — отключить.");
    return 1;
}

stock RaceAudio_Tick(playerid, elapsed)
{
    if (Broadcast_IsActiveForPlayer(playerid)) return 1;
    if (!gRaceAudioEnabled[playerid] || !gRaceAudioPlaying[playerid] ||
        gRaceAudioPreviewUntil[playerid] != 0) return 0;
    if (gRacePlaylistReady)
    {
        if (elapsed < gRaceAudioNextAt[playerid]) return 1;
        new next = RacePlaylist_NextStage(gRaceAudioStage[playerid]);
        if (!RaceAudio_Play(playerid, next))
        {
            RaceAudio_Stop(playerid);
            return 0;
        }
        gRaceAudioNextAt[playerid] = elapsed + RacePlaylist_StageDuration(next);
        RaceAudio_Announce(playerid, next);
        return 1;
    }
    if (elapsed >= RACE_AUDIO_TRACK_A_SEC && gRaceAudioStage[playerid] == 1)
    {
        if (!RaceAudio_Play(playerid, 2))
        {
            RaceAudio_Stop(playerid);
            return 0;
        }
        SendClientMessage(playerid, RACE_AUDIO_COLOR,
            "RACING RADIO: Stab 'em with Your Fork — BarkTunes.");
    }
    return 1;
}

stock RaceAudio_PreviewTick()
{
    new now = gettime();
    for (new playerid = 0; playerid < MAX_PLAYERS; playerid++)
        if (gRaceAudioPreviewUntil[playerid] > 0 &&
            now >= gRaceAudioPreviewUntil[playerid])
        {
            RaceAudio_Stop(playerid);
            if (IsPlayerConnected(playerid))
                SendClientMessage(playerid, RACE_AUDIO_COLOR,
                    "Проверка аудиопотока окончена.");
        }
    return 1;
}

stock RaceAudio_Status(playerid)
{
    if (!gRaceAudioEnabled[playerid])
        return SendClientMessage(playerid, RACE_AUDIO_COLOR,
            "Гоночное радио выключено. /raceaudio on — включить.");
    if (gRaceAudioPlaying[playerid])
        return SendClientMessage(playerid, RACE_AUDIO_COLOR,
            "Гоночное радио включено. Поток запрошен у клиента.");
    SendClientMessage(playerid, RACE_AUDIO_COLOR,
        "Гоночное радио включено. Ожидание старта.");
    return 1;
}

stock RaceAudio_ListPlaylist(playerid)
{
    if (gRacePlaylistReady)
        SendClientMessage(playerid, RACE_AUDIO_COLOR,
            "RACING RADIO: лицензированный плейлист из 6 композиций включён.");
    else
        SendClientMessage(playerid, RACE_AUDIO_COLOR,
            "RACING RADIO: плейлист ожидает лицензированные файлы; работает резервная музыка.");
    new message[160];
    for (new i = 0; i < RACE_PLAYLIST_COUNT; i++)
    {
        format(message, sizeof(message), "%d. %s - %s",
            i + 1, gRacePlaylistTitles[i], gRacePlaylistArtists[i]);
        SendClientMessage(playerid, COLOR_GREY, message);
    }
    return 1;
}

stock RaceAudio_Credits(playerid)
{
    SendClientMessage(playerid, RACE_AUDIO_COLOR,
        "Roger That, Elbucky / Stab 'em with Your Fork — BarkTunes (barktunes.jp).");
    SendClientMessage(playerid, COLOR_GREY,
        "Лицензия CC BY 4.0 — BarkTunes. Подробности: docs/RACE_AUDIO.md.");
    return 1;
}

stock RaceAudio_Command(playerid, const params[], bool:inRace, bool:raceStarted, elapsed)
{
    if (Broadcast_IsActiveForPlayer(playerid))
        return SendClientMessage(playerid, RACE_AUDIO_COLOR,
            "Во время объявления управление гоночным радио приостановлено.");
    if (isnull(params))
        return SendClientMessage(playerid, RACE_AUDIO_COLOR,
            "Команды: /raceaudio on | off | test | playlist | status | credits.");
    if (!strcmp(params, "status", true)) return RaceAudio_Status(playerid);
    if (!strcmp(params, "credits", true)) return RaceAudio_Credits(playerid);
    if (!strcmp(params, "playlist", true)) return RaceAudio_ListPlaylist(playerid);
    if (!strcmp(params, "off", true))
    {
        gRaceAudioEnabled[playerid] = false;
        RaceAudio_Stop(playerid);
        return SendClientMessage(playerid, RACE_AUDIO_COLOR, "Гоночное радио выключено.");
    }
    if (!strcmp(params, "on", true))
    {
        gRaceAudioEnabled[playerid] = true;
        // При позднем включении запускаем актуальную часть плейлиста сначала.
        if (inRace && raceStarted)
        {
            RaceAudio_Stop(playerid);
            new stage = gRacePlaylistReady ?
                RacePlaylist_StartingStage(gRaceAudioRound) :
                (elapsed >= RACE_AUDIO_TRACK_A_SEC ? 2 : 1);
            if (!RaceAudio_Play(playerid, stage))
                return SendClientMessage(playerid, COLOR_RED,
                    "Не удалось отправить музыкальный поток.");
            gRaceAudioNextAt[playerid] = elapsed +
                (gRacePlaylistReady ? RacePlaylist_StageDuration(stage) :
                RACE_AUDIO_TRACK_A_SEC);
            if (gRacePlaylistReady) RaceAudio_Announce(playerid, stage);
        }
        return SendClientMessage(playerid, RACE_AUDIO_COLOR, "Гоночное радио включено.");
    }
    if (!strcmp(params, "test", true))
    {
        if (inRace)
            return SendClientMessage(playerid, COLOR_GREY,
                "Во время гонки используйте /raceaudio on.");
        gRaceAudioEnabled[playerid] = true;
        RaceAudio_Stop(playerid);
        if (!RaceAudio_Play(playerid, 1))
            return SendClientMessage(playerid, COLOR_RED, "Не удалось запустить тест.");
        if (gRacePlaylistReady) RaceAudio_Announce(playerid, 1);
        gRaceAudioPreviewUntil[playerid] = gettime() + RACE_AUDIO_PREVIEW_SEC;
        return SendClientMessage(playerid, RACE_AUDIO_COLOR,
            "Тест BarkTunes / hard techno: 15 секунд. Проверьте звук в клиенте.");
    }
    return SendClientMessage(playerid, RACE_AUDIO_COLOR,
        "Команды: /raceaudio on | off | test | playlist | status | credits.");
}
