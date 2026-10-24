#if defined _tarkenso_race_playlist_included
    #endinput
#endif
#define _tarkenso_race_playlist_included

// RACE-PLAYLIST-02. Метаданные выбранных игроком записей не дают
// автоматически прав на публичное воспроизведение в игре.
// Включается только с корректным локальным списком разрешённых MP3/OGG URL.
#define RACE_PLAYLIST_FILE        "race_playlist.cfg"
#define RACE_PLAYLIST_COUNT       (6)
#define RACE_PLAYLIST_URL_SIZE    (192)
#define RACE_PLAYLIST_LINE_SIZE   (224)

new const gRacePlaylistTitles[RACE_PLAYLIST_COUNT][72] =
{
    "Vielleicht Vielleicht (Holy Priest & elMefti Remix)",
    "Vois sur ton chemin (Techno Mix)",
    "Derniere danse (Techno Mix)",
    "Heute Nacht",
    "Insomnia",
    "Rave Never Ends"
};

new const gRacePlaylistArtists[RACE_PLAYLIST_COUNT][48] =
{
    "MilleniumKid & JBs",
    "BENNETT",
    "Indila & BENNETT",
    "Maddix",
    "Restricted & NIKSTER",
    "Novah"
};

new gRacePlaylistUrls[RACE_PLAYLIST_COUNT][RACE_PLAYLIST_URL_SIZE];
new gRacePlaylistSeconds[RACE_PLAYLIST_COUNT];
new bool:gRacePlaylistReady;

stock RacePlaylist_Reset()
{
    gRacePlaylistReady = false;
    for (new i = 0; i < RACE_PLAYLIST_COUNT; i++)
    {
        gRacePlaylistUrls[i][0] = EOS;
        gRacePlaylistSeconds[i] = 0;
    }
    return 1;
}

stock RacePlaylist_Trim(line[])
{
    new length = strlen(line);
    while (length > 0 &&
        (line[length - 1] == '\r' || line[length - 1] == '\n'))
        line[--length] = EOS;
    return 1;
}

stock bool:RacePlaylist_AllowedUrl(const url[])
{
    // Только прямые медиафайлы с управляемого владельцем проекта домена.
    if (strfind(url, "https://tarkenso.ru/", true) != 0 &&
        strfind(url, "https://audio.tarkenso.ru/", true) != 0)
        return false;
    new length = strlen(url);
    if (length < 28 || length >= RACE_PLAYLIST_URL_SIZE)
        return false;
    if (strfind(url, ".mp3", true) != length - 4 &&
        strfind(url, ".ogg", true) != length - 4)
        return false;
    for (new i = 0; i < length; i++)
        if (url[i] < 33 || url[i] > 126 ||
            url[i] == '|' || url[i] == '\\' ||
            url[i] == '<' || url[i] == '>' || url[i] == '"')
            return false;
    return true;
}

stock bool:RacePlaylist_ParseLine(const line[], slot)
{
    new delimiter = strfind(line, "|");
    if (delimiter < 2 || delimiter > 4) return false;
    new duration = 0;
    for (new i = 0; i < delimiter; i++)
    {
        if (line[i] < '0' || line[i] > '9') return false;
        duration = duration * 10 + (line[i] - '0');
    }
    if (duration < 45 || duration > 1800) return false;
    new url[RACE_PLAYLIST_URL_SIZE];
    strmid(url, line, delimiter + 1, strlen(line), sizeof(url));
    if (!RacePlaylist_AllowedUrl(url)) return false;
    format(gRacePlaylistUrls[slot], RACE_PLAYLIST_URL_SIZE, "%s", url);
    gRacePlaylistSeconds[slot] = duration;
    return true;
}

stock bool:RacePlaylist_Load()
{
    RacePlaylist_Reset();
    // fopen ищет в scriptfiles относительно сервера. Отсутствие файла не
    // блокирует заезды: используется проверенный CC BY fallback.
    new File:file = fopen(RACE_PLAYLIST_FILE, io_read);
    if (!file)
    {
        print("[RACE] Licensed playlist not configured; fallback soundtrack enabled.");
        return false;
    }
    new line[RACE_PLAYLIST_LINE_SIZE];
    new bool:valid = true;
    for (new i = 0; i < RACE_PLAYLIST_COUNT; i++)
    {
        if (!fread(file, line, sizeof(line)))
        {
            valid = false;
            break;
        }
        RacePlaylist_Trim(line);
        if (!RacePlaylist_ParseLine(line, i))
        {
            valid = false;
            break;
        }
    }
    // Строго шесть строк. Лишние или неполные записи выключают весь набор.
    if (valid && fread(file, line, sizeof(line))) valid = false;
    fclose(file);
    if (!valid)
    {
        RacePlaylist_Reset();
        print("[RACE] Invalid race_playlist.cfg; using fallback soundtrack.");
        return false;
    }
    gRacePlaylistReady = true;
    print("[RACE] Licensed playlist configured: six media files.");
    return true;
}

stock RacePlaylist_StageDuration(stage)
{
    if (stage < 1 || stage > RACE_PLAYLIST_COUNT) return 0;
    return gRacePlaylistSeconds[stage - 1];
}

stock RacePlaylist_StartingStage(round)
{
    if (round < 1) return 1;
    return ((round - 1) % RACE_PLAYLIST_COUNT) + 1;
}

stock RacePlaylist_NextStage(stage)
{
    if (stage >= RACE_PLAYLIST_COUNT || stage < 1) return 1;
    return stage + 1;
}
