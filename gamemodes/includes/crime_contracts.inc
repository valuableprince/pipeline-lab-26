#if defined _tarkenso_crime_contracts_included
    #endinput
#endif
#define _tarkenso_crime_contracts_included

// CRIME-02.1: SQL-primary pilot. The DB owns state and unique reward settlement.
// No shared PvP objects, automatic payments or new timers in this phase.
#define CRIME_COLOR (0xC7C7C7FF)
#define CRIME_ERROR (0xE27E82FF)
#define CRIME_INTERACT_RADIUS (12.0)
#define CRIME_WORLD_RADIUS (3.5)

new bool:gCrimeReady;
forward Crime_Settle(playerid, id, reward, faction, decision);
forward bool:CrimeScenario_Info(playerid, id, &kind, &choice);
forward bool:CrimeContacts_Init();
forward CrimeContacts_Shutdown();
forward bool:CrimeContacts_Near(playerid, contactId);
forward CrimeContacts_TemplateContact(templateId);
forward bool:CrimeWorld_Init();
forward CrimeWorld_Shutdown();
forward CrimeWorld_Reset(playerid);
forward CrimeWorld_ShowClue(playerid, contractId, kind, phase, Float:x, Float:y, Float:z);
forward bool:CrimeWorld_EvidenceReady(playerid, id);
forward bool:CrimeWorld_InterviewReady(playerid, id);
forward bool:CrimeWorld_InterviewInfo(playerid, id, &question, &interviewedAt);
forward bool:CrimeWorld_Interview(playerid, id, faction, question);
forward bool:CrimeComp_RoundForContract(playerid, contractId, faction, &roundId);
forward bool:CrimeComp_Claim(playerid, roundId, contractId, faction);
forward bool:CrimeTender_Penalize(playerid, contractId);
forward bool:CrimeTender_PenalizeExpired(playerid);
forward bool:CrimeIncident_Init();
forward CrimeIncident_Shutdown();
forward CrimeIncident_Reset(playerid);
forward CrimeIncident_Route(playerid, contractId);

stock Crime_Init()
{
    gCrimeReady = false;
#if defined TARKENSO_DB_PRIMARY
    if (!DBPrimary_IsReady()) return 1;
    new Cache:c = mysql_query(gDBPrimary,
        "SELECT version FROM schema_migrations WHERE version=43 LIMIT 1");
    if (c == Cache:0) return 1;
    new rows = 0;
    new bool:ok = mysql_errno(gDBPrimary) == 0 &&
        cache_get_row_count(rows) && rows == 1;
    cache_delete(c);
    if (!ok)
    {
        print("[CRIME-02] Migration 043 missing; tender encounters disabled.");
        return 1;
    }
    if (!CrimeContacts_Init()) return 1;
    if (!CrimeWorld_Init())
    {
        CrimeContacts_Shutdown();
        return 1;
    }
    if (!CrimeIncident_Init())
    {
        CrimeWorld_Shutdown();
        CrimeContacts_Shutdown();
        return 1;
    }
    gCrimeReady = true;
    print("[CRIME-02] Solo NPC contracts enabled (SQL-primary).");
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
    if (!CrimeTender_PenalizeExpired(playerid))
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



// Read-only hint for menus. Acceptance repeats these rules atomically in SQL.
stock bool:Crime_OfferEligibility(playerid, templateId, &remaining, &oneTimeDone)
{
    remaining = 0;
    oneTimeDone = 0;
#if defined TARKENSO_DB_PRIMARY
    if (!Crime_Available(playerid) || templateId <= 0) return false;
    new q[880], rows = 0, cooldown, repeatable, lastFinished, completions, heat, kind;
    mysql_format(gDBPrimary, q, sizeof(q),
        "SELECT t.cooldown_seconds,t.is_repeatable,t.scenario_kind,COALESCE(p.heat,0) heat,COALESCE((SELECT MAX(c.finished_at) FROM crime_contracts c WHERE c.account_id=%d AND c.template_id=t.template_id AND c.state IN(3,5)),0) last_finished,(SELECT COUNT(*) FROM crime_contracts c WHERE c.account_id=%d AND c.template_id=t.template_id AND c.state=3) completions FROM crime_contract_templates t LEFT JOIN crime_contact_progress p ON p.account_id=%d AND p.contact_id=t.contact_id WHERE t.template_id=%d LIMIT 1",
        gDBPrimaryAccountId[playerid], gDBPrimaryAccountId[playerid],
        gDBPrimaryAccountId[playerid], templateId);
    new Cache:c = mysql_query(gDBPrimary, q);
    if (c == Cache:0 || mysql_errno(gDBPrimary) ||
        !cache_get_row_count(rows) || rows != 1 ||
        !cache_get_value_name_int(0, "cooldown_seconds", cooldown) ||
        !cache_get_value_name_int(0, "is_repeatable", repeatable) ||
        !cache_get_value_name_int(0, "scenario_kind", kind) ||
        !cache_get_value_name_int(0, "heat", heat) ||
        !cache_get_value_name_int(0, "last_finished", lastFinished) ||
        !cache_get_value_name_int(0, "completions", completions))
    {
        if (c != Cache:0) cache_delete(c);
        return false;
    }
    cache_delete(c);
    if (cooldown < 0 || cooldown > 86400 || heat < 0 || heat > 100) return false;
    oneTimeDone = !repeatable && completions > 0;
    if (kind > 0 && heat >= 70)
    {
        remaining = -1; // The contact refuses sensitive work until local heat falls.
        return true;
    }
    cooldown += heat * 10;
    if (lastFinished > 0 && cooldown > 0)
    {
        remaining = lastFinished + cooldown - gettime();
        if (remaining < 0) remaining = 0;
    }
    return true;
#else
    #pragma unused playerid, templateId
    return false;
#endif
}

stock Crime_Start(playerid, templateId)
{
    if (!Crime_Available(playerid))
        return SendClientMessage(playerid, CRIME_ERROR, "Контракты SQL недоступны.");
    new org = Leader_PlayerOrg(playerid);
    if (!Leader_IsCriminal(org))
        return SendClientMessage(playerid, CRIME_ERROR, "Только для сотрудников синдикатов 6-8.");
    new broker = CrimeContacts_TemplateContact(templateId);
    if (!CrimeContacts_Near(playerid, broker))
        return SendClientMessage(playerid, CRIME_ERROR, "Для принятия заказа необходимо лично поговорить с посредником.");
#if defined TARKENSO_DB_PRIMARY
    if (gGovTransferBusy[playerid] || !Crime_Expire(playerid))
        return SendClientMessage(playerid, CRIME_ERROR, "Операция временно недоступна.");
    if (!Crime_Exec("START TRANSACTION"))
        return SendClientMessage(playerid, CRIME_ERROR, "Нет соединения с базой.");
    new q[1800], changed;
    mysql_format(gDBPrimary, q, sizeof(q),
        "INSERT INTO crime_contracts(template_id,account_id,faction_id,mode_id,reward_amount,started_at,expires_at,contact_id,scenario_kind,scenario_event) SELECT t.template_id,m.account_id,m.faction_id,t.mode_id,t.reward_amount,UNIX_TIMESTAMP(),UNIX_TIMESTAMP()+t.duration_seconds,t.contact_id,t.scenario_kind,CASE WHEN t.scenario_kind=0 THEN 0 ELSE 1+FLOOR(RAND()*3) END FROM crime_contract_templates t JOIN faction_members m ON m.account_id=%d AND m.faction_id=%d JOIN crime_contact_progress p ON p.account_id=m.account_id AND p.contact_id=t.contact_id AND p.trust>=t.required_trust AND (t.scenario_kind=0 OR p.heat<70) WHERE t.template_id=%d AND t.contact_id=%d AND t.mode_id=1 AND t.is_enabled=1 AND (t.is_repeatable=1 OR NOT EXISTS(SELECT 1 FROM crime_contracts h WHERE h.account_id=m.account_id AND h.template_id=t.template_id AND h.state=3)) AND NOT EXISTS(SELECT 1 FROM crime_contracts h WHERE h.account_id=m.account_id AND h.template_id=t.template_id AND h.state IN(3,5) AND h.finished_at>UNIX_TIMESTAMP()-(t.cooldown_seconds+p.heat*10))",
        gDBPrimaryAccountId[playerid], org, templateId, broker);
    if (!DBPrimary_Execute(q, changed) || changed != 1)
    {
        Crime_Rollback();
        new left, finished;
        if (Crime_OfferEligibility(playerid, templateId, left, finished))
        {
            if (finished) return SendClientMessage(playerid, CRIME_ERROR,
                "Этот сюжетный контракт уже пройден. Повторно принять его нельзя.");
            if (left < 0) return SendClientMessage(playerid, CRIME_ERROR,
                "Посредник пока отказывает в сложных заказах: слишком много внимания. Выполняйте обычные перевозки.");
            if (left > 0)
            {
                new notice[130];
                format(notice, sizeof(notice),
                    "Следующий такой заказ будет доступен через %d мин.",
                    (left + 59) / 60);
                return SendClientMessage(playerid, CRIME_ERROR, notice);
            }
        }
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
            "Статус принятия неизвестен. Откройте меню повторно.");
    SendClientMessage(playerid, CRIME_COLOR, "Заказ принят. Отправляйтесь за грузом.");
    return 1;
#else
    return 1;
#endif
}

stock Crime_Advance(playerid, bool:deliveryStage, decision = 0)
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
            "Сейчас этот этап недоступен. Проверьте меню своего контракта.");
    if (faction != Leader_PlayerOrg(playerid))
        return SendClientMessage(playerid, CRIME_ERROR,
            "Вы больше не сотрудник организации, принявшей контракт.");
    if (!IsPlayerInRangeOfPoint(playerid, CRIME_INTERACT_RADIUS,
        deliveryStage ? drop[0] : pickup[0],
        deliveryStage ? drop[1] : pickup[1],
        deliveryStage ? drop[2] : pickup[2]))
        return SendClientMessage(playerid, CRIME_ERROR,
            "Вы далеко от точки операции. Откройте раздел маршрута.");
    new scenarioKind, priorChoice;
    if (!CrimeScenario_Info(playerid, id, scenarioKind, priorChoice) ||
        (scenarioKind == 0 && decision != 0) ||
        (scenarioKind != 0 && (decision < 1 || decision > 2)))
        return SendClientMessage(playerid, CRIME_ERROR, "Выбор для этого задания недоступен.");
    if (scenarioKind > 0)
    {
        if (GetPlayerInterior(playerid) != 0 ||
            GetPlayerVirtualWorld(playerid) != 0 ||
            IsPlayerInAnyVehicle(playerid) ||
            !IsPlayerInRangeOfPoint(playerid, CRIME_WORLD_RADIUS,
                deliveryStage ? drop[0] : pickup[0],
                deliveryStage ? drop[1] : pickup[1],
                deliveryStage ? drop[2] : pickup[2]) ||
            (!deliveryStage && !CrimeWorld_EvidenceReady(playerid, id)) ||
            (deliveryStage && !CrimeWorld_InterviewReady(playerid, id)))
            return SendClientMessage(playerid, CRIME_ERROR,
                "Сначала изучите улику или поговорите с участником рядом с ним (Y).");
    }
    if (deliveryStage) return Crime_Settle(playerid, id, reward, faction, decision);
    if (!Crime_Exec("START TRANSACTION"))
        return SendClientMessage(playerid, CRIME_ERROR, "Ошибка начала операции.");
    new q[500], changed;
    mysql_format(gDBPrimary, q, sizeof(q),
        "UPDATE crime_contracts SET state=2,picked_up_at=UNIX_TIMESTAMP(),scenario_choice=%d WHERE contract_id=%d AND account_id=%d AND faction_id=%d AND state=1 AND expires_at>UNIX_TIMESTAMP() AND (scenario_kind=0 OR (evidence_found_at>0 AND %d IN(1,2)))",
        decision, id, gDBPrimaryAccountId[playerid], faction, decision);
    if (!DBPrimary_Execute(q, changed) || changed != 1)
    {
        Crime_Rollback();
        return SendClientMessage(playerid, CRIME_ERROR, "Груз уже выдан или срок истёк.");
    }
    mysql_format(gDBPrimary, q, sizeof(q),
        "INSERT INTO crime_contract_events(contract_id,actor_account_id,event_type) VALUES (%d,%d,'%e')",
        id, gDBPrimaryAccountId[playerid],
        scenarioKind == 0 ? ("PICKED_UP") : ("CLUE_FOUND"));
    if (!Crime_Exec(q))
    {
        Crime_Rollback();
        return SendClientMessage(playerid, CRIME_ERROR, "Ошибка журнала выдачи.");
    }
    if (!Crime_Exec("COMMIT"))
        return SendClientMessage(playerid, CRIME_ERROR,
            "Статус выдачи неизвестен. Откройте меню повторно.");
    SendClientMessage(playerid, CRIME_COLOR,
        scenarioKind == 0 ? ("Груз получен. Отвезите его получателю.") :
        ("Улика найдена. Следующая точка позволит решить исход дела."));
    return 1;
#else
    #pragma unused deliveryStage
    return 1;
#endif
}

stock Crime_Settle(playerid, id, reward, faction, decision)
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
    new q[1800], changed, competitionRound=0;
    if (!CrimeComp_RoundForContract(playerid,id,faction,competitionRound) ||
        (competitionRound>0 && !CrimeComp_Claim(playerid,competitionRound,id,faction)))
    {
        Crime_Rollback();
        gGovTransferBusy[playerid]=false;
        return SendClientMessage(playerid,CRIME_ERROR,
            "Соперники уже выиграли либо общий конкурс закрыт. Награда не начислена.");
    }
    // Account row + contract row remain in the same transaction. CAS rejects
    // stale account versions. On errors every step (including money) rolls back.
    mysql_format(gDBPrimary, q, sizeof(q),
        "UPDATE accounts a JOIN crime_contracts c ON c.account_id=a.account_id SET a.balance=a.balance+c.reward_amount,a.txn_serial=a.txn_serial+1,a.row_version=a.row_version+1 WHERE a.account_id=%d AND a.row_version=%d AND a.balance<=1000000000-c.reward_amount AND a.txn_serial<1000000000 AND c.contract_id=%d AND c.faction_id=%d AND c.state=2 AND c.expires_at>UNIX_TIMESTAMP() AND (c.scenario_kind=0 OR (c.scenario_choice IN(1,2) AND c.interviewed_at>0 AND %d IN(1,2))) AND (c.mode_id<>3 OR EXISTS(SELECT 1 FROM crime_tender_incidents i WHERE i.contract_id=c.contract_id AND i.choice IN(1,2) AND i.resolved_at>0)) AND NOT EXISTS(SELECT 1 FROM crime_contract_rewards r WHERE r.contract_id=c.contract_id)",
        gDBPrimaryAccountId[playerid], oldVersion, id, faction, decision);
    if (!DBPrimary_Execute(q, changed) || changed != 1)
    {
        Crime_Rollback();
        gGovTransferBusy[playerid] = false;
        return SendClientMessage(playerid, CRIME_ERROR,
            "Выплата отклонена: заказ истёк, уже оплачен или кошелёк изменился.");
    }
    mysql_format(gDBPrimary, q, sizeof(q),
        "UPDATE crime_contracts SET state=3,finished_at=UNIX_TIMESTAMP(),scenario_resolution=%d,risk_delta=CASE WHEN mode_id=3 AND EXISTS(SELECT 1 FROM crime_tender_incidents i WHERE i.contract_id=%d AND i.choice=2) THEN 12 WHEN mode_id=3 THEN -12 WHEN scenario_kind=0 THEN -8 WHEN %d=2 THEN 20 WHEN interview_question=2 THEN 8 WHEN scenario_choice=2 THEN 5 ELSE -12 END WHERE contract_id=%d AND account_id=%d AND faction_id=%d AND state=2 AND expires_at>UNIX_TIMESTAMP() AND (scenario_kind=0 OR interviewed_at>0) AND (mode_id<>3 OR EXISTS(SELECT 1 FROM crime_tender_incidents i WHERE i.contract_id=%d AND i.choice IN(1,2) AND i.resolved_at>0))",
        decision, id, decision, id, gDBPrimaryAccountId[playerid], faction, id);
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
    // Reputation and its once-per-contract ledger are committed with money.
    // A duplicate completion can never add trust again.
    mysql_format(gDBPrimary, q, sizeof(q),
        "INSERT INTO crime_contact_trust_events(contract_id,account_id,contact_id,trust_delta) SELECT contract_id,account_id,contact_id,CASE WHEN scenario_kind=0 THEN 5 WHEN scenario_resolution=2 THEN 2 WHEN scenario_choice=1 THEN 5+(CASE WHEN interview_question=1 THEN 1 ELSE 0 END) ELSE 3 END FROM crime_contracts WHERE contract_id=%d AND account_id=%d AND state=3",
        id, gDBPrimaryAccountId[playerid]);
    if (!DBPrimary_Execute(q, changed) || changed != 1)
    {
        Crime_Rollback();
        gGovTransferBusy[playerid] = false;
        return SendClientMessage(playerid, CRIME_ERROR, "Ошибка журнала доверия. Выплата отменена.");
    }
    mysql_format(gDBPrimary, q, sizeof(q),
        "UPDATE crime_contact_progress p JOIN crime_contracts c ON c.account_id=p.account_id AND c.contact_id=p.contact_id SET p.trust=LEAST(100,p.trust+(CASE WHEN c.scenario_kind=0 THEN 5 WHEN c.scenario_resolution=2 THEN 2 WHEN c.scenario_choice=1 THEN 5+(CASE WHEN c.interview_question=1 THEN 1 ELSE 0 END) ELSE 3 END)),p.completed_count=p.completed_count+1 WHERE c.contract_id=%d AND c.account_id=%d AND c.state=3",
        id, gDBPrimaryAccountId[playerid]);
    if (!DBPrimary_Execute(q, changed) || changed != 1)
    {
        Crime_Rollback();
        gGovTransferBusy[playerid] = false;
        return SendClientMessage(playerid, CRIME_ERROR, "Не удалось сохранить доверие. Выплата отменена.");
    }
    // Heat is independent from trust and is settled in the very same
    // transaction as the wallet. The contract ID is its idempotency key.
    // The ledger records the ACTUAL delta after the 0..100 clamp.
    mysql_format(gDBPrimary, q, sizeof(q),
        "INSERT INTO crime_contact_heat_events(contract_id,account_id,contact_id,heat_delta) SELECT c.contract_id,c.account_id,c.contact_id,GREATEST(-CAST(p.heat AS SIGNED),LEAST(100-CAST(p.heat AS SIGNED),c.risk_delta)) FROM crime_contracts c JOIN crime_contact_progress p ON p.account_id=c.account_id AND p.contact_id=c.contact_id WHERE c.contract_id=%d AND c.account_id=%d AND c.state=3",
        id, gDBPrimaryAccountId[playerid]);
    if (!DBPrimary_Execute(q, changed) || changed != 1)
    {
        Crime_Rollback();
        gGovTransferBusy[playerid] = false;
        return SendClientMessage(playerid, CRIME_ERROR, "Не удалось записать риск. Выплата отменена.");
    }
    mysql_format(gDBPrimary, q, sizeof(q),
        "UPDATE crime_contact_progress p JOIN crime_contact_heat_events h ON h.account_id=p.account_id AND h.contact_id=p.contact_id SET p.heat=GREATEST(0,LEAST(100,CAST(p.heat AS SIGNED)+h.heat_delta)),p.heat_event_count=p.heat_event_count+1 WHERE h.contract_id=%d AND h.account_id=%d",
        id, gDBPrimaryAccountId[playerid]);
    if (!DBPrimary_Execute(q, changed) || changed != 1)
    {
        Crime_Rollback();
        gGovTransferBusy[playerid] = false;
        return SendClientMessage(playerid, CRIME_ERROR, "Не удалось сохранить риск. Выплата отменена.");
    }
    // The syndicate benefits even if only one member is online.
    // Assets improve influence / contact standing but NEVER player cash.
    // All rows and the unique contract ledger remain in the payout transaction.
    mysql_format(gDBPrimary, q, sizeof(q),
        "INSERT INTO crime_syndicate_influence_events(contract_id,faction_id,contact_id,influence_delta,standing_delta) SELECT c.contract_id,c.faction_id,c.contact_id,(CASE WHEN c.mode_id=2 THEN 8 WHEN c.scenario_kind=0 THEN 2 ELSE 4 END)+(CASE WHEN EXISTS(SELECT 1 FROM crime_syndicate_assets a WHERE a.faction_id=c.faction_id AND a.contact_id=c.contact_id AND a.asset_code=1 AND a.activated_at>0) THEN 1 ELSE 0 END),(CASE WHEN c.mode_id=2 THEN 5 WHEN c.scenario_kind=0 THEN 2 ELSE 4 END)+(CASE WHEN EXISTS(SELECT 1 FROM crime_syndicate_assets a WHERE a.faction_id=c.faction_id AND a.contact_id=c.contact_id AND a.asset_code=2 AND a.activated_at>0) THEN 1 ELSE 0 END) FROM crime_contracts c WHERE c.contract_id=%d AND c.account_id=%d AND c.faction_id=%d AND c.state=3",
        id, gDBPrimaryAccountId[playerid], faction);
    if (!DBPrimary_Execute(q, changed) || changed != 1)
    {
        Crime_Rollback();
        gGovTransferBusy[playerid] = false;
        return SendClientMessage(playerid, CRIME_ERROR, "Не удалось записать влияние. Выплата отменена.");
    }
    mysql_format(gDBPrimary, q, sizeof(q),
        "UPDATE crime_syndicate_progress p JOIN crime_syndicate_influence_events e ON e.faction_id=p.faction_id SET p.influence=LEAST(1000000,p.influence+e.influence_delta),p.completed_count=p.completed_count+1 WHERE e.contract_id=%d AND e.faction_id=%d",
        id, faction);
    if (!DBPrimary_Execute(q, changed) || changed != 1)
    {
        Crime_Rollback();
        gGovTransferBusy[playerid] = false;
        return SendClientMessage(playerid, CRIME_ERROR, "Не удалось сохранить влияние. Выплата отменена.");
    }
    mysql_format(gDBPrimary, q, sizeof(q),
        "UPDATE crime_syndicate_contacts p JOIN crime_syndicate_influence_events e ON e.faction_id=p.faction_id AND e.contact_id=p.contact_id SET p.standing=LEAST(100,p.standing+e.standing_delta),p.completed_count=p.completed_count+1 WHERE e.contract_id=%d AND e.faction_id=%d",
        id, faction);
    if (!DBPrimary_Execute(q, changed) || changed != 1)
    {
        Crime_Rollback();
        gGovTransferBusy[playerid] = false;
        return SendClientMessage(playerid, CRIME_ERROR, "Не удалось сохранить связи. Выплата отменена.");
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
        "Контракт #%d завершён. Начислено $%d. Результат сохранён в журнале.",
        id, reward);
    return SendClientMessage(playerid, CRIME_COLOR, message);
#else
    #pragma unused playerid, id, reward, faction, decision
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
    if (!CrimeTender_Penalize(playerid, id))
    {
        Crime_Rollback();
        return SendClientMessage(playerid, CRIME_ERROR, "Не удалось учесть последствия отказа.");
    }
    if (!Crime_Exec("COMMIT"))
        return SendClientMessage(playerid, CRIME_ERROR, "Проверьте статус операции.");
    return SendClientMessage(playerid, CRIME_COLOR, "Контракт отменён без выплаты.");
#else
    return 1;
#endif
}


