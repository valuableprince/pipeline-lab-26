#if defined _tarkenso_crime_contracts_included
    #endinput
#endif
#define _tarkenso_crime_contracts_included

// CRIME-02.1: SQL-primary pilot. The DB owns state and unique reward settlement.
// No shared PvP objects, automatic payments or new timers in this phase.
#define CRIME_COLOR (0xC7C7C7FF)
#define CRIME_ERROR (0xE27E82FF)
#define CRIME_INTERACT_RADIUS (12.0)

new bool:gCrimeReady;
forward Crime_Settle(playerid, id, reward, faction);

stock Crime_Init()
{
    gCrimeReady = false;
#if defined TARKENSO_DB_PRIMARY
    if (!DBPrimary_IsReady()) return 1;
    new Cache:c = mysql_query(gDBPrimary,
        "SELECT version FROM schema_migrations WHERE version=32 LIMIT 1");
    if (c == Cache:0) return 1;
    new rows = 0;
    new bool:ok = mysql_errno(gDBPrimary) == 0 &&
        cache_get_row_count(rows) && rows == 1;
    cache_delete(c);
    if (!ok)
    {
        print("[CRIME-02] Migration 032 missing; contracts disabled.");
        return 1;
    }
    gCrimeReady = true;
    print("[CRIME-02] Solo contract core enabled (SQL-primary).");
#else
    print("[CRIME-02] Solo contracts need MySQL-primary.");
#endif
    return 1;
}

stock bool:Crime_Available(playerid)
{
#if defined TARKENSO_DB_PRIMARY
    return gCrimeReady && DBPrimary_IsReady() && gDBPrimaryLoaded[playerid] &&
        gDBPrimaryAccountId[playerid] > 0 &&
        gAuthState[playerid] == AUTH_READY && !IsPlayerNPC(playerid);
#else
    #pragma unused playerid
    return false;
#endif
}

#if defined TARKENSO_DB_PRIMARY
stock bool:Crime_Exec(const query[])
{
    new changed;
    return DBPrimary_Execute(query, changed);
}

stock Crime_Rollback()
{
    Crime_Exec("ROLLBACK");
    return 1;
}

stock bool:Crime_Expire(playerid)
{
    // Expire only this account's outstanding contract and journal once.
    // Unique active-account constraint is released in the same transaction.
    if (!Crime_Exec("START TRANSACTION")) return false;
    new q[520], affected;
    mysql_format(gDBPrimary, q, sizeof(q),
        "INSERT IGNORE INTO crime_contract_events(contract_id,actor_account_id,event_type) SELECT contract_id,account_id,'EXPIRED' FROM crime_contracts WHERE account_id=%d AND state IN(1,2) AND expires_at<=UNIX_TIMESTAMP()",
        gDBPrimaryAccountId[playerid]);
    if (!DBPrimary_Execute(q, affected))
    {
        Crime_Rollback();
        return false;
    }
    mysql_format(gDBPrimary, q, sizeof(q),
        "UPDATE crime_contracts SET state=4,finished_at=UNIX_TIMESTAMP() WHERE account_id=%d AND state IN(1,2) AND expires_at<=UNIX_TIMESTAMP()",
        gDBPrimaryAccountId[playerid]);
    if (!DBPrimary_Execute(q, affected))
    {
        Crime_Rollback();
        return false;
    }
    if (!Crime_Exec("COMMIT")) return false;
    return true;
}

stock bool:Crime_LoadActive(playerid, &id, &phase, &faction,
    &reward, &deadline, Float:pickup[], Float:delivery[], title[], titleSize)
{
    id = 0;
    new q[520], rows = 0;
    mysql_format(gDBPrimary, q, sizeof(q),
        "SELECT c.contract_id,c.state,c.faction_id,c.reward_amount,c.expires_at,t.title,t.pickup_x,t.pickup_y,t.pickup_z,t.delivery_x,t.delivery_y,t.delivery_z FROM crime_contracts c JOIN crime_contract_templates t ON t.template_id=c.template_id WHERE c.account_id=%d AND c.state IN(1,2) LIMIT 2",
        gDBPrimaryAccountId[playerid]);
    new Cache:c = mysql_query(gDBPrimary, q);
    if (c == Cache:0 || mysql_errno(gDBPrimary) ||
        !cache_get_row_count(rows) || rows > 1)
    {
        if (c != Cache:0) cache_delete(c);
        return false;
    }
    if (rows == 1)
    {
        new bool:ok = cache_get_value_name_int(0, "contract_id", id) &&
            cache_get_value_name_int(0, "state", phase) &&
            cache_get_value_name_int(0, "faction_id", faction) &&
            cache_get_value_name_int(0, "reward_amount", reward) &&
            cache_get_value_name_int(0, "expires_at", deadline) &&
            cache_get_value_name(0, "title", title, titleSize) &&
            cache_get_value_name_float(0, "pickup_x", pickup[0]) &&
            cache_get_value_name_float(0, "pickup_y", pickup[1]) &&
            cache_get_value_name_float(0, "pickup_z", pickup[2]) &&
            cache_get_value_name_float(0, "delivery_x", delivery[0]) &&
            cache_get_value_name_float(0, "delivery_y", delivery[1]) &&
            cache_get_value_name_float(0, "delivery_z", delivery[2]);
        if (!ok || id <= 0 || reward <= 0 || reward > 50000 ||
            deadline <= gettime() || (phase != 1 && phase != 2))
        {
            cache_delete(c);
            return false;
        }
    }
    cache_delete(c);
    return true;
}

stock bool:Crime_ReadWallet(playerid, &balance, &version, &serial)
{
    new q[180], rows;
    mysql_format(gDBPrimary, q, sizeof(q),
        "SELECT balance,row_version,txn_serial FROM accounts WHERE account_id=%d LIMIT 1",
        gDBPrimaryAccountId[playerid]);
    new Cache:c = mysql_query(gDBPrimary, q);
    if (c == Cache:0 || mysql_errno(gDBPrimary) ||
        !cache_get_row_count(rows) || rows != 1)
    {
        if (c != Cache:0) cache_delete(c);
        return false;
    }
    new bool:ok = cache_get_value_name_int(0, "balance", balance) &&
        cache_get_value_name_int(0, "row_version", version) &&
        cache_get_value_name_int(0, "txn_serial", serial);
    cache_delete(c);
    return ok && balance >= 0 && balance <= ACCOUNT_MAX_MONEY &&
        version >= 0 && serial >= 0;
}
#endif

stock Crime_ShowOffer(playerid)
{
    if (!Crime_Available(playerid))
        return SendClientMessage(playerid, CRIME_ERROR,
            "Контракты недоступны: необходима MySQL-миграция 032.");
#if defined TARKENSO_DB_PRIMARY
    new Cache:c = mysql_query(gDBPrimary,
        "SELECT title,reward_amount,duration_seconds FROM crime_contract_templates WHERE template_id=1 AND mode_id=1 AND is_enabled=1");
    new rows, title[68], reward, duration;
    if (c == Cache:0 || mysql_errno(gDBPrimary) ||
        !cache_get_row_count(rows) || rows != 1 ||
        !cache_get_value_name(0, "title", title, sizeof(title)) ||
        !cache_get_value_name_int(0, "reward_amount", reward) ||
        !cache_get_value_name_int(0, "duration_seconds", duration))
    {
        if (c != Cache:0) cache_delete(c);
        return SendClientMessage(playerid, CRIME_ERROR, "Предложение недоступно.");
    }
    cache_delete(c);
    new message[160];
    format(message, sizeof(message), "Заказ: %s | Награда: $%d | Срок: %d мин.",
        title, reward, duration / 60);
    SendClientMessage(playerid, CRIME_COLOR, message);
    SendClientMessage(playerid, CRIME_COLOR,
        "Одиночный пробный рейс: /crime start | /crime status | /crime route.");
#endif
    return 1;
}

stock Crime_Status(playerid, bool:route = false)
{
    if (!Crime_Available(playerid))
        return SendClientMessage(playerid, CRIME_ERROR,
            "Контракты недоступны: необходима MySQL-миграция 032.");
#if defined TARKENSO_DB_PRIMARY
    if (!Crime_Expire(playerid))
        return SendClientMessage(playerid, CRIME_ERROR, "Ошибка обновления контрактов.");
    new id, phase, faction, reward, deadline, title[68];
    new Float:pickup[3], Float:delivery[3], msg[160];
    if (!Crime_LoadActive(playerid, id, phase, faction, reward, deadline,
        pickup, delivery, title, sizeof(title)))
        return SendClientMessage(playerid, CRIME_ERROR, "Не удалось прочитать контракт.");
    if (!id)
        return SendClientMessage(playerid, CRIME_COLOR,
            "Активного заказа нет. /crime offer, затем /crime start.");
    format(msg, sizeof(msg), "Контракт #%d: %s | %s | $%d | осталось %d мин.",
        id, title, phase == 1 ? ("Получите груз") : ("Доставьте груз"),
        reward, (deadline - gettime() + 59) / 60);
    SendClientMessage(playerid, CRIME_COLOR, msg);
    if (route)
    {
        new Float:x = phase == 1 ? pickup[0] : delivery[0],
            Float:y = phase == 1 ? pickup[1] : delivery[1],
            Float:z = phase == 1 ? pickup[2] : delivery[2];
        format(msg, sizeof(msg), "Точка: X %.1f, Y %.1f, Z %.1f | Радиус 12 м.",
            x, y, z);
        SendClientMessage(playerid, CRIME_COLOR, msg);
        SendClientMessage(playerid, CRIME_COLOR,
            phase == 1 ? ("На месте: /crime pickup") : ("На месте: /crime deliver"));
    }
#endif
    return 1;
}

stock Crime_Start(playerid)
{
    if (!Crime_Available(playerid))
        return SendClientMessage(playerid, CRIME_ERROR, "Контракты SQL недоступны.");
    new org = Leader_PlayerOrg(playerid);
    if (!Leader_IsCriminal(org))
        return SendClientMessage(playerid, CRIME_ERROR, "Только для сотрудников синдикатов 6-8.");
#if defined TARKENSO_DB_PRIMARY
    if (gGovTransferBusy[playerid] || !Crime_Expire(playerid))
        return SendClientMessage(playerid, CRIME_ERROR, "Операция временно недоступна.");
    if (!Crime_Exec("START TRANSACTION"))
        return SendClientMessage(playerid, CRIME_ERROR, "Нет соединения с базой.");
    new q[720], changed;
    mysql_format(gDBPrimary, q, sizeof(q),
        "INSERT INTO crime_contracts(template_id,account_id,faction_id,mode_id,reward_amount,started_at,expires_at) SELECT t.template_id,m.account_id,m.faction_id,t.mode_id,t.reward_amount,UNIX_TIMESTAMP(),UNIX_TIMESTAMP()+t.duration_seconds FROM crime_contract_templates t JOIN faction_members m ON m.account_id=%d AND m.faction_id=%d WHERE t.template_id=1 AND t.mode_id=1 AND t.is_enabled=1",
        gDBPrimaryAccountId[playerid], org);
    if (!DBPrimary_Execute(q, changed) || changed != 1)
    {
        Crime_Rollback();
        return SendClientMessage(playerid, CRIME_ERROR,
            "Нельзя принять контракт: проверьте членство и активные заказы.");
    }
    if (!Crime_Exec(
        "INSERT INTO crime_contract_events(contract_id,actor_account_id,event_type) SELECT contract_id,account_id,'ACCEPTED' FROM crime_contracts WHERE contract_id=LAST_INSERT_ID()"))
    {
        Crime_Rollback();
        return SendClientMessage(playerid, CRIME_ERROR, "Не удалось записать историю заказа.");
    }
    if (!Crime_Exec("COMMIT"))
        return SendClientMessage(playerid, CRIME_ERROR,
            "Статус принятия неизвестен. Используйте /crime status.");
    SendClientMessage(playerid, CRIME_COLOR, "Заказ принят. Отправляйтесь за грузом.");
    return Crime_Status(playerid, true);
#else
    return 1;
#endif
}

stock Crime_Advance(playerid, bool:deliveryStage)
{
    if (!Crime_Available(playerid)) return SendClientMessage(playerid, CRIME_ERROR,
        "Контракты SQL недоступны.");
#if defined TARKENSO_DB_PRIMARY
    if (gGovTransferBusy[playerid] || !Crime_Expire(playerid))
        return SendClientMessage(playerid, CRIME_ERROR, "Операция временно недоступна.");
    new id, phase, faction, reward, deadline, title[68];
    new Float:pickup[3], Float:drop[3];
    if (!Crime_LoadActive(playerid, id, phase, faction, reward, deadline,
        pickup, drop, title, sizeof(title)))
        return SendClientMessage(playerid, CRIME_ERROR, "Не удалось прочитать контракт.");
    if (!id || phase != (deliveryStage ? 2 : 1))
        return SendClientMessage(playerid, CRIME_ERROR,
            "Сейчас этот этап недоступен. Проверьте /crime status.");
    if (faction != Leader_PlayerOrg(playerid))
        return SendClientMessage(playerid, CRIME_ERROR,
            "Вы больше не сотрудник организации, принявшей контракт.");
    if (!IsPlayerInRangeOfPoint(playerid, CRIME_INTERACT_RADIUS,
        deliveryStage ? drop[0] : pickup[0],
        deliveryStage ? drop[1] : pickup[1],
        deliveryStage ? drop[2] : pickup[2]))
        return SendClientMessage(playerid, CRIME_ERROR,
            "Вы не у точки операции. Координаты: /crime route.");
    if (deliveryStage) return Crime_Settle(playerid, id, reward, faction);
    if (!Crime_Exec("START TRANSACTION"))
        return SendClientMessage(playerid, CRIME_ERROR, "Ошибка начала операции.");
    new q[440], changed;
    mysql_format(gDBPrimary, q, sizeof(q),
        "UPDATE crime_contracts SET state=2,picked_up_at=UNIX_TIMESTAMP() WHERE contract_id=%d AND account_id=%d AND faction_id=%d AND state=1 AND expires_at>UNIX_TIMESTAMP()",
        id, gDBPrimaryAccountId[playerid], faction);
    if (!DBPrimary_Execute(q, changed) || changed != 1)
    {
        Crime_Rollback();
        return SendClientMessage(playerid, CRIME_ERROR, "Груз уже выдан или срок истёк.");
    }
    mysql_format(gDBPrimary, q, sizeof(q),
        "INSERT INTO crime_contract_events(contract_id,actor_account_id,event_type) VALUES (%d,%d,'PICKED_UP')",
        id, gDBPrimaryAccountId[playerid]);
    if (!Crime_Exec(q))
    {
        Crime_Rollback();
        return SendClientMessage(playerid, CRIME_ERROR, "Ошибка журнала выдачи.");
    }
    if (!Crime_Exec("COMMIT"))
        return SendClientMessage(playerid, CRIME_ERROR,
            "Статус выдачи неизвестен. Проверьте /crime status.");
    SendClientMessage(playerid, CRIME_COLOR, "Груз получен. Перевезите его в точку передачи.");
    return Crime_Status(playerid, true);
#else
    #pragma unused deliveryStage
    return 1;
#endif
}

stock Crime_Settle(playerid, id, reward, faction)
{
#if defined TARKENSO_DB_PRIMARY
    if (reward < 1 || reward > 50000 ||
        gAccountMoney[playerid] > ACCOUNT_MAX_MONEY - reward ||
        gAccountTxnSerial[playerid] >= ACCOUNT_MAX_MONEY ||
        gDBPrimaryVersion[playerid] >= 2147483645 ||
        gGovTransferBusy[playerid])
        return SendClientMessage(playerid, CRIME_ERROR, "Кошелёк не готов к выплате.");
    // Persist every earlier balance change before a transaction-owned reward.
    if (!Auth_SaveAccount(playerid))
        return SendClientMessage(playerid, CRIME_ERROR,
            "Не удалось синхронизировать аккаунт. Награда не начислена.");
    gGovTransferBusy[playerid] = true;
    new oldVersion = gDBPrimaryVersion[playerid],
        oldSerial = gAccountTxnSerial[playerid], oldBalance = gAccountMoney[playerid];
    if (!Crime_Exec("START TRANSACTION"))
    {
        gGovTransferBusy[playerid] = false;
        return SendClientMessage(playerid, CRIME_ERROR, "Нет соединения с базой.");
    }
    new q[860], changed;
    // Account row + contract row remain in the same transaction. CAS rejects
    // stale account versions. On errors every step (including money) rolls back.
    mysql_format(gDBPrimary, q, sizeof(q),
        "UPDATE accounts a JOIN crime_contracts c ON c.account_id=a.account_id SET a.balance=a.balance+c.reward_amount,a.txn_serial=a.txn_serial+1,a.row_version=a.row_version+1 WHERE a.account_id=%d AND a.row_version=%d AND a.balance<=1000000000-c.reward_amount AND a.txn_serial<1000000000 AND c.contract_id=%d AND c.faction_id=%d AND c.state=2 AND c.expires_at>UNIX_TIMESTAMP() AND NOT EXISTS(SELECT 1 FROM crime_contract_rewards r WHERE r.contract_id=c.contract_id)",
        gDBPrimaryAccountId[playerid], oldVersion, id, faction);
    if (!DBPrimary_Execute(q, changed) || changed != 1)
    {
        Crime_Rollback();
        gGovTransferBusy[playerid] = false;
        return SendClientMessage(playerid, CRIME_ERROR,
            "Выплата отклонена: заказ истёк, уже оплачен или кошелёк изменился.");
    }
    mysql_format(gDBPrimary, q, sizeof(q),
        "UPDATE crime_contracts SET state=3,finished_at=UNIX_TIMESTAMP() WHERE contract_id=%d AND account_id=%d AND faction_id=%d AND state=2 AND expires_at>UNIX_TIMESTAMP()",
        id, gDBPrimaryAccountId[playerid], faction);
    if (!DBPrimary_Execute(q, changed) || changed != 1)
    {
        Crime_Rollback();
        gGovTransferBusy[playerid] = false;
        return SendClientMessage(playerid, CRIME_ERROR, "Завершение заказа отклонено.");
    }
    mysql_format(gDBPrimary, q, sizeof(q),
        "INSERT INTO crime_contract_rewards(contract_id,account_id,amount,balance_after,row_version_after,txn_serial_after) SELECT contract_id,%d,reward_amount,%d+reward_amount,%d+1,%d+1 FROM crime_contracts WHERE contract_id=%d AND state=3 AND account_id=%d",
        gDBPrimaryAccountId[playerid], oldBalance, oldVersion, oldSerial,
        id, gDBPrimaryAccountId[playerid]);
    if (!DBPrimary_Execute(q, changed) || changed != 1)
    {
        Crime_Rollback();
        gGovTransferBusy[playerid] = false;
        return SendClientMessage(playerid, CRIME_ERROR, "Не удалось сохранить выплату.");
    }
    mysql_format(gDBPrimary, q, sizeof(q),
        "INSERT INTO crime_contract_events(contract_id,actor_account_id,event_type) VALUES (%d,%d,'COMPLETED')",
        id, gDBPrimaryAccountId[playerid]);
    if (!Crime_Exec(q))
    {
        Crime_Rollback();
        gGovTransferBusy[playerid] = false;
        return SendClientMessage(playerid, CRIME_ERROR, "Ошибка записи результата.");
    }
    if (!Crime_Exec("COMMIT"))
    {
        // COMMIT may have happened even if the acknowledgement was lost.
        // Keep the wallet locked until a new login reloads its SQL version.
        return SendClientMessage(playerid, CRIME_ERROR,
            "Результат операции неизвестен. Перезайдите на сервер для сверки денег.");
    }
    new balance, version, serial;
    if (!Crime_ReadWallet(playerid, balance, version, serial) ||
        balance != oldBalance + reward || version != oldVersion + 1 ||
        serial != oldSerial + 1)
        return SendClientMessage(playerid, CRIME_ERROR,
            "Не удалось сверить выплату. Перезайдите на сервер.");
    gAccountMoney[playerid] = balance;
    gDBPrimaryVersion[playerid] = version;
    gAccountTxnSerial[playerid] = serial;
    gGovTransferBusy[playerid] = false;
    Economy_SyncHUD(playerid);
    new message[144];
    format(message, sizeof(message),
        "Контракт #%d завершён. Начислено $%d. Запись в журнале: /crime history.",
        id, reward);
    return SendClientMessage(playerid, CRIME_COLOR, message);
#else
    #pragma unused playerid, id, reward, faction
    return 1;
#endif
}

stock Crime_Abort(playerid)
{
    if (!Crime_Available(playerid)) return SendClientMessage(playerid, CRIME_ERROR,
        "Контракты SQL недоступны.");
#if defined TARKENSO_DB_PRIMARY
    if (gGovTransferBusy[playerid] || !Crime_Expire(playerid))
        return SendClientMessage(playerid, CRIME_ERROR, "Операция временно недоступна.");
    new id, phase, faction, reward, deadline, title[68];
    new Float:pickup[3], Float:delivery[3];
    if (!Crime_LoadActive(playerid, id, phase, faction, reward, deadline,
        pickup, delivery, title, sizeof(title)))
        return SendClientMessage(playerid, CRIME_ERROR, "Ошибка чтения контракта.");
    if (!id) return SendClientMessage(playerid, CRIME_ERROR, "Нет активного заказа.");
    if (!Crime_Exec("START TRANSACTION"))
        return SendClientMessage(playerid, CRIME_ERROR, "Ошибка начала операции.");
    new q[380], affected;
    mysql_format(gDBPrimary, q, sizeof(q),
        "UPDATE crime_contracts SET state=5,finished_at=UNIX_TIMESTAMP() WHERE contract_id=%d AND account_id=%d AND state IN(1,2)",
        id, gDBPrimaryAccountId[playerid]);
    if (!DBPrimary_Execute(q, affected) || affected != 1)
    {
        Crime_Rollback();
        return SendClientMessage(playerid, CRIME_ERROR, "Отмена не сохранена.");
    }
    mysql_format(gDBPrimary, q, sizeof(q),
        "INSERT INTO crime_contract_events(contract_id,actor_account_id,event_type) VALUES (%d,%d,'ABORTED')",
        id, gDBPrimaryAccountId[playerid]);
    if (!Crime_Exec(q))
    {
        Crime_Rollback();
        return SendClientMessage(playerid, CRIME_ERROR, "Ошибка журнала отмены.");
    }
    if (!Crime_Exec("COMMIT"))
        return SendClientMessage(playerid, CRIME_ERROR, "Проверьте статус операции.");
    return SendClientMessage(playerid, CRIME_COLOR, "Контракт отменён без выплаты.");
#else
    return 1;
#endif
}

stock Crime_History(playerid, bool:journal = false)
{
    if (!Crime_Available(playerid)) return SendClientMessage(playerid, CRIME_ERROR,
        "Контракты SQL недоступны.");
#if defined TARKENSO_DB_PRIMARY
    new q[430], rows, line[160];
    if (journal)
        mysql_format(gDBPrimary, q, sizeof(q),
            "SELECT e.contract_id,e.event_type FROM crime_contract_events e JOIN crime_contracts c ON c.contract_id=e.contract_id WHERE c.account_id=%d ORDER BY e.event_id DESC LIMIT 8",
            gDBPrimaryAccountId[playerid]);
    else
        mysql_format(gDBPrimary, q, sizeof(q),
            "SELECT contract_id,state,reward_amount FROM crime_contracts WHERE account_id=%d ORDER BY contract_id DESC LIMIT 6",
            gDBPrimaryAccountId[playerid]);
    new Cache:c = mysql_query(gDBPrimary, q);
    if (c == Cache:0 || mysql_errno(gDBPrimary) || !cache_get_row_count(rows))
    {
        if (c != Cache:0) cache_delete(c);
        return SendClientMessage(playerid, CRIME_ERROR, "Не удалось загрузить историю.");
    }
    SendClientMessage(playerid, CRIME_COLOR,
        journal ? ("Последние события контрактов:") : ("Последние контракты:"));
    for (new i; i < rows; i++)
    {
        new id, phase, reward, kind[24];
        if (!cache_get_value_name_int(i, "contract_id", id)) break;
        if (journal)
        {
            if (!cache_get_value_name(i, "event_type", kind, sizeof(kind))) break;
            format(line, sizeof(line), "#%d | %s", id, kind);
        }
        else
        {
            if (!cache_get_value_name_int(i, "state", phase) ||
                !cache_get_value_name_int(i, "reward_amount", reward)) break;
            format(line, sizeof(line), "#%d | состояние %d | награда $%d%s",
                id, phase, reward, phase == 3 ? (" выплачена") : (""));
        }
        SendClientMessage(playerid, CRIME_COLOR, line);
    }
    cache_delete(c);
    if (!rows) SendClientMessage(playerid, CRIME_COLOR, "Пока нет записей.");
#else
    #pragma unused journal
#endif
    return 1;
}

stock Crime_Command(playerid, const params[])
{
    new action[20];
    if (isnull(params) || sscanf(params, "s[20]", action))
    {
        SendClientMessage(playerid, CRIME_COLOR,
            "Теневой рынок: /crime offer, /crime start, /crime status, /crime route.");
        return SendClientMessage(playerid, CRIME_COLOR,
            "Действия: /crime pickup, /crime deliver, /crime abort, /crime history, /crime journal.");
    }
    if (!strcmp(action, "offer", true)) return Crime_ShowOffer(playerid);
    if (!strcmp(action, "start", true)) return Crime_Start(playerid);
    if (!strcmp(action, "status", true)) return Crime_Status(playerid);
    if (!strcmp(action, "route", true)) return Crime_Status(playerid, true);
    if (!strcmp(action, "pickup", true)) return Crime_Advance(playerid, false);
    if (!strcmp(action, "deliver", true)) return Crime_Advance(playerid, true);
    if (!strcmp(action, "abort", true)) return Crime_Abort(playerid);
    if (!strcmp(action, "history", true)) return Crime_History(playerid);
    if (!strcmp(action, "journal", true)) return Crime_History(playerid, true);
    return SendClientMessage(playerid, CRIME_ERROR, "Неизвестное действие. /crime");
}
