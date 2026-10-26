#if defined _tarkenso_radio_player_included
    #endinput
#endif
#define _tarkenso_radio_player_included

// RADIO-01: прямые потоки станций для персонального прослушивания.
// В клиенте open.mp одновременно может воспроизводиться только один URL.
// Доступность потока и поддержка кодека проверяются в игровом клиенте.
#define RADIO_DIALOG_LIST     (6950)
#define RADIO_STATION_COUNT   (10)
#define RADIO_COLOR           (0x8EACBCFF)

new const gRadioNames[][] =
{
    "Европа Плюс",
    "Ретро FM (СПб)",
    "Радио Рекорд",
    "Радио Орфей",
    "Radio Swiss Classic",
    "MDR Klassik",
    "ANTENNE BAYERN",
    "Heart 80s",
    "NPO Radio 2",
    "Radio Swiss Pop"
};

// Страна происхождения радиостанции, без группировки по категориям.
new const gRadioCountries[][] =
{
    "Россия",
    "Россия",
    "Россия",
    "Россия",
    "Швейцария",
    "Германия",
    "Германия",
    "Великобритания",
    "Нидерланды",
    "Швейцария"
};

new const gRadioUrls[][] =
{
    "https://europaplus.hostingradio.ru:8030/ep128",
    "https://emgspb.hostingradio.ru/retrofmspb128.mp3",
    "https://radiorecord.hostingradio.ru/rr_main96.aacp",
    "http://orfeyfm.hostingradio.ru:8034/orfeyfm128.mp3",
    "https://stream.srg-ssr.ch/srgssr/rsc_de/mp3/128",
    "https://mdr-284350-0.sslcast.mdr.de/mdr/284350/0/mp3/high/stream.mp3",
    "https://stream.antenne.de/antenne/stream/mp3",
    "https://media-ssl.musicradio.com/Heart80sMP3",
    "http://icecast.omroep.nl/radio2-bb-mp3",
    "https://stream.srg-ssr.ch/srgssr/rsp/mp3/128"
};

// -1 означает, что пользовательское радио не запускалось.
new gRadioStation[MAX_PLAYERS];

stock Radio_OnConnect(playerid)
{
    gRadioStation[playerid] = -1;
    return 1;
}

stock Radio_Stop(playerid)
{
    // Не прерываем гоночный саундтрек или поток другого модуля.
    if (gRadioStation[playerid] != -1 && IsPlayerConnected(playerid))
        StopAudioStreamForPlayer(playerid);
    gRadioStation[playerid] = -1;
    return 1;
}

stock Radio_OnDisconnect(playerid)
{
    Radio_Stop(playerid);
    return 1;
}

stock Radio_Open(playerid)
{
    new body[1024], row[112];
    format(body, sizeof(body), "№\tРадиостанция\tСтрана\n");
    for (new i = 0; i < RADIO_STATION_COUNT; i++)
    {
        format(row, sizeof(row), "%d\t%s\t%s\n",
            i + 1, gRadioNames[i], gRadioCountries[i]);
        strcat(body, row, sizeof(body));
    }
    strcat(body, "—\tВыключить радио\t—", sizeof(body));
    return ShowPlayerDialog(playerid, RADIO_DIALOG_LIST, DIALOG_STYLE_TABLIST_HEADERS,
        "Tarkenso RPG | Радио", body, "Слушать", "Закрыть");
}

stock Radio_Play(playerid, station)
{
    if (station < 0 || station >= RADIO_STATION_COUNT) return 1;
    Radio_Stop(playerid);
    if (!PlayAudioStreamForPlayer(playerid, gRadioUrls[station]))
        return SendClientMessage(playerid, COLOR_RED,
            "Не удалось передать поток клиенту. Выберите другую станцию.");

    gRadioStation[playerid] = station;
    new message[144];
    format(message, sizeof(message), "Радио: %s. Поток запрошен; /play — сменить или выключить.",
        gRadioNames[station]);
    SendClientMessage(playerid, RADIO_COLOR, message);
    return 1;
}

stock Radio_OnDialog(playerid, dialogid, response, listitem)
{
    if (dialogid != RADIO_DIALOG_LIST) return 0;
    if (gAuthState[playerid] != AUTH_READY ||
        AdminPunish_IsJailed(playerid) ||
        GetPlayerVirtualWorld(playerid) == RACE_WORLD_ID)
        return 1;
    if (!response) return 1;
    if (listitem == RADIO_STATION_COUNT)
    {
        Radio_Stop(playerid);
        return SendClientMessage(playerid, RADIO_COLOR, "Радио выключено.");
    }
    return Radio_Play(playerid, listitem);
}
