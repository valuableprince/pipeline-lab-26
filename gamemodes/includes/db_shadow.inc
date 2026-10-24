#if defined _tarkenso_db_shadow_included
    #endinput
#endif
#define _tarkenso_db_shadow_included

// DB-MIGRATION-02A. Необязательный асинхронный READ-ONLY адаптер.
// В обычной сборке нет MySQL-natives и не требуется плагин.
// Для тестовой сборки определить TARKENSO_DB_SHADOW при компиляции.
// Учетные данные находятся только в mysql.ini (корень игрового сервера) (вне Git).

#if defined TARKENSO_DB_SHADOW
    #define DB_SHADOW_ENABLED (1)
    // Совместимые объявления из pBlueG SA-MP-MySQL R41 (только используемые функции).
    // Не подключать совместно с a_mysql.inc во избежание дублирования natives.
    #define MYSQL_INVALID_HANDLE MySQL:0
    native MySQL:mysql_connect_file(const file_name[] = "mysql.ini");
    native mysql_close(MySQL:handle);
    native mysql_errno(MySQL:handle);
    native mysql_tquery(MySQL:handle, const query[], const callback[] = "",
        const format[] = "", {MySQL, Float, _}:...);
    native cache_get_row_count(&destination);
    native cache_get_value_name_int(row_idx, const column_name[], &destination);
    native cache_get_value_name_float(row_idx, const column_name[], &Float:destination);
    native cache_get_value_name(row_idx, const column_name[], destination[], max_len = sizeof(destination));

    new MySQL:gDBShadowHandle = MYSQL_INVALID_HANDLE;
    new bool:gDBShadowReady;
    new gDBShadowToken[MAX_PLAYERS];
    new gDBShadowName[MAX_PLAYERS][MAX_PLAYER_NAME];
    forward DBShadow_OnAccountResult(playerid, token);

    stock DBShadow_Init()
    {
        gDBShadowHandle = mysql_connect_file("mysql.ini");
        if (gDBShadowHandle == MYSQL_INVALID_HANDLE ||
            mysql_errno(gDBShadowHandle) != 0)
        {
            print("[DB-SHADOW] MySQL connection failed; file-based authentication remains active.");
            gDBShadowReady = false;
            return 0;
        }
        gDBShadowReady = true;
        print("[DB-SHADOW] READ ONLY: asynchronous account comparison enabled.");
        return 1;
    }

    stock DBShadow_Shutdown()
    {
        gDBShadowReady = false;
        if (gDBShadowHandle != MYSQL_INVALID_HANDLE)
        {
            mysql_close(gDBShadowHandle);
            gDBShadowHandle = MYSQL_INVALID_HANDLE;
        }
        return 1;
    }

    stock DBShadow_OnConnect(playerid)
    {
        gDBShadowToken[playerid]++;
        gDBShadowName[playerid][0] = EOS;
        return 1;
    }

    stock DBShadow_OnDisconnect(playerid)
    {
        gDBShadowToken[playerid]++;
        gDBShadowName[playerid][0] = EOS;
        return 1;
    }

    stock DBShadow_CheckAccount(playerid)
    {
        if (!gDBShadowReady || !IsPlayerConnected(playerid) ||
            IsPlayerNPC(playerid) || gAccountName[playerid][0] == EOS)
            return 0;
        // Ник строго нормализован [a-z0-9_]{3,20} в Auth_SetAccountName:
        // никаких пользовательских фрагментов SQL, кроме этого ключа.
        new query[256];
        format(gDBShadowName[playerid], MAX_PLAYER_NAME, "%s", gAccountName[playerid]);
        format(query, sizeof(query), "SELECT balance,txn_serial,mute_until,cleaner_paid,race_starts,race_finishes,race_wins,race_best,race_earned,race_last_paid,password_hash FROM accounts WHERE username='%s' LIMIT 1", gAccountName[playerid]);
        if (!mysql_tquery(gDBShadowHandle, query, "DBShadow_OnAccountResult",
            "ii", playerid, gDBShadowToken[playerid]))
        {
            print("[DB-SHADOW] Query enqueue failed; account files unchanged.");
            return 0;
        }
        return 1;
    }

    public DBShadow_OnAccountResult(playerid, token)
    {
        // Защита от поздних ответов после disconnect и повторного занятия playerid.
        if (!IsPlayerConnected(playerid) || IsPlayerNPC(playerid) ||
            token != gDBShadowToken[playerid] ||
            strcmp(gDBShadowName[playerid], gAccountName[playerid], false))
            return 1;
        new count = 0;
        if (!cache_get_row_count(count) || count != 1)
        {
            printf("[DB-SHADOW] Account %s has no unique database row.", gDBShadowName[playerid]);
            return 1;
        }
        new balance, serial, mute, paid, starts, finishes, wins, best, earned, last;
        new hash[65];
        cache_get_value_name_int(0, "balance", balance);
        cache_get_value_name_int(0, "txn_serial", serial);
        cache_get_value_name_int(0, "mute_until", mute);
        cache_get_value_name_int(0, "cleaner_paid", paid);
        cache_get_value_name_int(0, "race_starts", starts);
        cache_get_value_name_int(0, "race_finishes", finishes);
        cache_get_value_name_int(0, "race_wins", wins);
        cache_get_value_name_int(0, "race_best", best);
        cache_get_value_name_int(0, "race_earned", earned);
        cache_get_value_name_int(0, "race_last_paid", last);
        cache_get_value_name(0, "password_hash", hash, sizeof(hash));

        // Лишь отчёт о рассогласовании, никаких обновлений памяти/файлов/БД.
        if (balance != gAccountMoney[playerid] ||
            serial != gAccountTxnSerial[playerid] ||
            mute != gAdminMuteUntil[playerid] ||
            paid != gAccountCleanerPaid[playerid] ||
            starts != gAccountRaceStarts[playerid] ||
            finishes != gAccountRaceFinishes[playerid] ||
            wins != gAccountRaceWins[playerid] ||
            best != gAccountRaceBest[playerid] ||
            earned != gAccountRaceEarned[playerid] ||
            last != gAccountRaceLastPaid[playerid] ||
            strcmp(hash, gAccountHash[playerid], false))
        {
            printf("[DB-SHADOW] Account snapshot mismatch for %s (values and hashes omitted).",
                gDBShadowName[playerid]);
        }
        return 1;
    }
#else
    // Полностью совместимая с текущей файловой сборкой заглушка.
    stock DBShadow_Init() { return 1; }
    stock DBShadow_Shutdown() { return 1; }
    stock DBShadow_OnConnect(playerid)
    {
        #pragma unused playerid
        return 1;
    }
    stock DBShadow_OnDisconnect(playerid)
    {
        #pragma unused playerid
        return 1;
    }
    stock DBShadow_CheckAccount(playerid)
    {
        #pragma unused playerid
        return 0;
    }
#endif
