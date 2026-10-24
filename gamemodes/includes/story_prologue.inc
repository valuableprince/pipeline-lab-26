#if defined _tarkenso_story_prologue_included
    #endinput
#endif
#define _tarkenso_story_prologue_included

// STORY-01: "Signal 03:17 / Room 17" first playable investigation.
// Prototype markers in Fort Carson; exterior placement requires in-game UAT.
// Each objective is private. No checkpoints, NPCs or job state are modified.
#define STORY_DIALOG_LETTER    (6440)
#define STORY_DIALOG_RECEPTION (6441)
#define STORY_DIALOG_ROOM      (6442)
#define STORY_DIALOG_RADIO     (6443)
#define STORY_DIALOG_JOURNAL   (6444)
#define STORY_DIALOG_TAPE      (6445)
#define STORY_TAPE_CONFIG      "story_tape.cfg"
#define STORY_TAPE_URL_SIZE    (192)
#define STORY_DONE            (4)
#define STORY_MAP_ICON        (88)
#define STORY_COLOR           (0xE6BB79FF)
#define STORY_INTERACT_SQ     (16.0)

new bool:gStoryIntroSeen[MAX_PLAYERS];
new gStoryLastInteract[MAX_PLAYERS];
new PlayerText3D:gStoryLabel[MAX_PLAYERS];
new gStoryTapeUrl[STORY_TAPE_URL_SIZE];
new bool:gStoryTapePlaying[MAX_PLAYERS];

new const Float:gStoryLocations[3][3] = {
    {-252.0, 1104.0, 19.6}, // Pilot position: motel reception
    {-252.0, 1140.0, 19.6}, // Pilot position: Room 17
    {-287.0, 1122.0, 19.6}  // Pilot position: abandoned transmitter
};

// Copy story_tape.cfg to scriptfiles. One direct HTTPS MP3/OGG URL per file.
// Domain allowlist avoids passing arbitrary user-supplied URLs to game clients.
stock bool:Story_TapeAllowedUrl(const url[])
{
    if (strfind(url, "https://tarkenso.ru/", true) != 0 &&
        strfind(url, "https://audio.tarkenso.ru/", true) != 0)
        return false;
    new length = strlen(url);
    if (length < 28 || length >= STORY_TAPE_URL_SIZE) return false;
    if (strfind(url, ".mp3", true) != length - 4 &&
        strfind(url, ".ogg", true) != length - 4) return false;
    for (new i = 0; i < length; i++)
        if (url[i] < 33 || url[i] > 126 || url[i] == '"' ||
            url[i] == '\\' || url[i] == '<' || url[i] == '>')
            return false;
    return true;
}

stock Story_TapeInit()
{
    gStoryTapeUrl[0] = EOS;
    new File:handle = fopen(STORY_TAPE_CONFIG, io_read);
    if (!handle)
    {
        print("[STORY-AUDIO] story_tape.cfg not found; text-only cassette active.");
        return 1;
    }
    new line[STORY_TAPE_URL_SIZE];
    new bool:valid = bool:fread(handle, line, sizeof(line));
    if (valid)
    {
        new length = strlen(line);
        while (length > 0 && (line[length - 1] == '\r' ||
            line[length - 1] == '\n')) line[--length] = EOS;
        valid = Story_TapeAllowedUrl(line);
    }
    fclose(handle);
    if (!valid)
    {
        print("[STORY-AUDIO] Invalid URL; text-only cassette active.");
        return 1;
    }
    format(gStoryTapeUrl, sizeof(gStoryTapeUrl), "%s", line);
    print("[STORY-AUDIO] Room 17 cassette URL configured.");
    return 1;
}

stock Story_TapeStop(playerid)
{
    if (gStoryTapePlaying[playerid] && IsPlayerConnected(playerid))
        StopAudioStreamForPlayer(playerid);
    gStoryTapePlaying[playerid] = false;
    return 1;
}

stock Story_OnConnect(playerid)
{
    gStoryStage[playerid] = 0;
    gStoryIntroSeen[playerid] = false;
    gStoryLastInteract[playerid] = 0;
    gStoryLabel[playerid] = PlayerText3D:INVALID_3DTEXT_ID;
    gStoryTapePlaying[playerid] = false;
    return 1;
}

stock Story_ClearMarker(playerid)
{
    if (gStoryLabel[playerid] != PlayerText3D:INVALID_3DTEXT_ID)
    {
        DeletePlayer3DTextLabel(playerid, gStoryLabel[playerid]);
        gStoryLabel[playerid] = PlayerText3D:INVALID_3DTEXT_ID;
    }
    RemovePlayerMapIcon(playerid, STORY_MAP_ICON);
    return 1;
}

stock Story_OnDisconnect(playerid)
{
    Story_TapeStop(playerid);
    Story_ClearMarker(playerid);
    gStoryStage[playerid] = 0;
    gStoryIntroSeen[playerid] = false;
    gStoryLastInteract[playerid] = 0;
    return 1;
}

stock Story_ObjectiveText(playerid, dest[], size)
{
    switch (gStoryStage[playerid])
    {
        case 0: format(dest, size, "Прочитайте письмо Норы: /story.");
        case 1: format(dest, size,
            "Форт-Карсон: найдите стойку мотеля, заберите ключ от номера 17.");
        case 2: format(dest, size,
            "Форт-Карсон: откройте номер 17 и найдите запись.");
        case 3: format(dest, size,
            "Форт-Карсон: проверьте источник сигнала возле мотеля.");
        default: format(dest, size,
            "Пролог завершён. Находки сохранены в досье. Продолжение следует.");
    }
    return 1;
}

stock Story_UpdateMarker(playerid)
{
    Story_ClearMarker(playerid);
    new stage = gStoryStage[playerid];
    if (stage < 1 || stage >= STORY_DONE) return 1;
    new index = stage - 1, text[110];
    switch (stage)
    {
        case 1: format(text, sizeof(text), "{E6BB79}СИГНАЛ 03:17\n{FFFFFF}Стойка мотеля | левый ALT");
        case 2: format(text, sizeof(text), "{E6BB79}НОМЕР 17\n{FFFFFF}Осмотреть дверь | левый ALT");
        case 3: format(text, sizeof(text), "{E6BB79}РАДИОСИГНАЛ\n{FFFFFF}Проверить источник | левый ALT");
    }
    gStoryLabel[playerid] = CreatePlayer3DTextLabel(playerid, text, STORY_COLOR,
        gStoryLocations[index][0], gStoryLocations[index][1],
        gStoryLocations[index][2] + 0.65, 20.0);
    SetPlayerMapIcon(playerid, STORY_MAP_ICON,
        gStoryLocations[index][0], gStoryLocations[index][1],
        gStoryLocations[index][2], 0, STORY_COLOR, MAPICON_LOCAL);
    return 1;
}

stock bool:Story_NearObjective(playerid)
{
    new stage = gStoryStage[playerid];
    if (stage < 1 || stage >= STORY_DONE ||
        !IsPlayerConnected(playerid) ||
        gAuthState[playerid] != AUTH_READY ||
        GetPlayerVirtualWorld(playerid) != 0 ||
        GetPlayerInterior(playerid) != 0 ||
        GetPlayerState(playerid) != PLAYER_STATE_ONFOOT)
        return false;

    new Float:x, Float:y, Float:z;
    GetPlayerPos(playerid, x, y, z);
    new index = stage - 1;
    new Float:dx = x - gStoryLocations[index][0];
    new Float:dy = y - gStoryLocations[index][1];
    new Float:dz = z - gStoryLocations[index][2];
    return dx * dx + dy * dy + dz * dz <= STORY_INTERACT_SQ;
}

stock Story_TapePlay(playerid)
{
    // The radio URL does not exist yet until the owner publishes the voiceover.
    // Fail closed to the existing text scene; never fake successful playback.
    if (!gStoryTapeUrl[0]) return 0;
    if (gStoryStage[playerid] != 2 || !Story_NearObjective(playerid))
        return 0;
    Story_TapeStop(playerid);
    if (!PlayAudioStreamForPlayer(playerid, gStoryTapeUrl,
        gStoryLocations[1][0], gStoryLocations[1][1],
        gStoryLocations[1][2], 18.0, true))
        return 0;
    gStoryTapePlaying[playerid] = true;
    ShowPlayerDialog(playerid, STORY_DIALOG_TAPE, DIALOG_STYLE_MSGBOX,
        "Номер 17 | Кассета играет",
        "{E6BB79}КОНТУР / ПРОТОКОЛ 03:17\n{94A3B8}__________________________________\n\n{FFFFFF}Щелчок механизма. Шипение магнитной ленты.\nСлышен мужской голос, затем голос Норы.\n\n{94A3B8}Запись идёт из магнитофона рядом с вами.\nМожно забрать кассету или прослушать её снова.",
        "Забрать кассету", "Повторить");
    return 1;
}

stock Story_ShowLetter(playerid)
{
    if (gAuthState[playerid] != AUTH_READY) return 1;
    ShowPlayerDialog(playerid, STORY_DIALOG_LETTER, DIALOG_STYLE_MSGBOX,
        "03:17 | Письмо без обратного адреса",
        "{E6BB79}НОРА\n{94A3B8}Письмо получено три дня назад.\n\n{FFFFFF}«Если ты слышишь сигнал в 03:17,\nне отвечай. Меня не было в той машине.\nВ Форт-Карсоне есть старый мотель.\nНомер 17. Ключ оставлен у стойки.\nНе доверяй записям в архиве.»\n\n{94A3B8}Внизу — незнакомый символ и одна буква: Н.",
        "Прочесть", "Позже");
    return 1;
}

stock Story_ShowJournal(playerid)
{
    if (gAuthState[playerid] != AUTH_READY) return 1;
    new objective[150], body[650];
    Story_ObjectiveText(playerid, objective, sizeof(objective));
    format(body, sizeof(body),
        "{E6BB79}СИГНАЛ: 03:17 | НОМЕР 17\n{94A3B8}____________________________________\n\n{FFFFFF}Этап расследования: {D5B477}%d/%d\n\n{FFFFFF}Текущая задача:\n{E6BB79}%s\n\n{94A3B8}Точки отмечены на карте. Подойдите пешком\nи нажмите левый ALT для взаимодействия.\nСохранение: автоматически после каждой находки.",
        gStoryStage[playerid], STORY_DONE, objective);
    ShowPlayerDialog(playerid, STORY_DIALOG_JOURNAL, DIALOG_STYLE_MSGBOX,
        "TARKENSO | Журнал расследования", body, "Понятно", "Закрыть");
    return 1;
}

stock Story_Command(playerid)
{
    if (gStoryStage[playerid] == 0) return Story_ShowLetter(playerid);
    return Story_ShowJournal(playerid);
}

stock bool:Story_Advance(playerid, expected)
{
    if (gAuthState[playerid] != AUTH_READY || gStoryStage[playerid] != expected ||
        expected < 0 || expected >= STORY_DONE)
        return false;
    if (expected > 0 && !Story_NearObjective(playerid)) return false;
#if defined TARKENSO_DB_PRIMARY
    if (!DBPrimary_IsReady() || !gDBPrimaryLoaded[playerid] ||
        gDBPrimaryAccountId[playerid] <= 0 ||
        gDBPrimaryVersion[playerid] >= 2147483647)
        return false;
    new q[256], affected;
    mysql_format(gDBPrimary, q, sizeof(q),
        "UPDATE accounts SET story_stage=%d,row_version=row_version+1 WHERE account_id=%d AND row_version=%d AND story_stage=%d",
        expected + 1, gDBPrimaryAccountId[playerid],
        gDBPrimaryVersion[playerid], expected);
    if (!DBPrimary_Execute(q, affected) || affected != 1)
    {
        printf("[STORY] Failed to save stage for player %d (expected %d).", playerid, expected);
        return false;
    }
    gDBPrimaryVersion[playerid]++;
#endif
    if (expected == 2) Story_TapeStop(playerid);
    gStoryStage[playerid] = expected + 1;
    Story_UpdateMarker(playerid);
    new objective[150];
    Story_ObjectiveText(playerid, objective, sizeof(objective));
    SendClientMessage(playerid, STORY_COLOR, objective);
    if (gStoryStage[playerid] == STORY_DONE)
        SendClientMessage(playerid, STORY_COLOR,
            "ПРОЛОГ ЗАВЕРШЁН. Номер 17. История продолжится в следующей главе.");
    return true;
}

stock Story_OnSpawn(playerid)
{
    Story_TapeStop(playerid);
    if (gAuthState[playerid] != AUTH_READY) return 1;
    Story_UpdateMarker(playerid);
    if (gStoryStage[playerid] == 0 && !gStoryIntroSeen[playerid])
    {
        gStoryIntroSeen[playerid] = true;
        return Story_ShowLetter(playerid);
    }
    new objective[150];
    Story_ObjectiveText(playerid, objective, sizeof(objective));
    SendClientMessage(playerid, STORY_COLOR, objective);
    return 1;
}

stock Story_OnKeyStateChange(playerid, KEY:newkeys, KEY:oldkeys)
{
    if (!(newkeys & KEY_WALK) || (oldkeys & KEY_WALK)) return 0;
    if (!Story_NearObjective(playerid)) return 0;
    if (gStoryLastInteract[playerid] &&
        GetTickCount() - gStoryLastInteract[playerid] < 1000) return 1;
    gStoryLastInteract[playerid] = GetTickCount();
    switch (gStoryStage[playerid])
    {
        case 1:
            ShowPlayerDialog(playerid, STORY_DIALOG_RECEPTION, DIALOG_STYLE_MSGBOX,
                "Номер 17 | Стойка регистрации",
                "{94A3B8}Стойка покрыта пылью. Книга постояльцев открыта\nна странице, где есть только одна запись.\n\n{E6BB79}НОМЕР 17 — НОРА — 03:17\n\n{FFFFFF}Под книгой лежит ключ с цифрой 17.\nРядом записка: «Не стучи. Дверь уже открыта».",
                "Забрать", "Отойти");
        case 2:
        {
            if (gStoryTapeUrl[0])
                ShowPlayerDialog(playerid, STORY_DIALOG_ROOM, DIALOG_STYLE_MSGBOX,
                    "Номер 17 | Старый магнитофон",
                    "{94A3B8}На тумбочке стоит кассетный магнитофон.\n{FFFFFF}Внутри кассета с надписью «КОНТУР / 03:17».\n\n{E6BB79}Нажать PLAY и прослушать запись?",
                    "PLAY", "Отойти");
            else
                ShowPlayerDialog(playerid, STORY_DIALOG_ROOM, DIALOG_STYLE_MSGBOX,
                    "Номер 17 | Плёнка (текстовая версия)",
                    "{94A3B8}Магнитофон. Надпись: «КОНТУР. ПРОТОКОЛ 03:17».\n\n{FFFFFF}Запись повреждена. Из расшифровки: Нора сообщает,\nчто архивные записи подделаны и нужно найти передатчик.\n\n{94A3B8}Заберите кассету для расследования.",
                    "Взять кассету", "Назад");
        }
        case 3:
            ShowPlayerDialog(playerid, STORY_DIALOG_RADIO, DIALOG_STYLE_MSGBOX,
                "03:17 | Неизвестная частота",
                "{94A3B8}У старого передатчика внезапно загорается индикатор.\nВ эфире треск. Затем — знакомый голос.\n\n{E6BB79}«Я жива. Но это не я вернулась.\nНе верь архиву. Найди тех, кого не было».\n\n{FFFFFF}Тишина. На металлическом корпусе нацарапано:\n{E6BB79}КОНТУР / Н-17.\n\n{94A3B8}Первое расследование завершено.",
                "Запомнить", "Отойти");
    }
    return 1;
}

stock Story_OnDialog(playerid, dialogid, response)
{
    if (dialogid < STORY_DIALOG_LETTER || dialogid > STORY_DIALOG_TAPE)
        return 0;
    if (gAuthState[playerid] != AUTH_READY) return 1;
    if (dialogid == STORY_DIALOG_JOURNAL) return 1;
    if (dialogid == STORY_DIALOG_TAPE)
    {
        if (!response)
        {
            if (!Story_TapePlay(playerid))
                SendClientMessage(playerid, COLOR_RED,
                    "Не удалось повторить запись. Попробуйте взаимодействовать снова.");
            return 1;
        }
        Story_TapeStop(playerid);
        if (!Story_Advance(playerid, 2))
            SendClientMessage(playerid, COLOR_RED,
                "Не удалось сохранить кассету. Вернитесь к магнитофону.");
        return 1;
    }
    if (!response) return 1;
    if (dialogid == STORY_DIALOG_ROOM && gStoryTapeUrl[0])
    {
        if (Story_TapePlay(playerid)) return 1;
        // A failed stream request is not a confirmed recording playback.
        // Let the investigator complete the scene with readable evidence.
        SendClientMessage(playerid, STORY_COLOR,
            "Аудио недоступно. Расшифровка: архив подделан; найдите передатчик.");
    }
    new expected = dialogid - STORY_DIALOG_LETTER;
    if (!Story_Advance(playerid, expected))
        SendClientMessage(playerid, COLOR_RED,
            "Не удалось подтвердить находку. Проверьте позицию или соединение с БД.");
    return 1;
}
