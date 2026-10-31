// TARKENSO scaling benchmark: actual PayDay_Grant, Leader_ValidateRoster
// and StudioZone_Poll, but with fake connected players and in-memory database.
// 100/250/500 are simulated connected counts / activity operations, not
// actual networked clients or production SQL latency.
#define MAX_PLAYERS (500)
#include <open.mp>
#pragma dynamic 32768
#define TARKENSO_DB_PRIMARY
#define AUTH_READY (3)
#define PAYDAY_LEVEL_LIMIT (100)
#define PAYDAY_EXP_STEP (4)
#define FACTION_COUNT (4)
#define FACTION_ROSTER_MAX (64)
#define FACTION_DEPUTY_MAX (2)
#define STUDIO_ZONE_COUNT (16)
#define STUDIO_ZONE_COLOR (0xFFFFFFFF)
#define STUDIO_ZONE_MAX_RADIUS (50.0)

new gDBPrimary = 1;
new bool:gDBPrimaryLoaded[MAX_PLAYERS];
new gDBPrimaryAccountId[MAX_PLAYERS];
new gDBPrimaryVersion[MAX_PLAYERS];
new gCharacterLevel[MAX_PLAYERS];
new gCharacterEXP[MAX_PLAYERS];
new gCharacterLastPaydayHour[MAX_PLAYERS];
new gAuthState[MAX_PLAYERS];

new gFactionCount[FACTION_COUNT];
new gFactionMemberName[FACTION_COUNT][FACTION_ROSTER_MAX][MAX_PLAYER_NAME];
new gFactionMemberRank[FACTION_COUNT][FACTION_ROSTER_MAX];
new gFactionTermStart[FACTION_COUNT];
new gFactionTermEnd[FACTION_COUNT];
new bool:gFactionAvailable[FACTION_COUNT];

new bool:gStudioZoneActive[STUDIO_ZONE_COUNT];
new Float:gStudioZoneX[STUDIO_ZONE_COUNT];
new Float:gStudioZoneY[STUDIO_ZONE_COUNT];
new Float:gStudioZoneZ[STUDIO_ZONE_COUNT];
new Float:gStudioZoneRadius[STUDIO_ZONE_COUNT];
new gStudioZoneWorld[STUDIO_ZONE_COUNT];
new gStudioZoneInterior[STUDIO_ZONE_COUNT];
new gStudioZoneMode[STUDIO_ZONE_COUNT];
new bool:gStudioZoneInside[MAX_PLAYERS][STUDIO_ZONE_COUNT];

new gPerfOnline;
new gPerfMessages;
new gPerfSQL;
new gPerfScore;
new gPerfChecksum;

stock bool:DBPrimary_IsReady() { return true; }
stock mysql_format(conn, query[], limit, const template[],
    level, exp, hourKey, account, version, previous)
{
    #pragma unused conn, template, level, exp, hourKey, account, version, previous
    if (limit) query[0] = EOS;
    return 1;
}
stock bool:DBPrimary_Execute(const query[], &affected)
{
    #pragma unused query
    affected = 1; ++gPerfSQL;
    return true;
}
stock bool:Leader_OrgValid(org)
{
    return org >= 1 && org <= FACTION_COUNT && gFactionAvailable[org-1];
}
stock bool:Admin_NormalizeName(const input[], output[], size)
{
    new n = strlen(input);
    if (n < 3 || n > 20 || n >= size) return false;
    for (new i; i < n; i++)
    {
        new ch = input[i];
        if (ch >= 'A' && ch <= 'Z') ch += 32;
        if (!((ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9') || ch == '_')) return false;
        output[i] = ch;
    }
    output[n] = EOS;
    return true;
}
stock bool:Perf_IsConnected(playerid)
{
    return playerid >= 0 && playerid < gPerfOnline;
}
stock bool:Perf_IsNPC(playerid)
{
    #pragma unused playerid
    return false;
}
stock Perf_Score(playerid, value)
{
    #pragma unused playerid
    gPerfScore += value;
    return 1;
}
stock Perf_Send(playerid, color, const message[])
{
    #pragma unused playerid, color, message
    ++gPerfMessages;
    return 1;
}
// Isolated scaling benchmarks mock presentation just like other native UI.
// Production missions use notification_style.inc; performance metrics here
// focus on money/EXP SQL logic and synthetic player counts.
#define TNOTIFY_GENERAL (0)
#define TNOTIFY_INFO (0)
#define TNOTIFY_HINT (3)
stock TNotify_Send(playerid,system,severity,const body[])
{
    #pragma unused system,severity
    return Perf_Send(playerid,0xFFFFFFFF,body);
}
stock TNotify_Detail(playerid,severity,const body[])
{
    #pragma unused severity
    return Perf_Send(playerid,0xFFFFFFFF,body);
}
stock Perf_Pos(playerid, &Float:x, &Float:y, &Float:z)
{
    x = float(playerid % 100);
    y = float((playerid / 10) % 100);
    z = 20.0;
    return 1;
}
stock Perf_World(playerid)
{
    #pragma unused playerid
    return 0;
}
stock Perf_Interior(playerid)
{
    #pragma unused playerid
    return 0;
}

// Native call adapters are applied only to included production functions.
#define IsPlayerConnected Perf_IsConnected
#define IsPlayerNPC Perf_IsNPC
#define SetPlayerScore Perf_Score
#define SendClientMessage Perf_Send
#define GetPlayerPos Perf_Pos
#define GetPlayerVirtualWorld Perf_World
#define GetPlayerInterior Perf_Interior

#include "payday.inc"
#include "perf_leader_validate.inc"
forward StudioZone_Poll();
#include "perf_studio_poll.inc"

#define PERF_PASSES (7)
#define PERF_ROUNDS_PAYDAY (36)
#define PERF_ROUNDS_FACTION (6)
#define PERF_ROUNDS_ZONE (70)

main() {}
forward Perf_Run();

stock Perf_Initialize(count)
{
    gPerfOnline = count;
    gPerfMessages = 0;
    gPerfSQL = 0;
    gPerfScore = 0;
    gPerfChecksum = 0;
    for (new i; i < MAX_PLAYERS; i++)
    {
        gDBPrimaryLoaded[i] = i < count;
        gDBPrimaryAccountId[i] = i+1;
        gDBPrimaryVersion[i] = 1;
        gCharacterLevel[i] = 7;
        gCharacterEXP[i] = 2;
        gCharacterLastPaydayHour[i] = 0;
        gAuthState[i] = i < count ? AUTH_READY : 0;
        for (new z; z < STUDIO_ZONE_COUNT; z++)
            gStudioZoneInside[i][z] = false;
    }
    for (new org; org < FACTION_COUNT; org++)
    {
        gFactionAvailable[org] = true;
        gFactionCount[org] = FACTION_ROSTER_MAX;
        gFactionTermStart[org] = 1;
        gFactionTermEnd[org] = 200000;
        for (new j; j < FACTION_ROSTER_MAX; j++)
        {
            format(gFactionMemberName[org][j], MAX_PLAYER_NAME,
                "member_%d_%d", org, j);
            gFactionMemberRank[org][j] = j == 0 ? 10 : (j < 3 ? 9 : 1);
        }
    }
    for (new z; z < STUDIO_ZONE_COUNT; z++)
    {
        gStudioZoneActive[z] = true;
        gStudioZoneX[z] = float(z * 10);
        gStudioZoneY[z] = float(z * 10);
        gStudioZoneZ[z] = 20.0;
        gStudioZoneRadius[z] = 4.0;
        gStudioZoneWorld[z] = 0;
        gStudioZoneInterior[z] = 0;
        gStudioZoneMode[z] = z % 2;
    }
    return 1;
}

stock Perf_Scenario(count, kind)
{
    new start = GetTickCount();
    switch (kind)
    {
        case 0:
        {
            for (new r; r < PERF_ROUNDS_PAYDAY; r++)
            {
                for (new i; i < count; i++)
                {
                    gCharacterEXP[i] = 2;
                    gCharacterLastPaydayHour[i] = 0;
                    gDBPrimaryVersion[i] = 1;
                    if (PayDay_Grant(i, 1000)) gPerfChecksum++;
                }
            }
        }
        case 1:
        {
            for (new r; r < PERF_ROUNDS_FACTION; r++)
                for (new i; i < count; i++)
                    if (Leader_ValidateRoster((i % FACTION_COUNT)+1))
                        gPerfChecksum++;
        }
        case 2:
        {
            for (new r; r < PERF_ROUNDS_ZONE; r++)
            {
                StudioZone_Poll();
                gPerfChecksum += gPerfOnline;
            }
        }
    }
    return GetTickCount()-start;
}

public OnGameModeInit()
{
    SetTimer("Perf_Run", 100, false);
    return 1;
}
public Perf_Run()
{
    new labels[][] = {"payday", "organizations", "zones"};
    new cases[] = {100, 250, 500};
    for (new kind; kind < 3; kind++)
    {
        for (new scenario; scenario < sizeof(cases); scenario++)
        {
            new count = cases[scenario];
            Perf_Initialize(count);
            // Warm the same Pawn/AMX code before timing.
            Perf_Scenario(count, kind);
            for (new pass; pass < PERF_PASSES; pass++)
            {
                Perf_Initialize(count);
                new elapsed = Perf_Scenario(count, kind);
                new checksum = gPerfChecksum;
                // Include side effects to prevent silent no-op behaviour.
                if (kind == 0 && gPerfSQL != PERF_ROUNDS_PAYDAY*count)
                {
                    print("PERF_SCALE_ERROR: PayDay SQL mock count mismatch");
                    return 0;
                }
                if (checksum <= 0)
                {
                    print("PERF_SCALE_ERROR: zero result checksum");
                    return 0;
                }
                printf("PERF_SCALE:name=%s_%d,pass=%d,ms=%d,checksum=%d",
                    labels[kind], count, pass, elapsed, checksum);
            }
        }
    }
    print("PERF_SCALE_DONE:cases=9,samples=7");
    return 1;
}
