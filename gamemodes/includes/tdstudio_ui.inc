#if defined _tarkenso_tds_ui
    #endinput
#endif
#define _tarkenso_tds_ui

static const gTDSFieldNames[][] =
{
    "Координаты X Y", "Размер области W H", "Размер букв X Y", "Текст / спрайт",
    "Цвет RRGGBBAA", "Прозрачность 0..255", "Шрифт 0..3", "Выравнивание 1..3",
    "TextDraw Box", "Цвет Box RRGGBBAA", "Цвет фона RRGGBBAA", "Обводка 0..4",
    "Тень 0..4", "Пропорциональный шрифт", "Выбор мышью", "Модель",
    "Поворот X Y Z и Zoom", "Цвета автомобиля 0..255"
};

stock TDS_Dialog(playerid, dialog, DIALOG_STYLE:style, const title[], const body[],
    const accept[] = "Выбрать", const cancel[] = "Назад")
{
    new s = TDS_RequireSession(playerid);
    if (s < 0) return 0;
    if (gTDS[s][tsMode] != TDS_MENU_MODE && !TDS_SetMode(playerid, TDS_MENU_MODE)) return 0;
    gTDS[s][tsDialog] = dialog;
    if (!ShowPlayerDialog(playerid, dialog, style, title, "%s", accept, cancel, body))
    {
        gTDS[s][tsDialog] = 0;
        return 0;
    }
    return 1;
}

stock TDS_Main(playerid)
{
    new s = TDS_RequireSession(playerid);
    if (s < 0) return 0;
    new title[90];
    if (gTDS[s][tsProject]) format(title, sizeof(title), "TD Studio | %s | %d/%d",
        gTDS[s][tsName], gTDS[s][tsCount], TDS_ELEMENTS);
    else format(title, sizeof(title), "TARKENSO TextDraw Studio | Этап 1");
    return TDS_Dialog(playerid, TDS_MAIN, DIALOG_STYLE_LIST, title,
        "Создать проект\nПросмотр проекта / холст\nДобавить элемент\nСписок элементов\nСвойства выбранного элемента\nШаг перемещения\nПредпросмотр\nЗакрыть редактор", "Открыть", "Закрыть");
}

stock TDS_List(playerid)
{
    new s = TDS_RequireSession(playerid);
    if (s < 0 || !gTDS[s][tsProject]) return 0;
    new body[4096], line[96], rows;
    for (new i; i < TDS_ELEMENTS; i++)
    {
        if (!TDS_ElementValid(s, i)) continue;
        gTDS[s][tsRows][rows++] = i;
        format(line, sizeof(line), "ID %d | %s | %.1f, %.1f%s\n", gTDSElement[s][i][tdId],
            gTDSTypes[_:gTDSElement[s][i][tdType]], gTDSElement[s][i][tdX],
            gTDSElement[s][i][tdY], gTDS[s][tsSelected] == i ? (" *") : (""));
        strcat(body, line, sizeof(body));
    }
    gTDS[s][tsRowCount] = rows;
    if (!rows)
    {
        SendClientMessage(playerid, TDS_COLOR, "В проекте пока нет элементов. Добавьте первый элемент.");
        return TDS_Main(playerid);
    }
    return TDS_Dialog(playerid, TDS_LIST, DIALOG_STYLE_LIST, "TD Studio | Элементы", body);
}

stock TDS_Props(playerid)
{
    new s = TDS_RequireSession(playerid);
    if (s < 0) return 0;
    new e = gTDS[s][tsSelected];
    if (!TDS_ElementValid(s, e)) return TDS_List(playerid);
    new title[100], body[2048], line[100], rows;
    format(title, sizeof(title), "ID %d | X %.1f Y %.1f | %.1f x %.1f",
        gTDSElement[s][e][tdId], gTDSElement[s][e][tdX], gTDSElement[s][e][tdY],
        gTDSElement[s][e][tdW], gTDSElement[s][e][tdH]);
    for (new field; field <= _:TDS_VEH_COLOURS; field++)
    {
        if (!TDS_FieldAllowed(gTDSElement[s][e][tdType], E_TDS_FIELD:field)) continue;
        gTDS[s][tsRows][rows++] = field;
        format(line, sizeof(line), "%s", gTDSFieldNames[field]);
        switch (E_TDS_FIELD:field)
        {
            case TDS_BOX: format(line, sizeof(line), "%s: %d", gTDSFieldNames[field], _:gTDSElement[s][e][tdBox]);
            case TDS_SELECTABLE: format(line, sizeof(line), "%s: %d", gTDSFieldNames[field], _:gTDSElement[s][e][tdSelectable]);
            case TDS_PROPORTIONAL: format(line, sizeof(line), "%s: %d", gTDSFieldNames[field], _:gTDSElement[s][e][tdProportional]);
            case TDS_COLOUR: format(line, sizeof(line), "%s: %08x", gTDSFieldNames[field], gTDSElement[s][e][tdColour]);
        }
        strcat(body, line, sizeof(body));
        strcat(body, "\n", sizeof(body));
    }
    gTDS[s][tsRows][rows++] = -1;
    strcat(body, "Удалить элемент\n", sizeof(body));
    gTDS[s][tsRows][rows++] = -2;
    strcat(body, "На холст: перемещение мышью по кнопкам", sizeof(body));
    gTDS[s][tsRowCount] = rows;
    return TDS_Dialog(playerid, TDS_PROPS, DIALOG_STYLE_LIST, title, body);
}

stock TDS_Input(playerid, E_TDS_FIELD:field)
{
    new s = TDS_RequireSession(playerid);
    if (s < 0) return 0;
    new e = gTDS[s][tsSelected];
    if (!TDS_ElementValid(s, e) || !TDS_FieldAllowed(gTDSElement[s][e][tdType], field)) return TDS_Main(playerid);
    gTDS[s][tsField] = field;
    if (field == TDS_BOX || field == TDS_SELECTABLE || field == TDS_PROPORTIONAL)
    {
        TDS_Toggle(playerid, field);
        return TDS_Props(playerid);
    }
    new body[512];
    if (field == TDS_CONTENT && gTDSElement[s][e][tdType] == TDS_SPRITE_TYPE)
    {
        for (new i; i < sizeof(gTDSSprites); i++)
        {
            strcat(body, gTDSSprites[i], sizeof(body)); strcat(body, "\n", sizeof(body));
        }
        return TDS_Dialog(playerid, TDS_SPRITES, DIALOG_STYLE_LIST, "TD Studio | Стандартные TXD", body);
    }
    if (field == TDS_MODEL)
    {
        for (new i; i < sizeof(gTDSModels); i++)
        {
            strcat(body, gTDSModelNames[i], sizeof(body)); strcat(body, "\n", sizeof(body));
        }
        return TDS_Dialog(playerid, TDS_MODELS, DIALOG_STYLE_LIST, "TD Studio | Модели", body);
    }
    switch (field)
    {
        case TDS_POSITION: format(body, sizeof(body), "Сейчас: %.3f %.3f\nВведите X Y через пробел. X: 0..640, Y: 0..448.", gTDSElement[s][e][tdX], gTDSElement[s][e][tdY]);
        case TDS_SIZE: format(body, sizeof(body), "Сейчас: %.3f %.3f\nВведите W H через пробел. W: 0.1..640, H: 0.1..448.\nУ текста это область Box/выбора, размер букв задаётся отдельно.", gTDSElement[s][e][tdW], gTDSElement[s][e][tdH]);
        case TDS_LETTERS: format(body, sizeof(body), "Сейчас: %.3f %.3f\nРазмер букв X Y. X: 0.01..5, Y: 0.01..10.", gTDSElement[s][e][tdLetterX], gTDSElement[s][e][tdLetterY]);
        case TDS_CONTENT: format(body, sizeof(body), "Сейчас: %s\nТекст: 1..127 символов. Допустимы ~r~ ~g~ ~b~ ~w~ ~y~ ~p~ ~l~ ~h~ ~n~.", gTDSElement[s][e][tdText]);
        case TDS_COLOUR, TDS_BOX_COLOUR, TDS_BACKGROUND: format(body, sizeof(body), "Введите ровно 8 HEX-символов RRGGBBAA (без # и 0x).\nНапример FFFFFFFF; последние AA — прозрачность: 00..FF.");
        case TDS_ROTATION: format(body, sizeof(body), "Сейчас: %.1f %.1f %.1f %.2f\nВведите X Y Z Zoom: углы -360..360, Zoom 0.1..10.", gTDSElement[s][e][tdRotX], gTDSElement[s][e][tdRotY], gTDSElement[s][e][tdRotZ], gTDSElement[s][e][tdZoom]);
        case TDS_VEH_COLOURS: format(body, sizeof(body), "Сейчас: %d %d\nВведите два целых ID цветов автомобиля: 0..255.", gTDSElement[s][e][tdVehColour1], gTDSElement[s][e][tdVehColour2]);
        default: format(body, sizeof(body), "Введите целое число из диапазона в заголовке.\nВыравнивание: 1 — слева, 2 — центр, 3 — справа.");
    }
    return TDS_Dialog(playerid, TDS_INPUT, DIALOG_STYLE_INPUT, gTDSFieldNames[_:field], body, "Применить");
}

stock TDS_StepMenu(playerid)
{
    return TDS_Dialog(playerid, TDS_STEP, DIALOG_STYLE_LIST, "TD Studio | Шаг перемещения", "0.1\n1.0\n5.0\n10.0");
}

stock TDS_ConfirmClose(playerid)
{
    new s = TDS_RequireSession(playerid);
    if (s < 0) return 0;
    if (!gTDS[s][tsProject]) return TDS_Close(playerid);
    return TDS_Dialog(playerid, TDS_CLOSE, DIALOG_STYLE_MSGBOX, "TD Studio | Закрыть проект?",
        "Проект хранится только в памяти текущей сессии.\nЗакрытие удалит проект и все его элементы.", "Удалить", "Назад");
}

stock TDS_Open(playerid)
{
    if (!TDS_Allowed(playerid))
    {
        TDS_Close(playerid);
        return SendClientMessage(playerid, ADMIN_ERROR, "TextDraw Studio: нужен доступ разработчика. Основатель: /aduty; коллеги: /tdgrant ID 1.");
    }
    new now = GetTickCount();
    if (gTDSOpenLimited[playerid] && now - gTDSOpenTick[playerid] < 500) return 1;
    gTDSOpenLimited[playerid] = true; gTDSOpenTick[playerid] = now;
    new s = TDS_Session(playerid);
    if (s < 0)
    {
        if (TDS_Active(playerid)) TDS_Close(playerid);
        if (!TDSHost_CanOpen(playerid))
            return SendClientMessage(playerid, TDS_COLOR, "Завершите текущий интерфейс и выйдите пешком в игровой мир.");
        s = TDS_Allocate(playerid);
        if (s < 0) return SendClientMessage(playerid, ADMIN_ERROR, "Недостаточно ресурсов TextDraw Studio (до 8 сессий).");
    }
    if (gTDS[s][tsMode] == TDS_PREVIEW_MODE) return TDS_SetMode(playerid, TDS_EDIT_MODE);
    return TDS_Main(playerid);
}

stock TDS_OnDialog(playerid, dialogid, response, listitem, const input[])
{
    if (dialogid < TDS_FIRST_DIALOG || dialogid > TDS_LAST_DIALOG) return 0;
    new s = TDS_RequireSession(playerid);
    if (s < 0 || gTDS[s][tsDialog] != dialogid || (response != 0 && response != 1)) return 1;
    // Consume once before dispatch: stale/forged responses cannot call another menu.
    gTDS[s][tsDialog] = 0;
    if (!TDS_Action(s))
        return SendClientMessage(playerid, TDS_COLOR, "Слишком часто. /tdstudio — продолжить.");
    if (!response)
    {
        switch (dialogid)
        {
            case TDS_MAIN: return TDS_ConfirmClose(playerid);
            case TDS_INPUT, TDS_SPRITES, TDS_MODELS, TDS_DELETE: return TDS_Props(playerid);
            case TDS_PROPS: return TDS_List(playerid);
            default: return TDS_Main(playerid);
        }
    }
    switch (dialogid)
    {
        case TDS_MAIN:
        {
            if (listitem < 0 || listitem > 7) return TDS_Main(playerid);
            if (listitem == 0)
            {
                if (gTDS[s][tsProject])
                {
                    SendClientMessage(playerid, TDS_COLOR, "Для нового проекта закройте текущий проект.");
                    return TDS_Main(playerid);
                }
                return TDS_Dialog(playerid, TDS_NEW, DIALOG_STYLE_INPUT, "TD Studio | Новый проект",
                    "Название: 1..32 символа.\nНа этапе 1 проект будет удалён при закрытии редактора или отключении.", "Создать");
            }
            if (listitem == 7) return TDS_ConfirmClose(playerid);
            if (!gTDS[s][tsProject])
            {
                SendClientMessage(playerid, TDS_COLOR, "Сначала создайте проект.");
                return TDS_Main(playerid);
            }
            switch (listitem)
            {
                case 1: return TDS_SetMode(playerid, TDS_EDIT_MODE);
                case 2: return TDS_Dialog(playerid, TDS_ADD, DIALOG_STYLE_LIST, "TD Studio | Добавить", "Текст\nПрямоугольник\nСпрайт\nМодель");
                case 3: return TDS_List(playerid);
                case 4: return TDS_Props(playerid);
                case 5: return TDS_StepMenu(playerid);
                case 6: return TDS_SetMode(playerid, TDS_PREVIEW_MODE);
            }
        }
        case TDS_NEW:
        {
            if (!TDS_NewProject(playerid, input))
                SendClientMessage(playerid, ADMIN_ERROR, "Название: 1..32 печатных символа без { } ~.");
            return TDS_Main(playerid);
        }
        case TDS_ADD:
        {
            if (listitem < 0 || listitem > 3) return TDS_Main(playerid);
            if (TDS_AddElement(playerid, E_TDS_TYPE:listitem) < 0)
            {
                SendClientMessage(playerid, ADMIN_ERROR, "Элемент не создан: лимит 64 или закончились PlayerTextDraw.");
                return TDS_Main(playerid);
            }
            return TDS_Props(playerid);
        }
        case TDS_LIST:
        {
            if (listitem < 0 || listitem >= gTDS[s][tsRowCount]) return TDS_List(playerid);
            new slot = gTDS[s][tsRows][listitem];
            if (!TDS_ElementValid(s, slot)) return TDS_List(playerid);
            gTDS[s][tsSelected] = slot;
            return TDS_Props(playerid);
        }
        case TDS_PROPS:
        {
            if (listitem < 0 || listitem >= gTDS[s][tsRowCount]) return TDS_Props(playerid);
            new field = gTDS[s][tsRows][listitem];
            if (field == -1) return TDS_Dialog(playerid, TDS_DELETE, DIALOG_STYLE_MSGBOX,
                "TD Studio | Удалить элемент?", "Выбранный элемент будет удалён из проекта.", "Удалить");
            if (field == -2) return TDS_SetMode(playerid, TDS_EDIT_MODE);
            if (field < 0 || field > _:TDS_VEH_COLOURS) return TDS_Props(playerid);
            return TDS_Input(playerid, E_TDS_FIELD:field);
        }
        case TDS_INPUT:
        {
            if (!TDS_ApplyInput(playerid, gTDS[s][tsField], input))
            {
                if (TDS_Session(playerid) < 0)
                    return SendClientMessage(playerid, ADMIN_ERROR, "Ресурс TextDraw недоступен. Сессия редактора закрыта.");
                SendClientMessage(playerid, ADMIN_ERROR, "Неверное значение. Изменение не применено.");
                return TDS_Input(playerid, gTDS[s][tsField]);
            }
            return TDS_Props(playerid);
        }
        case TDS_STEP:
        {
            if (listitem >= 0 && listitem < sizeof(gTDSSteps)) gTDS[s][tsStep] = listitem;
            return TDS_SetMode(playerid, TDS_EDIT_MODE);
        }
        case TDS_SPRITES, TDS_MODELS:
        {
            new e = gTDS[s][tsSelected];
            if (!TDS_ElementValid(s, e)) return TDS_Main(playerid);
            if (dialogid == TDS_SPRITES)
            {
                if (gTDSElement[s][e][tdType] != TDS_SPRITE_TYPE || listitem < 0 || listitem >= sizeof(gTDSSprites)) return TDS_Props(playerid);
                gTDSElement[s][e][tdSprite] = listitem;
            }
            else
            {
                if (gTDSElement[s][e][tdType] != TDS_MODEL_TYPE || listitem < 0 || listitem >= sizeof(gTDSModels)) return TDS_Props(playerid);
                gTDSElement[s][e][tdModel] = gTDSModels[listitem];
            }
            if (!TDS_RenderElement(s, e)) return TDS_Close(playerid);
            return TDS_Props(playerid);
        }
        case TDS_DELETE:
        {
            TDS_DeleteElement(playerid, gTDS[s][tsSelected]);
            return TDS_List(playerid);
        }
        case TDS_CLOSE: return TDS_Close(playerid);
    }
    return 1;
}

stock TDS_OnClick(playerid, PlayerText:clicked)
{
    new s = TDS_RequireSession(playerid);
    if (s < 0 || !gTDS[s][tsMouse] || gTDS[s][tsMode] != TDS_EDIT_MODE) return 0;
    if (clicked == INVALID_PLAYER_TEXT_DRAW) return 1;
    for (new tool = 1; tool < TDS_TOOLS; tool++)
    {
        if (clicked != gTDSTool[s][tool]) continue;
        if (!TDS_Action(s)) return 1;
        new Float:step = gTDSSteps[gTDS[s][tsStep]];
        switch (tool)
        {
            case 1: return TDS_Main(playerid);
            case 2: return TDS_List(playerid);
            case 3: TDS_Move(playerid, -step, 0.0);
            case 4: TDS_Move(playerid, step, 0.0);
            case 5: TDS_Move(playerid, 0.0, -step);
            case 6: TDS_Move(playerid, 0.0, step);
            case 7: return TDS_StepMenu(playerid);
            case 8: return TDS_SetMode(playerid, TDS_PREVIEW_MODE);
        }
        return 1;
    }
    for (new e; e < TDS_ELEMENTS; e++)
    {
        if (!TDS_ElementValid(s, e) || clicked != gTDSDraw[s][e]) continue;
        if (!TDS_Action(s)) return 1;
        gTDS[s][tsSelected] = e;
        TDS_RenderStatus(s);
        return 1;
    }
    // Consume unknown handles while this editor owns selection; never route them to sorting.
    return 1;
}

stock TDS_OnCancel(playerid, Text:clicked)
{
    new s = TDS_RequireSession(playerid);
    if (s < 0 || !gTDS[s][tsMouse] || clicked != Text:INVALID_TEXT_DRAW) return 0;
    TDS_StopMouse(s, false); // ESC already cancelled; no recursive cancellation
    return TDS_Main(playerid);
}
