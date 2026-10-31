#if defined _signal_leader_system_included
    #endinput
#endif
#define _signal_leader_system_included

// LEADER-01. Организации независимы от административных уровней.
#define FACTION_DIRECTORY "signal_factions"
#define FACTION_COUNT (8)
#define FACTION_FINANCE (5)
#define FACTION_CRIME_FIRST (6)
#define FACTION_CRIME_LAST (8)
#define FACTION_ROSTER_MAX (64)
#define FACTION_DEPUTY_MAX (2)
#define FACTION_TERM_DAYS (30)
#define FACTION_TERM_SECONDS (2592000)
#define FACTION_INVITE_SECONDS (60)
#define FACTION_COLOR (0x9DD4B4FF)
#define FACTION_ERROR (0xE27E82FF)

new gFactionCount[FACTION_COUNT];
new gFactionMemberName[FACTION_COUNT][FACTION_ROSTER_MAX][MAX_PLAYER_NAME];
new gFactionMemberRank[FACTION_COUNT][FACTION_ROSTER_MAX];
new gFactionMemberJoined[FACTION_COUNT][FACTION_ROSTER_MAX];
new gFactionMemberSeen[FACTION_COUNT][FACTION_ROSTER_MAX];
new gFactionTermStart[FACTION_COUNT];
new gFactionTermEnd[FACTION_COUNT];
new bool:gFactionAvailable[FACTION_COUNT];
new gFactionPendingOrg[MAX_PLAYERS];
new gFactionPendingInviter[MAX_PLAYERS];
new gFactionPendingExpiry[MAX_PLAYERS];
new gFactionPendingToken[MAX_PLAYERS];
new gFactionInviteCooldown[MAX_PLAYERS];
// Кеш онлайн-игроков: быстрый доступ для /r и проверки полномочий.
new gFactionCachedOrg[MAX_PLAYERS];
new gFactionCachedRank[MAX_PLAYERS];

stock bool:Leader_OrgValid(org)
{
    return org >= 1 && org <= FACTION_COUNT && gFactionAvailable[org - 1];
}

stock Leader_OrgTitle(org, output[], size)
{
    switch (org)
    {
        case 1: format(output, size, "Департамент шерифа");
        case 2: format(output, size, "Медицинская служба");
        case 3: format(output, size, "Радиостанция 03:17");
        case 4: format(output, size, "Исследовательский отдел");
        case FACTION_FINANCE: format(output, size, "Министерство финансов");
        case 6: format(output, size, "Синдикат Гавань");
        case 7: format(output, size, "Консорциум Вектор");
        case 8: format(output, size, "Сеть Орион");
        default: format(output, size, "Неизвестная организация");
    }
    return 1;
}

// Keep citizens' public name and TAB color solid white. The radar is a
// separate per-viewer semi-transparent white square (RRGGBBAA).
#define PLAYER_COLOR_CIVILIAN (0xFFFFFFFF)
#define PLAYER_MARKER_CIVILIAN (0xFFFFFF77)

stock Leader_ColorForOrg(org)
{
    switch (org)
    {
        case 1: return 0x3478C0FF; // Sheriff Department
        case 2: return 0xE76B70FF; // Emergency Medical Service
        case 3: return 0xD6A64DFF; // Radio 03:17
        case 4: return 0x8B75C9FF; // Reserved Research Department
        case 5: return 0x48A889FF; // Ministry of Finance
        case 6: return 0xB87949FF; // Syndicate Haven (CRIME-01)
        case 7: return 0xA85D86FF; // Vector Consortium (CRIME-01)
        case 8: return 0x579BA8FF; // Orion Network (CRIME-01)
    }
    return PLAYER_COLOR_CIVILIAN;
}

stock Leader_RadarColorForOrg(org)
{
    if (!Leader_OrgValid(org)) return PLAYER_MARKER_CIVILIAN;
    return Leader_ColorForOrg(org);
}

// Per-viewer override only changes the rendered player marker, keeping
// SetPlayerColor's public white citizen RGB for nick and TAB.
stock Leader_RefreshRadarMarker(viewerid, targetid)
{
    if (!IsPlayerConnected(viewerid) || IsPlayerNPC(viewerid) ||
        !IsPlayerConnected(targetid) || IsPlayerNPC(targetid) ||
        viewerid == targetid) return 0;
    new org = gFactionCachedOrg[targetid];
    if (gAuthState[targetid] != AUTH_READY || !Leader_OrgValid(org)) org = 0;
    SetPlayerMarkerForPlayer(viewerid, targetid, Leader_RadarColorForOrg(org));
    return 1;
}

stock Leader_RefreshRadarForViewer(viewerid)
{
    if (!IsPlayerConnected(viewerid) || IsPlayerNPC(viewerid)) return 0;
    for (new targetid = 0; targetid < MAX_PLAYERS; targetid++)
        if (IsPlayerConnected(targetid) && viewerid != targetid)
            Leader_RefreshRadarMarker(viewerid, targetid);
    return 1;
}

// Called only at lifecycle transitions, never in a high-frequency player loop.
stock Leader_ApplyPlayerColor(playerid)
{
    if (!IsPlayerConnected(playerid)) return 0;
    new org = gFactionCachedOrg[playerid];
    if (gAuthState[playerid] != AUTH_READY || !Leader_OrgValid(org)) org = 0;
    // SetPlayerColor restores the default marker for all viewers, so reapply
    // their per-viewer marker afterwards (also handles faction changes).
    SetPlayerColor(playerid, Leader_ColorForOrg(org));
    for (new viewerid = 0; viewerid < MAX_PLAYERS; viewerid++)
        if (IsPlayerConnected(viewerid) && viewerid != playerid)
            Leader_RefreshRadarMarker(viewerid, playerid);
    return 1;
}

stock Leader_FilePath(org, output[], size)
{
    format(output, size, "%s/org_%d.dat", FACTION_DIRECTORY, org);
    return 1;
}

stock bool:Leader_ParseNumber(const input[], &number)
{
    new len = strlen(input);
    if (!len || len > 10) return false;
    for (new i = 0; i < len; i++)
        if (input[i] < '0' || input[i] > '9') return false;
    // Ограничение signed 32-bit Unix time.
    if (strlen(input) == 10 && strcmp(input, "2147483647", false) > 0) return false;
    number = strval(input);
    return number >= 0;
}

stock bool:Leader_ParseMember(const source[], name[], &rank, &joined, &seen)
{
    new parts[4][32], field = 0, offset = 0;
    for (new i = 0; source[i] != EOS; i++)
    {
        if (source[i] == '\n' || source[i] == '\r') break;
        if (source[i] == '|')
        {
            if (++field > 3) return false;
            offset = 0;
            continue;
        }
        if (offset >= 31 || source[i] < 32) return false;
        parts[field][offset++] = source[i];
    }
    if (field != 3) return false;
    new normalized[MAX_PLAYER_NAME];
    if (!Admin_NormalizeName(parts[0], normalized, sizeof(normalized)) ||
        strcmp(parts[0], normalized, true) != 0) return false;
    if (!Leader_ParseNumber(parts[1], rank) || rank < 1 || rank > 10 ||
        !Leader_ParseNumber(parts[2], joined) || !Leader_ParseNumber(parts[3], seen)) return false;
    format(name, MAX_PLAYER_NAME, "%s", normalized);
    return true;
}

stock bool:Leader_CopyFile(const source[], const target[])
{
    new File:input = fopen(source, io_read);
    if (!input) return false;
    new File:output = fopen(target, io_write);
    if (!output)
    {
        fclose(input);
        return false;
    }
    new line[160], bool:ok = true;
    while (fread(input, line, sizeof(line)))
    {
        if (fwrite(output, line) <= 0) { ok = false; break; }
    }
    fclose(input);
    fclose(output);
    if (!ok) fremove(target);
    return ok;
}

// Проверка инвариантов до записи: одна должность лидера, корректный срок,
// отсутствие дубликатов и не более двух заместителей.
// Crime organizations: 1..5 (5=leader, 4=deputy).
// Existing state and civil factions retain their own rank limits.
stock bool:Leader_IsCriminal(org)
{
    return org >= FACTION_CRIME_FIRST && org <= FACTION_CRIME_LAST;
}

stock Leader_MaxRank(org)
{
    if (org == FACTION_FINANCE) return 4;
    if (Leader_IsCriminal(org)) return 5;
    return 10;
}

stock Leader_LeaderRank(org)
{
    return Leader_MaxRank(org);
}

stock Leader_DeputyRank(org)
{
    if (org == FACTION_FINANCE) return 3;
    if (Leader_IsCriminal(org)) return 4;
    return 9;
}

stock Leader_CrimeRankTitle(rank, output[], size)
{
    switch (rank)
    {
        case 1: format(output, size, "Новобранец");
        case 2: format(output, size, "Исполнитель");
        case 3: format(output, size, "Координатор");
        case 4: format(output, size, "Заместитель");
        case 5: format(output, size, "Лидер");
        default: format(output, size, "Неизвестный ранг");
    }
    return 1;
}

stock bool:Leader_ValidateRoster(org)
{
    if (!Leader_OrgValid(org)) return false;
    new o = org - 1, leaders, deputies, normalized[MAX_PLAYER_NAME];
    if (gFactionCount[o] < 0 || gFactionCount[o] > FACTION_ROSTER_MAX) return false;
    for (new i = 0; i < gFactionCount[o]; i++)
    {
        if (!Admin_NormalizeName(gFactionMemberName[o][i], normalized, sizeof(normalized)) ||
            strcmp(gFactionMemberName[o][i], normalized, true) ||
            gFactionMemberRank[o][i] < 1 || gFactionMemberRank[o][i] > Leader_MaxRank(org))
            return false;
        if (gFactionMemberRank[o][i] == Leader_LeaderRank(org)) leaders++;
        if (gFactionMemberRank[o][i] == Leader_DeputyRank(org)) deputies++;
        for (new j = 0; j < i; j++)
            if (!strcmp(gFactionMemberName[o][i], gFactionMemberName[o][j], true))
                return false;
    }
    if (leaders > 1 || deputies > FACTION_DEPUTY_MAX) return false;
    if (!leaders) return gFactionTermStart[o] == 0 && gFactionTermEnd[o] == 0;
    return gFactionTermStart[o] > 0 && gFactionTermEnd[o] > gFactionTermStart[o];
}

stock bool:Leader_Save(org)
{
    if (!Leader_ValidateRoster(org)) return false;
#if defined TARKENSO_DB_PRIMARY
    return DBWorld_LeaderSave(org);
#else
    new o = org - 1, path[96], temp[112], backup[112], line[160];
    Leader_FilePath(org, path, sizeof(path));
    format(temp, sizeof(temp), "%s.tmp", path);
    format(backup, sizeof(backup), "%s.bak", path);
    if (fexist(temp) && !fremove(temp)) return false;
    new File:file = fopen(temp, io_write);
    if (!file) return false;
    new bool:ok = fwrite(file, "SIGNALFACTION|1\n") > 0;
    format(line, sizeof(line), "%d\n%d\n%d\n", gFactionTermStart[o], gFactionTermEnd[o], gFactionCount[o]);
    if (fwrite(file, line) <= 0) ok = false;
    for (new i = 0; i < gFactionCount[o]; i++)
    {
        format(line, sizeof(line), "%s|%d|%d|%d\n",
            gFactionMemberName[o][i], gFactionMemberRank[o][i],
            gFactionMemberJoined[o][i], gFactionMemberSeen[o][i]);
        if (fwrite(file, line) <= 0) ok = false;
    }
    fclose(file);
    if (!ok) { fremove(temp); return false; }

    // При замене старый рабочий файл не удаляется: переносится в .bak.
    new bool:hadOriginal = fexist(path) != 0;
    if (hadOriginal)
    {
        if (fexist(backup) && !fremove(backup))
        {
            fremove(temp);
            return false;
        }
        if (!frename(path, backup))
        {
            fremove(temp);
            return false;
        }
    }
    if (!frename(temp, path))
    {
        if (hadOriginal && !frename(backup, path))
        {
            gFactionAvailable[o] = false;
            printf("[LEADER] Cannot restore organization %d from .bak; faction disabled.", org);
        }
        fremove(temp);
        return false;
    }
    // Резервная копия не перезаписывает успешно сохранённый состав.
    if (hadOriginal && fexist(backup)) fremove(backup);
    return true;
#endif
}

stock bool:Leader_Load(org)
{
#if defined TARKENSO_DB_PRIMARY
    return DBWorld_LeaderLoad(org);
#else
    new o = org - 1, path[96], header[32], line[160], startLine[32], endLine[32], countLine[16];
    Leader_FilePath(org, path, sizeof(path));
    if (!fexist(path))
    {
        new backup[112];
        format(backup, sizeof(backup), "%s.bak", path);
        if (fexist(backup))
        {
            if (!Leader_CopyFile(backup, path)) return false;
        }
        else return Leader_Save(org);
    }
    new File:file = fopen(path, io_read);
    if (!file) return false;
    new bool:ok = true;
    if (!fread(file, header, sizeof(header)) ||
        !fread(file, startLine, sizeof(startLine)) ||
        !fread(file, endLine, sizeof(endLine)) ||
        !fread(file, countLine, sizeof(countLine))) ok = false;
    if (ok)
    {
        Auth_StripLine(header);
        Auth_StripLine(startLine);
        Auth_StripLine(endLine);
        Auth_StripLine(countLine);
        if (strcmp(header, "SIGNALFACTION|1", false) ||
            !Leader_ParseNumber(startLine, gFactionTermStart[o]) ||
            !Leader_ParseNumber(endLine, gFactionTermEnd[o]) ||
            !Leader_ParseNumber(countLine, gFactionCount[o]) ||
            gFactionCount[o] > FACTION_ROSTER_MAX) ok = false;
    }
    new leaders, deputies;
    for (new i = 0; ok && i < gFactionCount[o]; i++)
    {
        if (!fread(file, line, sizeof(line)) ||
            !Leader_ParseMember(line, gFactionMemberName[o][i],
                gFactionMemberRank[o][i], gFactionMemberJoined[o][i], gFactionMemberSeen[o][i]))
        {
            ok = false;
            break;
        }
        if (gFactionMemberRank[o][i] > Leader_MaxRank(org)) { ok = false; break; }
        if (gFactionMemberRank[o][i] == Leader_LeaderRank(org)) leaders++;
        if (gFactionMemberRank[o][i] == Leader_DeputyRank(org)) deputies++;
        for (new j = 0; j < i; j++)
            if (!strcmp(gFactionMemberName[o][j], gFactionMemberName[o][i], true))
                ok = false;
    }
    if (leaders > 1 || deputies > FACTION_DEPUTY_MAX) ok = false;
    if (leaders == 0 && (gFactionTermStart[o] || gFactionTermEnd[o])) ok = false;
    if (leaders == 1 && (gFactionTermStart[o] <= 0 ||
        gFactionTermEnd[o] <= gFactionTermStart[o])) ok = false;
    if (ok && fread(file, line, sizeof(line))) ok = false;
    fclose(file);
    return ok;
#endif
}

stock bool:Leader_Audit(const actor[], const action[], const details[])
{
    new File:file = fopen(FACTION_DIRECTORY "/audit.log", io_append);
    if (!file) return false;
    new safe[96], line[220];
    format(safe, sizeof(safe), "%s", details);
    Admin_SafeText(safe, sizeof(safe));
    format(line, sizeof(line), "%d|%s|%s|%s\n", gettime(), actor, action, safe);
    new wrote = fwrite(file, line);
    fclose(file);
    return wrote > 0;
}

stock bool:Leader_AuditPlayer(playerid, const action[], const details[])
{
    return Leader_Audit(gAccountName[playerid], action, details);
}

stock Leader_Find(org, const name[])
{
    if (!Leader_OrgValid(org)) return -1;
    new o = org - 1;
    for (new i = 0; i < gFactionCount[o]; i++)
        if (!strcmp(gFactionMemberName[o][i], name, true)) return i;
    return -1;
}

// Identity is case-insensitive in accounts.username_key. Every roster lookup
// must follow that invariant, including after MySQL reload and /unleader.
// Редкое O(организации * состав) обновление при входе и изменении состава.
// Частые запросы используют два кешированных значения O(1).
stock Leader_RefreshPlayer(playerid)
{
    gFactionCachedOrg[playerid] = 0;
    gFactionCachedRank[playerid] = 0;
    if (!IsPlayerConnected(playerid) || gAuthState[playerid] != AUTH_READY) return 0;
    for (new org = 1; org <= FACTION_COUNT; org++)
    {
        new idx = Leader_Find(org, gAccountName[playerid]);
        if (idx < 0) continue;
        gFactionCachedOrg[playerid] = org;
        gFactionCachedRank[playerid] = gFactionMemberRank[org - 1][idx];
        Leader_ApplyPlayerColor(playerid);
        return org;
    }
    Leader_ApplyPlayerColor(playerid);
    return 0;
}

stock Leader_RefreshAccount(const name[])
{
    // Запрос выполняется только после успешной фиксации состава.
    for (new playerid = 0; playerid < MAX_PLAYERS; playerid++)
    {
        if (IsPlayerConnected(playerid) && gAuthState[playerid] == AUTH_READY &&
            !strcmp(gAccountName[playerid], name, true))
        {
            Leader_RefreshPlayer(playerid);
            break;
        }
    }
    return 1;
}

stock Leader_PlayerOrg(playerid)
{
    if (!IsPlayerConnected(playerid) || gAuthState[playerid] != AUTH_READY) return 0;
    new org = gFactionCachedOrg[playerid];
    return Leader_OrgValid(org) ? org : 0;
}

stock Leader_PlayerRank(playerid)
{
    if (!Leader_PlayerOrg(playerid)) return 0;
    return gFactionCachedRank[playerid];
}

// Ranks are stored exactly as shown: finance 1..4, other factions 1..10.
stock Leader_PublicRank(org, internalRank)
{
    #pragma unused org
    return internalRank;
}

stock bool:Leader_ActiveTerm(org)
{
    if (!Leader_OrgValid(org) || gFactionTermEnd[org - 1] <= gettime()) return false;
    new o = org - 1;
    for (new i = 0; i < gFactionCount[o]; i++)
        if (gFactionMemberRank[o][i] == Leader_LeaderRank(org)) return true;
    return false;
}

stock bool:Leader_FinanceAuthorized(playerid, minPublicRank)
{
    if (Leader_PlayerOrg(playerid) != FACTION_FINANCE ||
        !Leader_ActiveTerm(FACTION_FINANCE)) return false;
    return Leader_PublicRank(FACTION_FINANCE,Leader_PlayerRank(playerid)) >= minPublicRank;
}

stock bool:Leader_StaffAccess(playerid)
{
    new rank = Leader_PlayerRank(playerid), org = Leader_PlayerOrg(playerid);
    return org && rank >= Leader_DeputyRank(org) && Leader_ActiveTerm(org);
}

stock bool:Leader_RequireStaff(playerid)
{
    if (!Leader_StaffAccess(playerid))
    {
        SendClientMessage(playerid, FACTION_ERROR, "Недостаточно прав организации либо истёк срок лидерства.");
        return false;
    }
    return true;
}

stock bool:Leader_RequireAdmin(playerid)
{
    if (!Admin_Allowed(playerid, PERM_MANAGE_STAFF) || !gAdminDuty[playerid])
    {
        SendClientMessage(playerid, FACTION_ERROR, "Назначения лидеров доступны администраторам 5+ на Duty.");
        return false;
    }
    return true;
}

stock Leader_Init()
{
    fcreatedir(FACTION_DIRECTORY);
    for (new o = 0; o < FACTION_COUNT; o++)
    {
        // Исследовательский отдел зарезервирован для сюжетного открытия.
        // Research department remains reserved; finance is a normal faction.
        gFactionAvailable[o] = o < 3 || o == FACTION_FINANCE - 1 || Leader_IsCriminal(o + 1);
        if (!gFactionAvailable[o]) continue;
        if (!Leader_Load(o + 1))
        {
            gFactionAvailable[o] = false;
            printf("[LEADER] Corrupt organization %d file; management disabled until restored.", o + 1);
        }
    }
    for (new o = 0; o < FACTION_COUNT; o++)
    {
        if (!gFactionAvailable[o]) continue;
        for (new i = 0; i < gFactionCount[o]; i++)
        {
            for (new other = 0; other < o; other++)
            {
                if (!gFactionAvailable[other]) continue;
                for (new j = 0; j < gFactionCount[other]; j++)
                    if (!strcmp(gFactionMemberName[o][i], gFactionMemberName[other][j], true))
                    {
                        gFactionAvailable[o] = false;
                        printf("[LEADER] Duplicate membership detected; organization %d blocked.", o + 1);
                    }
            }
        }
    }
    return 1;
}

stock Leader_ClearPending(playerid)
{
    gFactionPendingOrg[playerid] = 0;
    gFactionPendingInviter[playerid] = INVALID_PLAYER_ID;
    gFactionPendingExpiry[playerid] = 0;
    gFactionPendingToken[playerid] = 0;
    return 1;
}

stock Leader_OnConnect(playerid)
{
    gFactionInviteCooldown[playerid] = 0;
    Leader_ClearPending(playerid);
    gFactionCachedOrg[playerid] = 0;
    gFactionCachedRank[playerid] = 0;
    return 1;
}

stock Leader_OnDisconnect(playerid)
{
    Leader_OnConnect(playerid);
    for (new i = 0; i < MAX_PLAYERS; i++)
        if (gFactionPendingInviter[i] == playerid)
            Leader_ClearPending(i); // приглашение удаляем, членство не сбрасываем
    return 1;
}

stock Leader_OnReady(playerid)
{
    Leader_RefreshPlayer(playerid);
    new org = Leader_PlayerOrg(playerid);
    if (!org) return 1;
    new index = Leader_Find(org, gAccountName[playerid]), o = org - 1;
    new previous = gFactionMemberSeen[o][index];
    gFactionMemberSeen[o][index] = gettime();
    if (!Leader_Save(org)) gFactionMemberSeen[o][index] = previous;
    new title[48], message[120];
    Leader_OrgTitle(org, title, sizeof(title));
    format(message, sizeof(message), "Организация: %s, ранг %d. /fmenu", title, Leader_PlayerRank(playerid));
    TNotify_Send(playerid,TNOTIFY_FACTION,TNOTIFY_INFO,message);
    if (gFactionMemberRank[o][index] >= Leader_DeputyRank(org) && !Leader_ActiveTerm(org))
        TNotify_Send(playerid,TNOTIFY_FACTION,TNOTIFY_WARNING,
            "Срок полномочий руководства истёк. Требуется продление.");
    return 1;
}

stock bool:Leader_AddMember(org, const name[], rank)
{
    if (!Leader_OrgValid(org) || gFactionCount[org - 1] >= FACTION_ROSTER_MAX ||
        rank < 1 || rank > Leader_MaxRank(org)) return false;
    new o = org - 1, i = gFactionCount[o]++;
    format(gFactionMemberName[o][i], MAX_PLAYER_NAME, "%s", name);
    gFactionMemberRank[o][i] = rank;
    gFactionMemberJoined[o][i] = gettime();
    gFactionMemberSeen[o][i] = gettime();
    if (!Leader_Save(org))
    {
        gFactionCount[o]--;
        gFactionMemberName[o][i][0] = EOS;
        return false;
    }
    Leader_RefreshAccount(name);
    return true;
}

// Only /unleader may remove the current leader: ordinary staff dismissal and
// /fleave remain unable to bypass administrator-only leadership removal.
stock bool:Leader_RemoveMember(org, index, bool:removeLeader = false)
{
    if (!Leader_OrgValid(org)) return false;
    new o = org - 1;
    if (index < 0 || index >= gFactionCount[o]) return false;
    new bool:isLeader = gFactionMemberRank[o][index] == Leader_LeaderRank(org);
    if (isLeader != removeLeader) return false;
    new oldStart = gFactionTermStart[o], oldEnd = gFactionTermEnd[o];
    new name[MAX_PLAYER_NAME], rank = gFactionMemberRank[o][index],
        joined = gFactionMemberJoined[o][index], seen = gFactionMemberSeen[o][index];
    format(name, sizeof(name), "%s", gFactionMemberName[o][index]);
    for (new j = index; j < gFactionCount[o] - 1; j++)
    {
        format(gFactionMemberName[o][j], MAX_PLAYER_NAME, "%s", gFactionMemberName[o][j + 1]);
        gFactionMemberRank[o][j] = gFactionMemberRank[o][j + 1];
        gFactionMemberJoined[o][j] = gFactionMemberJoined[o][j + 1];
        gFactionMemberSeen[o][j] = gFactionMemberSeen[o][j + 1];
    }
    gFactionCount[o]--;
    if (isLeader)
    {
        // The leader is removed from the roster in the SAME atomic save as
        // the cleared term; no implicit demotion or provisional membership.
        gFactionTermStart[o] = 0;
        gFactionTermEnd[o] = 0;
    }
    if (!Leader_Save(org))
    {
        gFactionTermStart[o] = oldStart;
        gFactionTermEnd[o] = oldEnd;
        for (new j = gFactionCount[o]; j > index; j--)
        {
            format(gFactionMemberName[o][j], MAX_PLAYER_NAME, "%s", gFactionMemberName[o][j - 1]);
            gFactionMemberRank[o][j] = gFactionMemberRank[o][j - 1];
            gFactionMemberJoined[o][j] = gFactionMemberJoined[o][j - 1];
            gFactionMemberSeen[o][j] = gFactionMemberSeen[o][j - 1];
        }
        gFactionCount[o]++;
        format(gFactionMemberName[o][index], MAX_PLAYER_NAME, "%s", name);
        gFactionMemberRank[o][index] = rank;
        gFactionMemberJoined[o][index] = joined;
        gFactionMemberSeen[o][index] = seen;
        return false;
    }
    Leader_RefreshAccount(name);
    return true;
}

// Полное назначение одним сохранением: участник, ранг 10 и срок.
stock bool:Leader_AssignLeader(org, const name[])
{
    if (!Leader_OrgValid(org)) return false;
    new o = org - 1, idx = Leader_Find(org, name);
    if (idx < 0 && gFactionCount[o] >= FACTION_ROSTER_MAX) return false;
    for (new i = 0; i < gFactionCount[o]; i++)
        if (gFactionMemberRank[o][i] == Leader_LeaderRank(org)) return false;
    for (new other = 1; other <= FACTION_COUNT; other++)
        if (other != org && Leader_Find(other, name) >= 0) return false;

    new previousCount = gFactionCount[o], oldRank = 0;
    new oldStart = gFactionTermStart[o], oldEnd = gFactionTermEnd[o];
    if (idx < 0)
    {
        idx = gFactionCount[o]++;
        format(gFactionMemberName[o][idx], MAX_PLAYER_NAME, "%s", name);
        gFactionMemberJoined[o][idx] = gettime();
        gFactionMemberSeen[o][idx] = gettime();
    }
    else oldRank = gFactionMemberRank[o][idx];

    new now = gettime();
    gFactionMemberRank[o][idx] = Leader_LeaderRank(org);
    gFactionTermStart[o] = now;
    gFactionTermEnd[o] = now + FACTION_TERM_SECONDS;

    if (!Leader_Save(org))
    {
        // Восстановление всех изменений памяти, включая добавленный слот.
        gFactionTermStart[o] = oldStart;
        gFactionTermEnd[o] = oldEnd;
        if (previousCount != gFactionCount[o])
        {
            gFactionCount[o] = previousCount;
            gFactionMemberName[o][idx][0] = EOS;
            gFactionMemberRank[o][idx] = 0;
            gFactionMemberJoined[o][idx] = 0;
            gFactionMemberSeen[o][idx] = 0;
        }
        else gFactionMemberRank[o][idx] = oldRank;
        return false;
    }
    Leader_RefreshAccount(name);
    return true;
}

stock bool:Leader_ChangeRank(org, index, rank)
{
    if (!Leader_OrgValid(org) || index < 0 || index >= gFactionCount[org - 1] ||
        rank < 1 || rank >= Leader_LeaderRank(org)) return false;
    new o = org - 1, previous = gFactionMemberRank[o][index];
    gFactionMemberRank[o][index] = rank;
    if (!Leader_Save(org))
    {
        gFactionMemberRank[o][index] = previous;
        return false;
    }
    Leader_RefreshAccount(gFactionMemberName[o][index]);
    return true;
}

stock Leader_FindOnline(const name[])
{
    for (new i = 0; i < MAX_PLAYERS; i++)
        if (Admin_Ready(i) && !strcmp(gAccountName[i], name, true)) return i;
    return INVALID_PLAYER_ID;
}

stock Leader_List(playerid)
{
    new org = Leader_PlayerOrg(playerid);
    if (!org) return SendClientMessage(playerid, FACTION_ERROR, "Вы не состоите в организации.");
    new o = org - 1, title[48], message[128];
    Leader_OrgTitle(org, title, sizeof(title));
    if (Leader_IsCriminal(org))
    {
        new role[32];
        Leader_CrimeRankTitle(Leader_PlayerRank(playerid), role, sizeof(role));
        format(message, sizeof(message), "%s | участников: %d | ранг %d: %s",
            title, gFactionCount[o], Leader_PlayerRank(playerid), role);
    }
    else format(message, sizeof(message), "%s | участников: %d | ваш ранг: %d",
        title, gFactionCount[o], Leader_PublicRank(org,Leader_PlayerRank(playerid)));
    SendClientMessage(playerid, FACTION_COLOR, message);
    for (new i = 0; i < gFactionCount[o] && i < 12; i++)
    {
        if (Leader_IsCriminal(org))
        {
            new role[32];
            Leader_CrimeRankTitle(gFactionMemberRank[o][i], role, sizeof(role));
            format(message, sizeof(message), "%s: %d (%s)%s", gFactionMemberName[o][i],
                gFactionMemberRank[o][i], role,
                Leader_FindOnline(gFactionMemberName[o][i]) != INVALID_PLAYER_ID ? (" [online]") : (""));
        }
        else format(message, sizeof(message), "%s: ранг %d%s", gFactionMemberName[o][i],
            Leader_PublicRank(org,gFactionMemberRank[o][i]), Leader_FindOnline(gFactionMemberName[o][i]) != INVALID_PLAYER_ID ? (" [online]") : (""));
        SendClientMessage(playerid, COLOR_GREY, message);
    }
    return 1;
}

stock Leader_Leaders(playerid)
{
    new line[144], title[48], found;
    SendClientMessage(playerid, FACTION_COLOR, "Руководители организаций:");
    for (new org = 1; org <= FACTION_COUNT; org++)
    {
        if (!Leader_OrgValid(org)) continue;
        Leader_OrgTitle(org, title, sizeof(title));
        found = -1;
        for (new i = 0; i < gFactionCount[org - 1]; i++)
            if (gFactionMemberRank[org - 1][i] == Leader_LeaderRank(org)) found = i;
        if (found < 0) format(line, sizeof(line), "%s: вакансия.", title);
        else
        {
            new until = gFactionTermEnd[org - 1], last = gFactionMemberSeen[org - 1][found];
            format(line, sizeof(line), "%s: %s (срок %s, активность: %d дней назад)",
                title, gFactionMemberName[org - 1][found],
                until > gettime() ? ("активен") : ("истёк"),
                last > 0 ? (gettime() - last) / 86400 : 0);
        }
        SendClientMessage(playerid, COLOR_GREY, line);
        if (found >= 0)
        {
            new days = (gettime() - gFactionMemberSeen[org - 1][found]) / 86400;
            if (days >= 14) SendClientMessage(playerid, FACTION_ERROR, "Неактивность лидера 14+ дней: требуется решение администрации.");
            else if (days >= 7) SendClientMessage(playerid, FACTION_ERROR, "Неактивность лидера 7+ дней: предупреждение.");
        }
    }
    return 1;
}

stock Leader_Command(playerid, const command[], const params[])
{
    new first, second, detail[112], message[160], org, idx, o, target;
    if (!strcmp(command, "leaders", true)) return Leader_Leaders(playerid);
    if (!strcmp(command, "fstats", true)) return Leader_List(playerid);
    if (!strcmp(command, "fmenu", true)) return Leader_UI_Main(playerid);
    if (!strcmp(command, "leader", true)) return Leader_UI_Leader(playerid);
    if (!strcmp(command, "r", true))
    {
        if (gAdminMuteUntil[playerid] > gettime())
            return SendClientMessage(playerid, FACTION_ERROR, "Ваш чат временно заблокирован.");
        org = Leader_PlayerOrg(playerid);
        if (!org) return SendClientMessage(playerid, FACTION_ERROR, "Вы не состоите в организации.");
        if (isnull(params)) return SendClientMessage(playerid, FACTION_ERROR, "Использование: /r сообщение.");
        new text[100];
        format(text, sizeof(text), "%s", params);
        Admin_SafeText(text, sizeof(text));
        format(message, sizeof(message), "[R] %s: %s", gAccountName[playerid], text);
        // Раньше здесь для каждого игрока повторно перебирались все составы.
        // Теперь проверка кеша O(1) на участника.
        for (new i = 0; i < MAX_PLAYERS; i++)
            if (Leader_PlayerOrg(i) == org) SendClientMessage(i, FACTION_COLOR, message);
        return 1;
    }
    if (!strcmp(command, "faccept", true))
    {
        org = gFactionPendingOrg[playerid];
        target = gFactionPendingInviter[playerid];
        if (!Leader_OrgValid(org) || gFactionPendingExpiry[playerid] < gettime() ||
            gFactionPendingToken[playerid] != gAuthToken[playerid] ||
            target < 0 || target >= MAX_PLAYERS || !Admin_Ready(target) ||
            Leader_PlayerOrg(target) != org || !Leader_StaffAccess(target) ||
            Leader_PlayerOrg(playerid))
        {
            Leader_ClearPending(playerid);
            return SendClientMessage(playerid, FACTION_ERROR, "Приглашение недействительно.");
        }
        format(detail, sizeof(detail), "%s -> %d", gAccountName[playerid], org);
        if (!Leader_AuditPlayer(target, "INVITE_ACCEPT", detail) ||
            !Leader_AddMember(org, gAccountName[playerid], 1))
            return SendClientMessage(playerid, FACTION_ERROR, "Не удалось сохранить вступление.");
        Leader_ClearPending(playerid);
        TNotify_Send(playerid,TNOTIFY_FACTION,TNOTIFY_SUCCESS,"Вы приняты в организацию. /fmenu");
        return 1;
    }
    if (!strcmp(command, "setleader", true) || !strcmp(command, "renewleader", true))
    {
        if (!Leader_RequireAdmin(playerid)) return 1;
        if (!strcmp(command, "setleader", true))
        {
            if (sscanf(params, "ii", first, second))
                return SendClientMessage(playerid, FACTION_ERROR, "Использование: /setleader ID организация(1-3, 5=Минфин, 6-8=криминал).");
        }
        else if (sscanf(params, "i", first))
            return SendClientMessage(playerid, FACTION_ERROR, "Использование: /renewleader ID.");
        // SOLO-UAT: a level-5+ admin on /aduty may appoint or renew
        // their own leadership. All organization, term, uniqueness,
        // audit and durable-save checks below still apply.
        if (first < 0 || first >= MAX_PLAYERS || !Admin_Ready(first))
            return SendClientMessage(playerid, FACTION_ERROR, "Некорректный ID игрока.");
        target = first;
        org = !strcmp(command, "setleader", true) ? second : Leader_PlayerOrg(target);
        if (!Leader_OrgValid(org)) return SendClientMessage(playerid, FACTION_ERROR, "Организация недоступна.");
        o = org - 1;
        idx = Leader_Find(org, gAccountName[target]);
        if (!strcmp(command, "renewleader", true))
        {
            if (idx < 0 || gFactionMemberRank[o][idx] != Leader_LeaderRank(org))
                return SendClientMessage(playerid, FACTION_ERROR, "Цель не является лидером.");
            format(detail, sizeof(detail), "%s -> org %d", gAccountName[target], org);
            if (!Leader_AuditPlayer(playerid, "RENEW_LEADER_INTENT", detail))
                return SendClientMessage(playerid, FACTION_ERROR, "Журнал недоступен.");
            new oldStart = gFactionTermStart[o], oldEnd = gFactionTermEnd[o];
            gFactionTermStart[o] = gettime();
            gFactionTermEnd[o] = gettime() + FACTION_TERM_SECONDS;
            if (!Leader_Save(org))
            {
                gFactionTermStart[o] = oldStart; gFactionTermEnd[o] = oldEnd;
                return SendClientMessage(playerid, FACTION_ERROR, "Ошибка сохранения.");
            }
            if (!Leader_AuditPlayer(playerid, "RENEW_LEADER", detail))
                print("[LEADER] WARN: term extended, but audit COMMIT record failed.");
            return SendClientMessage(playerid, FACTION_COLOR, "Полномочия лидера продлены на 30 дней.");
        }
        if (Leader_PlayerOrg(target) && Leader_PlayerOrg(target) != org)
            return SendClientMessage(playerid, FACTION_ERROR, "Игрок уже в другой организации.");
        for (new i = 0; i < gFactionCount[o]; i++)
            if (gFactionMemberRank[o][i] == Leader_LeaderRank(org))
                return SendClientMessage(playerid, FACTION_ERROR, "У организации уже есть лидер.");
        if (idx < 0 && gFactionCount[o] >= FACTION_ROSTER_MAX)
            return SendClientMessage(playerid, FACTION_ERROR, "Состав организации заполнен.");
        format(detail, sizeof(detail), "%s -> org %d", gAccountName[target], org);
        if (!Leader_AuditPlayer(playerid, "SET_LEADER_INTENT", detail))
            return SendClientMessage(playerid, FACTION_ERROR, "Журнал недоступен.");
        if (!Leader_AssignLeader(org, gAccountName[target]))
            return SendClientMessage(playerid, FACTION_ERROR, "Назначение не сохранено. Изменения отменены.");
        if (!Leader_AuditPlayer(playerid, "SET_LEADER", detail))
            print("[LEADER] WARN: leader assigned, but audit COMMIT record failed.");
        SendClientMessage(target, FACTION_COLOR, "Вы назначены лидером организации на 30 дней.");
        return SendClientMessage(playerid, FACTION_COLOR, "Лидер назначен.");
    }
    if (!strcmp(command, "unleader", true))
    {
        if (!Leader_RequireAdmin(playerid)) return 1;
        new name[MAX_PLAYER_NAME], pos;
        if (!Admin_Next(params, pos, name, sizeof(name)) ||
            !Admin_NormalizeName(name, name, sizeof(name)))
            return SendClientMessage(playerid, FACTION_ERROR, "Использование: /unleader ник.");
        org = 0;
        for (new x = 1; x <= FACTION_COUNT; x++)
            if (Leader_OrgValid(x))
            {
                idx = Leader_Find(x, name);
                if (idx >= 0 && gFactionMemberRank[x - 1][idx] == Leader_LeaderRank(x)) { org = x; break; }
            }
        if (!org) return SendClientMessage(playerid, FACTION_ERROR, "Лидер не найден.");
        format(detail, sizeof(detail), "%s -> org %d", name, org);
        if (!Leader_AuditPlayer(playerid, "UNLEADER_INTENT", detail))
            return SendClientMessage(playerid, FACTION_ERROR, "Журнал недоступен.");
        // Atomic roster removal + term reset. The former leader is now
        // a civilian, with no faction permissions, immediately and after relog.
        if (!Leader_RemoveMember(org, idx, true))
            return SendClientMessage(playerid, FACTION_ERROR, "Увольнение не сохранено. Изменения отменены.");
        if (!Leader_AuditPlayer(playerid, "UNLEADER", detail))
            print("[LEADER] WARN: leader removed, but audit COMMIT record failed.");
        target = Leader_FindOnline(name);
        if (target != INVALID_PLAYER_ID)
            SendClientMessage(target, FACTION_COLOR,
                "Вы сняты с должности руководителя и уволены из организации.");
        return SendClientMessage(playerid, FACTION_COLOR,
            "Руководитель снят и уволен из организации. Членство прекращено.");
    }

    if (!strcmp(command, "fleave", true))
    {
        org = Leader_PlayerOrg(playerid);
        if (!org) return SendClientMessage(playerid, FACTION_ERROR, "Вы не состоите в организации.");
        idx = Leader_Find(org, gAccountName[playerid]);
        if (Leader_PlayerRank(playerid) == Leader_LeaderRank(org))
            return SendClientMessage(playerid, FACTION_ERROR, "Лидера сначала должен снять администратор.");
        if (!Leader_AuditPlayer(playerid, "FACTION_LEAVE", gAccountName[playerid]) ||
            !Leader_RemoveMember(org, idx))
            return SendClientMessage(playerid, FACTION_ERROR, "Не удалось сохранить увольнение.");
        return SendClientMessage(playerid, FACTION_COLOR, "Вы покинули организацию.");
    }
    if (!Leader_RequireStaff(playerid)) return 1;
    org = Leader_PlayerOrg(playerid);
    o = org - 1;
    if (!strcmp(command, "finvite", true))
    {
        if (sscanf(params, "i", first) || first < 0 || first >= MAX_PLAYERS ||
            !Admin_Ready(first) || first == playerid)
            return SendClientMessage(playerid, FACTION_ERROR, "Использование: /finvite ID.");
        target = first;
        if (Leader_PlayerOrg(target)) return SendClientMessage(playerid, FACTION_ERROR, "Игрок уже состоит в организации.");
        if (gFactionCount[o] >= FACTION_ROSTER_MAX)
            return SendClientMessage(playerid, FACTION_ERROR, "В организации нет мест.");
        if (gFactionInviteCooldown[playerid] > gettime())
            return SendClientMessage(playerid, FACTION_ERROR, "Подождите перед следующим приглашением.");
        gFactionInviteCooldown[playerid] = gettime() + 15;
        gFactionPendingOrg[target] = org;
        gFactionPendingInviter[target] = playerid;
        gFactionPendingExpiry[target] = gettime() + FACTION_INVITE_SECONDS;
        gFactionPendingToken[target] = gAuthToken[target];
        format(message, sizeof(message), "Приглашение от %s в организацию %d. /faccept (60 сек.)",
            gAccountName[playerid], org);
        SendClientMessage(target, FACTION_COLOR, message);
        return SendClientMessage(playerid, FACTION_COLOR, "Приглашение отправлено.");
    }
    if (!strcmp(command, "funinvite", true) || !strcmp(command, "frank", true))
    {
        if (!strcmp(command, "frank", true))
        {
            if (sscanf(params, "ii", first, second))
                return SendClientMessage(playerid, FACTION_ERROR, "Использование: /frank ID ранг.");
        }
        else if (sscanf(params, "i", first))
            return SendClientMessage(playerid, FACTION_ERROR, "Использование: /funinvite ID.");
        if (first < 0 || first >= MAX_PLAYERS || !Admin_Ready(first) || first == playerid)
            return SendClientMessage(playerid, FACTION_ERROR, "Игрок недоступен.");
        target = first;
        idx = Leader_Find(org, gAccountName[target]);
        if (idx < 0) return SendClientMessage(playerid, FACTION_ERROR, "Игрок не в вашей организации.");
        new existingRank = gFactionMemberRank[o][idx], myRank = Leader_PlayerRank(playerid);
        if (existingRank >= myRank || existingRank == Leader_LeaderRank(org))
            return SendClientMessage(playerid, FACTION_ERROR, "Нельзя управлять равным или старшим сотрудником.");
        if (!strcmp(command, "funinvite", true))
        {
            format(detail, sizeof(detail), "%s -> org %d", gAccountName[target], org);
            if (!Leader_AuditPlayer(playerid, "FACTION_KICK", detail) ||
                !Leader_RemoveMember(org, idx))
                return SendClientMessage(playerid, FACTION_ERROR, "Ошибка сохранения увольнения.");
            SendClientMessage(target, FACTION_ERROR, "Вы уволены из организации.");
            return SendClientMessage(playerid, FACTION_COLOR, "Сотрудник уволен.");
        }
        if (org == FACTION_FINANCE)
        {
            if (second < 1 || second > 3)
                return SendClientMessage(playerid, FACTION_COLOR,
                    "Для Министерства финансов доступен ранг от 1 до 3.");
            // Finance ranks are persisted directly as 1,2,3,4.
        }
        if (Leader_IsCriminal(org) && (second < 1 || second > 4))
            return SendClientMessage(playerid, FACTION_ERROR,
                "Синдикат: допустимые ранги 1-4; ранг 5 назначает администратор.");
        if (second < 1 || second >= myRank || second >= Leader_LeaderRank(org))
            return SendClientMessage(playerid, FACTION_ERROR, "Недопустимый ранг.");
        if (myRank == Leader_DeputyRank(org) && second >= Leader_DeputyRank(org))
            return SendClientMessage(playerid, FACTION_ERROR, "Назначать заместителей может только лидер.");
        if (second == Leader_DeputyRank(org) &&
            existingRank != Leader_DeputyRank(org))
        {
            new deputies;
            for (new i = 0; i < gFactionCount[o]; i++)
                if (gFactionMemberRank[o][i] == Leader_DeputyRank(org)) deputies++;
            if (deputies >= FACTION_DEPUTY_MAX)
                return SendClientMessage(playerid, FACTION_ERROR, "Уже назначены два заместителя.");
        }
        format(detail, sizeof(detail), "%s -> org %d rank %d", gAccountName[target], org, second);
        if (!Leader_AuditPlayer(playerid, "FACTION_RANK", detail) ||
            !Leader_ChangeRank(org, idx, second))
            return SendClientMessage(playerid, FACTION_ERROR, "Ошибка сохранения ранга.");
        SendClientMessage(target, FACTION_COLOR, "Ваш ранг в организации изменён.");
        return SendClientMessage(playerid, FACTION_COLOR, "Ранг сотрудника изменён.");
    }
    return 0;
}
