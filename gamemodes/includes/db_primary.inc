#if defined _tarkenso_db_primary_included
    #endinput
#endif
#define _tarkenso_db_primary_included

// DB-MIGRATION-02B: SQL-first account storage (opt-in for closed UAT).
#if defined TARKENSO_DB_PRIMARY
    #if defined TARKENSO_DB_SHADOW
        #error "TARKENSO_DB_PRIMARY and TARKENSO_DB_SHADOW cannot run together"
    #endif
    #define MYSQL_INVALID_HANDLE MySQL:0
    native MySQL:mysql_connect_file(const file_name[] = "mysql.ini");
    native mysql_close(MySQL:handle);
    native mysql_errno(MySQL:handle);
    native Cache:mysql_query(MySQL:handle, const query[], bool:use_cache = true);
    native mysql_tquery(MySQL:handle, const query[], const callback[] = "", const format[] = "", {MySQL, Float, _}:...);
    native mysql_format(MySQL:handle, output[], max_len, const format[], {MySQL, Float, _}:...);
    native cache_get_row_count(&destination);
    native cache_get_value_name_int(row_idx, const column_name[], &destination);
    native cache_get_value_name_float(row_idx, const column_name[], &Float:destination);
    native cache_get_value_name(row_idx, const column_name[], destination[], max_len = sizeof(destination));
    native cache_affected_rows();
    native cache_insert_id();
    native cache_delete(Cache:cache_id);

    new MySQL:gDBPrimary = MYSQL_INVALID_HANDLE;
    new bool:gDBPrimaryReady;
    new gDBPrimaryAccountId[MAX_PLAYERS];
    new gDBPrimaryVersion[MAX_PLAYERS];
    new bool:gDBPrimaryLoaded[MAX_PLAYERS];
    new gDBPrimaryRequestName[MAX_PLAYERS][MAX_PLAYER_NAME];
    // YYYY-MM-DD taken from accounts.created_at, never guessed from login time.
    new gDBPrimaryRegistrationDate[MAX_PLAYERS][11];
    #define DB_PRIMARY_AUTH_TIMEOUT_MS (30000)
    #define DB_PRIMARY_RECOVERY_INTERVAL_MS (15000)
    new bool:gDBPrimaryAuthPending[MAX_PLAYERS];
    new bool:gDBPrimaryDegraded;
    new bool:gDBPrimaryProbePending;
    new gDBPrimaryProbeSent;
    new gDBPrimaryRecoveryTimer;
    forward DBPrimary_AccountTimeout(playerid, token);
    forward DBPrimary_RecoveryTick();
    forward DBPrimary_ProbeDone();

    forward DBPrimary_OnAccount(playerid, token, fromIntro);
    forward OnQueryError(errorid, const error[], const callback[], const query[], MySQL:handle);

    stock bool:DBPrimary_IsReady()
    {
        return gDBPrimaryReady && gDBPrimary != MYSQL_INVALID_HANDLE;
    }

    stock DBPrimary_OnConnect(playerid)
    {
        gDBPrimaryAccountId[playerid] = 0;
        gDBPrimaryVersion[playerid] = 0;
        gDBPrimaryLoaded[playerid] = false;
        gDBPrimaryRequestName[playerid][0] = EOS;
        gDBPrimaryRegistrationDate[playerid][0] = EOS;
        gDBPrimaryAuthPending[playerid] = false;
        return 1;
    }

    stock DBPrimary_OnDisconnect(playerid)
    {
        return DBPrimary_OnConnect(playerid);
    }

    stock DBPrimary_Init()
    {
        print("[DB-PRIMARY] Opening mysql.ini and connecting to database...");
        gDBPrimary = mysql_connect_file("mysql.ini");
        if (gDBPrimary == MYSQL_INVALID_HANDLE)
        {
            print("[DB-PRIMARY] ERROR: mysql_connect_file returned invalid handle; inspect mysql.ini and mysql plugin log.");
            return 0;
        }
        new code = mysql_errno(gDBPrimary);
        if (code != 0)
        {
            printf("[DB-PRIMARY] ERROR: mysql_connect_file failed; MySQL errno=%d. Check DB host, port, credentials and access grants.", code);
            mysql_close(gDBPrimary);
            gDBPrimary = MYSQL_INVALID_HANDLE;
            return 0;
        }
        print("[DB-PRIMARY] Connected. Checking schema_migrations version 10...");
        gDBPrimaryReady = true;
        new Cache:c = mysql_query(gDBPrimary, "SELECT version FROM schema_migrations WHERE version=10 LIMIT 1");
        if (c == Cache:0)
        {
            printf("[DB-PRIMARY] ERROR: schema query returned invalid cache; MySQL errno=%d.", mysql_errno(gDBPrimary));
            gDBPrimaryReady = false;
            return 0;
        }
        code = mysql_errno(gDBPrimary);
        if (code != 0)
        {
            printf("[DB-PRIMARY] ERROR: schema query failed; MySQL errno=%d.", code);
            cache_delete(c);
            gDBPrimaryReady = false;
            return 0;
        }
        new rows;
        if (!cache_get_row_count(rows))
        {
            print("[DB-PRIMARY] ERROR: cannot read row count for schema version query.");
            cache_delete(c);
            gDBPrimaryReady = false;
            return 0;
        }
        if (rows != 1)
        {
            printf("[DB-PRIMARY] ERROR: expected schema version 10, rows found=%d.", rows);
            cache_delete(c);
            gDBPrimaryReady = false;
            return 0;
        }
        cache_delete(c);
        print("[DB-PRIMARY] MySQL schema v10 detected; editor containers and punishments enabled.");
        return 1;
    }

    // The existing MySQL R41 handle can recover asynchronously when
    // auto_reconnect is configured; the game thread never opens a replacement
    // connection from the periodic watchdog.
    stock DBPrimary_StartRecoveryMonitor()
    {
        gDBPrimaryDegraded = false;
        gDBPrimaryProbePending = false;
        gDBPrimaryProbeSent = 0;
        gDBPrimaryRecoveryTimer = SetTimer("DBPrimary_RecoveryTick",
            DB_PRIMARY_RECOVERY_INTERVAL_MS, true);
        if (!gDBPrimaryRecoveryTimer)
            print("[DB-PRIMARY] WARNING: recovery monitor could not start.");
        return 1;
    }

    public DBPrimary_RecoveryTick()
    {
        if (!gDBPrimaryDegraded || gDBPrimary == MYSQL_INVALID_HANDLE)
            return 1;
        if (gDBPrimaryProbePending)
        {
            if (gettime() - gDBPrimaryProbeSent < 45) return 1;
            gDBPrimaryProbePending = false;
        }
        // On an outage, the probe is queued instead of synchronously blocking
        // a game tick. Network-reconnect support depends on mysql.ini.
        if (mysql_tquery(gDBPrimary,
            "SELECT version FROM schema_migrations WHERE version=10 LIMIT 1",
            "DBPrimary_ProbeDone"))
        {
            gDBPrimaryProbePending = true;
            gDBPrimaryProbeSent = gettime();
        }
        return 1;
    }

    public DBPrimary_ProbeDone()
    {
        gDBPrimaryProbePending = false;
        if (!gDBPrimaryDegraded) return 1;
        new rows;
        if (!cache_get_row_count(rows) || rows != 1)
        {
            print("[DB-PRIMARY] Recovery probe rejected: missing schema version 10.");
            return 1;
        }
        gDBPrimaryReady = true;
        gDBPrimaryDegraded = false;
        print("[DB-PRIMARY] Database connectivity restored; writes are enabled.");
        for (new i = 0; i < MAX_PLAYERS; i++)
            if (IsPlayerConnected(i) && !IsPlayerNPC(i) &&
                gAuthState[i] == AUTH_READY)
                SendClientMessage(i, COLOR_AMBER,
                    "Связь с базой восстановлена. Проверяйте сохранение прогресса.");
        return 1;
    }

    public DBPrimary_AccountTimeout(playerid, token)
    {
        if (!IsPlayerConnected(playerid) || IsPlayerNPC(playerid) ||
            token != gAuthToken[playerid] || !gDBPrimaryAuthPending[playerid])
            return 1;
        gDBPrimaryAuthPending[playerid] = false;
        SendClientMessage(playerid, COLOR_RED,
            "База данных не ответила при входе. Повторите подключение позднее.");
        Kick(playerid);
        return 1;
    }

    stock DBPrimary_Shutdown()
    {
        if (gDBPrimaryRecoveryTimer) KillTimer(gDBPrimaryRecoveryTimer);
        gDBPrimaryRecoveryTimer = 0;
        gDBPrimaryReady = false;
        if (gDBPrimary != MYSQL_INVALID_HANDLE)
        {
            mysql_close(gDBPrimary);
            gDBPrimary = MYSQL_INVALID_HANDLE;
        }
        return 1;
    }

    // Synchronous writes preserve the old bool/rollback economy contract.
    // This blocks the server tick: use only for closed beta until async redesign.
    stock bool:DBPrimary_Execute(const query[], &affected)
    {
        affected = 0;
        if (!DBPrimary_IsReady()) return false;
        new Cache:c = mysql_query(gDBPrimary, query);
        if (c == Cache:0 || mysql_errno(gDBPrimary))
        {
            if (c != Cache:0) cache_delete(c);
            print("[DB-PRIMARY] Operation not persisted due to SQL error.");
            return false;
        }
        affected = cache_affected_rows();
        cache_delete(c);
        return true;
    }

    stock bool:DBPrimary_AccountExists(playerid)
    {
        if (!DBPrimary_IsReady()) return true;
        new q[180], rows;
        mysql_format(gDBPrimary, q, sizeof(q),
            "SELECT account_id FROM accounts WHERE username_key=LOWER('%e') LIMIT 1", gAccountName[playerid]);
        new Cache:c = mysql_query(gDBPrimary, q);
        if (c == Cache:0 || mysql_errno(gDBPrimary) || !cache_get_row_count(rows))
        {
            if (c != Cache:0) cache_delete(c);
            return true; // fail closed: never overwrite another account
        }
        cache_delete(c);
        return rows != 0;
    }

    stock DBPrimary_BeginAccount(playerid, bool:fromIntro = false)
    {
        if (!DBPrimary_IsReady()) { Kick(playerid); return 0; }
        gAuthState[playerid] = AUTH_VERIFYING; // block dialogs until callback
        format(gDBPrimaryRequestName[playerid], MAX_PLAYER_NAME, "%s", gAccountName[playerid]);
        new q[1400];
        mysql_format(gDBPrimary, q, sizeof(q),
            "SELECT account_id,username,password_hash,background,mute_until,balance,health,armour,cleaner_paid,txn_serial,race_starts,race_finishes,race_wins,race_best,race_earned,race_last_paid,start_region,character_level,character_exp,last_payday_hour,story_stage,row_version,DATE(created_at) AS registered_date FROM accounts WHERE username_key=LOWER('%e') LIMIT 1", gAccountName[playerid]);
        gDBPrimaryAuthPending[playerid] = true;
        if (!mysql_tquery(gDBPrimary, q, "DBPrimary_OnAccount", "iii",
            playerid, gAuthToken[playerid], _:fromIntro))
        {
            gDBPrimaryAuthPending[playerid] = false;
            Kick(playerid);
            return 0;
        }
        SetTimerEx("DBPrimary_AccountTimeout", DB_PRIMARY_AUTH_TIMEOUT_MS,
            false, "ii", playerid, gAuthToken[playerid]);
        return 1;
    }

    public DBPrimary_OnAccount(playerid, token, fromIntro)
    {
        if (IsPlayerConnected(playerid) && token == gAuthToken[playerid])
            gDBPrimaryAuthPending[playerid] = false;
        if (!IsPlayerConnected(playerid) || IsPlayerNPC(playerid) ||
            token != gAuthToken[playerid] || gAuthState[playerid] != AUTH_VERIFYING ||
            strcmp(gDBPrimaryRequestName[playerid], gAccountName[playerid], false))
            return 1;
        if (!DBPrimary_IsReady()) { Kick(playerid); return 1; }
        new rows;
        if (!cache_get_row_count(rows) || rows > 1) { Kick(playerid); return 1; }
        if (!rows)
        {
            // Existing disk account not imported: NEVER silently re-register it.
            new legacyPath[96], legacyBackup[112];
            Auth_AccountPath(playerid, legacyPath, sizeof(legacyPath));
            format(legacyBackup, sizeof(legacyBackup), "%s.bak", legacyPath);
            if (fexist(legacyPath) || fexist(legacyBackup))
            {
                print("[DB-PRIMARY] File account missing its SQL row; migration required.");
                SendClientMessage(playerid, COLOR_RED, "Аккаунт ожидает переноса в MySQL. Обратитесь к администрации.");
                Kick(playerid);
                return 1;
            }
            if (!strcmp(gAccountName[playerid], FOUNDER_NAME, true))
            {
                SendClientMessage(playerid, COLOR_RED, "Регистрация имени основателя запрещена.");
                Kick(playerid);
            }
            else if (fromIntro) Auth_ShowRegister(playerid);
            else Auth_ShowIntro(playerid);
            return 1;
        }
        new id, background, muted, balance, paid, serial, starts, finishes;
        new wins, best, earned, lastPaid, region, version, hash[61], savedName[MAX_PLAYER_NAME];
        new characterLevel, characterExp, lastPaydayHour, storyStage, registered[11];
        new Float:health, Float:armour;
        new bool:valid = true;
        if (!cache_get_value_name_int(0, "account_id", id) ||
            !cache_get_value_name(0, "username", savedName, sizeof(savedName)) ||
            !cache_get_value_name(0, "password_hash", hash, sizeof(hash)) ||
            !cache_get_value_name_int(0, "background", background) ||
            !cache_get_value_name_int(0, "mute_until", muted) ||
            !cache_get_value_name_int(0, "balance", balance) ||
            !cache_get_value_name_float(0, "health", health) ||
            !cache_get_value_name_float(0, "armour", armour) ||
            !cache_get_value_name_int(0, "cleaner_paid", paid) ||
            !cache_get_value_name_int(0, "txn_serial", serial) ||
            !cache_get_value_name_int(0, "race_starts", starts) ||
            !cache_get_value_name_int(0, "race_finishes", finishes) ||
            !cache_get_value_name_int(0, "race_wins", wins) ||
            !cache_get_value_name_int(0, "race_best", best) ||
            !cache_get_value_name_int(0, "race_earned", earned) ||
            !cache_get_value_name_int(0, "race_last_paid", lastPaid) ||
            !cache_get_value_name_int(0, "start_region", region) ||
            !cache_get_value_name_int(0, "character_level", characterLevel) ||
            !cache_get_value_name_int(0, "character_exp", characterExp) ||
            !cache_get_value_name_int(0, "last_payday_hour", lastPaydayHour) ||
            !cache_get_value_name_int(0, "story_stage", storyStage) ||
            !cache_get_value_name(0, "registered_date", registered, sizeof(registered)) ||
            !cache_get_value_name_int(0, "row_version", version))
            valid = false;
        if (id <= 0 || version < 0 || strlen(hash) != 60 ||
            strcmp(savedName, gAccountName[playerid], true) != 0 ||
            hash[0] != '$' || hash[1] != '2' ||
            background < 1 || background > 3 ||
            region < REGION_FORT_CARSON || region > REGION_PALOMINO || balance < 0 || balance > ACCOUNT_MAX_MONEY ||
            paid < -1 || paid > CLEANER_MAX_TOTAL || muted < 0 || serial < 0 ||
            starts < 0 || finishes < 0 || wins < 0 || best < 0 || best > RACE_TIME_LIMIT ||
            earned < 0 || lastPaid < 0 || finishes > starts || wins > finishes ||
            (finishes == 0 && best != 0) || (finishes > 0 && best == 0) ||
            (earned == 0 && lastPaid != 0) || (earned > 0 && lastPaid == 0) ||
            health < 1.0 || health > 100.0 || armour < 0.0 || armour > 100.0 ||
            characterLevel < 1 || characterLevel > PAYDAY_LEVEL_LIMIT ||
            characterExp < 0 || (characterLevel < PAYDAY_LEVEL_LIMIT &&
                characterExp >= characterLevel * PAYDAY_EXP_STEP) ||
            lastPaydayHour < 0 || storyStage < 0 || storyStage > 4 ||
            strlen(registered) != 10 ||
            registered[4] != '-' || registered[7] != '-')
            valid = false;
        if (!valid)
        {
            print("[DB-PRIMARY] Invalid MySQL account row; login denied.");
            Kick(playerid);
            return 1;
        }
        format(gAccountName[playerid], MAX_PLAYER_NAME, "%s", savedName);
        gDBPrimaryAccountId[playerid] = id;
        gDBPrimaryVersion[playerid] = version;
        gDBPrimaryLoaded[playerid] = true;
        format(gAccountHash[playerid], sizeof(gAccountHash[]), "%s", hash);
        gBackground[playerid] = background;
        gStartRegion[playerid] = region;
        gAdminMuteUntil[playerid] = muted > gettime() ? muted : 0;
        gAccountMoney[playerid] = balance;
        gAccountHealth[playerid] = health;
        gAccountArmour[playerid] = armour;
        gAccountCleanerPaid[playerid] = paid;
        gAccountTxnSerial[playerid] = serial;
        gAccountRaceStarts[playerid] = starts;
        gAccountRaceFinishes[playerid] = finishes;
        gAccountRaceWins[playerid] = wins;
        gAccountRaceBest[playerid] = best;
        gAccountRaceEarned[playerid] = earned;
        gAccountRaceLastPaid[playerid] = lastPaid;
        gCharacterLevel[playerid] = characterLevel;
        gCharacterEXP[playerid] = characterExp;
        gCharacterLastPaydayHour[playerid] = lastPaydayHour;
        gStoryStage[playerid] = storyStage;
        format(gDBPrimaryRegistrationDate[playerid],
            sizeof(gDBPrimaryRegistrationDate[]), "%s", registered);
        gAccountLegacy[playerid] = false;
        if (Auth_IsLocked(playerid)) { Kick(playerid); return 1; }
        Auth_ShowLogin(playerid);
        return 1;
    }

    stock bool:DBPrimary_WriteAccount(playerid, bool:create = false)
    {
        if (!DBPrimary_IsReady() || !gAccountName[playerid][0] ||
            strlen(gAccountHash[playerid]) != 60 || gBackground[playerid] < 1 ||
            gBackground[playerid] > 3 ||
            gStartRegion[playerid] < REGION_FORT_CARSON ||
            gStartRegion[playerid] > REGION_PALOMINO || gAccountMoney[playerid] < 0 ||
            gAccountMoney[playerid] > ACCOUNT_MAX_MONEY ||
            gAccountCleanerPaid[playerid] < -1 || gAccountCleanerPaid[playerid] > CLEANER_MAX_TOTAL ||
            gAccountTxnSerial[playerid] < 0 || gAccountTxnSerial[playerid] > ACCOUNT_MAX_MONEY ||
            gAccountRaceStarts[playerid] < 0 || gAccountRaceFinishes[playerid] < 0 ||
            gAccountRaceWins[playerid] < 0 || gAccountRaceBest[playerid] < 0 ||
            gAccountRaceBest[playerid] > RACE_TIME_LIMIT ||
            gAccountRaceFinishes[playerid] > gAccountRaceStarts[playerid] ||
            gAccountRaceWins[playerid] > gAccountRaceFinishes[playerid] ||
            (gAccountRaceFinishes[playerid] == 0 && gAccountRaceBest[playerid] != 0) ||
            (gAccountRaceFinishes[playerid] > 0 && gAccountRaceBest[playerid] == 0) ||
            gAccountRaceEarned[playerid] < 0 || gAccountRaceEarned[playerid] > ACCOUNT_MAX_MONEY ||
            gAccountRaceLastPaid[playerid] < 0 ||
            (gAccountRaceEarned[playerid] == 0 && gAccountRaceLastPaid[playerid] != 0) ||
            (gAccountRaceEarned[playerid] > 0 && gAccountRaceLastPaid[playerid] == 0))
            return false;
        new q[1100], affected;
        if (create)
        {
            if (!strcmp(gAccountName[playerid], FOUNDER_NAME, true) || gDBPrimaryLoaded[playerid])
                return false;
            mysql_format(gDBPrimary, q, sizeof(q),
                "INSERT INTO accounts (username,password_hash,background,start_region) VALUES ('%e','%e',%d,%d)",
                gAccountName[playerid], gAccountHash[playerid], gBackground[playerid], gStartRegion[playerid]);
            if (!DBPrimary_Execute(q, affected) || affected != 1) return false;
            new Cache:c = mysql_query(gDBPrimary,
                "SELECT account_id AS id,DATE(created_at) AS registered_date FROM accounts WHERE account_id=LAST_INSERT_ID()");
            new rows;
            if (c == Cache:0 || mysql_errno(gDBPrimary) ||
                !cache_get_row_count(rows) || rows != 1 ||
                !cache_get_value_name_int(0, "id", gDBPrimaryAccountId[playerid]) ||
                !cache_get_value_name(0, "registered_date",
                    gDBPrimaryRegistrationDate[playerid],
                    sizeof(gDBPrimaryRegistrationDate[])) ||
                strlen(gDBPrimaryRegistrationDate[playerid]) != 10)
            {
                if (c != Cache:0) cache_delete(c);
                return false;
            }
            cache_delete(c);
            gDBPrimaryVersion[playerid] = 0;
            gDBPrimaryLoaded[playerid] = true;
            return true;
        }
        if (!gDBPrimaryLoaded[playerid] || gDBPrimaryAccountId[playerid] <= 0 ||
            gDBPrimaryVersion[playerid] >= 2147483647)
            return false;
        mysql_format(gDBPrimary, q, sizeof(q),
            "UPDATE accounts SET start_region=%d,mute_until=%d,balance=%d,health=%f,armour=%f,cleaner_paid=%d,txn_serial=%d,race_starts=%d,race_finishes=%d,race_wins=%d,race_best=%d,race_earned=%d,race_last_paid=%d,row_version=row_version+1 WHERE account_id=%d AND row_version=%d",
            gStartRegion[playerid], gAdminMuteUntil[playerid], gAccountMoney[playerid],
            gAccountHealth[playerid], gAccountArmour[playerid], gAccountCleanerPaid[playerid],
            gAccountTxnSerial[playerid], gAccountRaceStarts[playerid], gAccountRaceFinishes[playerid],
            gAccountRaceWins[playerid], gAccountRaceBest[playerid], gAccountRaceEarned[playerid],
            gAccountRaceLastPaid[playerid], gDBPrimaryAccountId[playerid], gDBPrimaryVersion[playerid]);
        if (!DBPrimary_Execute(q, affected) || affected != 1)
        {
            print("[DB-PRIMARY] UPDATE rejected: version conflict or SQL error.");
            return false;
        }
        gDBPrimaryVersion[playerid]++;
        return true;
    }


    stock bool:DBPrimary_LoadCleaner(playerid)
    {
        if (!DBPrimary_IsReady() || !gDBPrimaryLoaded[playerid]) return false;
        new q[200], rows;
        mysql_format(gDBPrimary, q, sizeof(q),
            "SELECT xp,collected,earned,night_completed,night_last_paid FROM cleaner_profiles WHERE account_id=%d LIMIT 1", gDBPrimaryAccountId[playerid]);
        new Cache:x = mysql_query(gDBPrimary, q);
        if (x == Cache:0 || mysql_errno(gDBPrimary) || !cache_get_row_count(rows))
        {
            if (x != Cache:0) cache_delete(x);
            return false;
        }
        if (rows == 0) { cache_delete(x); return true; }
        new xp, collected, earned, nights, last;
        new bool:ok =
            cache_get_value_name_int(0, "xp", xp) &&
            cache_get_value_name_int(0, "collected", collected) &&
            cache_get_value_name_int(0, "earned", earned) &&
            cache_get_value_name_int(0, "night_completed", nights) &&
            cache_get_value_name_int(0, "night_last_paid", last);
        cache_delete(x);
        if (!ok || xp < 0 || xp > CLEANER_MAX_TOTAL ||
            collected != xp || nights < 0 || earned < 0 ||
            earned > CLEANER_MAX_TOTAL ||
            earned != collected * CLEANER_PAY_PER_UNIT + nights * CLEANER_NIGHT_BONUS ||
            (nights == 0 && last != 0) || (nights > 0 && last <= 0))
            return false;
        gCleanerXP[playerid] = xp;
        gCleanerCollected[playerid] = collected;
        gCleanerEarned[playerid] = earned;
        gCleanerNightCompleted[playerid] = nights;
        gCleanerNightLastPaid[playerid] = last;
        return true;
    }

    stock bool:DBPrimary_SaveCleaner(playerid)
    {
        if (!DBPrimary_IsReady() || !gDBPrimaryLoaded[playerid] ||
            gCleanerCollected[playerid] < 0 || gCleanerCollected[playerid] > CLEANER_MAX_TOTAL ||
            gCleanerXP[playerid] != gCleanerCollected[playerid] ||
            gCleanerEarned[playerid] < 0 || gCleanerEarned[playerid] > CLEANER_MAX_TOTAL ||
            gCleanerNightCompleted[playerid] < 0 ||
            gCleanerEarned[playerid] != gCleanerCollected[playerid] * CLEANER_PAY_PER_UNIT +
               gCleanerNightCompleted[playerid] * CLEANER_NIGHT_BONUS)
            return false;
        new q[480], affected;
        mysql_format(gDBPrimary, q, sizeof(q),
            "INSERT INTO cleaner_profiles (account_id,xp,collected,earned,night_completed,night_last_paid) VALUES (%d,%d,%d,%d,%d,%d) ON DUPLICATE KEY UPDATE xp=VALUES(xp),collected=VALUES(collected),earned=VALUES(earned),night_completed=VALUES(night_completed),night_last_paid=VALUES(night_last_paid)",
            gDBPrimaryAccountId[playerid], gCleanerXP[playerid],
            gCleanerCollected[playerid], gCleanerEarned[playerid],
            gCleanerNightCompleted[playerid], gCleanerNightLastPaid[playerid]);
        return DBPrimary_Execute(q, affected);
    }

    stock bool:DBPrimary_ReadRole(playerid)
    {
        if (!DBPrimary_IsReady() || !gDBPrimaryLoaded[playerid]) return false;
        new q[180], rows;
        mysql_format(gDBPrimary, q, sizeof(q),
            "SELECT admin_level,event_role FROM admin_roles WHERE account_id=%d LIMIT 1", gDBPrimaryAccountId[playerid]);
        new Cache:x = mysql_query(gDBPrimary, q);
        if (x == Cache:0 || mysql_errno(gDBPrimary) || !cache_get_row_count(rows))
        {
            if (x != Cache:0) cache_delete(x);
            return false;
        }
        if (rows == 0)
        {
            cache_delete(x);
            gAdminLevel[playerid] = 0;
            gAdminEvent[playerid] = false;
            return true;
        }
        new level, eventRole;
        new bool:ok = cache_get_value_name_int(0, "admin_level", level) &&
            cache_get_value_name_int(0, "event_role", eventRole);
        cache_delete(x);
        if (!ok || level < 0 || level > 6 || eventRole < 0 || eventRole > 1)
            return false;
        gAdminLevel[playerid] = level;
        gAdminEvent[playerid] = bool:eventRole;
        return true;
    }

    stock bool:DBPrimary_WriteRole(playerid, level, bool:eventRole)
    {
        if (!DBPrimary_IsReady() || !gDBPrimaryLoaded[playerid] ||
            Admin_IsFounderName(gAccountName[playerid]) ||
            level < 0 || level > 6)
            return false;
        new q[300], affected;
        mysql_format(gDBPrimary, q, sizeof(q),
            "INSERT INTO admin_roles (account_id,admin_level,event_role) VALUES (%d,%d,%d) ON DUPLICATE KEY UPDATE admin_level=VALUES(admin_level),event_role=VALUES(event_role)",
            gDBPrimaryAccountId[playerid], level, _:eventRole);
        return DBPrimary_Execute(q, affected);
    }

    stock bool:DBPrimary_BanExists(const name[])
    {
        if (!DBPrimary_IsReady()) return true;
        new q[150], rows;
        mysql_format(gDBPrimary, q, sizeof(q),
            "SELECT username FROM bans WHERE LOWER(username)=LOWER('%e') LIMIT 1", name);
        new Cache:x = mysql_query(gDBPrimary, q);
        if (x == Cache:0 || mysql_errno(gDBPrimary) || !cache_get_row_count(rows))
        {
            if (x != Cache:0) cache_delete(x);
            return true; // fail closed
        }
        cache_delete(x);
        return rows != 0;
    }

    stock bool:DBPrimary_UnbanName(const name[])
    {
        if (!DBPrimary_IsReady()) return false;
        new q[180], affected;
        mysql_format(gDBPrimary, q, sizeof(q),
            "DELETE FROM bans WHERE LOWER(username)=LOWER('%e')", name);
        return DBPrimary_Execute(q, affected) && affected == 1;
    }

    stock bool:DBPrimary_IsBanned(playerid)
    {
        if (!DBPrimary_IsReady()) return true;
        new q[190], rows, until;
        mysql_format(gDBPrimary, q, sizeof(q),
            "SELECT banned_until FROM bans WHERE LOWER(username)=LOWER('%e') LIMIT 1", gAccountName[playerid]);
        new Cache:x = mysql_query(gDBPrimary, q);
        if (x == Cache:0 || mysql_errno(gDBPrimary) || !cache_get_row_count(rows))
        {
            if (x != Cache:0) cache_delete(x);
            return true;
        }
        if (rows == 1 && !cache_get_value_name_int(0, "banned_until", until))
        {
            cache_delete(x);
            return true;
        }
        cache_delete(x);
        if (rows == 0) return false;
        if (until > 0 && until <= gettime())
            return !DBPrimary_UnbanName(gAccountName[playerid]);
        return true;
    }

    stock bool:DBPrimary_WriteBan(playerid, until, const reason[])
    {
        if (!DBPrimary_IsReady() || !gDBPrimaryLoaded[playerid] ||
            Admin_IsFounderName(gAccountName[playerid]) || until < 0)
            return false;
        new q[480], affected;
        mysql_format(gDBPrimary, q, sizeof(q),
            "INSERT INTO bans (username,banned_until,reason) VALUES (LOWER('%e'),%d,'%e') ON DUPLICATE KEY UPDATE banned_until=VALUES(banned_until),reason=VALUES(reason)",
            gAccountName[playerid], until, reason);
        return DBPrimary_Execute(q, affected);
    }

    public OnQueryError(errorid, const error[], const callback[], const query[], MySQL:handle)
    {
        // Do not log the SQL or error text: password hashes may be embedded.
        #pragma unused error, callback, query, handle
        printf("[DB-PRIMARY] Asynchronous SQL failed (code %d).", errorid);
        if (errorid == 2002 || errorid == 2003 || errorid == 2006 ||
            errorid == 2013 || errorid == 2055)
        {
            gDBPrimaryProbePending = false;
            if (!gDBPrimaryDegraded)
            {
                print("[DB-PRIMARY] Connection lost: writes suspended; attempting asynchronous probes.");
                for (new i = 0; i < MAX_PLAYERS; i++)
                    if (IsPlayerConnected(i) && !IsPlayerNPC(i) &&
                        gAuthState[i] == AUTH_READY)
                        SendClientMessage(i, COLOR_RED,
                            "Соединение с базой потеряно: сохранение временно недоступно.");
            }
            gDBPrimaryDegraded = true;
            gDBPrimaryReady = false;
        }
        // SQL syntax and constraint failures must not disconnect unrelated players.
        return 1;
    }
#else
    stock DBPrimary_Init() { return 1; }
    stock DBPrimary_StartRecoveryMonitor() { return 1; }
    stock DBPrimary_Shutdown() { return 1; }
    stock DBPrimary_OnConnect(playerid)
    {
        #pragma unused playerid
        return 1;
    }
    stock DBPrimary_OnDisconnect(playerid)
    {
        #pragma unused playerid
        return 1;
    }
#endif
