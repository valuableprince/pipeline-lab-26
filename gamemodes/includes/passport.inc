#if defined _tarkenso_passport_included
    #endinput
#endif
#define _tarkenso_passport_included

// PASSPORT-01. All accounts automatically have a non-expiring passport.
// Serial is a separate SQL AUTO_INCREMENT, not accounts.account_id.
#define PASSPORT_DLG_SELF       (7960)
#define PASSPORT_DLG_SHARED     (7961)
#define PASSPORT_DLG_BIRTH      (7962)
#define PASSPORT_SHOW_RADIUS    (3.0)
#define PASSPORT_SHOW_COOLDOWN  (5)

new gPassportID[MAX_PLAYERS];
new gPassportIssued[MAX_PLAYERS][11];
new gPassportBirth[MAX_PLAYERS][11];
new gPassportPendingBirth[MAX_PLAYERS][11];
new gPassportNextShow[MAX_PLAYERS];
new bool:gPassportBirthDialog[MAX_PLAYERS];

stock Passport_Reset(playerid)
{
    gPassportID[playerid] = 0;
    gPassportIssued[playerid][0] = EOS;
    gPassportBirth[playerid][0] = EOS;
    gPassportPendingBirth[playerid][0] = EOS;
    gPassportNextShow[playerid] = 0;
    gPassportBirthDialog[playerid] = false;
    return 1;
}

stock bool:Passport_ParseBirth(const input[], output[], size)
{
    // DD.MM.YYYY, 18-100 years old, fully validated calendar date.
    if (strlen(input) != 10 || size < 11 || input[2] != '.' || input[5] != '.')
        return false;
    for (new i = 0; i < 10; i++)
        if (i != 2 && i != 5 && (input[i] < '0' || input[i] > '9'))
            return false;
    new day = (input[0] - '0') * 10 + input[1] - '0';
    new month = (input[3] - '0') * 10 + input[4] - '0';
    new year = (input[6] - '0') * 1000 + (input[7] - '0') * 100 +
        (input[8] - '0') * 10 + input[9] - '0';
    if (month < 1 || month > 12) return false;
    new days = 31;
    switch (month)
    {
        case 4, 6, 9, 11: days = 30;
        case 2:
        {
            days = 28;
            if ((year % 4 == 0 && year % 100 != 0) || year % 400 == 0)
                days = 29;
        }
    }
    if (day < 1 || day > days) return false;
    new currentYear, currentMonth, currentDay;
    getdate(currentYear, currentMonth, currentDay);
    new age = currentYear - year;
    if (month > currentMonth || (month == currentMonth && day > currentDay))
        age--;
    if (age < 18 || age > 100) return false;
    format(output, size, "%04d-%02d-%02d", year, month, day);
    return true;
}

stock Passport_FriendlyDate(const iso[], output[], size)
{
    if (strlen(iso) != 10)
        format(output, size, "Не указана");
    else format(output, size, "%c%c.%c%c.%c%c%c%c",
        iso[8], iso[9], iso[5], iso[6], iso[0], iso[1], iso[2], iso[3]);
    return 1;
}

stock Passport_Region(region, output[], size)
{
    switch (region)
    {
        case REGION_FORT_CARSON: format(output, size, "Fort Carson");
        case REGION_LOS_SANTOS: format(output, size, "Los Santos");
        case REGION_PALOMINO: format(output, size, "Palomino Creek");
        default: format(output, size, "Не определено");
    }
    return 1;
}

#if defined TARKENSO_DB_PRIMARY
new bool:gPassportSchemaReady;

stock bool:Passport_Init()
{
    gPassportSchemaReady = false;
    if (!DBPrimary_IsReady()) return false;
    new Cache:c = mysql_query(gDBPrimary,
        "SELECT version FROM schema_migrations WHERE version=47 LIMIT 1");
    new count;
    if (c == Cache:0 || mysql_errno(gDBPrimary) ||
        !cache_get_row_count(count) || count != 1)
    {
        if (c != Cache:0) cache_delete(c);
        print("[PASSPORT] Migration 047 missing. Start rejected to avoid partial passport data.");
        return false;
    }
    cache_delete(c);
    gPassportSchemaReady = true;
    return true;
}

stock bool:Passport_Load(playerid)
{
    if (!gPassportSchemaReady || !DBPrimary_IsReady() ||
        !gDBPrimaryLoaded[playerid] || gDBPrimaryAccountId[playerid] <= 0)
        return false;

    new query[360], affected;
    mysql_format(gDBPrimary, query, sizeof(query),
        "INSERT INTO player_passports (account_id,issued_on) SELECT account_id,DATE(created_at) FROM accounts WHERE account_id=%d ON DUPLICATE KEY UPDATE account_id=account_id",
        gDBPrimaryAccountId[playerid]);
    if (!DBPrimary_Execute(query, affected)) return false;

    // New character DOB collected before bcrypt registration; persist it once.
    // Existing records cannot be overwritten by repeat registration.
    if (gPassportPendingBirth[playerid][0])
    {
        mysql_format(gDBPrimary, query, sizeof(query),
            "UPDATE player_passports SET birth_date='%e' WHERE account_id=%d AND birth_date IS NULL",
            gPassportPendingBirth[playerid], gDBPrimaryAccountId[playerid]);
        if (!DBPrimary_Execute(query, affected)) return false;
        gPassportPendingBirth[playerid][0] = EOS;
    }

    mysql_format(gDBPrimary, query, sizeof(query),
        "SELECT passport_id,CAST(issued_on AS CHAR(10)) AS issue_date,COALESCE(CAST(birth_date AS CHAR(10)),'') AS birth_date FROM player_passports WHERE account_id=%d LIMIT 1",
        gDBPrimaryAccountId[playerid]);
    new Cache:c = mysql_query(gDBPrimary, query);
    new rows, passportID, issueDate[11], birthDate[11];
    new bool:valid = c != Cache:0 && !mysql_errno(gDBPrimary);
    if (valid)
        valid = cache_get_row_count(rows) && rows == 1 &&
            cache_get_value_name_int(0, "passport_id", passportID) &&
            cache_get_value_name(0, "issue_date", issueDate, sizeof(issueDate)) &&
            cache_get_value_name(0, "birth_date", birthDate, sizeof(birthDate));
    if (c != Cache:0) cache_delete(c);
    if (!valid || passportID <= 0 || strlen(issueDate) != 10 ||
        (birthDate[0] && strlen(birthDate) != 10)) return false;

    gPassportID[playerid] = passportID;
    format(gPassportIssued[playerid], sizeof(gPassportIssued[]), "%s", issueDate);
    format(gPassportBirth[playerid], sizeof(gPassportBirth[]), "%s", birthDate);
    return true;
}

stock bool:Passport_SaveBirth(playerid, const birthISO[])
{
    if (!gPassportSchemaReady || !DBPrimary_IsReady() ||
        gPassportID[playerid] <= 0 || gPassportBirth[playerid][0])
        return false;
    new q[220], changed;
    mysql_format(gDBPrimary, q, sizeof(q),
        "UPDATE player_passports SET birth_date='%e' WHERE passport_id=%d AND account_id=%d AND birth_date IS NULL",
        birthISO, gPassportID[playerid], gDBPrimaryAccountId[playerid]);
    if (!DBPrimary_Execute(q, changed) || changed != 1) return false;
    format(gPassportBirth[playerid], sizeof(gPassportBirth[]), "%s", birthISO);
    return true;
}
#else
stock bool:Passport_Init() { return true; }
stock bool:Passport_Load(playerid)
{
    #pragma unused playerid
    return false;
}
stock bool:Passport_SaveBirth(playerid, const birthISO[])
{
    #pragma unused playerid, birthISO
    return false;
}
#endif

stock Passport_OnReady(playerid)
{
    if (!Passport_Load(playerid))
        SendClientMessage(playerid, COLOR_RED,
            "Паспорт пока недоступен. Обратитесь к администрации.");
    return 1;
}

stock Passport_Build(playerid, body[], size)
{
    new region[40], birth[20], issued[20];
    Passport_Region(gStartRegion[playerid], region, sizeof(region));
    Passport_FriendlyDate(gPassportBirth[playerid], birth, sizeof(birth));
    Passport_FriendlyDate(gPassportIssued[playerid], issued, sizeof(issued));
    format(body, size,
        "{D5B477}TARKENSO ROLEPLAY GAME\n{8EACBC}ГОСУДАРСТВЕННЫЙ РЕЕСТР НАСЕЛЕНИЯ\n{8EACBC}____________________________________\n\n{8EACBC}Имя гражданина: {FFFFFF}%s\n\n{8EACBC}Категория резидента: {D5B477}%d\n\n{8EACBC}Номер паспорта: {D5B477}TK-%06d\n\n{8EACBC}Дата рождения: {FFFFFF}%s\n{8EACBC}Гражданство: {FFFFFF}Штат Сан-Андреас\n{8EACBC}Место регистрации: {FFFFFF}%s\n{8EACBC}Дата выдачи: {FFFFFF}%s\n{8EACBC}Срок действия: {FFFFFF}Бессрочно\n\n{D5B477}ОФИЦИАЛЬНЫЙ ДОКУМЕНТ  {8EACBC}|  TARKENSO",
        gAccountName[playerid], gCharacterLevel[playerid], gPassportID[playerid],
        birth, region, issued);
    return 1;
}

stock Passport_BirthPrompt(playerid, bool:invalid = false)
{
    if (gAuthState[playerid] != AUTH_READY || gPassportID[playerid] <= 0 ||
        gPassportBirth[playerid][0]) return 1;
    new message[360];
    format(message, sizeof(message),
        "{D5B477}ДАТА РОЖДЕНИЯ ПЕРСОНАЖА\n\n{FFFFFF}Для завершения паспорта укажите дату рождения.\n{8EACBC}Формат: ДД.ММ.ГГГГ, возраст 18-100 лет.\n{8EACBC}Дату можно указать только один раз.\n\n%s{FFFFFF}Введите дату рождения:",
        invalid ? "{E57373}Некорректная дата. Проверьте календарь и возраст.\n\n" : "");
    gPassportBirthDialog[playerid] = true;
    ShowPlayerDialog(playerid, PASSPORT_DLG_BIRTH, DIALOG_STYLE_INPUT,
        "{D5B477}TARKENSO | Данные паспорта", message, "Сохранить", "Позже");
    return 1;
}

stock Passport_Open(playerid)
{
    if (gAuthState[playerid] != AUTH_READY || !gAccountSpawned[playerid] ||
        gAccountDead[playerid]) return 1;
    if (!gPassportID[playerid])
    {
        if (!Passport_Load(playerid))
            return SendClientMessage(playerid, COLOR_RED,
                "Паспорт недоступен: проверьте соединение с реестром.");
    }
    if (!gPassportBirth[playerid][0]) return Passport_BirthPrompt(playerid);
    new body[850];
    Passport_Build(playerid, body, sizeof(body));
    return ShowPlayerDialog(playerid, PASSPORT_DLG_SELF, DIALOG_STYLE_MSGBOX,
        "{D5B477}TARKENSO | Паспорт гражданина", body, "Закрыть", "");
}

stock Passport_Show(playerid, target)
{
    if (gAuthState[playerid] != AUTH_READY || !gAccountSpawned[playerid] ||
        gAccountDead[playerid] || !gPassportID[playerid] ||
        !gPassportBirth[playerid][0])
        return SendClientMessage(playerid, COLOR_RED,
            "Сначала оформите данные паспорта через /pass.");
    if (target == playerid || !IsPlayerConnected(target) || IsPlayerNPC(target) ||
        gAuthState[target] != AUTH_READY || !gAccountSpawned[target] ||
        gAccountDead[target])
        return SendClientMessage(playerid, COLOR_RED,
            "Укажите другого авторизованного игрока.");
    if (GetPlayerVirtualWorld(playerid) != GetPlayerVirtualWorld(target) ||
        GetPlayerInterior(playerid) != GetPlayerInterior(target))
        return SendClientMessage(playerid, COLOR_RED, "Игрок находится в другом мире.");
    new Float:x, Float:y, Float:z;
    GetPlayerPos(playerid, x, y, z);
    if (!IsPlayerInRangeOfPoint(target, PASSPORT_SHOW_RADIUS, x, y, z))
        return SendClientMessage(playerid, COLOR_RED,
            "Подойдите к игроку ближе (не далее 3 метров).");
    if (gettime() < gPassportNextShow[playerid])
        return SendClientMessage(playerid, COLOR_RED,
            "Не показывайте паспорт слишком часто.");
    new body[850];
    Passport_Build(playerid, body, sizeof(body));
    if (!ShowPlayerDialog(target, PASSPORT_DLG_SHARED, DIALOG_STYLE_MSGBOX,
        "{D5B477}TARKENSO | Предъявленный паспорт", body, "Закрыть", ""))
        return SendClientMessage(playerid, COLOR_RED, "Не удалось показать паспорт.");
    gPassportNextShow[playerid] = gettime() + PASSPORT_SHOW_COOLDOWN;
    SendClientMessage(playerid, 0xD5B477FF, "Вы предъявили паспорт игроку.");
    return 1;
}

stock Passport_OnDialog(playerid, dialogid, response, const inputtext[])
{
    if (dialogid == PASSPORT_DLG_SELF || dialogid == PASSPORT_DLG_SHARED)
        return 1;
    if (dialogid != PASSPORT_DLG_BIRTH) return 0;
    if (!gPassportBirthDialog[playerid] ||
        gAuthState[playerid] != AUTH_READY) return 1;
    gPassportBirthDialog[playerid] = false;
    if (!response) return 1;
    if (gPassportBirth[playerid][0]) return 1;
    new iso[11];
    if (!Passport_ParseBirth(inputtext, iso, sizeof(iso)))
        return Passport_BirthPrompt(playerid, true);
    if (!Passport_SaveBirth(playerid, iso))
        return SendClientMessage(playerid, COLOR_RED,
            "Не удалось сохранить дату рождения. Попробуйте /pass ещё раз.");
    return Passport_Open(playerid);
}
