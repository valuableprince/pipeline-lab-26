#if defined _tarkenso_broadcast_state_included
    #endinput
#endif
#define _tarkenso_broadcast_state_included

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

