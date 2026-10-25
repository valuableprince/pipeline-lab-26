#if defined _tarkenso_admin_punish_included
    #endinput
#endif
#define _tarkenso_admin_punish_included

// ADMIN-PUNISH-03. All new punishments require schema version 9.
#define ADMIN_JAIL_WORLD (27000)
#define ADMIN_JAIL_INTERIOR (6)
#define ADMIN_JAIL_X (264.63)
#define ADMIN_JAIL_Y (77.57)
#define ADMIN_JAIL_Z (1001.04)
#define ADMIN_JAIL_MAX_MINUTES (43200)
#define ADMIN_NOTE_MAX (160)

new gAdminJailUntil[MAX_PLAYERS];
new gAdminJailSentenceId[MAX_PLAYERS];
new gAdminJailReason[MAX_PLAYERS][ADMIN_REASON_MAX + 1];
new gAdminJailTimer;

stock AdminPunish_Reset(playerid)
{
    gAdminJailUntil[playerid] = 0;
    gAdminJailSentenceId[playerid] = 0;
    gAdminJailReason[playerid][0] = EOS;
    return 1;
}

stock bool:AdminPunish_IsJailed(playerid)
{
    return gAdminJailSentenceId[playerid] > 0;
}

stock bool:AdminPunish_Load(playerid)
{
#if defined TARKENSO_DB_PRIMARY
    AdminPunish_Reset(playerid);
    if (!DBPrimary_IsReady() || !gDBPrimaryLoaded[playerid]) return false;
    new q[300], rows;
    mysql_format(gDBPrimary, q, sizeof(q),
        "SELECT sentence_id,jailed_until,reason FROM admin_jail_sentences WHERE account_id=%d AND released_at IS NULL ORDER BY sentence_id DESC LIMIT 1",
        gDBPrimaryAccountId[playerid]);
    new Cache:c = mysql_query(gDBPrimary, q);
    if (c == Cache:0 || mysql_errno(gDBPrimary) || !cache_get_row_count(rows) || rows > 1)
    {
        if (c != Cache:0) cache_delete(c);
        return false;
    }
    if (!rows) { cache_delete(c); return true; }
    new sentence, until;
    new bool:ok = cache_get_value_name_int(0, "sentence_id", sentence) &&
        cache_get_value_name_int(0, "jailed_until", until) &&
        cache_get_value_name(0, "reason", gAdminJailReason[playerid], sizeof(gAdminJailReason[]));
    cache_delete(c);
    if (!ok || sentence < 1 || until < 1) return false;
    gAdminJailSentenceId[playerid] = sentence;
    gAdminJailUntil[playerid] = until;
    // Do not remove expired sentences until DB confirms a successful release.
    if (until <= gettime())
        return AdminPunish_Release(playerid, "SYSTEM");
    return true;
#else
    #pragma unused playerid
    return true;
#endif
}

stock bool:AdminPunish_Release(playerid, const actor[])
{
#if defined TARKENSO_DB_PRIMARY
    if (!DBPrimary_IsReady() || !gAdminJailSentenceId[playerid]) return false;
    new q[360], affected;
    mysql_format(gDBPrimary, q, sizeof(q),
        "UPDATE admin_jail_sentences SET released_at=NOW(),released_by='%e' WHERE sentence_id=%d AND account_id=%d AND released_at IS NULL",
        actor, gAdminJailSentenceId[playerid], gDBPrimaryAccountId[playerid]);
    if (!DBPrimary_Execute(q, affected) || affected != 1) return false;
    AdminPunish_Reset(playerid);
    if (gAuthState[playerid] == AUTH_READY && gAccountSpawned[playerid])
    {
        SpawnPlayer(playerid);
        SendClientMessage(playerid, ADMIN_OK, "Срок административного заключения завершён.");
    }
    return true;
#else
    #pragma unused playerid, actor
    return false;
#endif
}

stock AdminPunish_Apply(playerid)
{
    if (!AdminPunish_IsJailed(playerid)) return 0;
    if (GetPlayerState(playerid) == PLAYER_STATE_DRIVER ||
        GetPlayerState(playerid) == PLAYER_STATE_PASSENGER)
        RemovePlayerFromVehicle(playerid);
    Story_TapeStop(playerid);
    Story_ClearMarker(playerid);
    SetPlayerVirtualWorld(playerid, ADMIN_JAIL_WORLD);
    SetPlayerInterior(playerid, ADMIN_JAIL_INTERIOR);
    SetPlayerPos(playerid, ADMIN_JAIL_X, ADMIN_JAIL_Y, ADMIN_JAIL_Z);
    SetCameraBehindPlayer(playerid);
    new msg[120], minutes = (gAdminJailUntil[playerid] - gettime() + 59) / 60;
    if (minutes < 0) minutes = 0;
    format(msg, sizeof(msg), "Административная тюрьма: осталось %d мин. Команды ограничены.", minutes);
    SendClientMessage(playerid, ADMIN_ERROR, msg);
    return 1;
}

stock bool:AdminPunish_OnSpawn(playerid)
{
    if (!AdminPunish_IsJailed(playerid)) return false;
    AdminPunish_Apply(playerid);
    return true;
}

forward AdminPunish_Tick();
public AdminPunish_Tick()
{
    for (new playerid = 0; playerid < MAX_PLAYERS; playerid++)
    {
        if (!Admin_Ready(playerid) || !AdminPunish_IsJailed(playerid)) continue;
        if (gAdminJailUntil[playerid] <= gettime())
        {
            if (!AdminPunish_Release(playerid, "SYSTEM"))
                print("[ADMIN-PUNISH] ERROR: failed to release expired jail sentence.");
            continue;
        }
        // A normal teleport, script or client update must not move a prisoner out.
        if (GetPlayerVirtualWorld(playerid) != ADMIN_JAIL_WORLD ||
            GetPlayerInterior(playerid) != ADMIN_JAIL_INTERIOR)
        {
            AdminPunish_Apply(playerid);
            continue;
        }
        new Float:x, Float:y, Float:z;
        GetPlayerPos(playerid, x, y, z);
        if (floatabs(x - ADMIN_JAIL_X) > 12.0 ||
            floatabs(y - ADMIN_JAIL_Y) > 12.0 ||
            floatabs(z - ADMIN_JAIL_Z) > 12.0)
            AdminPunish_Apply(playerid);
    }
    return 1;
}

stock bool:AdminPunish_AuthorizeTarget(playerid, target)
{
    if (!Admin_ValidTarget(playerid, target)) return false;
    if (!gDBPrimaryLoaded[target] || gDBPrimaryAccountId[target] < 1) return false;
    return true;
}

stock bool:AdminPunish_ParseTarget(const params[], &pos, &target)
{
    new text[24];
    if (!Admin_Next(params, pos, text, sizeof(text)) || !Admin_UnsignedNumber(text)) return false;
    target = strval(text);
    return target >= 0 && target < MAX_PLAYERS;
}

stock AdminPunish_Jail(playerid, const params[])
{
    if (!Admin_Require(playerid, PERM_BAN_DAY)) return 1;
    new pos, target, minutes, number[24], reason[ADMIN_REASON_MAX + 1];
    if (!AdminPunish_ParseTarget(params, pos, target) ||
        !Admin_Next(params, pos, number, sizeof(number)) ||
        !Admin_UnsignedNumber(number)) return AdminHelp_Usage(playerid, "ajail");
    minutes = strval(number);
    Admin_Remainder(params, pos, reason, sizeof(reason));
    if (minutes < 1 || minutes > ADMIN_JAIL_MAX_MINUTES || !reason[0])
        return AdminHelp_Usage(playerid, "ajail");
    if (!AdminPunish_AuthorizeTarget(playerid, target)) return 1;
    if (AdminPunish_IsJailed(target))
        return SendClientMessage(playerid, ADMIN_ERROR, "Игрок уже отбывает наказание.");
    if (Race_IsParticipant(target) || !gAccountSpawned[target] || gAccountDead[target] ||
        GetPlayerState(target) == PLAYER_STATE_SPECTATING)
        return SendClientMessage(playerid, ADMIN_ERROR, "Дождитесь завершения гонки или появления игрока.");
    new until = gettime() + minutes * 60;
    new detail[160], q[590], affected;
    format(detail, sizeof(detail), "%s: %d мин; %.90s", gAccountName[target], minutes, reason);
    if (!Admin_LogPlayer(playerid, "AJAIL_REQUEST", detail))
        return SendClientMessage(playerid, ADMIN_ERROR, "Аудит недоступен. Заключение отменено.");
    mysql_format(gDBPrimary, q, sizeof(q),
        "INSERT INTO admin_jail_sentences (account_id,issued_by,reason,jailed_until) VALUES (%d,'%e','%e',%d)",
        gDBPrimaryAccountId[target], gAccountName[playerid], reason, until);
    if (!DBPrimary_Execute(q, affected) || affected != 1)
        return SendClientMessage(playerid, ADMIN_ERROR, "Ошибка MariaDB: наказание не применено.");
    new Cache:c = mysql_query(gDBPrimary, "SELECT LAST_INSERT_ID() AS id");
    new rows, id;
    if (c == Cache:0 || mysql_errno(gDBPrimary) ||
        !cache_get_row_count(rows) || rows != 1 ||
        !cache_get_value_name_int(0, "id", id) || id < 1)
    {
        if (c != Cache:0) cache_delete(c);
        Kick(target); // fail closed, next login recovers sentence from MariaDB
        return SendClientMessage(playerid, ADMIN_ERROR, "Наказание сохранено. Игрок отключён для безопасной повторной загрузки.");
    }
    cache_delete(c);
    gAdminJailSentenceId[target] = id;
    gAdminJailUntil[target] = until;
    format(gAdminJailReason[target], sizeof(gAdminJailReason[]), "%s", reason);
    AdminPunish_Apply(target);
    return SendClientMessage(playerid, ADMIN_OK, "Игрок отправлен в административную тюрьму.");
}

stock AdminPunish_Unjail(playerid, const params[])
{
    if (!Admin_Require(playerid, PERM_BAN_DAY)) return 1;
    new target;
    if (!AdminTools_ReadTarget(params, target)) return AdminHelp_Usage(playerid, "unjail");
    if (!AdminPunish_AuthorizeTarget(playerid, target)) return 1;
    if (!AdminPunish_IsJailed(target))
        return SendClientMessage(playerid, ADMIN_ERROR, "Игрок не находится в административной тюрьме.");
    new detail[90];
    format(detail, sizeof(detail), "target=%s", gAccountName[target]);
    if (!Admin_LogPlayer(playerid, "UNJAIL_REQUEST", detail))
        return SendClientMessage(playerid, ADMIN_ERROR, "Аудит недоступен. Освобождение отменено.");
    if (!AdminPunish_Release(target, gAccountName[playerid]))
        return SendClientMessage(playerid, ADMIN_ERROR, "Ошибка MariaDB. Наказание остаётся активным.");
    return SendClientMessage(playerid, ADMIN_OK, "Игрок освобождён.");
}

stock AdminPunish_JailInfo(playerid, const params[])
{
    if (!Admin_Require(playerid, PERM_MODERATE)) return 1;
    new target, msg[114];
    if (!AdminTools_ReadTarget(params, target)) return AdminHelp_Usage(playerid, "jailinfo");
    if (!Admin_Ready(target)) return SendClientMessage(playerid, ADMIN_ERROR, "Игрок не найден.");
    new detail[96];
    format(detail, sizeof(detail), "target=%s", gAccountName[target]);
    if (!Admin_LogPlayer(playerid, "JAILINFO_VIEW", detail))
        return SendClientMessage(playerid, ADMIN_ERROR, "Аудит недоступен.");
    if (!AdminPunish_IsJailed(target)) return SendClientMessage(playerid, ADMIN_COLOR, "Игрок не заключён.");
    new remaining = (gAdminJailUntil[target] - gettime() + 59) / 60;
    if (remaining < 0) remaining = 0;
    format(msg, sizeof(msg), "%s: %d мин. Причина: %.65s",
        gAccountName[target], remaining, gAdminJailReason[target]);
    return SendClientMessage(playerid, ADMIN_COLOR, msg);
}

stock AdminPunish_Warns(playerid, const params[])
{
    if (!Admin_Require(playerid, PERM_MODERATE)) return 1;
    new target;
    if (!AdminTools_ReadTarget(params, target)) return AdminHelp_Usage(playerid, "warns");
    if (!AdminPunish_AuthorizeTarget(playerid, target)) return 1;
    if (!Admin_LogPlayer(playerid, "WARN_VIEW", gAccountName[target]))
        return SendClientMessage(playerid, ADMIN_ERROR, "Аудит недоступен.");
    new q[240], rows, line[160], reason[97], id;
    mysql_format(gDBPrimary, q, sizeof(q),
        "SELECT warning_id,reason FROM admin_warnings WHERE account_id=%d AND revoked_at IS NULL ORDER BY warning_id DESC LIMIT 5",
        gDBPrimaryAccountId[target]);
    new Cache:c = mysql_query(gDBPrimary, q);
    if (c == Cache:0 || mysql_errno(gDBPrimary) || !cache_get_row_count(rows))
    {
        if (c != Cache:0) cache_delete(c);
        return SendClientMessage(playerid, ADMIN_ERROR, "Не удалось прочитать предупреждения.");
    }
    format(line, sizeof(line), "Предупреждения %s (последние %d):", gAccountName[target], rows);
    SendClientMessage(playerid, ADMIN_COLOR, line);
    for (new i = 0; i < rows; i++)
    {
        if (!cache_get_value_name_int(i, "warning_id", id) ||
            !cache_get_value_name(i, "reason", reason, sizeof(reason))) break;
        format(line, sizeof(line), "#%d: %.96s", id, reason);
        SendClientMessage(playerid, ADMIN_COLOR, line);
    }
    cache_delete(c);
    return 1;
}

stock bool:AdminPunish_WriteWarning(playerid, target, const reason[])
{
    if (!DBPrimary_IsReady() || !gDBPrimaryLoaded[target]) return false;
    new q[440], affected;
    mysql_format(gDBPrimary, q, sizeof(q),
        "INSERT INTO admin_warnings (account_id,issued_by,reason) VALUES (%d,'%e','%e')",
        gDBPrimaryAccountId[target], gAccountName[playerid], reason);
    return DBPrimary_Execute(q, affected) && affected == 1;
}

stock AdminPunish_Unwarn(playerid, const params[])
{
    if (!Admin_Require(playerid, PERM_BAN_MONTH)) return 1;
    new pos, target, warningid, num[24], reason[ADMIN_REASON_MAX + 1];
    if (!AdminPunish_ParseTarget(params, pos, target) ||
        !Admin_Next(params, pos, num, sizeof(num)) || !Admin_UnsignedNumber(num))
        return AdminHelp_Usage(playerid, "unwarn");
    warningid = strval(num);
    Admin_Remainder(params, pos, reason, sizeof(reason));
    if (warningid < 1 || !reason[0]) return AdminHelp_Usage(playerid, "unwarn");
    if (!AdminPunish_AuthorizeTarget(playerid, target)) return 1;
    new detail[166], q[440], affected;
    format(detail, sizeof(detail), "%s warning=%d: %.90s", gAccountName[target], warningid, reason);
    if (!Admin_LogPlayer(playerid, "UNWARN_REQUEST", detail))
        return SendClientMessage(playerid, ADMIN_ERROR, "Аудит недоступен. Действие отменено.");
    mysql_format(gDBPrimary, q, sizeof(q),
        "UPDATE admin_warnings SET revoked_at=NOW(),revoked_by='%e',revoked_reason='%e' WHERE warning_id=%d AND account_id=%d AND revoked_at IS NULL",
        gAccountName[playerid], reason, warningid, gDBPrimaryAccountId[target]);
    if (!DBPrimary_Execute(q, affected) || affected != 1)
        return SendClientMessage(playerid, ADMIN_ERROR, "Предупреждение не найдено или ошибка MariaDB.");
    SendClientMessage(target, ADMIN_COLOR, "Администратор снял ваше предупреждение.");
    return SendClientMessage(playerid, ADMIN_OK, "Предупреждение снято.");
}

stock AdminPunish_ANote(playerid, const params[])
{
    if (!Admin_Require(playerid, PERM_MODERATE)) return 1;
    new pos, target, note[ADMIN_NOTE_MAX + 1];
    if (!AdminPunish_ParseTarget(params, pos, target)) return AdminHelp_Usage(playerid, "anote");
    Admin_Remainder(params, pos, note, sizeof(note));
    if (!note[0] || strlen(note) > ADMIN_NOTE_MAX) return AdminHelp_Usage(playerid, "anote");
    if (!AdminPunish_AuthorizeTarget(playerid, target)) return 1;
    new detail[100], q[600], affected;
    format(detail, sizeof(detail), "target=%s note_len=%d", gAccountName[target], strlen(note));
    if (!Admin_LogPlayer(playerid, "ADMIN_NOTE_REQUEST", detail))
        return SendClientMessage(playerid, ADMIN_ERROR, "Аудит недоступен. Запись отменена.");
    mysql_format(gDBPrimary, q, sizeof(q),
        "INSERT INTO admin_player_notes (account_id,author,note) VALUES (%d,'%e','%e')",
        gDBPrimaryAccountId[target], gAccountName[playerid], note);
    if (!DBPrimary_Execute(q, affected) || affected != 1)
        return SendClientMessage(playerid, ADMIN_ERROR, "Ошибка MariaDB: заметка не записана.");
    return SendClientMessage(playerid, ADMIN_OK, "Служебная заметка сохранена.");
}

stock AdminPunish_ANotes(playerid, const params[])
{
    if (!Admin_Require(playerid, PERM_MODERATE)) return 1;
    new target;
    if (!AdminTools_ReadTarget(params, target)) return AdminHelp_Usage(playerid, "anotes");
    if (!AdminPunish_AuthorizeTarget(playerid, target)) return 1;
    if (!Admin_LogPlayer(playerid, "NOTES_VIEW", gAccountName[target]))
        return SendClientMessage(playerid, ADMIN_ERROR, "Аудит недоступен.");
    new q[280], rows, line[184], author[24], note[ADMIN_NOTE_MAX + 1], id;
    mysql_format(gDBPrimary, q, sizeof(q),
        "SELECT note_id,author,note FROM admin_player_notes WHERE account_id=%d ORDER BY note_id DESC LIMIT 5",
        gDBPrimaryAccountId[target]);
    new Cache:c = mysql_query(gDBPrimary, q);
    if (c == Cache:0 || mysql_errno(gDBPrimary) || !cache_get_row_count(rows))
    {
        if (c != Cache:0) cache_delete(c);
        return SendClientMessage(playerid, ADMIN_ERROR, "Не удалось прочитать заметки.");
    }
    format(line, sizeof(line), "Служебные заметки %s (последние %d):", gAccountName[target], rows);
    SendClientMessage(playerid, ADMIN_COLOR, line);
    for (new i = 0; i < rows; i++)
    {
        if (!cache_get_value_name_int(i, "note_id", id) ||
            !cache_get_value_name(i, "author", author, sizeof(author)) ||
            !cache_get_value_name(i, "note", note, sizeof(note))) break;
        format(line, sizeof(line), "#%d %s: %.125s", id, author, note);
        SendClientMessage(playerid, ADMIN_COLOR, line);
    }
    cache_delete(c);
    return 1;
}

stock AdminPunish_OfflineBan(playerid, const params[])
{
    if (!Admin_Require(playerid, PERM_BAN_PERMANENT)) return 1;
    new pos, rawName[24], name[24], timeText[24], reason[ADMIN_REASON_MAX + 1];
    if (!Admin_Next(params, pos, rawName, sizeof(rawName)) ||
        !Admin_Next(params, pos, timeText, sizeof(timeText)) ||
        !Admin_UnsignedNumber(timeText) ||
        !Admin_NormalizeName(rawName, name, sizeof(name)))
        return AdminHelp_Usage(playerid, "offlineban");
    new minutes = strval(timeText);
    Admin_Remainder(params, pos, reason, sizeof(reason));
    if (minutes < 0 || minutes > 43200 || !reason[0])
        return AdminHelp_Usage(playerid, "offlineban");
    if (Admin_IsFounderName(name))
        return SendClientMessage(playerid, ADMIN_ERROR, "Аккаунт основателя защищён.");
    if (Admin_FindOnlineName(name) != INVALID_PLAYER_ID)
        return SendClientMessage(playerid, ADMIN_ERROR, "Игрок онлайн: используйте /ban.");

    // Always check the persisted administrative role, not only online role arrays.
    new q[480], rows, rank = 0, affected;
    mysql_format(gDBPrimary, q, sizeof(q),
        "SELECT COALESCE(r.admin_level,0) AS lvl FROM accounts a LEFT JOIN admin_roles r ON r.account_id=a.account_id WHERE a.username_key=LOWER('%e') LIMIT 1", name);
    new Cache:c = mysql_query(gDBPrimary, q);
    if (c == Cache:0 || mysql_errno(gDBPrimary) || !cache_get_row_count(rows) || rows > 1)
    {
        if (c != Cache:0) cache_delete(c);
        return SendClientMessage(playerid, ADMIN_ERROR, "Не удалось проверить права. Блокировка отменена.");
    }
    if (rows == 1 && !cache_get_value_name_int(0, "lvl", rank))
    {
        cache_delete(c);
        return SendClientMessage(playerid, ADMIN_ERROR, "Ошибка проверки роли.");
    }
    cache_delete(c);
    if (rank >= gAdminLevel[playerid] && rank > 0)
        return SendClientMessage(playerid, ADMIN_ERROR, "Нельзя заблокировать администратора равного или старшего уровня.");

    new until = minutes == 0 ? 0 : gettime() + minutes * 60;
    new detail[160];
    format(detail, sizeof(detail), "%s: %d мин; %.90s", name, minutes, reason);
    if (!Admin_LogPlayer(playerid, "OFFLINE_BAN_REQUEST", detail))
        return SendClientMessage(playerid, ADMIN_ERROR, "Аудит недоступен. Блокировка отменена.");
    mysql_format(gDBPrimary, q, sizeof(q),
        "INSERT INTO bans (username,banned_until,reason) VALUES (LOWER('%e'),%d,'%e') ON DUPLICATE KEY UPDATE banned_until=VALUES(banned_until),reason=VALUES(reason)",
        name, until, reason);
    if (!DBPrimary_Execute(q, affected))
        return SendClientMessage(playerid, ADMIN_ERROR, "Ошибка MariaDB. Блокировка не применена.");
    return SendClientMessage(playerid, ADMIN_OK, "Оффлайн-аккаунт заблокирован.");
}
