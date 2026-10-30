// AUTH-UI-01: диалоги в стиле игрового архива; без TextDraw.
// Цветовые вставки {RRGGBB} поддерживаются стандартными диалогами SA-MP/open.mp.

// Только активный диалог может изменять состояние авторизации.
stock AuthDialog_Present(playerid, dialogid, DIALOG_STYLE:style,
    const title[], const body[], const action[], const cancel[])
{
    gAuthDialogOpen[playerid] = false;
    if (!ShowPlayerDialog(playerid, dialogid, style, title, body, action, cancel))
        return 0;
    gAuthDialogOpen[playerid] = true;
    return 1;
}

stock AuthDialog_Intro(playerid)
{
    new nickname[MAX_PLAYER_NAME], body[640];
    GetPlayerName(playerid, nickname, sizeof(nickname));
    format(body, sizeof(body),
        "{3B82F6}Tarkenso RPG  {94A3B8}/  СИГНАЛ: 03:17\n{94A3B8}________________________________________________\n\n{FFFFFF}Добро пожаловать, {3B82F6}%s{FFFFFF}.\n\n{E6BB79}ПИСЬМО БЕЗ ОТПРАВИТЕЛЯ\n{FFFFFF}На ваше имя пришёл конверт без обратного адреса.\nВнутри лежат ключ от номера 17 и старая фотография.\n\n{94A3B8}На обороте написано:\n{FFFFFF}«Если ты читаешь это, значит, они снова начали».\n\n{3B82F6}Откройте личное дело, чтобы начать историю.",
        nickname);
    return AuthDialog_Present(playerid, DIALOG_INTRO, DIALOG_STYLE_MSGBOX,
        "{3B82F6}Tarkenso RPG  {FFFFFF}|  Пролог",
        body, "Открыть", "Выйти");
}

stock AuthDialog_Register(playerid, error = 0)
{
    new nickname[MAX_PLAYER_NAME], body[640], errorText[130];
    GetPlayerName(playerid, nickname, sizeof(nickname));
    errorText[0] = EOS;
    if (error == 1)
        format(errorText, sizeof(errorText),
            "{EF4444}ОШИБКА: пароль должен содержать 8-64 символа.\n\n");
    else if (error == 2)
        format(errorText, sizeof(errorText),
            "{EF4444}ОШИБКА: пароли не совпали. Начните заново.\n\n");

    format(body, sizeof(body),
        "{3B82F6}РЕГИСТРАЦИЯ  {94A3B8}/  ШАГ 1 ИЗ 4\n{94A3B8}________________________________________________\n\n{FFFFFF}Создание личного дела\n\n{94A3B8}Игровой ник: {FFFFFF}%s\n{94A3B8}Статус: {E6BB79}новый игрок\n\n%s{FFFFFF}Придумайте надёжный пароль.\n{94A3B8}Длина: 8-64 символа без пробелов.\nЛатиница, цифры, специальные символы.\n\n{34D399}Пароль будет защищён bcrypt и не попадёт в журнал.\n\n{3B82F6}Введите пароль в поле ниже:",
        nickname, errorText);
    return AuthDialog_Present(playerid, DIALOG_REGISTER, DIALOG_STYLE_PASSWORD,
        "{3B82F6}Tarkenso RPG  {FFFFFF}|  Регистрация",
        body, "Продолжить", "Назад");
}

stock AuthDialog_Confirm(playerid)
{
    new body[440];
    format(body, sizeof(body),
        "{3B82F6}РЕГИСТРАЦИЯ  {94A3B8}/  ШАГ 2 ИЗ 4\n{94A3B8}________________________________________________\n\n{FFFFFF}Контрольный отпечаток\n\n{FFFFFF}Повторите пароль из предыдущего шага.\n{94A3B8}Это необходимо для проверки совпадения.\n\n{E6BB79}Не сообщайте пароль другим игрокам.\n\n{3B82F6}Введите пароль ещё раз:");
    return AuthDialog_Present(playerid, DIALOG_CONFIRM, DIALOG_STYLE_PASSWORD,
        "{3B82F6}Tarkenso RPG  {FFFFFF}|  Подтверждение",
        body, "Подтвердить", "Назад");
}

stock AuthDialog_Background(playerid)
{
    return AuthDialog_Present(playerid, DIALOG_BACKGROUND, DIALOG_STYLE_LIST,
        "{3B82F6}Tarkenso RPG  {FFFFFF}|  Память",
        "{E6BB79}Я не помню ночь аварии\n{FFFFFF}В радиоэфире кто-то назвал моё имя\n{94A3B8}Подпись в письме была не её",
        "Выбрать", "Назад");
}

stock AuthDialog_Region(playerid)
{
    return AuthDialog_Present(playerid, DIALOG_REGION, DIALOG_STYLE_LIST,
        "{3B82F6}Tarkenso RPG  {FFFFFF}|  Место прибытия",
        "{E6BB79}Форт-Карсон {94A3B8}— городок в пустыне\n{FFFFFF}Лос-Сантос {94A3B8}— большой город\n{FFFFFF}Паломино-Крик {94A3B8}— тихий посёлок",
        "Выбрать", "Назад");
}

stock AuthDialog_BirthDate(playerid, error = 0)
{
    new body[440];
    format(body, sizeof(body),
        "{D5B477}РЕГИСТРАЦИЯ  {8EACBC}/  ШАГ 4 ИЗ 4\n\n"
        "{FFFFFF}Дата рождения вашего персонажа\n\n"
        "{8EACBC}Введите дату в формате ДД.ММ.ГГГГ.\n"
        "Допустимый возраст: 18-100 лет.\n"
        "После оформления паспорт выдаётся автоматически.\n\n%s"
        "{FFFFFF}Дата рождения:",
        error ? "{E57373}Некорректная дата. Попробуйте ещё раз.\n\n" : "");
    return AuthDialog_Present(playerid, DIALOG_BIRTH, DIALOG_STYLE_INPUT,
        "{D5B477}TARKENSO | Паспорт", body, "Продолжить", "Назад");
}

stock AuthDialog_Login(playerid, failures = 0)
{
    new nickname[MAX_PLAYER_NAME], body[670], errorText[160];
    GetPlayerName(playerid, nickname, sizeof(nickname));
    errorText[0] = EOS;
    if (failures > 0)
    {
        format(errorText, sizeof(errorText),
            "{EF4444}Неверный пароль. Осталось попыток: %d.\n\n",
            3 - failures);
    }
    format(body, sizeof(body),
        "{3B82F6}АВТОРИЗАЦИЯ  {94A3B8}/  ЛИЧНОЕ ДЕЛО\n{94A3B8}________________________________________________\n\n{FFFFFF}С возвращением!\n{94A3B8}Войдите, чтобы продолжить расследование.\n\n{94A3B8}Игровой ник: {FFFFFF}%s\n{94A3B8}Статус: {34D399}аккаунт найден\n\n%s{FFFFFF}Для подтверждения личности введите пароль.\n{94A3B8}Три ошибки подряд блокируют вход на 5 минут.\n\n{3B82F6}Введите пароль в поле ниже:",
        nickname, errorText);
    return AuthDialog_Present(playerid, DIALOG_LOGIN, DIALOG_STYLE_PASSWORD,
        "{3B82F6}Tarkenso RPG  {FFFFFF}|  Вход",
        body, "Войти", "Выход");
}

stock AuthDialog_Success(playerid, bool:registered)
{
    new nickname[MAX_PLAYER_NAME], body[550];
    GetPlayerName(playerid, nickname, sizeof(nickname));
    if (registered)
    {
        format(body, sizeof(body),
            "{34D399}АККАУНТ СОЗДАН\n{94A3B8}________________________________________________\n\n{FFFFFF}Добро пожаловать, {3B82F6}%s{FFFFFF}!\n\n{FFFFFF}Ваше личное дело сохранено.\n{34D399}Пароль защищён, регистрация завершена.\n\n{E6BB79}История начинается в выбранном вами населённом пункте.\n\n{3B82F6}Нажмите «Начать», чтобы войти в игру.", nickname);
    }
    else
    {
        format(body, sizeof(body),
            "{34D399}ЛИЧНОСТЬ ПОДТВЕРЖДЕНА\n{94A3B8}________________________________________________\n\n{FFFFFF}С возвращением, {3B82F6}%s{FFFFFF}!\n\n{34D399}Доступ к личному делу восстановлен.\n{94A3B8}Ваше последнее воспоминание сохранено.\n\n{3B82F6}Нажмите «Продолжить», чтобы войти в игру.", nickname);
    }
    return AuthDialog_Present(playerid, DIALOG_SUCCESS, DIALOG_STYLE_MSGBOX,
        "{34D399}Tarkenso RPG  {FFFFFF}|  Доступ открыт",
        body, registered ? "Начать" : "Продолжить", "");
}
