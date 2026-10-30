#if defined _tarkenso_crime_scenarios_included
    #endinput
#endif
#define _tarkenso_crime_scenarios_included

// CRIME-02.3: frozen scenario type and choices are persisted per instance.
// Zero means a legacy courier. No player-side choice can bypass the SQL stages.
stock bool:CrimeScenario_Info(playerid, id, &kind, &choice)
{
    kind = 0;
    choice = 0;
#if defined TARKENSO_DB_PRIMARY
    if (!Crime_Available(playerid) || id < 1) return false;
    new q[260], rows = 0;
    mysql_format(gDBPrimary, q, sizeof(q),
        "SELECT scenario_kind,scenario_choice FROM crime_contracts WHERE contract_id=%d AND account_id=%d AND state IN(1,2) LIMIT 1",
        id, gDBPrimaryAccountId[playerid]);
    new Cache:c = mysql_query(gDBPrimary, q);
    if (c == Cache:0 || mysql_errno(gDBPrimary) ||
        !cache_get_row_count(rows) || rows != 1 ||
        !cache_get_value_name_int(0, "scenario_kind", kind) ||
        !cache_get_value_name_int(0, "scenario_choice", choice))
    {
        if (c != Cache:0) cache_delete(c);
        return false;
    }
    cache_delete(c);
    return kind >= 0 && kind <= 3 && choice >= 0 && choice <= 2;
#else
    #pragma unused playerid, id
    return false;
#endif
}

stock CrimeScenario_StageLabel(kind, phase, output[], size)
{
    if (kind == 0)
    {
        format(output, size, phase == 1 ? ("Получить груз") : ("Передать груз"));
        return 1;
    }
    switch (kind)
    {
        case 1: format(output, size, phase == 1 ? ("Найти след курьера") : ("Выяснить судьбу курьера"));
        case 2: format(output, size, phase == 1 ? ("Осмотреть склад") : ("Составить заключение"));
        case 3: format(output, size, phase == 1 ? ("Подготовить переговоры") : ("Разрешить спор"));
        default: format(output, size, "Неизвестный сценарий");
    }
    return 1;
}

stock CrimeScenario_Choices(kind, phase, body[], size)
{
    // In DIALOG_STYLE_TABLIST_HEADERS the header is not selectable.
    switch (kind)
    {
        case 1:
        {
            if (phase == 1)
                format(body, size, "{FFD15C}Проверка\t{FFD15C}Что даст{FFFFFF}\nИзучить маршрутный журнал\tНадёжная улика\nОпросить случайного свидетеля\tНеполные сведения");
            else
                format(body, size, "{FFD15C}Решение\t{FFD15C}Последствие{FFFFFF}\nПередать полное досье\tДоверие и честная сделка\nСкрыть часть сведений\tПосредник заметит риск");
        }
        case 2:
        {
            if (phase == 1)
                format(body, size, "{FFD15C}Проверка\t{FFD15C}Что даст{FFFFFF}\nСверить складские записи\tДокументальная улика\nОсмотреть только пломбы\tКосвенные признаки");
            else
                format(body, size, "{FFD15C}Решение\t{FFD15C}Последствие{FFFFFF}\nСообщить обо всех нарушениях\tПрозрачный отчёт\nУмалчивать о находке\tСнижение доверия");
        }
        case 3:
        {
            if (phase == 1)
                format(body, size, "{FFD15C}Подготовка\t{FFD15C}Что даст{FFFFFF}\nПроверить расписку\tТочные условия сделки\nВыслушать одну сторону\tНеполная картина");
            else
                format(body, size, "{FFD15C}Решение\t{FFD15C}Последствие{FFFFFF}\nПредложить честное примирение\tРост доверия\nПоддержать одну сторону\tПодозрения посредника");
        }
        default: format(body, size, "Сценарий недоступен");
    }
    return 1;
}

 // Contract-specific circumstances are rolled once on acceptance and persist
 // through login/restart. They never grant money directly.
stock bool:CrimeScenario_EventInfo(playerid, id, &eventId)
{
    eventId = 0;
#if defined TARKENSO_DB_PRIMARY
    if (!Crime_Available(playerid) || id <= 0) return false;
    new q[220], rows = 0;
    mysql_format(gDBPrimary, q, sizeof(q),
        "SELECT scenario_event FROM crime_contracts WHERE account_id=%d AND contract_id=%d AND state IN(1,2) LIMIT 1",
        gDBPrimaryAccountId[playerid], id);
    new Cache:c = mysql_query(gDBPrimary, q);
    if (c == Cache:0 || mysql_errno(gDBPrimary) ||
        !cache_get_row_count(rows) || rows != 1 ||
        !cache_get_value_name_int(0, "scenario_event", eventId))
    {
        if (c != Cache:0) cache_delete(c);
        return false;
    }
    cache_delete(c);
    return eventId >= 0 && eventId <= 3;
#else
    #pragma unused playerid, id
    return false;
#endif
}

// Three independently varied situations for each of the 3 solo narratives.
// This text belongs in the route/dialog, not in client-trusted state.
stock CrimeScenario_EventText(kind, eventId, phase, output[], size)
{
    if (kind == 0)
    {
        format(output, size, "Обычная доставка. Груз ждёт получателя.");
        return 1;
    }
    switch (kind)
    {
        case 1:
        {
            switch (eventId)
            {
                case 1: format(output, size, phase == 1 ?
                    ("Курьера видели у товарной станции. Накладная может содержать маршрут.") :
                    ("Номер вагона не совпадает с показаниями. Решите, что сообщить заказчику."));
                case 2: format(output, size, phase == 1 ?
                    ("Телефон курьера обнаружен без SIM-карты. Свидетель помнит спор.") :
                    ("В свидетельских показаниях есть пробел. Соберите окончательное досье."));
                case 3: format(output, size, phase == 1 ?
                    ("В порту нашли чужую куртку с запиской. Время ухода неизвестно.") :
                    ("В записке указан другой адрес. Оцените, насколько надёжна улика."));
            }
        }
        case 2:
        {
            switch (eventId)
            {
                case 1: format(output, size, phase == 1 ?
                    ("В накладных склада недостаёт двух коробок. Пломбы выглядят целыми.") :
                    ("Вторая ведомость противоречит первой. Зафиксируйте итог проверки."));
                case 2: format(output, size, phase == 1 ?
                    ("Ночью сработала сигнализация, но журнал охраны пуст.") :
                    ("Охранник дал противоречивые объяснения. Решите, что сообщить."));
                case 3: format(output, size, phase == 1 ?
                    ("Найдена подмена номера контейнера на складе.") :
                    ("Часть документов подписана задним числом. Составьте заключение."));
            }
        }
        case 3:
        {
            switch (eventId)
            {
                case 1: format(output, size, phase == 1 ?
                    ("Получатель требует пересмотреть цену и отказывается от прежней расписки.") :
                    ("Условия расходятся с первоначальными. Предложите разрешение спора."));
                case 2: format(output, size, phase == 1 ?
                    ("Перевозчик опоздал, а заказчик обвиняет поставщика.") :
                    ("Обе стороны согласны встретиться. Решите, какой исход предложить."));
                case 3: format(output, size, phase == 1 ?
                    ("В договоре оказалась неверная дата. Одна сторона угрожает разрывом.") :
                    ("Исправленный документ готов. Выберите итог переговоров."));
            }
        }
    }
    if (eventId < 1 || eventId > 3)
        format(output, size, "События дела недоступны. Проверьте состояние контракта.");
    return 1;
}

stock CrimeScenario_InterviewChoices(kind, output[], size)
{
    switch (kind)
    {
        case 1: format(output, size,
            "{FFD15C}Вопрос\t{FFD15C}Подход{FFFFFF}\nСверить показания с найденной уликой\tПроверка фактов\nПотребовать объяснений немедленно\tДавление");
        case 2: format(output, size,
            "{FFD15C}Вопрос\t{FFD15C}Подход{FFFFFF}\nСопоставить запись и журнал охраны\tПроверка фактов\nОбвинить охранника в пропаже\tДавление");
        case 3: format(output, size,
            "{FFD15C}Вопрос\t{FFD15C}Подход{FFFFFF}\nУточнить условия по расписке\tПроверка фактов\nПотребовать немедленного согласия\tДавление");
        default: format(output, size, "Свидетель недоступен");
    }
    return 1;
}

// NPC testimony is deterministic for the frozen event, the evidence
// gathered earlier and the selected question. There is no client-side roll.
stock CrimeScenario_Testimony(kind, eventId, question, clueChoice, output[], size)
{
    new witness[65], subject[110], reaction[175];
    switch (kind)
    {
        case 1:
        {
            format(witness, sizeof(witness), "Свидетель исчезновения");
            switch (eventId)
            {
                case 1: format(subject, sizeof(subject), "Накладная указывает иной номер вагона.");
                case 2: format(subject, sizeof(subject), "Телефон исчезнувшего нашли без SIM-карты.");
                case 3: format(subject, sizeof(subject), "Записка на куртке противоречит маршруту.");
            }
        }
        case 2:
        {
            format(witness, sizeof(witness), "Работник склада");
            switch (eventId)
            {
                case 1: format(subject, sizeof(subject), "В учёте не хватает двух коробок.");
                case 2: format(subject, sizeof(subject), "Ночной сигнал тревоги не внесён в журнал.");
                case 3: format(subject, sizeof(subject), "Номер контейнера подменён в ведомости.");
            }
        }
        case 3:
        {
            format(witness, sizeof(witness), "Участник переговоров");
            switch (eventId)
            {
                case 1: format(subject, sizeof(subject), "Сумма в расписке не совпадает с требованием.");
                case 2: format(subject, sizeof(subject), "Перевозчик и заказчик спорят о сроке.");
                case 3: format(subject, sizeof(subject), "В договоре обнаружена неверная дата.");
            }
        }
    }
    if (question == 0)
        format(reaction, sizeof(reaction),
            "Это дело началось до обновления бесед. Предыдущие показания сохранены.");
    else if (question == 1)
    {
        if (clueChoice == 1)
            format(reaction, sizeof(reaction),
                "NPC сверил подробную улику и подтвердил расхождение. Досье стало убедительнее.");
        else
            format(reaction, sizeof(reaction),
                "NPC ответил на вопросы, но поверхностной улики недостаточно для полной проверки.");
    }
    else format(reaction, sizeof(reaction),
        "После давления NPC замкнулся. Свидетельство получено, но его надёжность вызывает сомнения.");
    format(output, size,
        "{FFD15C}СОБЕСЕДНИК:{FFFFFF} %s\n{FFD15C}ФАКТ:{FFFFFF} %s\n\n%s\n\n{FFD15C}ИТОГ:{FFFFFF} Подготовьте решение для посредника.",
        witness, subject, reaction);
    return 1;
}
