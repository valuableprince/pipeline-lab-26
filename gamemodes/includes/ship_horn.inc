#if defined _tarkenso_ship_horn_included
    #endinput
#endif
#define _tarkenso_ship_horn_included

// SHIP-HORN-01: Pixabay "ship horn very close" (sound ID 14642).
// The licensed file is not redistributed with the game source.
// Host the MP3 yourself, then put its DIRECT HTTPS URL in horn_url.txt.
// Audio streaming shares the single native audio channel with radio; skip
// all players already using the server radio or race soundtrack.
#define SHIP_HORN_CFG SHIP_DIR "/horn_url.txt"
#define SHIP_HORN_URL_MAX (255)
#define SHIP_HORN_RADIUS (220.0)
#define SHIP_HORN_COOLDOWN (12)

new gShipHornURL[SHIP_HORN_URL_MAX + 1];
new gShipHornLastTime;

stock bool:ShipHorn_URLValid(const url[])
{
    new len = strlen(url);
    if (len < 13 || len > SHIP_HORN_URL_MAX) return false;
    if (strfind(url, "https://", true) != 0) return false;
    if (strfind(url, ".mp3", true) < 0 && strfind(url, ".ogg", true) < 0)
        return false;
    for (new i = 0; i < len; i++)
        // Reject spaces/control characters and URL-breaking punctuation.
        if (url[i] <= 32 || url[i] == 92 || url[i] == 34 ||
            url[i] == 39 || url[i] == 60 || url[i] == 62)
            return false;
    return true;
}

stock ShipHorn_Load()
{
    gShipHornURL[0] = EOS;
    if (!fexist(SHIP_HORN_CFG))
    {
        print("[ShipHorn] URL not configured; place a direct MP3 URL in shipstudio/horn_url.txt.");
        return 0;
    }
    new File:file = fopen(SHIP_HORN_CFG, io_read);
    if (!file) return 0;
    new url[SHIP_HORN_URL_MAX + 3];
    new bool:ok = fread(file, url, sizeof(url)) != 0;
    fclose(file);
    if (!ok) return 0;
    Auth_StripLine(url);
    if (!ShipHorn_URLValid(url))
    {
        print("[ShipHorn] Invalid URL: expected direct HTTPS .mp3 or .ogg in horn_url.txt.");
        return 0;
    }
    format(gShipHornURL, sizeof(gShipHornURL), "%s", url);
    print("[ShipHorn] Configured for the selected Pixabay ship horn.");
    return 1;
}

// Returns -1 when the asset has not been configured, -2 for cooldown,
// otherwise returns the number of player streams requested (may be zero).
stock ShipHorn_Play()
{
    if (!Ship_Ready()) return -1;
    if (!gShipHornURL[0]) return -1;
    new now = gettime();
    if (gShipHornLastTime && now - gShipHornLastTime < SHIP_HORN_COOLDOWN)
        return -2;
    new Float:x, Float:y, Float:z;
    GetObjectPos(gShipRoot, x, y, z);
    new Float:maxDistance2 = SHIP_HORN_RADIUS * SHIP_HORN_RADIUS;
    new heard = 0;
    for (new playerid = 0; playerid < MAX_PLAYERS; playerid++)
    {
        if (!IsPlayerConnected(playerid) || IsPlayerNPC(playerid) ||
            gAuthState[playerid] != AUTH_READY ||
            GetPlayerVirtualWorld(playerid) != 0 ||
            GetPlayerInterior(playerid) != 0 ||
            gRadioStation[playerid] != -1 ||
            Race_IsParticipant(playerid))
            continue;

        new Float:px, Float:py, Float:pz;
        GetPlayerPos(playerid, px, py, pz);
        new Float:dx = px-x, Float:dy = py-y, Float:dz = pz-z;
        if (dx*dx+dy*dy+dz*dz > maxDistance2) continue;
        // The selected sound is a single 5-6s MP3. No StopAudioStream timer:
        // stopping later would interrupt a player who started the radio.
        if (PlayAudioStreamForPlayer(playerid, gShipHornURL,
            x, y, z, SHIP_HORN_RADIUS, true))
            heard++;
    }
    gShipHornLastTime = now;
    return heard;
}

stock ShipHorn_Manual(playerid)
{
    if (!Studio_Require(playerid)) return 1;
    if (!Ship_Ready())
        return SendClientMessage(playerid, ADMIN_ERROR,
            "[Tarkenso RPG] Сначала создайте или загрузите корабль.");
    new result = ShipHorn_Play();
    if (result == -1)
        return SendClientMessage(playerid, ADMIN_ERROR,
            "[Tarkenso RPG] Гудок не настроен: нужен shipstudio/horn_url.txt с прямым HTTPS MP3.");
    if (result == -2)
        return SendClientMessage(playerid, ADMIN_ERROR,
            "[Tarkenso RPG] Подождите перед повторным гудком (12 секунд).");
    new msg[128];
    format(msg, sizeof(msg),
        "[Tarkenso RPG] Гудок отправлен %d игрокам рядом с кораблём (радио не прерывается).",
        result);
    return SendClientMessage(playerid, SHIP_COLOR, msg);
}

stock ShipHorn_Init()
{
    gShipHornLastTime = 0;
    ShipHorn_Load();
    return 1;
}
