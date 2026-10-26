#if defined _tarkenso_server_guard_included
    #endinput
#endif
#define _tarkenso_server_guard_included

// SERVER-GUARD-01. Ограничение прикладного флуда и диагностика задержек.
// Сетевая DDoS-фильтрация должна выполняться на хостинге, не в Pawn.
#define GUARD_LOG_PATH                 "server_guard.log"
#define GUARD_HEARTBEAT_INTERVAL_MS    (1000)
#define GUARD_HEARTBEAT_WARNING_MS     (2500)
#define GUARD_SLOW_COMMAND_MS          (50)
#define GUARD_CHAT_WINDOW_SEC          (8)
#define GUARD_CHAT_LIMIT               (6)
#define GUARD_COMMAND_WINDOW_SEC       (5)
#define GUARD_COMMAND_LIMIT            (15)
#define GUARD_CONNECTION_WINDOW_SEC    (60)
#define GUARD_CONNECTION_ALERT         (30)
#define GUARD_PLAYER_LOG_INTERVAL_SEC  (60)
#define GUARD_STALL_LOG_INTERVAL_SEC   (15)
#define GUARD_WARNING_INTERVAL_SEC     (5)

new gGuardHeartbeatTimer;
new gGuardLastHeartbeatTick;
new gGuardLastHeartbeatMs;
new gGuardWorstHeartbeatMs;
new gGuardDelayedHeartbeatCount;
new gGuardNextStallLog;
new gGuardChatPassed, gGuardChatDropped;
new gGuardCommandPassed, gGuardCommandDropped;
new gGuardSlowCommands, gGuardWorstCommandMs;
new gGuardConnectWindowStart, gGuardConnectWindowCount;
new gGuardConnectionTotal;
new gGuardChatWindowStart[MAX_PLAYERS];
new gGuardChatWindowCount[MAX_PLAYERS];
new gGuardCommandWindowStart[MAX_PLAYERS];
new gGuardCommandWindowCount[MAX_PLAYERS];
new gGuardChatNextLog[MAX_PLAYERS];
new gGuardCommandNextLog[MAX_PLAYERS];
new gGuardSlowCommandNextLog[MAX_PLAYERS];
new gGuardNextWarning[MAX_PLAYERS];
new gGuardCommandStartTick[MAX_PLAYERS];
new bool:gGuardCommandTiming[MAX_PLAYERS];

forward Guard_Heartbeat();

stock Guard_LogEvent(const eventName[], playerid, value)
{
    // Не сохраняем сообщения, пароли, IP и параметры команд.
    new line[144];
    format(line, sizeof(line), "%d|%s|player=%d|value=%d\n",
        gettime(), eventName, playerid, value);
    printf("[TARKENSO GUARD] %s: player=%d value=%d", eventName, playerid, value);
    new File:log = fopen(GUARD_LOG_PATH, io_append);
    if (!log) return 0;
    new written = fwrite(log, line);
    fclose(log);
    return written > 0;
}

stock Guard_ResetPlayer(playerid)
{
    gGuardChatWindowStart[playerid] = 0;
    gGuardChatWindowCount[playerid] = 0;
    gGuardCommandWindowStart[playerid] = 0;
    gGuardCommandWindowCount[playerid] = 0;
    gGuardChatNextLog[playerid] = 0;
    gGuardCommandNextLog[playerid] = 0;
    gGuardSlowCommandNextLog[playerid] = 0;
    gGuardNextWarning[playerid] = 0;
    gGuardCommandStartTick[playerid] = 0;
    gGuardCommandTiming[playerid] = false;
    return 1;
}

stock Guard_WarnPlayer(playerid)
{
    new now = gettime();
    if (now >= gGuardNextWarning[playerid])
    {
        gGuardNextWarning[playerid] = now + GUARD_WARNING_INTERVAL_SEC;
        SendClientMessage(playerid, COLOR_RED, "Слишком много действий. Уменьшите частоту сообщений или команд.");
    }
    return 1;
}

stock Guard_Init()
{
    gGuardLastHeartbeatTick = GetTickCount();
    gGuardLastHeartbeatMs = 0;
    gGuardWorstHeartbeatMs = 0;
    gGuardDelayedHeartbeatCount = 0;
    gGuardNextStallLog = 0;
    gGuardChatPassed = 0;
    gGuardChatDropped = 0;
    gGuardCommandPassed = 0;
    gGuardCommandDropped = 0;
    gGuardSlowCommands = 0;
    gGuardWorstCommandMs = 0;
    gGuardConnectWindowStart = gettime();
    gGuardConnectWindowCount = 0;
    gGuardConnectionTotal = 0;
    gGuardHeartbeatTimer = SetTimer("Guard_Heartbeat", GUARD_HEARTBEAT_INTERVAL_MS, true);
    if (!gGuardHeartbeatTimer)
        print("[TARKENSO GUARD] ERROR: watchdog timer failed to start.");
    return 1;
}

stock Guard_Shutdown()
{
    if (gGuardHeartbeatTimer) KillTimer(gGuardHeartbeatTimer);
    gGuardHeartbeatTimer = 0;
    return 1;
}

public Guard_Heartbeat()
{
    new tick = GetTickCount();
    new elapsedHeartbeatMs = tick - gGuardLastHeartbeatTick;
    gGuardLastHeartbeatTick = tick;

    // Отрицательное значение возможно при переполнении GetTickCount.
    if (elapsedHeartbeatMs < 0) return 1;
    gGuardLastHeartbeatMs = elapsedHeartbeatMs;
    if (elapsedHeartbeatMs > gGuardWorstHeartbeatMs) gGuardWorstHeartbeatMs = elapsedHeartbeatMs;
    if (elapsedHeartbeatMs >= GUARD_HEARTBEAT_WARNING_MS)
    {
        gGuardDelayedHeartbeatCount++;
        new now = gettime();
        if (now >= gGuardNextStallLog)
        {
            gGuardNextStallLog = now + GUARD_STALL_LOG_INTERVAL_SEC;
            Guard_LogEvent("HEARTBEAT_DELAY", -1, elapsedHeartbeatMs);
        }
    }
    return 1;
}

stock Guard_OnConnect(playerid)
{
    Guard_ResetPlayer(playerid);
    // Штатные NPC не считаются в сигнале всплеска подключений.
    if (IsPlayerNPC(playerid)) return 1;
    gGuardConnectionTotal++;
    new now = gettime();
    if (now - gGuardConnectWindowStart >= GUARD_CONNECTION_WINDOW_SEC)
    {
        gGuardConnectWindowStart = now;
        gGuardConnectWindowCount = 0;
    }
    gGuardConnectWindowCount++;
    // Только сигнал оператору, без кика: высокий онлайн не равен атаке.
    if (gGuardConnectWindowCount == GUARD_CONNECTION_ALERT)
        Guard_LogEvent("CONNECTION_SPIKE", -1, gGuardConnectWindowCount);
    return 1;
}

stock Guard_OnDisconnect(playerid)
{
    Guard_ResetPlayer(playerid);
    return 1;
}

stock bool:Guard_AllowChat(playerid)
{
    new now = gettime();
    if (!gGuardChatWindowStart[playerid] ||
        now - gGuardChatWindowStart[playerid] >= GUARD_CHAT_WINDOW_SEC)
    {
        gGuardChatWindowStart[playerid] = now;
        gGuardChatWindowCount[playerid] = 0;
    }
    if (gGuardChatWindowCount[playerid] < 1000000)
        gGuardChatWindowCount[playerid]++;
    if (gGuardChatWindowCount[playerid] <= GUARD_CHAT_LIMIT)
    {
        gGuardChatPassed++;
        return true;
    }
    gGuardChatDropped++;
    Guard_WarnPlayer(playerid);
    if (now >= gGuardChatNextLog[playerid])
    {
        gGuardChatNextLog[playerid] = now + GUARD_PLAYER_LOG_INTERVAL_SEC;
        Guard_LogEvent("CHAT_FLOOD", playerid, gGuardChatWindowCount[playerid]);
    }
    return false;
}

stock bool:Guard_AllowCommand(playerid)
{
    new now = gettime();
    if (!gGuardCommandWindowStart[playerid] ||
        now - gGuardCommandWindowStart[playerid] >= GUARD_COMMAND_WINDOW_SEC)
    {
        gGuardCommandWindowStart[playerid] = now;
        gGuardCommandWindowCount[playerid] = 0;
    }
    if (gGuardCommandWindowCount[playerid] < 1000000)
        gGuardCommandWindowCount[playerid]++;
    if (gGuardCommandWindowCount[playerid] <= GUARD_COMMAND_LIMIT)
    {
        gGuardCommandPassed++;
        return true;
    }
    gGuardCommandDropped++;
    Guard_WarnPlayer(playerid);
    if (now >= gGuardCommandNextLog[playerid])
    {
        gGuardCommandNextLog[playerid] = now + GUARD_PLAYER_LOG_INTERVAL_SEC;
        Guard_LogEvent("COMMAND_FLOOD", playerid, gGuardCommandWindowCount[playerid]);
    }
    return false;
}

stock Guard_CommandStart(playerid)
{
    gGuardCommandStartTick[playerid] = GetTickCount();
    gGuardCommandTiming[playerid] = true;
    return 1;
}

stock Guard_CommandFinish(playerid)
{
    if (!gGuardCommandTiming[playerid]) return 1;
    gGuardCommandTiming[playerid] = false;
    new elapsed = GetTickCount() - gGuardCommandStartTick[playerid];
    if (elapsed < 0) return 1;
    if (elapsed > gGuardWorstCommandMs) gGuardWorstCommandMs = elapsed;
    if (elapsed >= GUARD_SLOW_COMMAND_MS)
    {
        gGuardSlowCommands++;
        new now = gettime();
        if (now >= gGuardSlowCommandNextLog[playerid])
        {
            gGuardSlowCommandNextLog[playerid] = now + GUARD_PLAYER_LOG_INTERVAL_SEC;
            Guard_LogEvent("SLOW_COMMAND", playerid, elapsed);
        }
    }
    return 1;
}

stock Guard_ShowStatus(playerid)
{
    if (!Admin_Allowed(playerid, PERM_REPORT))
    {
        SendClientMessage(playerid, COLOR_RED, "Доступно только администрации.");
        return 1;
    }
    new line[144];
    SendClientMessage(playerid, COLOR_AMBER, "Tarkenso RPG Server Guard | диагностика");
    format(line, sizeof(line), "Таймер: %d мс; максимум: %d мс; задержек: %d.",
        gGuardLastHeartbeatMs, gGuardWorstHeartbeatMs, gGuardDelayedHeartbeatCount);
    SendClientMessage(playerid, COLOR_WHITE, line);
    format(line, sizeof(line), "Чат: пропущено %d / отклонено %d. Команды: %d / %d.",
        gGuardChatPassed, gGuardChatDropped, gGuardCommandPassed, gGuardCommandDropped);
    SendClientMessage(playerid, COLOR_WHITE, line);
    format(line, sizeof(line), "Медленных команд: %d; максимум: %d мс.",
        gGuardSlowCommands, gGuardWorstCommandMs);
    SendClientMessage(playerid, COLOR_WHITE, line);
    new recentConnects = gGuardConnectWindowCount;
    if (gettime() - gGuardConnectWindowStart >= GUARD_CONNECTION_WINDOW_SEC)
        recentConnects = 0;
    format(line, sizeof(line), "Подключений всего: %d; за текущее окно 60 с: %d.",
        gGuardConnectionTotal, recentConnects);
    SendClientMessage(playerid, COLOR_GREY, "Журнал: scriptfiles/server_guard.log (проверить на хостинге).");
    return 1;
}
