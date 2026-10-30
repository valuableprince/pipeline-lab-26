#if defined _tarkenso_crime_ui_included
    #endinput
#endif
#define _tarkenso_crime_ui_included

// CRIME-02.1 | One entry point, server-authoritative dialog-driven workflow.
// IDs are reserved for this module; CI verifies global uniqueness.
#define CRIME_DLG_MAIN       (7950)
#define CRIME_DLG_OFFERS     (7951)
#define CRIME_DLG_OFFER      (7952)
#define CRIME_DLG_ACTIVE     (7953)
#define CRIME_DLG_ROUTE      (7954)
#define CRIME_DLG_CANCEL     (7955)
#define CRIME_DLG_HISTORY    (7956)
#define CRIME_DLG_JOURNAL    (7957)
#define CRIME_DLG_HELP       (7958)
#define CRIME_DLG_CONTACTS   (7959)
#define CRIME_DLG_CONTACT    (7960)
#define CRIME_DLG_SCENE      (7961)
#define CRIME_DLG_INTERVIEW  (7962)
#define CRIME_DLG_TESTIMONY  (7963)
#define CRIME_DLG_ORG        (7964)
#define CRIME_DLG_ORG_CONTACT (7965)
#define CRIME_DLG_ORG_ASSETS  (7966)
#define CRIME_DLG_ORG_BUY     (7967)
#define CRIME_OFFER_MAX      (8)

forward CrimeUI_Testimony(playerid);
forward CrimeAssets_Show(playerid, contactId, stash, network);
forward bool:CrimeAssets_Status(playerid, contactId, assetCode, &activated, &remaining, &services);

new gCrimeUIDialog[MAX_PLAYERS];
new gCrimeUIAuthToken[MAX_PLAYERS];
new gCrimeUIAccountId[MAX_PLAYERS];
new gCrimeUIContact[MAX_PLAYERS];
new gCrimeUISelectedTemplate[MAX_PLAYERS];
new gCrimeUISceneContract[MAX_PLAYERS];
new gCrimeUIScenePhase[MAX_PLAYERS];
new gCrimeUIWitnessContract[MAX_PLAYERS];
new gCrimeUIOrgContact[MAX_PLAYERS];
new gCrimeUIOrgAsset[MAX_PLAYERS];
new gCrimeUIOfferCount[MAX_PLAYERS];
new gCrimeUIOfferId[MAX_PLAYERS][CRIME_OFFER_MAX];

stock CrimeUI_Reset(playerid)
{
    gCrimeUIDialog[playerid] = 0;
    gCrimeUIAuthToken[playerid] = 0;
    gCrimeUIAccountId[playerid] = 0;
    gCrimeUIContact[playerid] = 0;
    gCrimeUISelectedTemplate[playerid] = 0;
    gCrimeUISceneContract[playerid] = 0;
    gCrimeUIScenePhase[playerid] = 0;
    gCrimeUIWitnessContract[playerid] = 0;
    gCrimeUIOrgContact[playerid] = 0;
    gCrimeUIOrgAsset[playerid] = 0;
    gCrimeUIOfferCount[playerid] = 0;
    return 1;
}

stock CrimeUI_Open(playerid, dialogid)
{
    gCrimeUIDialog[playerid] = dialogid;
    gCrimeUIAuthToken[playerid] = gAuthToken[playerid];
#if defined TARKENSO_DB_PRIMARY
    gCrimeUIAccountId[playerid] = gDBPrimaryAccountId[playerid];
#else
    gCrimeUIAccountId[playerid] = 0;
#endif
    return 1;
}

stock bool:CrimeUI_ReadActive(playerid, &id, &phase, &faction,
    &reward, &deadline, Float:pickup[], Float:delivery[], title[], titleSize)
{
#if defined TARKENSO_DB_PRIMARY
    if (!Crime_Expire(playerid)) return false;
    return Crime_LoadActive(playerid, id, phase, faction, reward, deadline,
        pickup, delivery, title, titleSize);
#else
    #pragma unused playerid, id, phase, faction, reward, deadline
    #pragma unused pickup, delivery, title, titleSize
    return false;
#endif
}

stock CrimeUI_Main(playerid)
{
    if (!Crime_Available(playerid))
        return SendClientMessage(playerid, CRIME_ERROR,
            "Теневой рынок временно недоступен. Проверьте подключение к базе.");
    if (!Leader_IsCriminal(Leader_PlayerOrg(playerid)))
        return SendClientMessage(playerid, CRIME_ERROR,
            "Теневой рынок доступен сотрудникам криминальных организаций.");
    new id, phase, faction, reward, deadline, title[68];
    new Float:pickup[3], Float:delivery[3], body[690], status[96];
    if (!CrimeUI_ReadActive(playerid, id, phase, faction, reward, deadline,
        pickup, delivery, title, sizeof(title)))
        return SendClientMessage(playerid, CRIME_ERROR,
            "Не удалось загрузить заказы. Попробуйте открыть меню позднее.");
    if (id)
        format(status, sizeof(status), "#%d: %s", id,
            phase == 1 ? ("Ожидает получения") : ("В пути"));
    else format(status, sizeof(status), "Нет активного заказа");
    format(body, sizeof(body),
        "{FFD15C}Раздел\t{FFD15C}Состояние{FFFFFF}\nПосредники и заказы\t%s\nМой контракт\t%s\nМаршрут и действие\t%s\nИстория контрактов\tПоследние 6\nЖурнал операций\tПоследние 8\nОтказаться от контракта\t%s\nОбучение\tКак это работает\nСиндикат и активы\tВлияние и связи\nОбщие конкурсы\tТри синдиката\nКонкурсы предложений\tВыбор NPC\nОперативные сводки\tПроверка документов", 
        id ? ("Доступны другие контакты") : ("Три города"),
        status, id ? ("Доступен") : ("Нет активного заказа"),
        id ? ("Доступно") : ("Нет активного заказа"));
    CrimeUI_Open(playerid, CRIME_DLG_MAIN);
    return ShowPlayerDialog(playerid, CRIME_DLG_MAIN, DIALOG_STYLE_TABLIST_HEADERS,
        "{FFD15C}TARKENSO | Теневой рынок", body, "Выбрать", "Закрыть");
}

stock CrimeUI_Contacts(playerid)
{
    new body[560] = "{FFD15C}Посредник\t{FFD15C}Город{FFFFFF}\n", line[125];
    for (new i = 0; i < CRIME_CONTACT_COUNT; i++)
    {
        if (!gCrimeContactEnabled[i]) continue;
        format(line, sizeof(line), "%s\t%s\n",
            gCrimeContactName[i], gCrimeContactCity[i]);
        strcat(body, line, sizeof(body));
    }
    CrimeUI_Open(playerid, CRIME_DLG_CONTACTS);
    return ShowPlayerDialog(playerid, CRIME_DLG_CONTACTS, DIALOG_STYLE_TABLIST_HEADERS,
        "{FFD15C}Теневой рынок | Посредники", body, "Подробнее", "Назад");
}

stock CrimeUI_ContactDetail(playerid, contactId)
{
    if (contactId < 1 || contactId > CRIME_CONTACT_COUNT ||
        !gCrimeContactEnabled[contactId - 1]) return CrimeUI_Contacts(playerid);
    new trust, completed, introduced;
    if (!CrimeContacts_Progress(playerid, contactId, trust, completed, introduced))
        return SendClientMessage(playerid, CRIME_ERROR, "Не удалось загрузить доверие.");
    new heat;
    if (!CrimeContacts_Heat(playerid, contactId, heat))
        return SendClientMessage(playerid, CRIME_ERROR, "Не удалось загрузить уровень внимания.");
    new i = contactId - 1, body[720];
    format(body, sizeof(body),
        "{FFD15C}ПОСРЕДНИК:{FFFFFF} %s\n{FFD15C}ГОРОД:{FFFFFF} %s\n{FFD15C}ДОВЕРИЕ:{FFFFFF} %d/100\n{FFD15C}ВНИМАНИЕ:{FFFFFF} %d/100\n{FFD15C}ВЫПОЛНЕННЫЕ ЗАКАЗЫ:{FFFFFF} %d\n{FFD15C}ЗНАКОМСТВО:{FFFFFF} %s\n\n{FFD15C}МЕСТО ВСТРЕЧИ:{FFFFFF}\nX %.1f, Y %.1f, Z %.1f\n\nПодойдите к посреднику на расстояние до 5 метров и нажмите «Поговорить». После знакомства откроются его предложения.",
        gCrimeContactName[i], gCrimeContactCity[i], trust, heat, completed,
        introduced ? ("Состоится повторная встреча") : ("Вы ещё не знакомы"),
        gCrimeContactPos[i][0], gCrimeContactPos[i][1], gCrimeContactPos[i][2]);
    gCrimeUIContact[playerid] = contactId;
    CrimeUI_Open(playerid, CRIME_DLG_CONTACT);
    return ShowPlayerDialog(playerid, CRIME_DLG_CONTACT, DIALOG_STYLE_MSGBOX,
        "{FFD15C}Теневой рынок | Контакт", body, "Поговорить", "Назад");
}

stock CrimeUI_Offers(playerid)
{
#if defined TARKENSO_DB_PRIMARY
    new broker = gCrimeUIContact[playerid], trust, completed, introduced;
    if (!CrimeContacts_Near(playerid, broker) ||
        !CrimeContacts_Progress(playerid, broker, trust, completed, introduced) ||
        !introduced)
        return SendClientMessage(playerid, CRIME_ERROR,
            "Сначала поговорите с посредником рядом с ним.");
    new q[360], rows = 0, body[1280], line[150];
    mysql_format(gDBPrimary, q, sizeof(q),
        "SELECT template_id,title,reward_amount,duration_seconds FROM crime_contract_templates WHERE contact_id=%d AND is_enabled=1 AND mode_id=1 AND required_trust<=%d ORDER BY template_id LIMIT 8",
        broker, trust);
    new Cache:c = mysql_query(gDBPrimary, q);
    if (c == Cache:0 || mysql_errno(gDBPrimary) ||
        !cache_get_row_count(rows) || rows > CRIME_OFFER_MAX)
    {
        if (c != Cache:0) cache_delete(c);
        return SendClientMessage(playerid, CRIME_ERROR, "Не удалось получить предложения.");
    }
    new offerIds[CRIME_OFFER_MAX], offerRewards[CRIME_OFFER_MAX],
        offerDuration[CRIME_OFFER_MAX], offerTitles[CRIME_OFFER_MAX][68];
    new count = 0;
    // Copy the active MySQL result before issuing eligibility queries.
    // Nested mysql_query calls must not invalidate the outer cache.
    for (new i; i < rows; i++)
    {
        if (!cache_get_value_name_int(i, "template_id", offerIds[i]) ||
            !cache_get_value_name_int(i, "reward_amount", offerRewards[i]) ||
            !cache_get_value_name_int(i, "duration_seconds", offerDuration[i]) ||
            !cache_get_value_name(i, "title", offerTitles[i], 68))
        {
            cache_delete(c);
            return SendClientMessage(playerid, CRIME_ERROR, "Ошибка чтения предложений.");
        }
        count++;
    }
    cache_delete(c);
    format(body, sizeof(body),
        "{FFD15C}Предложение\t{FFD15C}Награда\t{FFD15C}Доступность{FFFFFF}\n");
    gCrimeUIOfferCount[playerid] = 0;
    for (new i; i < count; i++)
    {
        new remaining, alreadyDone, status[45];
        if (!Crime_OfferEligibility(playerid, offerIds[i], remaining, alreadyDone))
            return SendClientMessage(playerid, CRIME_ERROR,
                "Не удалось проверить доступность заданий.");
        gCrimeUIOfferId[playerid][i] = offerIds[i];
        gCrimeUIOfferCount[playerid]++;
        if (alreadyDone) format(status, sizeof(status), "Пройдено");
        else if (remaining < 0) format(status, sizeof(status), "Высокое внимание");
        else if (remaining > 0)
            format(status, sizeof(status), "Через %d мин", (remaining + 59) / 60);
        else format(status, sizeof(status), "Доступно (%d мин)", offerDuration[i] / 60);
        format(line, sizeof(line), "%s\t$%d\t%s\n",
            offerTitles[i], offerRewards[i], status);
        strcat(body, line, sizeof(body));
    }
    if (!gCrimeUIOfferCount[playerid])
        strcat(body, "Пока нет заказов\t-\t-\n", sizeof(body));
    CrimeUI_Open(playerid, CRIME_DLG_OFFERS);
    return ShowPlayerDialog(playerid, CRIME_DLG_OFFERS, DIALOG_STYLE_TABLIST_HEADERS,
        "{FFD15C}Предложения посредника", body, "Подробнее", "Назад");
#else
    return CrimeUI_Main(playerid);
#endif
}

stock CrimeUI_OfferDetail(playerid, templateId)
{
#if defined TARKENSO_DB_PRIMARY
    new broker = gCrimeUIContact[playerid], trust, completed, introduced;
    if (!CrimeContacts_Near(playerid, broker) ||
        !CrimeContacts_Progress(playerid, broker, trust, completed, introduced) ||
        !introduced) return CrimeUI_Contacts(playerid);
    new q[320], rows = 0, title[68], reward, duration, requiredTrust, scenarioKind;
    mysql_format(gDBPrimary, q, sizeof(q),
        "SELECT title,reward_amount,duration_seconds,required_trust,scenario_kind FROM crime_contract_templates WHERE template_id=%d AND contact_id=%d AND required_trust<=%d AND mode_id=1 AND is_enabled=1",
        templateId, broker, trust);
    new Cache:c = mysql_query(gDBPrimary, q);
    if (c == Cache:0 || mysql_errno(gDBPrimary) ||
        !cache_get_row_count(rows) || rows != 1 ||
        !cache_get_value_name(0, "title", title, sizeof(title)) ||
        !cache_get_value_name_int(0, "reward_amount", reward) ||
        !cache_get_value_name_int(0, "duration_seconds", duration) ||
        !cache_get_value_name_int(0, "required_trust", requiredTrust) ||
        !cache_get_value_name_int(0, "scenario_kind", scenarioKind))
    {
        if (c != Cache:0) cache_delete(c);
        return SendClientMessage(playerid, CRIME_ERROR, "Предложение больше недоступно.");
    }
    cache_delete(c);
    new remaining, alreadyDone, availability[125];
    if (!Crime_OfferEligibility(playerid, templateId, remaining, alreadyDone))
        return SendClientMessage(playerid, CRIME_ERROR, "Нельзя проверить доступность.");
    if (alreadyDone) format(availability, sizeof(availability),
        "Уникальный заказ уже пройден: повторное принятие запрещено.");
    else if (remaining < 0) format(availability, sizeof(availability),
        "Слишком много внимания. Выполняйте обычные перевозки, чтобы снизить риск.");
    else if (remaining > 0) format(availability, sizeof(availability),
        "Повтор будет доступен через %d минут.", (remaining + 59) / 60);
    else format(availability, sizeof(availability),
        "Заказ доступен. После выполнения потребуется перерыв.");
    new body[720];
    format(body, sizeof(body),
        "{FFD15C}ЗАКАЗ:{FFFFFF} %s\n\n{FFD15C}НАГРАДА:{FFFFFF} $%d\n{FFD15C}СРОК:{FFFFFF} %d минут\n{FFD15C}НЕОБХОДИМО ДОВЕРИЕ:{FFFFFF} %d\n\n%s\n\n{FFD15C}ДОСТУПНОСТЬ:{FFFFFF} %s",
        title, reward, duration / 60, requiredTrust,
        scenarioKind == 0 ? ("Перевезите груз. Успешная сдача даст +5 доверия.") :
        ("Найдите улики и примите два решения. От выборов зависит доверие."),
        availability);
    gCrimeUISelectedTemplate[playerid] = alreadyDone || remaining != 0 ? 0 : templateId;
    CrimeUI_Open(playerid, CRIME_DLG_OFFER);
    return ShowPlayerDialog(playerid, CRIME_DLG_OFFER, DIALOG_STYLE_MSGBOX,
        "{FFD15C}Заказ посредника", body,
        gCrimeUISelectedTemplate[playerid] ? ("Принять") : ("К списку"), "Назад");
#else
    #pragma unused templateId
    return CrimeUI_Main(playerid);
#endif
}

stock CrimeUI_Active(playerid)
{
    new id, phase, faction, reward, deadline, title[68];
    new Float:pickup[3], Float:delivery[3];
    if (!CrimeUI_ReadActive(playerid, id, phase, faction, reward, deadline,
        pickup, delivery, title, sizeof(title)))
        return SendClientMessage(playerid, CRIME_ERROR, "Ошибка загрузки заказа.");
    if (!id)
    {
        SendClientMessage(playerid, CRIME_ERROR, "Сейчас нет активного контракта.");
        return CrimeUI_Main(playerid);
    }
    new kind, choice, task[64];
    if (!CrimeScenario_Info(playerid, id, kind, choice))
        return SendClientMessage(playerid, CRIME_ERROR, "Ошибка чтения сценария.");
    CrimeScenario_StageLabel(kind, phase, task, sizeof(task));
    new body[520];
    format(body, sizeof(body),
        "{FFD15C}КОНТРАКТ:{FFFFFF} #%d\n{FFD15C}ЗАКАЗ:{FFFFFF} %s\n{FFD15C}ОРГАНИЗАЦИЯ:{FFFFFF} %d\n{FFD15C}НАГРАДА:{FFFFFF} $%d\n{FFD15C}ОСТАЛОСЬ:{FFFFFF} %d минут\n{FFD15C}ЭТАП:{FFFFFF} %s\n\nНажмите «Маршрут», чтобы увидеть нужную точку и выполнить следующее действие.",
        id, title, faction, reward, (deadline - gettime() + 59) / 60,
        task);
    CrimeUI_Open(playerid, CRIME_DLG_ACTIVE);
    return ShowPlayerDialog(playerid, CRIME_DLG_ACTIVE, DIALOG_STYLE_MSGBOX,
        "{FFD15C}Мой криминальный контракт", body, "Маршрут", "Назад");
}

stock CrimeUI_Route(playerid)
{
    new id, phase, faction, reward, deadline, title[68];
    new Float:pickup[3], Float:delivery[3];
    if (!CrimeUI_ReadActive(playerid, id, phase, faction, reward, deadline,
        pickup, delivery, title, sizeof(title)))
        return SendClientMessage(playerid, CRIME_ERROR, "Ошибка загрузки маршрута.");
    if (!id)
    {
        SendClientMessage(playerid, CRIME_COLOR, "Активного заказа нет.");
        return CrimeUI_Main(playerid);
    }
    new kind, choice, task[64];
    if (!CrimeScenario_Info(playerid, id, kind, choice))
        return SendClientMessage(playerid, CRIME_ERROR, "Ошибка чтения сценария.");
    CrimeScenario_StageLabel(kind, phase, task, sizeof(task));
    if (phase == 2)
    {
        new encounter = CrimeIncident_Route(playerid, id);
        if (encounter < 0)
            return SendClientMessage(playerid, CRIME_ERROR,
                "Не удалось загрузить промежуточное событие. Доставка закрыта.");
        if (encounter > 0) return 1;
        new followup = CrimeAftermath_Route(playerid, id);
        if (followup < 0)
            return SendClientMessage(playerid, CRIME_ERROR,
                "Не удалось загрузить последствия выбора. Доставка закрыта.");
        if (followup > 0) return 1;
    }
    new Float:x = phase == 1 ? pickup[0] : delivery[0],
        Float:y = phase == 1 ? pickup[1] : delivery[1],
        Float:z = phase == 1 ? pickup[2] : delivery[2];
    new body[860];
    format(body, sizeof(body),
        "{FFD15C}КОНТРАКТ:{FFFFFF} #%d — %s\n{FFD15C}ЗАДАЧА:{FFFFFF} %s\n\n{FFD15C}ТОЧКА НА КАРТЕ:{FFFFFF}\nX: %.1f\nY: %.1f\nZ: %.1f\n{FFD15C}РАДИУС:{FFFFFF} 12 метров\n\nДоберитесь до указанного места, затем нажмите «%s». Если вы далеко от точки, операция не выполнится.",
        id, title, task,
        x, y, z, kind == 0 ? (phase == 1 ? ("Забрать") : ("Передать")) : ("Исследовать"));
    if (kind > 0)
    {
        CrimeWorld_ShowClue(playerid, id, kind, phase, x, y, z);
        new eventId, narrative[240];
        if (!CrimeScenario_EventInfo(playerid, id, eventId) || eventId < 1)
            return SendClientMessage(playerid, CRIME_ERROR, "Обстоятельства дела недоступны.");
        CrimeScenario_EventText(kind, eventId, phase, narrative, sizeof(narrative));
        strcat(body, "\n\n{FFD15C}ОБСТОЯТЕЛЬСТВА:{FFFFFF} ", sizeof(body));
        strcat(body, narrative, sizeof(body));
        strcat(body, "\n\n{FFD15C}ВЗАИМОДЕЙСТВИЕ:{FFFFFF} Подойдите к улике или NPC на 3.5 м и нажмите Y.", sizeof(body));
    }
    CrimeUI_Open(playerid, CRIME_DLG_ROUTE);
    return ShowPlayerDialog(playerid, CRIME_DLG_ROUTE, DIALOG_STYLE_MSGBOX,
        "{FFD15C}Теневой рынок | Маршрут", body,
        kind == 0 ? (phase == 1 ? ("Забрать") : ("Передать")) : ("Понятно"), "Назад");
}

stock CrimeUI_Scene(playerid)
{
    new id, phase, faction, reward, deadline, title[68];
    new Float:pickup[3], Float:delivery[3];
    if (!CrimeUI_ReadActive(playerid, id, phase, faction, reward, deadline,
        pickup, delivery, title, sizeof(title)) || !id)
        return CrimeUI_Main(playerid);
    new kind, choice;
    if (!CrimeScenario_Info(playerid, id, kind, choice) || kind == 0)
        return CrimeUI_Main(playerid);
    new Float:x = phase == 1 ? pickup[0] : delivery[0],
        Float:y = phase == 1 ? pickup[1] : delivery[1],
        Float:z = phase == 1 ? pickup[2] : delivery[2];
    if (GetPlayerVirtualWorld(playerid) != 0 || GetPlayerInterior(playerid) != 0 ||
        IsPlayerInAnyVehicle(playerid) ||
        !IsPlayerInRangeOfPoint(playerid, CRIME_WORLD_RADIUS, x, y, z) ||
        (phase == 1 && !CrimeWorld_EvidenceReady(playerid, id)))
    {
        SendClientMessage(playerid, CRIME_ERROR,
            "Подойдите к объекту, нажмите Y и изучите улику.");
        return CrimeUI_Route(playerid);
    }
    if (phase == 2 && !CrimeWorld_InterviewReady(playerid, id))
    {
        SendClientMessage(playerid, CRIME_ERROR,
            "Сначала поговорите с участником дела (Y).");
        return CrimeUI_Route(playerid);
    }
    new body[470];
    CrimeScenario_Choices(kind, phase, body, sizeof(body));
    gCrimeUISceneContract[playerid] = id;
    gCrimeUIScenePhase[playerid] = phase;
    CrimeUI_Open(playerid, CRIME_DLG_SCENE);
    return ShowPlayerDialog(playerid, CRIME_DLG_SCENE, DIALOG_STYLE_TABLIST_HEADERS,
        "{FFD15C}Решение по делу", body, "Выбрать", "Назад");
}

// Witness interview is a separate mandatory stage between evidence and
// final resolution. Every dialog reopening checks the current SQL contract.
stock CrimeUI_Witness(playerid)
{
    new id, phase, faction, reward, deadline, title[68];
    new Float:pickup[3], Float:delivery[3];
    if (!CrimeUI_ReadActive(playerid, id, phase, faction, reward, deadline,
        pickup, delivery, title, sizeof(title)) || !id || phase != 2 ||
        faction != Leader_PlayerOrg(playerid) ||
        IsPlayerInAnyVehicle(playerid) || GetPlayerInterior(playerid) != 0 ||
        GetPlayerVirtualWorld(playerid) != 0 ||
        !IsPlayerInRangeOfPoint(playerid, CRIME_WORLD_RADIUS,
            delivery[0], delivery[1], delivery[2]))
        return CrimeUI_Main(playerid);
    new kind, choice, question, interviewedAt;
    if (!CrimeScenario_Info(playerid, id, kind, choice) || kind == 0 ||
        !CrimeWorld_InterviewInfo(playerid, id, question, interviewedAt))
        return CrimeUI_Main(playerid);
    if (interviewedAt > 0) return CrimeUI_Testimony(playerid);
    new body[460];
    CrimeScenario_InterviewChoices(kind, body, sizeof(body));
    gCrimeUIWitnessContract[playerid] = id;
    CrimeUI_Open(playerid, CRIME_DLG_INTERVIEW);
    return ShowPlayerDialog(playerid, CRIME_DLG_INTERVIEW, DIALOG_STYLE_TABLIST_HEADERS,
        "{FFD15C}Разговор с участником", body, "Спросить", "Назад");
}

stock CrimeUI_Testimony(playerid)
{
    new id, phase, faction, reward, deadline, title[68];
    new Float:pickup[3], Float:delivery[3];
    if (!CrimeUI_ReadActive(playerid, id, phase, faction, reward, deadline,
        pickup, delivery, title, sizeof(title)) || !id || phase != 2 ||
        faction != Leader_PlayerOrg(playerid) ||
        IsPlayerInAnyVehicle(playerid) || GetPlayerInterior(playerid) != 0 ||
        GetPlayerVirtualWorld(playerid) != 0 ||
        !IsPlayerInRangeOfPoint(playerid, CRIME_WORLD_RADIUS,
            delivery[0], delivery[1], delivery[2]))
        return CrimeUI_Main(playerid);
    new kind, choice, question, interviewedAt, eventId;
    if (!CrimeScenario_Info(playerid, id, kind, choice) || kind == 0 ||
        !CrimeWorld_InterviewInfo(playerid, id, question, interviewedAt) ||
        interviewedAt <= 0 ||
        !CrimeScenario_EventInfo(playerid, id, eventId))
        return CrimeUI_Main(playerid);
    new body[540];
    CrimeScenario_Testimony(kind, eventId, question, choice, body, sizeof(body));
    gCrimeUIWitnessContract[playerid] = id;
    CrimeUI_Open(playerid, CRIME_DLG_TESTIMONY);
    return ShowPlayerDialog(playerid, CRIME_DLG_TESTIMONY, DIALOG_STYLE_MSGBOX,
        "{FFD15C}Показания участника", body, "Решение", "Назад");
}

// Organization progression and assets: viewable by any faction member;
// purchases are strictly leader/deputy only, rechecked by the SQL transaction.
stock CrimeUI_Organization(playerid)
{
    new influence, done;
    if (!CrimeOrg_ReadFaction(playerid, influence, done))
        return SendClientMessage(playerid, CRIME_ERROR, "Состояние синдиката недоступно.");
    new body[760], line[180], standing, count, stash, network;
    format(body, sizeof(body),
        "{FFD15C}Посредник\t{FFD15C}Отношения\t{FFD15C}Активы{FFFFFF}\n");
    for (new i = 1; i <= CRIME_CONTACT_COUNT; i++)
    {
        if (!CrimeOrg_ReadContact(playerid, i, standing, count, stash, network))
            return SendClientMessage(playerid, CRIME_ERROR, "Не удалось прочитать связи синдиката.");
        format(line, sizeof(line), "%s\t%d/100\t%d/2\n",
            gCrimeContactName[i-1], standing, stash + network);
        strcat(body, line, sizeof(body));
    }
    new title[115];
    format(title, sizeof(title), "Синдикат #%d | Влияние: %d | Дел: %d",
        Leader_PlayerOrg(playerid), influence, done);
    CrimeUI_Open(playerid, CRIME_DLG_ORG);
    return ShowPlayerDialog(playerid, CRIME_DLG_ORG, DIALOG_STYLE_TABLIST_HEADERS,
        title, body, "Подробнее", "Назад");
}

stock CrimeUI_OrgContact(playerid, contactId)
{
    if (contactId < 1 || contactId > CRIME_CONTACT_COUNT) return CrimeUI_Organization(playerid);
    new standing, done, stash, network, influence, total;
    if (!CrimeOrg_ReadContact(playerid, contactId, standing, done, stash, network) ||
        !CrimeOrg_ReadFaction(playerid, influence, total))
        return SendClientMessage(playerid, CRIME_ERROR, "Связи синдиката недоступны.");
    gCrimeUIOrgContact[playerid] = contactId;
    CrimeAssets_Show(playerid, contactId, stash, network);
    new body[720];
    format(body, sizeof(body),
        "{FFD15C}ПОСРЕДНИК:{FFFFFF} %s\n{FFD15C}ОТНОШЕНИЯ:{FFFFFF} %d/100\n{FFD15C}ДЕЛА СИНДИКАТА:{FFFFFF} %d\n{FFD15C}ОБЩЕЕ ВЛИЯНИЕ:{FFFFFF} %d\n\n{FFD15C}ТАЙНИК:{FFFFFF} %s\nБонус после активации: +1 влияние за заказ.\n{FFD15C}СЕТЬ ИНФОРМАТОРОВ:{FFFFFF} %s\nБонус после активации: +1 отношения за заказ.\n\nОбслуживание: откройте маршрут к NPC, подойдите к объекту и нажмите Y. Повтор — через 30 минут. Покупка для руководства (ранги 4-5).",
        gCrimeContactName[contactId-1], standing, done, influence,
        stash ? ("Приобретён") : ("Нет"),
        network ? ("Приобретена") : ("Нет"));
    CrimeUI_Open(playerid, CRIME_DLG_ORG_CONTACT);
    return ShowPlayerDialog(playerid, CRIME_DLG_ORG_CONTACT, DIALOG_STYLE_MSGBOX,
        "{FFD15C}Связи синдиката", body, "Активы", "Назад");
}

stock CrimeUI_OrgAssets(playerid)
{
    new cid = gCrimeUIOrgContact[playerid];
    if (cid < 1 || cid > CRIME_CONTACT_COUNT) return CrimeUI_Organization(playerid);
    new standing, done, stash, network, body[530];
    if (!CrimeOrg_ReadContact(playerid, cid, standing, done, stash, network))
        return SendClientMessage(playerid, CRIME_ERROR, "Активы недоступны.");
    new activated, remaining, services, statusStash[65], statusNetwork[65];
    if (!CrimeAssets_Status(playerid, cid, 1, activated, remaining, services))
        return SendClientMessage(playerid, CRIME_ERROR, "Не удалось проверить тайник.");
    if (!stash) format(statusStash, sizeof(statusStash), "Не приобретён");
    else if (!activated) format(statusStash, sizeof(statusStash), "Нужна активация");
    else if (remaining > 0) format(statusStash, sizeof(statusStash), "Через %d мин", (remaining+59)/60);
    else format(statusStash, sizeof(statusStash), "Доступно (Y)");
    if (!CrimeAssets_Status(playerid, cid, 2, activated, remaining, services))
        return SendClientMessage(playerid, CRIME_ERROR, "Не удалось проверить информаторов.");
    if (!network) format(statusNetwork, sizeof(statusNetwork), "Не приобретена");
    else if (!activated) format(statusNetwork, sizeof(statusNetwork), "Нужна активация");
    else if (remaining > 0) format(statusNetwork, sizeof(statusNetwork), "Через %d мин", (remaining+59)/60);
    else format(statusNetwork, sizeof(statusNetwork), "Доступно (Y)");
    format(body, sizeof(body),
        "{FFD15C}Актив\t{FFD15C}Цена\t{FFD15C}Обслуживание{FFFFFF}\nТайник\t15 влияния\t%s\nСеть информаторов\t30 влияния\t%s",
        statusStash, statusNetwork);
    CrimeAssets_Show(playerid, cid, stash, network);
    CrimeUI_Open(playerid, CRIME_DLG_ORG_ASSETS);
    return ShowPlayerDialog(playerid, CRIME_DLG_ORG_ASSETS, DIALOG_STYLE_TABLIST_HEADERS,
        "{FFD15C}Активы посредника", body, "Подробнее", "Назад");
}

stock CrimeUI_OrgBuyDialog(playerid, assetCode)
{
    if (assetCode != 1 && assetCode != 2) return CrimeUI_OrgAssets(playerid);
    new cid = gCrimeUIOrgContact[playerid], standing, done, stash, network, influence, total;
    if (!CrimeOrg_ReadContact(playerid, cid, standing, done, stash, network) ||
        !CrimeOrg_ReadFaction(playerid, influence, total))
        return CrimeUI_Organization(playerid);
    new cost = CrimeOrg_AssetCost(assetCode),
        needed = CrimeOrg_AssetStanding(assetCode),
        owned = assetCode == 1 ? stash : network,
        body[400];
    format(body, sizeof(body),
        "{FFD15C}АКТИВ:{FFFFFF} %s\n{FFD15C}ПОСРЕДНИК:{FFFFFF} %s\n{FFD15C}ЦЕНА:{FFFFFF} %d влияния\n{FFD15C}ТРЕБУЮТСЯ ОТНОШЕНИЯ:{FFFFFF} %d/100\n{FFD15C}СЕЙЧАС:{FFFFFF} влияние %d; отношения %d\n{FFD15C}СТАТУС:{FFFFFF} %s\n\nСписывается только влияние организации, не деньги игрока.",
        assetCode == 1 ? ("Тайник") : ("Сеть информаторов"),
        gCrimeContactName[cid-1], cost, needed, influence, standing,
        owned ? ("Уже куплен") : ("Не приобретён"));
    gCrimeUIOrgAsset[playerid] = owned || influence < cost ||
        standing < needed ? 0 : assetCode;
    CrimeUI_Open(playerid, CRIME_DLG_ORG_BUY);
    return ShowPlayerDialog(playerid, CRIME_DLG_ORG_BUY, DIALOG_STYLE_MSGBOX,
        "{FFD15C}Развитие синдиката", body,
        gCrimeUIOrgAsset[playerid] ? ("Приобрести") : ("Назад"), "Закрыть");
}

stock CrimeUI_Cancel(playerid)
{
    new id, phase, faction, reward, deadline, title[68];
    new Float:pickup[3], Float:delivery[3];
    if (!CrimeUI_ReadActive(playerid, id, phase, faction, reward, deadline,
        pickup, delivery, title, sizeof(title)))
        return SendClientMessage(playerid, CRIME_ERROR, "Ошибка загрузки контракта.");
    if (!id)
    {
        SendClientMessage(playerid, CRIME_ERROR, "Нет контракта для отмены.");
        return CrimeUI_Main(playerid);
    }
    new body[350];
    format(body, sizeof(body),
        "Вы действительно хотите отказаться от контракта #%d — %s?\n\n{FFD15C}ПОСЛЕДСТВИЯ:{FFFFFF}\nЗаказ будет закрыт без выплаты. Отмену невозможно будет откатить.",
        id, title);
    CrimeUI_Open(playerid, CRIME_DLG_CANCEL);
    return ShowPlayerDialog(playerid, CRIME_DLG_CANCEL, DIALOG_STYLE_MSGBOX,
        "{FFD15C}Подтверждение отказа", body, "Отказаться", "Назад");
}

stock CrimeUI_History(playerid, bool:journal = false)
{
#if defined TARKENSO_DB_PRIMARY
    new q[850], body[1300], line[165], rows;
    if (journal)
        mysql_format(gDBPrimary, q, sizeof(q),
            "SELECT e.contract_id,e.event_type FROM crime_contract_events e JOIN crime_contracts c ON c.contract_id=e.contract_id WHERE c.account_id=%d ORDER BY e.event_id DESC LIMIT 8",
            gDBPrimaryAccountId[playerid]);
    else
        mysql_format(gDBPrimary, q, sizeof(q),
            "SELECT c.contract_id,c.state,c.reward_amount,c.scenario_kind,COALESCE(c.competition_id,0) competition_id,COALESCE(r.trust_delta,0) trust_delta,COALESCE(h.heat_delta,0) heat_delta FROM crime_contracts c LEFT JOIN crime_contact_trust_events r ON r.contract_id=c.contract_id LEFT JOIN crime_contact_heat_events h ON h.contract_id=c.contract_id WHERE c.account_id=%d ORDER BY c.contract_id DESC LIMIT 6",
            gDBPrimaryAccountId[playerid]);
    new Cache:c = mysql_query(gDBPrimary, q);
    if (c == Cache:0 || mysql_errno(gDBPrimary) || !cache_get_row_count(rows))
    {
        if (c != Cache:0) cache_delete(c);
        return SendClientMessage(playerid, CRIME_ERROR, "Не удалось загрузить историю.");
    }
    if (journal) format(body, sizeof(body),
        "{FFD15C}Контракт\t{FFD15C}Событие{FFFFFF}\n");
    else format(body, sizeof(body),
        "{FFD15C}Контракт\t{FFD15C}Состояние\t{FFD15C}Награда{FFFFFF}\n");
    for (new i; i < rows; i++)
    {
        new id, phase, reward, kind = 0, roundId = 0, trustGain = 0, heatDelta = 0, event[24], label[64];
        if (!cache_get_value_name_int(i, "contract_id", id)) break;
        if (journal)
        {
            if (!cache_get_value_name(i, "event_type", event, sizeof(event))) break;
            if (!strcmp(event, "ACCEPTED")) format(label, sizeof(label), "Принят");
            else if (!strcmp(event, "PICKED_UP")) format(label, sizeof(label), "Груз получен");
            else if (!strcmp(event, "CLUE_FOUND")) format(label, sizeof(label), "Улика изучена");
            else if (!strcmp(event, "EVIDENCE_FOUND")) format(label, sizeof(label), "Улика обнаружена");
            else if (!strcmp(event, "WITNESS_INTERVIEWED")) format(label, sizeof(label), "Свидетель опрошен");
            else if (!strcmp(event, "RACE_WON")) format(label, sizeof(label), "Победа в конкурсе");
            else if (!strcmp(event, "INCIDENT_RESOLVED")) format(label, sizeof(label), "Событие разрешено");
            else if (!strcmp(event, "AFTERMATH_DONE")) format(label, sizeof(label), "Прикрытие проверено");
            else if (!strcmp(event, "RACE_LOST")) format(label, sizeof(label), "Уступил соперникам");
            else if (!strcmp(event, "COMPLETED")) format(label, sizeof(label), "Выполнен");
            else if (!strcmp(event, "EXPIRED")) format(label, sizeof(label), "Срок истёк");
            else if (!strcmp(event, "ABORTED")) format(label, sizeof(label), "Отменён");
            else format(label, sizeof(label), "Другое событие");
            format(line, sizeof(line), "#%d\t%s\n", id, label);
        }
        else
        {
            if (!cache_get_value_name_int(i, "state", phase) ||
                !cache_get_value_name_int(i, "reward_amount", reward) ||
                !cache_get_value_name_int(i, "scenario_kind", kind) ||
                !cache_get_value_name_int(i, "competition_id", roundId) ||
                !cache_get_value_name_int(i, "trust_delta", trustGain) ||
                !cache_get_value_name_int(i, "heat_delta", heatDelta)) break;
            switch (phase)
            {
                case 1: format(label, sizeof(label), "Ожидает груза");
                case 2: format(label, sizeof(label), "В перевозке");
                case 3: format(label, sizeof(label), "Выполнен");
                case 4: format(label, sizeof(label), "Просрочен");
                case 5: format(label, sizeof(label), "Отменён");
                default: format(label, sizeof(label), "Неизвестен");
            }
            if (roundId > 0 && phase == 5) format(label, sizeof(label), "Уступил соперникам");
            else if (roundId > 0 && phase == 3) format(label, sizeof(label), "Победа синдиката");
            if (kind > 0 && phase == 3 && trustGain > 0)
            {
                format(line, sizeof(line), "#%d\t%s (+%d дов., риск %d)\t$%d\n",
                    id, label, trustGain, heatDelta, reward);
            }
            else if (phase == 3)
                format(line, sizeof(line), "#%d\t%s (риск %d)\t$%d\n",
                    id, label, heatDelta, reward);
            else format(line, sizeof(line), "#%d\t%s\t$%d\n", id, label, reward);
        }
        strcat(body, line, sizeof(body));
    }
    cache_delete(c);
    if (!rows) strcat(body, "Нет записей\t-\n", sizeof(body));
    CrimeUI_Open(playerid, journal ? CRIME_DLG_JOURNAL : CRIME_DLG_HISTORY);
    if (journal)
        return ShowPlayerDialog(playerid, CRIME_DLG_JOURNAL, DIALOG_STYLE_TABLIST_HEADERS,
            "{FFD15C}Теневой рынок | Журнал", body, "Назад", "Закрыть");
    return ShowPlayerDialog(playerid, CRIME_DLG_HISTORY, DIALOG_STYLE_TABLIST_HEADERS,
        "{FFD15C}Теневой рынок | История", body, "Назад", "Закрыть");
#else
    #pragma unused journal
    return CrimeUI_Main(playerid);
#endif
}

stock CrimeUI_Help(playerid)
{
    CrimeUI_Open(playerid, CRIME_DLG_HELP);
    return ShowPlayerDialog(playerid, CRIME_DLG_HELP, DIALOG_STYLE_MSGBOX,
        "{FFD15C}Теневой рынок | Обучение",
        "{FFD15C}1. Посредник{FFFFFF}\nПриезжайте к NPC и откройте его заказы.\n\n{FFD15C}2. Задание{FFFFFF}\nПеревозите груз или изучите обстоятельства дела на двух точках.\n\n{FFD15C}3. Решения{FFFFFF}\nУ улики и у NPC нажмите Y. Поговорите со свидетелем, затем выберите исход. Точная проверка фактов улучшает доверие. Рискованные решения повышают внимание; доставки снижают его.\n\n{FFD15C}4. Повторение{FFFFFF}\nПосле перевозки нужно подождать 5 минут, после расследования — 15. Уникальные сюжетные дела нельзя проходить повторно.\n\n{FFD15C}5. Сохранение{FFFFFF}\nДеньги, прогресс и обстоятельства сохраняются в базе.\n\n{FFD15C}6. Синдикат{FFFFFF}\nКонтракты дают влияние и связи с NPC. Руководство покупает активы за влияние. Для активации и обслуживания подойдите к объекту возле посредника и нажмите Y.\n\n{FFD15C}7. Общие конкурсы{FFFFFF}\nКаждый час появляются три общих заказа. Первый доставивший синдикат получает награду и дополнительное влияние. Можно победить даже одному.\n\n{FFD15C}8. Конкурсы предложений{FFFFFF}\nУ NPC выберите срочный, сбалансированный или осторожный способ доставки. Через минуту посредник выбирает выгоднейшее предложение; при равных условиях учитывается порядок подачи. Даже один игрок может победить. Победителю назначается перевозка с промежуточным NPC-событием: после получения груза откройте маршрут, найдите NPC, подойдите пешком и нажмите Y. Без решения доставить груз нельзя. После события маршрут ведёт ко второму связному: осторожный выбор требует подтвердить прикрытие (доверие +7, связи +4), рискованный — снять наблюдение (доплата $300, доверие +2, связи +1). В обоих случаях нужно подойти пешком и нажать Y. Изменение внимания: -12 или +12. Без второго этапа награда недоступна. Отказ или просрочка: -3 доверия, -2 отношений синдиката, +5 внимания. Если после подачи вы начнёте другой заказ, NPC может отказать.\n\n{FFD15C}9. Оперативные сводки{FFFFFF}\nПосле успешной тендерной доставки откройте раздел «Оперативные сводки» (30 минут). Вернитесь пешком к исходному посреднику, найдите ошибку в документах. За неверный ответ — 45 секунд ожидания. После успешной проверки решите: передать данные синдикату (+4 влияния, +1 связей) либо скрыть следы (-10 внимания, +1 доверия). Повторно использовать досье нельзя.\n\nВсё управление доступно из /crime и меню организации.",
        "Назад", "Закрыть");
}

stock CrimeUI_OnDialog(playerid, dialogid, response, listitem)
{
    if (dialogid < CRIME_DLG_MAIN || dialogid > CRIME_DLG_ORG_BUY) return 0;
#if defined TARKENSO_DB_PRIMARY
    if (gCrimeUIDialog[playerid] != dialogid ||
        gCrimeUIAuthToken[playerid] != gAuthToken[playerid] ||
        gCrimeUIAccountId[playerid] != gDBPrimaryAccountId[playerid] ||
        !Crime_Available(playerid))
    {
        CrimeUI_Reset(playerid);
        return 1;
    }
#else
    #pragma unused response, listitem
    return 1;
#endif
    // Single-use dialog stage: rejects replayed responses and stale windows.
    gCrimeUIDialog[playerid] = 0;
    if (!response)
    {
        if (dialogid == CRIME_DLG_MAIN) return 1;
        if (dialogid == CRIME_DLG_OFFER) return CrimeUI_Offers(playerid);
        if (dialogid == CRIME_DLG_OFFERS) return CrimeUI_ContactDetail(playerid, gCrimeUIContact[playerid]);
        if (dialogid == CRIME_DLG_CONTACT) return CrimeUI_Contacts(playerid);
        if (dialogid == CRIME_DLG_SCENE) return CrimeUI_Route(playerid);
        if (dialogid == CRIME_DLG_INTERVIEW || dialogid == CRIME_DLG_TESTIMONY)
            return CrimeUI_Route(playerid);
        if (dialogid == CRIME_DLG_ORG_CONTACT) return CrimeUI_Organization(playerid);
        if (dialogid == CRIME_DLG_ORG_ASSETS) return CrimeUI_OrgContact(playerid, gCrimeUIOrgContact[playerid]);
        if (dialogid == CRIME_DLG_ORG_BUY) return CrimeUI_OrgAssets(playerid);
        return CrimeUI_Main(playerid);
    }
    switch (dialogid)
    {
        case CRIME_DLG_MAIN:
        {
            if (listitem < 0 || listitem > 10) return 1;
            switch (listitem)
            {
                case 0: return CrimeUI_Contacts(playerid);
                case 1: return CrimeUI_Active(playerid);
                case 2: return CrimeUI_Route(playerid);
                case 3: return CrimeUI_History(playerid);
                case 4: return CrimeUI_History(playerid, true);
                case 5: return CrimeUI_Cancel(playerid);
                case 6: return CrimeUI_Help(playerid);
                case 7: return CrimeUI_Organization(playerid);
                case 8: return CrimeCompUI_Board(playerid);
                case 9: return CrimeTenderUI_Board(playerid);
                case 10: return CrimeIntelUI_Board(playerid);
            }
        }
        case CRIME_DLG_ORG:
        {
            if (listitem < 0 || listitem >= CRIME_CONTACT_COUNT)
                return CrimeUI_Organization(playerid);
            return CrimeUI_OrgContact(playerid, listitem+1);
        }
        case CRIME_DLG_ORG_CONTACT:
            return CrimeUI_OrgAssets(playerid);
        case CRIME_DLG_ORG_ASSETS:
        {
            if (listitem < 0 || listitem > 1) return CrimeUI_OrgAssets(playerid);
            return CrimeUI_OrgBuyDialog(playerid, listitem+1);
        }
        case CRIME_DLG_ORG_BUY:
        {
            new cid = gCrimeUIOrgContact[playerid],
                code = gCrimeUIOrgAsset[playerid];
            if (code == 0) return CrimeUI_OrgAssets(playerid);
            gCrimeUIOrgAsset[playerid] = 0; // Single-use even with a replayed click.
            CrimeOrg_Buy(playerid, cid, code);
            return CrimeUI_OrgContact(playerid, cid);
        }
        case CRIME_DLG_CONTACTS:
        {
            if (listitem < 0 || listitem >= CRIME_CONTACT_COUNT) return 1;
            return CrimeUI_ContactDetail(playerid, listitem + 1);
        }
        case CRIME_DLG_CONTACT:
        {
            new broker = gCrimeUIContact[playerid];
            if (!CrimeContacts_Introduce(playerid, broker))
            {
                SendClientMessage(playerid, CRIME_ERROR,
                    "Подойдите к посреднику и повторите разговор.");
                return CrimeUI_ContactDetail(playerid, broker);
            }
            return CrimeUI_Offers(playerid);
        }
        case CRIME_DLG_OFFERS:
        {
            if (listitem < 0 || listitem >= gCrimeUIOfferCount[playerid])
                return CrimeUI_Offers(playerid);
            return CrimeUI_OfferDetail(playerid, gCrimeUIOfferId[playerid][listitem]);
        }
        case CRIME_DLG_OFFER:
        {
            if (gCrimeUISelectedTemplate[playerid] <= 0)
                return CrimeUI_Offers(playerid);
            Crime_Start(playerid, gCrimeUISelectedTemplate[playerid]); // SQL and proximity rechecked.
            return CrimeUI_Route(playerid);
        }
        case CRIME_DLG_ACTIVE: return CrimeUI_Route(playerid);
        case CRIME_DLG_ROUTE:
        {
            new id, phase, faction, reward, deadline, title[68];
            new Float:pickup[3], Float:delivery[3];
            if (!CrimeUI_ReadActive(playerid, id, phase, faction, reward, deadline,
                pickup, delivery, title, sizeof(title)) || !id)
                return CrimeUI_Main(playerid);
            // An intermediate tender encounter is NOT an ordinary delivery
            // button. It must be resolved by walking to its NPC and using Y.
            if (phase == 2)
            {
                new contact, code, incidentChoice;
                if (CrimeIncident_Info(playerid, id, contact, code, incidentChoice) &&
                    incidentChoice == 0) return CrimeUI_Main(playerid);
            }
            new kind, choice;
            if (!CrimeScenario_Info(playerid, id, kind, choice))
                return CrimeUI_Main(playerid);
            if (kind != 0) return 1; // World action is activated only by KEY_YES.
            if (phase == 2)
            {
                new contact, incidentChoice, stage;
                if (CrimeAftermath_Info(playerid,id,contact,incidentChoice,stage)==1 && stage!=2)
                    return CrimeUI_Route(playerid);
            }
            Crime_Advance(playerid, phase == 2); // Classic courier remains unchanged.
            if (phase == 1) return CrimeUI_Route(playerid);
            return CrimeUI_Main(playerid);
        }
        case CRIME_DLG_INTERVIEW:
        {
            if (listitem < 0 || listitem > 1) return CrimeUI_Main(playerid);
            new id, phase, faction, reward, deadline, title[68];
            new Float:pickup[3], Float:delivery[3];
            if (!CrimeUI_ReadActive(playerid, id, phase, faction, reward, deadline,
                pickup, delivery, title, sizeof(title)) || !id || phase != 2 ||
                id != gCrimeUIWitnessContract[playerid])
                return CrimeUI_Main(playerid);
            if (!CrimeWorld_Interview(playerid, id, faction, listitem + 1))
            {
                SendClientMessage(playerid, CRIME_ERROR,
                    "Не удалось сохранить разговор. Проверьте свой контракт.");
                return CrimeUI_Route(playerid);
            }
            return CrimeUI_Testimony(playerid);
        }
        case CRIME_DLG_TESTIMONY:
        {
            new id, phase, faction, reward, deadline, title[68];
            new Float:pickup[3], Float:delivery[3];
            if (!CrimeUI_ReadActive(playerid, id, phase, faction, reward, deadline,
                pickup, delivery, title, sizeof(title)) || !id || phase != 2 ||
                id != gCrimeUIWitnessContract[playerid] ||
                !CrimeWorld_InterviewReady(playerid, id))
                return CrimeUI_Main(playerid);
            return CrimeUI_Scene(playerid);
        }
        case CRIME_DLG_SCENE:
        {
            if (listitem < 0 || listitem > 1) return CrimeUI_Main(playerid);
            new id, phase, faction, reward, deadline, title[68];
            new Float:pickup[3], Float:delivery[3];
            if (!CrimeUI_ReadActive(playerid, id, phase, faction, reward, deadline,
                pickup, delivery, title, sizeof(title)) || !id ||
                id != gCrimeUISceneContract[playerid] ||
                phase != gCrimeUIScenePhase[playerid])
                return CrimeUI_Main(playerid);
            new kind, choice;
            if (!CrimeScenario_Info(playerid, id, kind, choice) || kind == 0)
                return CrimeUI_Main(playerid);
            Crime_Advance(playerid, phase == 2, listitem + 1);
            CrimeWorld_Reset(playerid);
            if (phase == 1) return CrimeUI_Route(playerid);
            return CrimeUI_Main(playerid);
        }
        case CRIME_DLG_CANCEL:
        {
            Crime_Abort(playerid);
            CrimeWorld_Reset(playerid);
            return CrimeUI_Main(playerid);
        }
        case CRIME_DLG_HISTORY, CRIME_DLG_JOURNAL, CRIME_DLG_HELP:
            return CrimeUI_Main(playerid);
    }
    return 1;
}
