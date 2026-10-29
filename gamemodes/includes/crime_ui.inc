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

new gCrimeUIDialog[MAX_PLAYERS];
new gCrimeUIAuthToken[MAX_PLAYERS];
new gCrimeUIAccountId[MAX_PLAYERS];

stock CrimeUI_Reset(playerid)
{
    gCrimeUIDialog[playerid] = 0;
    gCrimeUIAuthToken[playerid] = 0;
    gCrimeUIAccountId[playerid] = 0;
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
    new id, phase, faction, reward, deadline, title[68];
    new Float:pickup[3], Float:delivery[3], body[450], status[96];
    if (!CrimeUI_ReadActive(playerid, id, phase, faction, reward, deadline,
        pickup, delivery, title, sizeof(title)))
        return SendClientMessage(playerid, CRIME_ERROR,
            "Не удалось загрузить заказы. Попробуйте открыть меню позднее.");
    if (id)
        format(status, sizeof(status), "#%d: %s", id,
            phase == 1 ? ("Ожидает получения") : ("В пути"));
    else format(status, sizeof(status), "Нет активного заказа");
    format(body, sizeof(body),
        "{FFD15C}Раздел\t{FFD15C}Состояние{FFFFFF}\nДоступные заказы\t%s\nМой контракт\t%s\nМаршрут и действие\t%s\nИстория контрактов\tПоследние 6\nЖурнал операций\tПоследние 8\nОтказаться от контракта\t%s\nОбучение\tКак это работает",
        id ? ("Уже есть заказ") : ("Ночной рейс"),
        status, id ? ("Доступен") : ("Нет активного заказа"),
        id ? ("Доступно") : ("Нет активного заказа"));
    CrimeUI_Open(playerid, CRIME_DLG_MAIN);
    return ShowPlayerDialog(playerid, CRIME_DLG_MAIN, DIALOG_STYLE_TABLIST_HEADERS,
        "{FFD15C}TARKENSO | Теневой рынок", body, "Выбрать", "Закрыть");
}

stock CrimeUI_Offers(playerid)
{
#if defined TARKENSO_DB_PRIMARY
    new Cache:c = mysql_query(gDBPrimary,
        "SELECT title,reward_amount,duration_seconds FROM crime_contract_templates WHERE template_id=1 AND mode_id=1 AND is_enabled=1");
    new rows = 0, title[68], reward, duration;
    if (c == Cache:0 || mysql_errno(gDBPrimary) ||
        !cache_get_row_count(rows) || rows > 1)
    {
        if (c != Cache:0) cache_delete(c);
        return SendClientMessage(playerid, CRIME_ERROR, "Не удалось загрузить предложения.");
    }
    new body[300];
    if (!rows) format(body, sizeof(body),
        "{FFD15C}Предложение\t{FFD15C}Вознаграждение\t{FFD15C}Срок{FFFFFF}\nНет доступных заказов\t-\t-");
    else
    {
        if (!cache_get_value_name(0, "title", title, sizeof(title)) ||
            !cache_get_value_name_int(0, "reward_amount", reward) ||
            !cache_get_value_name_int(0, "duration_seconds", duration))
        {
            cache_delete(c);
            return SendClientMessage(playerid, CRIME_ERROR, "Неверные данные предложения.");
        }
        format(body, sizeof(body),
            "{FFD15C}Предложение\t{FFD15C}Вознаграждение\t{FFD15C}Срок{FFFFFF}\n%s\t$%d\t%d мин.", title, reward, duration / 60);
    }
    cache_delete(c);
    CrimeUI_Open(playerid, CRIME_DLG_OFFERS);
    return ShowPlayerDialog(playerid, CRIME_DLG_OFFERS, DIALOG_STYLE_TABLIST_HEADERS,
        "{FFD15C}Криминальные контракты | Предложения", body, "Подробнее", "Назад");
#else
    return CrimeUI_Main(playerid);
#endif
}

stock CrimeUI_OfferDetail(playerid)
{
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
    new body[520];
    format(body, sizeof(body),
        "{FFD15C}ЗАКАЗ: {FFFFFF}%s\n\n{FFD15C}НАГРАДА:{FFFFFF} $%d\n{FFD15C}СРОК:{FFFFFF} %d минут\n{FFD15C}ТИП:{FFFFFF} одиночная перевозка\n\nПолучите груз в порту Лос-Сантоса и отвезите его получателю. На маршруте нужно будет открыть меню и подтвердить каждое действие.\n\n{FFD15C}УСЛОВИЯ:{FFFFFF} сотрудник синдиката 6-8, не более одного активного заказа. Принятие фиксируется в базе.",
        title, reward, duration / 60);
    CrimeUI_Open(playerid, CRIME_DLG_OFFER);
    return ShowPlayerDialog(playerid, CRIME_DLG_OFFER, DIALOG_STYLE_MSGBOX,
        "{FFD15C}Теневой рынок | Подробности", body, "Принять", "Назад");
#else
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
    new body[520];
    format(body, sizeof(body),
        "{FFD15C}КОНТРАКТ:{FFFFFF} #%d\n{FFD15C}ЗАКАЗ:{FFFFFF} %s\n{FFD15C}ОРГАНИЗАЦИЯ:{FFFFFF} %d\n{FFD15C}НАГРАДА:{FFFFFF} $%d\n{FFD15C}ОСТАЛОСЬ:{FFFFFF} %d минут\n{FFD15C}ЭТАП:{FFFFFF} %s\n\nНажмите «Маршрут», чтобы увидеть нужную точку и выполнить следующее действие.",
        id, title, faction, reward, (deadline - gettime() + 59) / 60,
        phase == 1 ? ("Получение груза") : ("Передача получателю"));
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
    new Float:x = phase == 1 ? pickup[0] : delivery[0],
        Float:y = phase == 1 ? pickup[1] : delivery[1],
        Float:z = phase == 1 ? pickup[2] : delivery[2];
    new body[540];
    format(body, sizeof(body),
        "{FFD15C}КОНТРАКТ:{FFFFFF} #%d — %s\n{FFD15C}ЗАДАЧА:{FFFFFF} %s\n\n{FFD15C}ТОЧКА НА КАРТЕ:{FFFFFF}\nX: %.1f\nY: %.1f\nZ: %.1f\n{FFD15C}РАДИУС:{FFFFFF} 12 метров\n\nДоберитесь до указанного места, затем нажмите «%s». Если вы далеко от точки, операция не выполнится.",
        id, title, phase == 1 ? ("Забрать груз") : ("Доставить груз"),
        x, y, z, phase == 1 ? ("Забрать") : ("Передать"));
    CrimeUI_Open(playerid, CRIME_DLG_ROUTE);
    return ShowPlayerDialog(playerid, CRIME_DLG_ROUTE, DIALOG_STYLE_MSGBOX,
        "{FFD15C}Теневой рынок | Маршрут", body,
        phase == 1 ? ("Забрать") : ("Передать"), "Назад");
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
    new q[420], body[1300], line[165], rows;
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
    if (journal) format(body, sizeof(body),
        "{FFD15C}Контракт\t{FFD15C}Событие{FFFFFF}\n");
    else format(body, sizeof(body),
        "{FFD15C}Контракт\t{FFD15C}Состояние\t{FFD15C}Награда{FFFFFF}\n");
    for (new i; i < rows; i++)
    {
        new id, phase, reward, event[24], label[40];
        if (!cache_get_value_name_int(i, "contract_id", id)) break;
        if (journal)
        {
            if (!cache_get_value_name(i, "event_type", event, sizeof(event))) break;
            if (!strcmp(event, "ACCEPTED")) format(label, sizeof(label), "Принят");
            else if (!strcmp(event, "PICKED_UP")) format(label, sizeof(label), "Груз получен");
            else if (!strcmp(event, "COMPLETED")) format(label, sizeof(label), "Выполнен");
            else if (!strcmp(event, "EXPIRED")) format(label, sizeof(label), "Срок истёк");
            else if (!strcmp(event, "ABORTED")) format(label, sizeof(label), "Отменён");
            else format(label, sizeof(label), "Другое событие");
            format(line, sizeof(line), "#%d\t%s\n", id, label);
        }
        else
        {
            if (!cache_get_value_name_int(i, "state", phase) ||
                !cache_get_value_name_int(i, "reward_amount", reward)) break;
            switch (phase)
            {
                case 1: format(label, sizeof(label), "Ожидает груза");
                case 2: format(label, sizeof(label), "В перевозке");
                case 3: format(label, sizeof(label), "Выполнен");
                case 4: format(label, sizeof(label), "Просрочен");
                case 5: format(label, sizeof(label), "Отменён");
                default: format(label, sizeof(label), "Неизвестен");
            }
            format(line, sizeof(line), "#%d\t%s\t$%d\n", id, label, reward);
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
        "{FFD15C}1. Предложение{FFFFFF}\nВ разделе «Доступные заказы» выберите работу и подтвердите принятие.\n\n{FFD15C}2. Получение груза{FFFFFF}\nОткройте «Маршрут и действие», доберитесь до точки и нажмите «Забрать».\n\n{FFD15C}3. Доставка{FFFFFF}\nПовторите действие в новой точке и нажмите «Передать».\n\n{FFD15C}4. Оплата{FFFFFF}\nДеньги зачисляются после подтверждения MySQL. Повторная выплата невозможна.\n\n{FFD15C}5. Сохранение{FFFFFF}\nСостояние сохраняется при выходе. Срок задания продолжает идти.\n\nПри необходимости откажитесь от заказа в главном меню. Все действия доступны через один вход: /crime.",
        "Назад", "Закрыть");
}

stock CrimeUI_OnDialog(playerid, dialogid, response, listitem)
{
    if (dialogid < CRIME_DLG_MAIN || dialogid > CRIME_DLG_HELP) return 0;
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
        return CrimeUI_Main(playerid);
    }
    switch (dialogid)
    {
        case CRIME_DLG_MAIN:
        {
            if (listitem < 0 || listitem > 6) return 1;
            switch (listitem)
            {
                case 0: return CrimeUI_Offers(playerid);
                case 1: return CrimeUI_Active(playerid);
                case 2: return CrimeUI_Route(playerid);
                case 3: return CrimeUI_History(playerid);
                case 4: return CrimeUI_History(playerid, true);
                case 5: return CrimeUI_Cancel(playerid);
                case 6: return CrimeUI_Help(playerid);
            }
        }
        case CRIME_DLG_OFFERS:
        {
            if (listitem != 0) return CrimeUI_Offers(playerid);
            return CrimeUI_OfferDetail(playerid);
        }
        case CRIME_DLG_OFFER:
        {
            Crime_Start(playerid); // Revalidates org, unique active and DB conditions.
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
            Crime_Advance(playerid, phase == 2); // DB + position checked again.
            if (phase == 1) return CrimeUI_Route(playerid);
            return CrimeUI_Main(playerid);
        }
        case CRIME_DLG_CANCEL:
        {
            Crime_Abort(playerid);
            return CrimeUI_Main(playerid);
        }
        case CRIME_DLG_HISTORY, CRIME_DLG_JOURNAL, CRIME_DLG_HELP:
            return CrimeUI_Main(playerid);
    }
    return 1;
}
