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
