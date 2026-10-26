#if defined _signal_admin_system_included
    #endinput
#endif
#define _signal_admin_system_included

// ADMIN-01. Server-side permissions; never trust the client-side panel.
#define ADMIN_DIRECTORY "signal_admins"
#define FOUNDER_NAME "veyran"
#define BAN_DIRECTORY "signal_bans"
#define ADMIN_REPORT_LIMIT (64)
#define ADMIN_REASON_MAX (96)
#define ADMIN_COLOR 0x92B8FFFF
#define ADMIN_ERROR 0xE27E82FF
#define ADMIN_OK 0x86C6A4FF
#define ADMIN_REPORT_COOLDOWN (60)

enum E_ADMIN_PERMISSION
{
    PERM_REPORT,
    PERM_MODERATE,
    PERM_BAN_DAY,
    PERM_BAN_MONTH,
    PERM_BAN_PERMANENT,
    PERM_MANAGE_STAFF,
    PERM_OWNER,
    PERM_EVENT
};

new gAdminLevel[MAX_PLAYERS];
new bool:gAdminEvent[MAX_PLAYERS];
new bool:gAdminDuty[MAX_PLAYERS];
new gAdminMuteUntil[MAX_PLAYERS];
new gAdminReportNext[MAX_PLAYERS];
new bool:gAdminFrozen[MAX_PLAYERS];
new gAdminSpectate[MAX_PLAYERS];
new Float:gAdminSpecPos[MAX_PLAYERS][3];
new gAdminSpecInterior[MAX_PLAYERS];
new gAdminSpecWorld[MAX_PLAYERS];
new gAdminWeather;
new gAdminClockHour;
new gAdminClockMinute;
new bool:gAdminClockAuto;
new gAdminClockTimer;
#define ADMIN_CLOCK_SYNC_MS (15000)
forward Admin_ClockTick();

new gReportSerial;
new gReportNumber[ADMIN_REPORT_LIMIT];
new gReportState[ADMIN_REPORT_LIMIT]; // 0 free, 1 open, 2 closed
new gReportAuthor[ADMIN_REPORT_LIMIT][MAX_PLAYER_NAME];
new gReportAssignee[ADMIN_REPORT_LIMIT][MAX_PLAYER_NAME];
new gReportMessage[ADMIN_REPORT_LIMIT][121];
new gReportCreated[ADMIN_REPORT_LIMIT];

stock bool:Admin_Ready(playerid)
{
    return IsPlayerConnected(playerid) && gAuthState[playerid] == AUTH_READY;
}

// The name is a reservation; privileges ALSO require authenticated bcrypt login
// and a manually provisioned level-6 role file.
stock bool:Admin_IsFounderName(const name[])
{
    return !strcmp(name, FOUNDER_NAME, true);
}

stock bool:Admin_IsFounder(playerid)
{
    return Admin_Ready(playerid) &&
        Admin_IsFounderName(gAccountName[playerid]) && gAdminLevel[playerid] == 6;
}

stock bool:Admin_Allowed(playerid, E_ADMIN_PERMISSION:permission)
{
    if (!Admin_Ready(playerid)) return false;
    switch (permission)
    {
        case PERM_REPORT: return gAdminLevel[playerid] >= 1;
        case PERM_MODERATE: return gAdminLevel[playerid] >= 2;
        case PERM_BAN_DAY: return gAdminLevel[playerid] >= 3;
        case PERM_BAN_MONTH: return gAdminLevel[playerid] >= 4;
        case PERM_BAN_PERMANENT, PERM_MANAGE_STAFF: return gAdminLevel[playerid] >= 5;
        case PERM_OWNER: return gAdminLevel[playerid] == 6;
        case PERM_EVENT: return Admin_IsFounder(playerid) || (gAdminEvent[playerid] && gAdminLevel[playerid] >= 1);
    }
    return false;
}

stock bool:Admin_Require(playerid, E_ADMIN_PERMISSION:permission)
{
    if (!Admin_Allowed(playerid, permission))
    {
        SendClientMessage(playerid, ADMIN_ERROR, "Недостаточно административных полномочий.");
        return false;
    }
    if (!gAdminDuty[playerid])
    {
        SendClientMessage(playerid, ADMIN_ERROR, "Включите административный режим: /aduty.");
        return false;
    }
    return true;
}

stock Admin_SafeText(text[], maxlen)
{
    for (new i = 0; i < maxlen && text[i] != EOS; i++)
    {
        if (text[i] == '\n' || text[i] == '\r' || text[i] == '|' ||
            text[i] == '\t' || text[i] < 32) text[i] = ' ';
    }
    return 1;
}

stock bool:Admin_Next(const input[], &at, output[], capacity)
{
    while (input[at] == ' ') at++;
    new written = 0;
    while (input[at] && input[at] != ' ')
    {
        if (written < capacity - 1) output[written++] = input[at];
        at++;
    }
    output[written] = EOS;
    return written > 0;
}

stock Admin_Remainder(const input[], at, output[], size)
{
    while (input[at] == ' ') at++;
    new n = 0;
    while (input[at] && n < size - 1) output[n++] = input[at++];
    output[n] = EOS;
    Admin_SafeText(output, size);
    return n;
}

stock bool:Admin_UnsignedNumber(const input[])
{
    new length = strlen(input);
    if (length == 0 || length > 9) return false;
    for (new i = 0; i < length; i++)
        if (input[i] < '0' || input[i] > '9') return false;
    return true;
}

stock bool:Admin_NormalizeName(const input[], output[], size)
{
    new n = strlen(input);
    if (n < 3 || n > 20 || n >= size) return false;
    for (new i = 0; i < n; i++)
    {
        new letter = input[i];
        if (letter >= 'A' && letter <= 'Z') letter += 32;
        if (!((letter >= 'a' && letter <= 'z') ||
              (letter >= '0' && letter <= '9') || letter == '_')) return false;
        output[i] = letter;
    }
    output[n] = EOS;
    return true;
}

stock Admin_Audit(const actor[], const action[], const detail[])
{
    new File:handle = fopen(ADMIN_DIRECTORY "/audit.log", io_append);
    if (!handle)
    {
        print("[ADMIN] ERROR: failed to open administration audit log.");
        return 0;
    }
    new clean[ADMIN_REASON_MAX + 1], line[240];
    format(clean, sizeof(clean), "%s", detail);
    Admin_SafeText(clean, sizeof(clean));
    format(line, sizeof(line), "%d|%s|%s|%s\n", gettime(), actor, action, clean);
    new written = fwrite(handle, line);
    fclose(handle);
    return written > 0;
}

stock Admin_LogPlayer(playerid, const action[], const detail[])
{
    return Admin_Audit(gAccountName[playerid], action, detail);
}

stock Admin_RolePath(const name[], path[], size)
{
    format(path, size, "%s/%s.adm", ADMIN_DIRECTORY, name);
    return 1;
}

// Замена без предварительного удаления оригинала. После сбоя .bak можно восстановить.
stock bool:Admin_ReplaceFile(const path[], const temp[], const backup[])
{
    new bool:hadOriginal = fexist(path) != 0;
    if (hadOriginal)
    {
        if (fexist(backup) && !fremove(backup)) return false;
        if (!frename(path, backup)) return false;
    }
    if (!frename(temp, path))
    {
        if (hadOriginal && !frename(backup, path))
            print("[ADMIN] ERROR: restore file manually from .bak backup.");
        return false;
    }
    if (hadOriginal && fexist(backup)) fremove(backup);
    return true;
}

// Старые .mute читаются только для однократного переноса в файл аккаунта.
stock bool:Admin_ParseMuteTime(const value[], &until)
{
    new length = strlen(value);
    if (length < 1 || length > 10) return false;
    for (new i = 0; i < length; i++)
        if (value[i] < '0' || value[i] > '9') return false;
    if (length == 10 && strcmp(value, "2147483647", false) > 0) return false;
    until = strval(value);
    return until > 0;
}

stock Admin_MutePath(const name[], path[], size)
{
    format(path, size, "%s/%s.mute", ADMIN_DIRECTORY, name);
    return 1;
}

stock bool:Admin_LoadMute(playerid)
{
#if defined TARKENSO_DB_PRIMARY
    #pragma unused playerid
    return true;
#else
    new path[92], backup[108], header[32], timeText[24], until;
    Admin_MutePath(gAccountName[playerid], path, sizeof(path));
    format(backup, sizeof(backup), "%s.bak", path);
    if (!fexist(path) && fexist(backup) && !frename(backup, path)) return false;
    if (!fexist(path)) return true;
    new File:file = fopen(path, io_read);
    if (!file) return false;
    new bool:ok = fread(file, header, sizeof(header)) != 0;
    if (!fread(file, timeText, sizeof(timeText))) ok = false;
    fclose(file);
    if (!ok) return false;
    Auth_StripLine(header);
    Auth_StripLine(timeText);
    if (strcmp(header, "SIGNALMUTE|1", false) ||
        !Admin_ParseMuteTime(timeText, until)) return false;

    new oldUntil = gAdminMuteUntil[playerid];
    if (until > gettime() && until > oldUntil)
    {
        gAdminMuteUntil[playerid] = until;
        if (!Auth_SaveAccount(playerid))
        {
            gAdminMuteUntil[playerid] = oldUntil;
            return false;
        }
    }
    // Удаление допустимо лишь после успешной миграции или истечения срока.
    if (!fremove(path)) return false;
    if (fexist(backup) && !fremove(backup)) return false;
    return true;
#endif
}

// Единственный источник действующего наказания — аккаунт SIGNAL0317|2.
stock bool:Admin_WriteMute(playerid, until)
{
    if (until <= gettime()) return false;
    new previous = gAdminMuteUntil[playerid];
    gAdminMuteUntil[playerid] = until;
    if (!Auth_SaveAccount(playerid))
    {
        gAdminMuteUntil[playerid] = previous;
        return false;
    }
    return true;
}

stock Admin_ReadRole(playerid)
{
#if defined TARKENSO_DB_PRIMARY
    return DBPrimary_ReadRole(playerid) ? 1 : -1;
#else
    new path[92], backup[108], header[32], levelText[16], eventText[16];
    Admin_RolePath(gAccountName[playerid], path, sizeof(path));
    format(backup, sizeof(backup), "%s.bak", path);
    if (!fexist(path) && fexist(backup) && !frename(backup, path)) return -1;
    if (!fexist(path)) return 0;
    new File:handle = fopen(path, io_read);
    if (!handle) return -1;
    new bool:valid = true;
    if (!fread(handle, header, sizeof(header))) valid = false;
    if (!fread(handle, levelText, sizeof(levelText))) valid = false;
    if (!fread(handle, eventText, sizeof(eventText))) valid = false;
    fclose(handle);
    if (!valid) return -1;
    Auth_StripLine(header);
    Auth_StripLine(levelText);
    Auth_StripLine(eventText);
    if (strcmp(header, "SIGNALADMIN|1", false) ||
        strlen(levelText) != 1 || levelText[0] < '0' || levelText[0] > '6' ||
        strlen(eventText) != 1 || (eventText[0] != '0' && eventText[0] != '1')) return -1;
    gAdminLevel[playerid] = strval(levelText);
    gAdminEvent[playerid] = (eventText[0] == '1');
    return 1;
#endif
}

stock bool:Admin_WriteRole(playerid, level, bool:eventRole)
{
#if defined TARKENSO_DB_PRIMARY
    return DBPrimary_WriteRole(playerid, level, eventRole);
#else
    if (level < 0 || level > 6 || Admin_IsFounderName(gAccountName[playerid])) return false; // Founder role: offline provisioning only.
    new path[92], tmp[108], backup[108], line[20];
    Admin_RolePath(gAccountName[playerid], path, sizeof(path));
    format(tmp, sizeof(tmp), "%s.tmp", path);
    format(backup, sizeof(backup), "%s.bak", path);
    if (fexist(tmp) && !fremove(tmp)) return false;
    new File:handle = fopen(tmp, io_write);
    if (!handle) return false;
    new bool:ok = fwrite(handle, "SIGNALADMIN|1\n") > 0;
    format(line, sizeof(line), "%d\n%d\n", level, _:eventRole);
    if (fwrite(handle, line) <= 0) ok = false;
    fclose(handle);
    if (!ok)
    {
        fremove(tmp);
        return false;
    }
    if (!Admin_ReplaceFile(path, tmp, backup))
    {
        fremove(tmp);
        return false;
    }
    return true;
#endif
}

stock Admin_BanPath(const name[], path[], size)
{
    format(path, size, "%s/%s.ban", BAN_DIRECTORY, name);
    return 1;
}

stock bool:Admin_IsBanned(playerid)
{
#if defined TARKENSO_DB_PRIMARY
    return DBPrimary_IsBanned(playerid);
#else
    new path[92], header[32], untilText[24];
    Admin_BanPath(gAccountName[playerid], path, sizeof(path));
    if (!fexist(path)) return false;
    new File:handle = fopen(path, io_read);
    if (!handle) return true; // fail closed
    new bool:valid = true;
    if (!fread(handle, header, sizeof(header))) valid = false;
    if (!fread(handle, untilText, sizeof(untilText))) valid = false;
    fclose(handle);
    if (!valid) return true;
    Auth_StripLine(header);
    Auth_StripLine(untilText);
    if (strcmp(header, "SIGNALBAN|1", false) || !Admin_UnsignedNumber(untilText)) return true;
    new until = strval(untilText);
    if (until > 0 && until <= gettime())
    {
        fremove(path);
        return false;
    }
    return true;
#endif
}

stock bool:Admin_WriteBan(playerid, until, const reason[])
{
#if defined TARKENSO_DB_PRIMARY
    return DBPrimary_WriteBan(playerid, until, reason);
#else
    new path[92], line[160];
    if (Admin_IsFounderName(gAccountName[playerid])) return false;
    Admin_BanPath(gAccountName[playerid], path, sizeof(path));
    new File:handle = fopen(path, io_write);
    if (!handle) return false;
    new bool:ok = fwrite(handle, "SIGNALBAN|1\n") > 0;
    format(line, sizeof(line), "%d\n%s\n", until, reason);
    if (fwrite(handle, line) <= 0) ok = false;
    fclose(handle);
    return ok;
#endif
}

stock Admin_Init()
{
    fcreatedir(ADMIN_DIRECTORY);
    fcreatedir(BAN_DIRECTORY);
    gReportSerial = 0;
    gAdminWeather = 9; // Default ordinary weather; /weather may override.
    gAdminClockAuto = true;
    new second;
    gettime(gAdminClockHour, gAdminClockMinute, second);
    gAdminClockTimer = SetTimer("Admin_ClockTick", ADMIN_CLOCK_SYNC_MS, true);
    if (!gAdminClockTimer) print("[CLOCK] ERROR: real-time synchronization timer unavailable.");
    for (new i = 0; i < ADMIN_REPORT_LIMIT; i++) gReportState[i] = 0;
    return 1;
}

stock Admin_OnConnect(playerid)
{
    gAdminLevel[playerid] = 0;
    gAdminEvent[playerid] = false;
    gAdminDuty[playerid] = false;
    gAdminMuteUntil[playerid] = 0;
    gAdminReportNext[playerid] = 0;
    gAdminFrozen[playerid] = false;
    gAdminSpectate[playerid] = INVALID_PLAYER_ID;
    return 1;
}

stock bool:Admin_OnReady(playerid)
{
    if (Admin_IsBanned(playerid))
    {
        SendClientMessage(playerid, ADMIN_ERROR, "Ваш аккаунт заблокирован администрацией.");
        Kick(playerid);
        return false;
    }
    if (!Admin_LoadMute(playerid))
    {
        SendClientMessage(playerid, ADMIN_ERROR, "Файл наказания повреждён или недоступен. Обратитесь к администрации.");
        Kick(playerid);
        return false;
    }
    new role = Admin_ReadRole(playerid);
    if (role < 0)
    {
        print("[ADMIN] Invalid permissions file: administrator access denied.");
        gAdminLevel[playerid] = 0;
        gAdminEvent[playerid] = false;
    }
    if (Admin_IsFounder(playerid))
        SendClientMessage(playerid, ADMIN_OK, "VEYRAN | FOUNDER: /aduty, /admin, /founder.");
    else if (gAdminLevel[playerid] > 0)
        SendClientMessage(playerid, ADMIN_COLOR, "Доступ администрации: /aduty и /admin.");
    return true;
}

stock Admin_StopSpectate(playerid)
{
    if (gAdminSpectate[playerid] == INVALID_PLAYER_ID) return 0;
    gAdminSpectate[playerid] = INVALID_PLAYER_ID;
    TogglePlayerSpectating(playerid, false);
    SetPlayerInterior(playerid, gAdminSpecInterior[playerid]);
    SetPlayerVirtualWorld(playerid, gAdminSpecWorld[playerid]);
    SetPlayerPos(playerid, gAdminSpecPos[playerid][0], gAdminSpecPos[playerid][1], gAdminSpecPos[playerid][2]);
    SetCameraBehindPlayer(playerid);
    return 1;
}

stock Admin_OnDisconnect(playerid)
{
    // Release claims and spectators before slot reuse.
    for (new i = 0; i < ADMIN_REPORT_LIMIT; i++)
        if (gReportState[i] == 1 && !strcmp(gReportAssignee[i], gAccountName[playerid], false))
            gReportAssignee[i][0] = EOS;
    for (new i = 0; i < MAX_PLAYERS; i++)
        if (i != playerid && Admin_Ready(i) && gAdminSpectate[i] == playerid)
            Admin_StopSpectate(i);
    gAdminLevel[playerid] = 0;
    gAdminEvent[playerid] = false;
    gAdminDuty[playerid] = false;
    gAdminSpectate[playerid] = INVALID_PLAYER_ID;
    return 1;
}

// Server OS local clock is authoritative in auto mode. Never derive time
// from a player's GTA clock: it can run faster than real-world minutes.
stock Admin_ClockRefresh()
{
    if (gAdminClockAuto)
    {
        new second;
        gettime(gAdminClockHour, gAdminClockMinute, second);
    }
    return 1;
}

public Admin_ClockTick()
{
    Admin_ClockRefresh();
    for (new i = 0; i < MAX_PLAYERS; i++)
        if (Admin_Ready(i)) SetPlayerTime(i, gAdminClockHour, gAdminClockMinute);
    return 1;
}

stock Admin_ClockShutdown()
{
    if (gAdminClockTimer) KillTimer(gAdminClockTimer);
    gAdminClockTimer = 0;
    return 1;
}

stock Admin_ApplyEvent(playerid)
{
    // Weather is server-wide; on spawn/join apply the current OS time (or
    // a deliberate admin override) rather than the old scripted 03:17.
    Admin_ClockRefresh();
    SetPlayerWeather(playerid, gAdminWeather);
    SetPlayerTime(playerid, gAdminClockHour, gAdminClockMinute);
    return 1;
}

// Единое правило для санкций: выше по рангу нельзя, Founder неприкосновенен.
// Равный уровень разрешён для санкций, но не для изменения полномочий.
stock bool:Admin_CanPunish(playerid, targetid)
{
    if (!Admin_Ready(playerid) || !Admin_Ready(targetid) || playerid == targetid)
        return false;
    if (Admin_IsFounderName(gAccountName[targetid])) return false;
    return gAdminLevel[playerid] >= gAdminLevel[targetid];
}

stock bool:Admin_ValidTarget(playerid, targetid)
{
    if (targetid < 0 || targetid >= MAX_PLAYERS || !Admin_Ready(targetid))
    {
        SendClientMessage(playerid, ADMIN_ERROR, "Игрок не найден или не авторизован.");
        return false;
    }
    if (!Admin_CanPunish(playerid, targetid))
    {
        SendClientMessage(playerid, ADMIN_ERROR, "Нельзя применять действие к себе, основателю или администратору более высокого уровня.");
        return false;
    }
    return true;
}

stock Admin_OpenReportCount()
{
    new count;
    for (new i = 0; i < ADMIN_REPORT_LIMIT; i++)
        if (gReportState[i] == 1) count++;
    return count;
}

stock Admin_ReportIndex(reportid)
{
    for (new i = 0; i < ADMIN_REPORT_LIMIT; i++)
        if (gReportState[i] != 0 && gReportNumber[i] == reportid) return i;
    return -1;
}

stock Admin_ReportList(playerid)
{
    if (!Admin_Require(playerid, PERM_REPORT)) return 1;
    new line[180], count;
    format(line, sizeof(line), "Открытых обращений: %d. /accept ID, /areply ID текст, /close ID.", Admin_OpenReportCount());
    SendClientMessage(playerid, ADMIN_COLOR, line);
    for (new i = 0; i < ADMIN_REPORT_LIMIT && count < 8; i++)
    {
        if (gReportState[i] != 1) continue;
        format(line, sizeof(line), "#%d %s: %.80s", gReportNumber[i], gReportAuthor[i], gReportMessage[i]);
        SendClientMessage(playerid, COLOR_GREY, line);
        count++;
    }
    return 1;
}

stock bool:Admin_ReportAccess(playerid, index)
{
    if (index < 0 || gReportState[index] != 1) return false;
    return !strcmp(gReportAssignee[index], gAccountName[playerid], false);
}

stock Admin_ReportCreate(playerid, const message[])
{
    if (gettime() < gAdminReportNext[playerid])
    {
        SendClientMessage(playerid, ADMIN_ERROR, "Подождите перед повторным обращением.");
        return 1;
    }
    if (strlen(message) < 10)
    {
        SendClientMessage(playerid, ADMIN_ERROR, "Использование: /report причина (не менее 10 символов).");
        return 1;
    }
    new slot = -1;
    for (new i = 0; i < ADMIN_REPORT_LIMIT; i++)
        if (gReportState[i] != 1) { slot = i; break; }
    if (slot < 0)
    {
        SendClientMessage(playerid, ADMIN_ERROR, "Все обращения заняты. Повторите позже.");
        return 1;
    }
    gReportState[slot] = 1;
    gReportNumber[slot] = ++gReportSerial;
    format(gReportAuthor[slot], MAX_PLAYER_NAME, "%s", gAccountName[playerid]);
    gReportAssignee[slot][0] = EOS;
    format(gReportMessage[slot], 121, "%s", message);
    gReportCreated[slot] = gettime();
    gAdminReportNext[playerid] = gettime() + ADMIN_REPORT_COOLDOWN;
    new line[180];
    format(line, sizeof(line), "Обращение #%d создано.", gReportNumber[slot]);
    SendClientMessage(playerid, ADMIN_OK, line);
    format(line, sizeof(line), "Новый репорт #%d от %s. /reports", gReportNumber[slot], gReportAuthor[slot]);
    for (new i = 0; i < MAX_PLAYERS; i++)
        if (Admin_Allowed(i, PERM_REPORT) && gAdminDuty[i])
            SendClientMessage(i, ADMIN_COLOR, line);
    Admin_LogPlayer(playerid, "REPORT_OPEN", message);
    return 1;
}

stock Admin_FindOnlineName(const name[])
{
    for (new i = 0; i < MAX_PLAYERS; i++)
        if (Admin_Ready(i) && !strcmp(name, gAccountName[i], false)) return i;
    return INVALID_PLAYER_ID;
}

stock Admin_StaffChat(playerid, const message[])
{
    if (!Admin_Require(playerid, PERM_REPORT)) return 1;
    new line[180];
    format(line, sizeof(line), "[STAFF %d] %s: %s", gAdminLevel[playerid], gAccountName[playerid], message);
    for (new i = 0; i < MAX_PLAYERS; i++)
        if (Admin_Allowed(i, PERM_REPORT)) SendClientMessage(i, ADMIN_COLOR, line);
    return 1;
}

stock Admin_ShowStaff(playerid)
{
    new line[100];
    SendClientMessage(playerid, ADMIN_COLOR, "Администрация онлайн:");
    for (new i = 0; i < MAX_PLAYERS; i++)
    {
        if (!Admin_Allowed(i, PERM_REPORT)) continue;
        format(line, sizeof(line), "%s (ID %d, уровень %d, duty %d)",
            gAccountName[i], i, gAdminLevel[i], _:gAdminDuty[i]);
        SendClientMessage(playerid, COLOR_GREY, line);
    }
    return 1;
}

stock Admin_RecentLogs(playerid)
{
    if (!Admin_Require(playerid, PERM_BAN_MONTH)) return 1;
    new File:handle = fopen(ADMIN_DIRECTORY "/audit.log", io_read);
    if (!handle)
    {
        SendClientMessage(playerid, ADMIN_ERROR, "Журнал отсутствует.");
        return 1;
    }
    new lines[5][240], line[240], count;
    while (fread(handle, line, sizeof(line)))
    {
        Auth_StripLine(line);
        format(lines[count % 5], 240, "%s", line);
        count++;
    }
    fclose(handle);
    new start = count > 5 ? count - 5 : 0;
    for (new i = start; i < count; i++)
        SendClientMessage(playerid, COLOR_GREY, lines[i % 5]);
    return 1;
}

stock Admin_Execute(playerid, const input[])
{
    new offset, command[24], arg1[32], arg2[32], reason[ADMIN_REASON_MAX + 1];
    if (!Admin_Next(input, offset, command, sizeof(command))) return 0;

    if (!strcmp(command, "/report", true))
    {
        Admin_Remainder(input, offset, reason, sizeof(reason));
        return Admin_ReportCreate(playerid, reason);
    }
    if (!strcmp(command, "/admins", true)) return Admin_ShowStaff(playerid);
    if (!strcmp(command, "/founder", true))
    {
        if (!Admin_IsFounder(playerid))
            return SendClientMessage(playerid, ADMIN_ERROR, "Команда зарезервирована для основателя Tarkenso.");
        if (!Admin_Next(input, offset, arg1, sizeof(arg1)))
        {
            new online, staff, line[120];
            for (new i = 0; i < MAX_PLAYERS; i++)
            {
                if (!Admin_Ready(i)) continue;
                online++;
                if (gAdminLevel[i] > 0) staff++;
            }
            format(line, sizeof(line), "Tarkenso RPG | VEYRAN (FOUNDER) | Онлайн: %d | Администрация: %d.", online, staff);
            SendClientMessage(playerid, ADMIN_OK, line);
            return SendClientMessage(playerid, ADMIN_COLOR, "Команда: /founder announce текст (только в Duty).");
        }
        if (strcmp(arg1, "announce", true))
            return SendClientMessage(playerid, ADMIN_ERROR, "Использование: /founder announce текст.");
        if (!Admin_IsFounder(playerid) || !Admin_Require(playerid, PERM_OWNER)) return 1;
        Admin_Remainder(input, offset, reason, sizeof(reason));
        if (!reason[0]) return SendClientMessage(playerid, ADMIN_ERROR, "Введите текст объявления.");
        Admin_SafeText(reason, sizeof(reason));
        if (!reason[0]) return SendClientMessage(playerid, ADMIN_ERROR, "Пустое объявление.");
        if (!Admin_LogPlayer(playerid, "FOUNDER_ANNOUNCE", reason))
            return SendClientMessage(playerid, ADMIN_ERROR, "Журнал недоступен. Объявление отменено.");
        new line[160];
        format(line, sizeof(line), "[Tarkenso RPG | FOUNDER VEYRAN] %s", reason);
        SendClientMessageToAll(ADMIN_OK, line);
        return 1;
    }
    if (!strcmp(command, "/aduty", true))
    {
        if (!Admin_Allowed(playerid, PERM_REPORT))
        {
            SendClientMessage(playerid, ADMIN_ERROR, "Нет прав администратора.");
            return 1;
        }
        if (!Admin_LogPlayer(playerid, "DUTY", gAdminDuty[playerid] ? "ON" : "OFF"))
            return SendClientMessage(playerid, ADMIN_ERROR, "Недоступен журнал действий: режим не изменён.");
        gAdminDuty[playerid] = !gAdminDuty[playerid];
        if (!gAdminDuty[playerid])
        {
            AdminFly_Stop(playerid);
            Admin_StopSpectate(playerid);
            AdminTP_Reset(playerid);
        }
        SendClientMessage(playerid, ADMIN_OK, gAdminDuty[playerid] ? "Режим администратора включён." : "Режим администратора выключен.");
        return 1;
    }
    if (!strcmp(command, "/admin", true))
    {
        if (!Admin_Allowed(playerid, PERM_REPORT))
        {
            Admin_Require(playerid, PERM_REPORT);
            return 1;
        }
        return Admin_UI_Toggle(playerid);
    }
    if (!strcmp(command, "/a", true))
    {
        Admin_Remainder(input, offset, reason, sizeof(reason));
        if (!reason[0]) return SendClientMessage(playerid, ADMIN_ERROR, "Использование: /a сообщение.");
        return Admin_StaffChat(playerid, reason);
    }
    if (!strcmp(command, "/reports", true)) return Admin_ReportList(playerid);
    if (!strcmp(command, "/alog", true)) return Admin_RecentLogs(playerid);
    if (!strcmp(command, "/accept", true) || !strcmp(command, "/close", true) ||
        !strcmp(command, "/areply", true))
    {
        if (!Admin_Require(playerid, PERM_REPORT)) return 1;
        if (!Admin_Next(input, offset, arg1, sizeof(arg1)) || !Admin_UnsignedNumber(arg1))
            return SendClientMessage(playerid, ADMIN_ERROR, "Требуется номер обращения.");
        new index = Admin_ReportIndex(strval(arg1));
        if (index < 0 || gReportState[index] != 1)
            return SendClientMessage(playerid, ADMIN_ERROR, "Обращение не найдено.");
        if (!strcmp(command, "/accept", true))
        {
            if (gReportAssignee[index][0] && !Admin_ReportAccess(playerid, index))
                return SendClientMessage(playerid, ADMIN_ERROR, "Обращение уже занято другим сотрудником.");
            format(gReportAssignee[index], MAX_PLAYER_NAME, "%s", gAccountName[playerid]);
            Admin_LogPlayer(playerid, "REPORT_ACCEPT", arg1);
            return SendClientMessage(playerid, ADMIN_OK, "Обращение принято.");
        }
        if (!Admin_ReportAccess(playerid, index))
            return SendClientMessage(playerid, ADMIN_ERROR, "Сначала примите обращение: /accept ID.");
        if (!strcmp(command, "/close", true))
        {
            gReportState[index] = 2;
            Admin_LogPlayer(playerid, "REPORT_CLOSE", arg1);
            return SendClientMessage(playerid, ADMIN_OK, "Обращение закрыто.");
        }
        Admin_Remainder(input, offset, reason, sizeof(reason));
        if (!reason[0]) return SendClientMessage(playerid, ADMIN_ERROR, "Использование: /areply ID текст.");
        new recipient = Admin_FindOnlineName(gReportAuthor[index]), line[180];
        if (recipient == INVALID_PLAYER_ID)
            return SendClientMessage(playerid, ADMIN_ERROR, "Автор обращения сейчас не в сети.");
        format(line, sizeof(line), "[Ответ на #%d] %s", gReportNumber[index], reason);
        SendClientMessage(recipient, ADMIN_COLOR, line);
        SendClientMessage(playerid, ADMIN_OK, "Ответ отправлен.");
        format(line, sizeof(line), "#%d %.95s", gReportNumber[index], reason);
        Admin_LogPlayer(playerid, "REPORT_REPLY", line);
        return 1;
    }

    // All commands below require an authenticated staff member.
    if (!strcmp(command, "/spec", true))
    {
        if (!Admin_Require(playerid, PERM_MODERATE)) return 1;
        if (!Admin_Next(input, offset, arg1, sizeof(arg1)))
            return SendClientMessage(playerid, ADMIN_ERROR, "Использование: /spec ID|off");
        if (!strcmp(arg1, "off", true))
        {
            Admin_StopSpectate(playerid);
            return SendClientMessage(playerid, ADMIN_OK, "Наблюдение завершено.");
        }
        if (!Admin_UnsignedNumber(arg1) || !Admin_ValidTarget(playerid, strval(arg1))) return 1;
        new target = strval(arg1);
        if (gAdminSpectate[playerid] != INVALID_PLAYER_ID) Admin_StopSpectate(playerid);
        GetPlayerPos(playerid, gAdminSpecPos[playerid][0], gAdminSpecPos[playerid][1], gAdminSpecPos[playerid][2]);
        gAdminSpecInterior[playerid] = GetPlayerInterior(playerid);
        gAdminSpecWorld[playerid] = GetPlayerVirtualWorld(playerid);
        gAdminSpectate[playerid] = target;
        TogglePlayerSpectating(playerid, true);
        SetPlayerInterior(playerid, GetPlayerInterior(target));
        SetPlayerVirtualWorld(playerid, GetPlayerVirtualWorld(target));
        PlayerSpectatePlayer(playerid, target);
        Admin_LogPlayer(playerid, "SPEC", gAccountName[target]);
        return 1;
    }

    if (!strcmp(command, "/weather", true))
    {
        if (!Admin_Require(playerid, PERM_MODERATE)) return 1;
        if (!Admin_Next(input, offset, arg1, sizeof(arg1)) ||
            !Admin_UnsignedNumber(arg1) ||
            Admin_Next(input, offset, arg2, sizeof(arg2)))
            return AdminHelp_Usage(playerid, "weather");
        new weatherid = strval(arg1);
        if (weatherid > 45)
            return AdminHelp_Usage(playerid, "weather");
        new detail[24], message[64];
        format(detail, sizeof(detail), "%d", weatherid);
        if (!Admin_LogPlayer(playerid, "WEATHER", detail))
            return SendClientMessage(playerid, ADMIN_ERROR, "Журнал недоступен. Погода не изменена.");
        gAdminWeather = weatherid;
        for (new i = 0; i < MAX_PLAYERS; i++)
            if (Admin_Ready(i)) Admin_ApplyEvent(i);
        format(message, sizeof(message), "Погода на сервере изменена: ID %d.", weatherid);
        return SendClientMessage(playerid, ADMIN_OK, message);
    }

    if (!strcmp(command, "/event", true))
    {
        if (!Admin_Require(playerid, PERM_EVENT)) return 1;
        if (!Admin_Next(input, offset, arg1, sizeof(arg1)))
            return AdminHelp_Usage(playerid, "event");
        if (!strcmp(arg1, "fog", true))
        {
            gAdminWeather = 20;
            gAdminClockAuto = false;
            gAdminClockHour = 3;
            gAdminClockMinute = 17;
        }
        else if (!strcmp(arg1, "night", true))
        {
            gAdminWeather = 9;
            gAdminClockAuto = false;
            gAdminClockHour = 0;
            gAdminClockMinute = 0;
        }
        else if (!strcmp(arg1, "clear", true))
        {
            gAdminWeather = 9;
            gAdminClockAuto = true;
            Admin_ClockRefresh();
        }
        else return AdminHelp_Usage(playerid, "event");
        for (new i = 0; i < MAX_PLAYERS; i++)
            if (Admin_Ready(i)) Admin_ApplyEvent(i);
        Admin_LogPlayer(playerid, "EVENT", arg1);
        return SendClientMessage(playerid, ADMIN_OK, "Сюжетный эффект применён.");
    }

    if (!strcmp(command, "/setadmin", true) || !strcmp(command, "/setevent", true))
    {
        if (!Admin_Require(playerid, PERM_MANAGE_STAFF)) return 1;
        if (!Admin_Next(input, offset, arg1, sizeof(arg1)) ||
            !Admin_Next(input, offset, arg2, sizeof(arg2)) ||
            !Admin_UnsignedNumber(arg1) || !Admin_UnsignedNumber(arg2))
            return AdminHelp_FromSlash(playerid, command);
        new target = strval(arg1), value = strval(arg2);
        if (target < 0 || target >= MAX_PLAYERS || !Admin_Ready(target) || target == playerid ||
            Admin_IsFounderName(gAccountName[target]) ||
            (gAdminLevel[target] >= gAdminLevel[playerid] && !Admin_IsFounder(playerid)))
            return SendClientMessage(playerid, ADMIN_ERROR, "Цель недоступна или имеет равные/старшие права.");
        if (!strcmp(command, "/setadmin", true))
        {
            new limit = gAdminLevel[playerid] == 6 ? 5 : 4;
            if (value > limit)
                return SendClientMessage(playerid, ADMIN_ERROR, "Нельзя назначить этот уровень.");
            format(reason, sizeof(reason), "%s -> %d", gAccountName[target], value);
            if (!Admin_LogPlayer(playerid, "SET_ADMIN_REQUEST", reason))
                return SendClientMessage(playerid, ADMIN_ERROR, "Журнал недоступен. Права не изменены.");
            if (!Admin_WriteRole(target, value, gAdminEvent[target]))
                return SendClientMessage(playerid, ADMIN_ERROR, "Ошибка записи роли. Права не изменены.");
            gAdminLevel[target] = value;
            if (value == 0) gAdminDuty[target] = false;
            format(reason, sizeof(reason), "%s -> %d", gAccountName[target], value);
            Admin_LogPlayer(playerid, "SET_ADMIN", reason);
        }
        else
        {
            if (value > 1) return SendClientMessage(playerid, ADMIN_ERROR, "Флаг события: 0 или 1.");
            format(reason, sizeof(reason), "%s -> %d", gAccountName[target], value);
            if (!Admin_LogPlayer(playerid, "SET_EVENT_REQUEST", reason))
                return SendClientMessage(playerid, ADMIN_ERROR, "Журнал недоступен. Права не изменены.");
            if (!Admin_WriteRole(target, gAdminLevel[target], bool:value))
                return SendClientMessage(playerid, ADMIN_ERROR, "Ошибка записи роли. Права не изменены.");
            gAdminEvent[target] = bool:value;
            format(reason, sizeof(reason), "%s -> %d", gAccountName[target], value);
            Admin_LogPlayer(playerid, "SET_EVENT_ROLE", reason);
        }
        return SendClientMessage(playerid, ADMIN_OK, "Полномочия обновлены.");
    }

    if (!strcmp(command, "/unban", true))
    {
        if (!Admin_Require(playerid, PERM_BAN_PERMANENT)) return 1;
        if (!Admin_Next(input, offset, arg1, sizeof(arg1)))
            return AdminHelp_Usage(playerid, "unban");
        new name[MAX_PLAYER_NAME], path[92];
        if (!Admin_NormalizeName(arg1, name, sizeof(name)))
            return SendClientMessage(playerid, ADMIN_ERROR, "Недопустимое имя.");
        Admin_BanPath(name, path, sizeof(path));
#if defined TARKENSO_DB_PRIMARY
        if (!DBPrimary_BanExists(name))
#else
        if (!fexist(path))
#endif
            return SendClientMessage(playerid, ADMIN_ERROR, "Блокировка не найдена.");
        if (!Admin_LogPlayer(playerid, "UNBAN_REQUEST", name))
            return SendClientMessage(playerid, ADMIN_ERROR, "Журнал недоступен. Блокировка не снята.");
#if defined TARKENSO_DB_PRIMARY
        if (!DBPrimary_UnbanName(name))
            return SendClientMessage(playerid, ADMIN_ERROR, "Не удалось удалить блокировку из MySQL.");
#else
        if (!fremove(path))
            return SendClientMessage(playerid, ADMIN_ERROR, "Файл блокировки недоступен.");
#endif
        Admin_LogPlayer(playerid, "UNBAN", name);
        return SendClientMessage(playerid, ADMIN_OK, "Блокировка снята.");
    }

    if (!strcmp(command, "/warn", true) || !strcmp(command, "/mute", true) ||
        !strcmp(command, "/kick", true) || !strcmp(command, "/freeze", true) ||
        !strcmp(command, "/goto", true) || !strcmp(command, "/gethere", true) ||
        !strcmp(command, "/ban", true))
    {
        new bool:moderation = strcmp(command, "/goto", true) &&
            strcmp(command, "/gethere", true) && strcmp(command, "/ban", true);
        if (!Admin_Require(playerid, moderation ? PERM_MODERATE : PERM_BAN_DAY)) return 1;
        if (!Admin_Next(input, offset, arg1, sizeof(arg1)) || !Admin_UnsignedNumber(arg1))
            return AdminHelp_FromSlash(playerid, command);
        new target = strval(arg1);
        if (!Admin_ValidTarget(playerid, target)) return 1;
        if (!strcmp(command, "/goto", true) || !strcmp(command, "/gethere", true))
        {
            if (AdminPunish_IsJailed(target) || AdminPunish_IsJailed(playerid))
                return SendClientMessage(playerid, ADMIN_ERROR, "Телепортация в административную тюрьму запрещена.");
            new source = target, destination = playerid;
            if (!strcmp(command, "/gethere", true)) { source = playerid; destination = target; }
            new Float:x, Float:y, Float:z;
            GetPlayerPos(source, x, y, z);
            SetPlayerInterior(destination, GetPlayerInterior(source));
            SetPlayerVirtualWorld(destination, GetPlayerVirtualWorld(source));
            SetPlayerPos(destination, x + 1.0, y, z);
            Admin_LogPlayer(playerid, command, gAccountName[target]);
            return 1;
        }
        if (!strcmp(command, "/freeze", true))
        {
            if (!Admin_LogPlayer(playerid, "FREEZE", gAccountName[target]))
                return SendClientMessage(playerid, ADMIN_ERROR, "Журнал недоступен. Действие отменено.");
            gAdminFrozen[target] = !gAdminFrozen[target];
            TogglePlayerControllable(target, !gAdminFrozen[target]);
            return SendClientMessage(playerid, ADMIN_OK, "Состояние заморозки изменено.");
        }
        new minutes;
        if (!strcmp(command, "/mute", true) || !strcmp(command, "/ban", true))
        {
            if (!Admin_Next(input, offset, arg2, sizeof(arg2)) || !Admin_UnsignedNumber(arg2))
                return AdminHelp_FromSlash(playerid, command);
            minutes = strval(arg2);
            if (minutes < 0 || minutes > 43200)
                return AdminHelp_FromSlash(playerid, command);
            if (!strcmp(command, "/mute", true) && (minutes < 1 || minutes > 1440))
                return AdminHelp_Usage(playerid, "mute");
            if (!strcmp(command, "/ban", true))
            {
                if (minutes == 0 && !Admin_Allowed(playerid, PERM_BAN_PERMANENT))
                    return SendClientMessage(playerid, ADMIN_ERROR, "Постоянная блокировка с 5 уровня.");
                if (minutes > 1440 && !Admin_Allowed(playerid, PERM_BAN_MONTH))
                    return SendClientMessage(playerid, ADMIN_ERROR, "Бан более 24 часов доступен с 4 уровня.");
            }
        }
        Admin_Remainder(input, offset, reason, sizeof(reason));
        if (!reason[0]) return AdminHelp_FromSlash(playerid, command);
        new detail[170];
        format(detail, sizeof(detail), "%s: %.90s", gAccountName[target], reason);
        if (!strcmp(command, "/mute", true))
        {
            if (!Admin_LogPlayer(playerid, "MUTE", detail))
                return SendClientMessage(playerid, ADMIN_ERROR, "Журнал недоступен. Действие отменено.");
            new until = gettime() + minutes * 60;
            if (!Admin_WriteMute(target, until))
                return SendClientMessage(playerid, ADMIN_ERROR, "Не удалось сохранить mute. Наказание отменено.");
            return SendClientMessage(target, ADMIN_ERROR, "Ваш чат временно заблокирован.");
        }
        if (!strcmp(command, "/ban", true))
        {
            new expires = minutes == 0 ? 0 : gettime() + minutes * 60;
            if (!Admin_LogPlayer(playerid, "BAN_REQUEST", detail))
                return SendClientMessage(playerid, ADMIN_ERROR, "Журнал недоступен. Действие отменено.");
            if (!Admin_WriteBan(target, expires, reason))
                return SendClientMessage(playerid, ADMIN_ERROR, "Блокировка не сохранена. Игрок не отключён.");
            Admin_LogPlayer(playerid, "BAN", detail);
            SendClientMessage(target, ADMIN_ERROR, "Ваш аккаунт заблокирован.");
            Kick(target);
            return 1;
        }
        if (!strcmp(command, "/warn", true))
        {
            if (!Admin_LogPlayer(playerid, "WARN_REQUEST", detail))
                return SendClientMessage(playerid, ADMIN_ERROR, "Журнал недоступен. Действие отменено.");
            if (!AdminPunish_WriteWarning(playerid, target, reason))
                return SendClientMessage(playerid, ADMIN_ERROR, "Ошибка MariaDB. Предупреждение не выдано.");
            Admin_LogPlayer(playerid, "WARN", detail);
            SendClientMessage(target, ADMIN_ERROR, "Вы получили предупреждение администрации.");
            return 1;
        }
        if (!Admin_LogPlayer(playerid, "KICK", detail))
            return SendClientMessage(playerid, ADMIN_ERROR, "Журнал недоступен. Действие отменено.");
        SendClientMessage(target, ADMIN_ERROR, "Вы исключены с сервера администрацией.");
        Kick(target);
        return 1;
    }
    return 0;
}
