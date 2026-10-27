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
    if (gTDS[s][tsBusy])
        return TDS_Dialog(playerid,TDS_MAIN,DIALOG_STYLE_MSGBOX,"TD Studio | Операция выполняется",
            "Дождитесь ответа БД или завершения экспорта.\nРедактирование временно заблокировано. Закрытие сессии допустимо.","Закрыть","Остаться");
    new title[96];
    if (gTDS[s][tsProject]) format(title,sizeof(title),"TD Studio | %s%s | #%d",
        gTDS[s][tsName],gTDS[s][tsDirty] ? (" *") : (""),gTDS[s][tsProjectId]);
    else format(title,sizeof(title),"TARKENSO TextDraw Studio | Этап 2");
    return TDS_Dialog(playerid,TDS_MAIN,DIALOG_STYLE_LIST,title,
        "Создать проект\nОткрыть проект\nСохранить\nСохранить как новый проект\nПереименовать\nУдалить проект из БД\nЭкспортировать Pawn\nРедактировать интерфейс\nПараметры проекта\nЗакрыть редактор\nИстория действий\nБиблиотека шаблонов");
}

stock TDS_ContinueLeave(playerid)
{
    new s = TDS_RequireSession(playerid);
    if (s < 0 || gTDS[s][tsBusy]) return 0;
    if (gTDS[s][tsPending] == 2) return TDSDB_Load(playerid,gTDS[s][tsPendingProject]);
    TDS_ClearProject(s);
    return TDS_Dialog(playerid,TDS_NEW,DIALOG_STYLE_INPUT,"TD Studio | Новый проект",
        "Введите название: 1..32 символа. После создания сохраните проект через главное меню.","Создать");
}

stock TDS_Leave(playerid, action)
{
    new s = TDS_RequireSession(playerid);
    if (s < 0 || gTDS[s][tsBusy]) return 0;
    gTDS[s][tsPending] = action;
    if (gTDS[s][tsDirty]) return TDS_Dialog(playerid,TDS_DISCARD,DIALOG_STYLE_MSGBOX,
        "TD Studio | Несохранённые изменения", "Текущие изменения будут потеряны. Продолжить?", "Продолжить");
    return TDS_ContinueLeave(playerid);
}

stock TDS_Settings(playerid)
{
    new s = TDS_RequireSession(playerid);
    if (s < 0 || !gTDS[s][tsProject]) return TDS_Main(playerid);
    new body[512], line[384];
    format(body,sizeof(body),"Тип: %s\nИзменить описание\n",gTDS[s][tsInterface] ? ("PlayerTextDraw") : ("TextDraw"));
    format(line,sizeof(line),"ID %d | Создатель %d | Ревизия %d\nСоздан: %s\nИзменён: %s\nОписание: %s",
        gTDS[s][tsProjectId],gTDS[s][tsCreator],gTDS[s][tsRevision],gTDS[s][tsCreated],gTDS[s][tsUpdated],gTDS[s][tsDescription]);
    strcat(body,line,sizeof(body));
    return TDS_Dialog(playerid,TDS_SETTINGS,DIALOG_STYLE_LIST,"TD Studio | Параметры проекта",body);
}

stock TDS_EditMenu(playerid)
{
    new s = TDS_RequireSession(playerid);
    if (s < 0) return 0;
    new title[90];
    if (gTDS[s][tsProject]) format(title, sizeof(title), "TD Studio | %s | %d/%d",
        gTDS[s][tsName], gTDS[s][tsCount], TDS_ELEMENTS);
    else format(title, sizeof(title), "TARKENSO TextDraw Studio | Этап 2");
    return TDS_Dialog(playerid, TDS_EDIT_MENU, DIALOG_STYLE_LIST, title,
        "Главное меню проекта\nПросмотр проекта / холст\nДобавить элемент\nСписок элементов / слоёв\nСвойства активного элемента\nШаг перемещения\nПредпросмотр\nПеремещение выделения (клавиатура)\nЗакрыть редактор\nСлои\nВыделение\nГруппы\nДублировать активный элемент\nГеометрия: размер / масштаб / сетка\nИстория действий\nБиблиотека шаблонов", "Открыть", "Закрыть");
}

stock TDS_List(playerid)
{
    new s = TDS_RequireSession(playerid);
    if (s < 0 || !gTDS[s][tsProject]) return 0;
    new body[4096], line[96], rows, slots[TDS_ELEMENTS], count = TDS_Order(s, slots);
    for (new layer; layer < count; layer++)
    {
        new i = slots[layer];
        gTDS[s][tsRows][rows++] = i;
        format(line, sizeof(line), "L%d ID%d %s G%d%s\n", layer, gTDSElement[s][i][tdId],
            gTDSTypes[_:gTDSElement[s][i][tdType]], gTDSElement[s][i][tdGroup], gTDSPicked[s][i] ? (" *") : (""));
        strcat(body, line, sizeof(body));
    }
    gTDS[s][tsRowCount] = rows;
    if (!rows)
    {
        TDSHost_Message(playerid, TDS_COLOR, "В проекте пока нет элементов. Добавьте первый элемент.");
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
    strcat(body, "На холст: перемещение мышью по кнопкам\n", sizeof(body));
    gTDS[s][tsRows][rows++] = -3;
    strcat(body, "Перемещение элемента (клавиатура)", sizeof(body));
    gTDS[s][tsRows][rows++] = -4;
    strcat(body, "\nДублировать активный элемент", sizeof(body));
    gTDS[s][tsRows][rows++] = -5;
    strcat(body, "\nГеометрия: точные параметры / resize / scale", sizeof(body));
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
    new body[96], line[16];
    for (new i; i < sizeof(gTDSSteps); i++)
    {
        format(line, sizeof(line), "%.1f\n", gTDSSteps[i]);
        strcat(body, line, sizeof(body));
    }
    return TDS_Dialog(playerid, TDS_STEP, DIALOG_STYLE_LIST, "TD Studio | Шаг перемещения", body);
}

stock TDS_ConfirmClose(playerid)
{
    new s = TDS_RequireSession(playerid);
    if (s < 0) return 0;
    if (!gTDS[s][tsDirty] && !gTDS[s][tsBusy]) return TDS_Close(playerid);
    new body[384];
    format(body,sizeof(body),"Несохранённые изменения будут потеряны.\nСохранённая версия в БД при закрытии не удаляется.");
    if (gTDS[s][tsBusy] > 0) strcat(body,"\nУже отправленный запрос БД может завершиться после закрытия.",sizeof(body));
    if (gTDS[s][tsBusy] == -1) strcat(body,"\nНезавершённый экспорт будет отменён.",sizeof(body));
    return TDS_Dialog(playerid,TDS_CLOSE,DIALOG_STYLE_MSGBOX,"TD Studio | Закрыть проект?",body,"Закрыть","Назад");
}

stock TDS_Open(playerid)
{
    if (!TDS_Allowed(playerid))
    {
        TDS_Close(playerid);
        return TDSHost_Message(playerid, ADMIN_ERROR, "TextDraw Studio: нужен доступ разработчика. Основатель: /aduty; коллеги: /tdgrant ID 1.");
    }
    new now = GetTickCount();
    if (gTDSOpenLimited[playerid] && now - gTDSOpenTick[playerid] < 500) return 1;
    gTDSOpenLimited[playerid] = true; gTDSOpenTick[playerid] = now;
    new s = TDS_Session(playerid);
    if (s < 0)
    {
        if (TDS_Active(playerid)) TDS_Close(playerid);
        if (!TDSHost_CanOpen(playerid))
            return TDSHost_Message(playerid, TDS_COLOR, "Завершите текущий интерфейс и выйдите пешком в игровой мир.");
        s = TDS_Allocate(playerid);
        if (s < 0) return TDSHost_Message(playerid, ADMIN_ERROR, "Недостаточно ресурсов TextDraw Studio (до 8 сессий).");
    }
    if (gTDS[s][tsMode] == TDS_MOVE_MODE)
    {
        // Explicit escape hatch when no TextDraw cursor is active (ESC is local).
        TDSInput_Finish(playerid, false);
        return 1;
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
        return TDSHost_Message(playerid, TDS_COLOR, "Слишком часто. /tdstudio — продолжить.");
    if (gTDS[s][tsBusy] && dialogid != TDS_CLOSE)
    {
        if (dialogid == TDS_MAIN && response) return TDS_ConfirmClose(playerid);
        return TDS_Main(playerid);
    }
    if (dialogid >= TDS_TEMPLATE_MENU) return TDS_OnTemplateDialog(playerid, dialogid, response, listitem, input);
    if (dialogid >= TDS_HISTORY_MENU) return TDS_OnHistoryDialog(playerid, dialogid, response, listitem);
    if (dialogid >= TDS_GEOMETRY) return TDS_OnGeoDialog(playerid, dialogid, response, listitem, input);
    if (dialogid >= TDS_LAYERS) return TDS_OnLayoutDialog(playerid, dialogid, response, listitem, input);
    if (!response)
    {
        switch (dialogid)
        {
            case TDS_MAIN: return TDS_ConfirmClose(playerid);
            case TDS_INPUT, TDS_SPRITES, TDS_MODELS, TDS_DELETE: return TDS_Props(playerid);
            case TDS_PROPS: return TDS_List(playerid);
            case TDS_EDIT_MENU, TDS_PROJECTS, TDS_RENAME, TDS_SETTINGS, TDS_DESC_DIALOG, TDS_PROJECT_DELETE, TDS_DISCARD: return TDS_Main(playerid);
            default: return TDS_Main(playerid);
        }
    }
    switch (dialogid)
    {
        case TDS_MAIN:
        {
            if (listitem < 0 || listitem > 11) return TDS_Main(playerid);
            if (listitem == 11) return TDS_TplMenu(playerid);
            if (listitem == 0) return TDS_Leave(playerid,1);
            if (listitem == 1) return TDSDB_List(playerid);
            if (listitem == 9) return TDS_ConfirmClose(playerid);
            if (!gTDS[s][tsProject]) return TDS_Main(playerid);
            switch (listitem)
            {
                case 2: return TDSDB_Save(playerid);
                case 3: return TDSDB_Save(playerid,true);
                case 4: return TDS_Dialog(playerid,TDS_RENAME,DIALOG_STYLE_INPUT,"TD Studio | Переименовать", "Введите новое название (1..32 символа):", "Применить");
                case 5:
                {
                    if (!gTDS[s][tsProjectId])
                    {
                        TDSHost_Message(playerid,TDS_COLOR,"Проект ещё не сохранён. Для удаления черновика закройте редактор.");
                        return TDS_Main(playerid);
                    }
                    return TDS_Dialog(playerid,TDS_PROJECT_DELETE,DIALOG_STYLE_MSGBOX,"TD Studio | Удалить проект из БД?",
                        "Сохранённая версия и текущие несохранённые изменения будут удалены. Действие необратимо.","Удалить");
                }
                case 6: return TDS_Export(playerid);
                case 7: return TDS_EditMenu(playerid);
                case 8: return TDS_Settings(playerid);
                case 10: return TDS_HMenu(playerid);
            }
        }
        case TDS_PROJECTS:
        {
            if (listitem < 0 || listitem > gTDS[s][tsProjectRowCount]+1) return TDS_Main(playerid);
            if (listitem < gTDS[s][tsProjectRowCount])
            {
                gTDS[s][tsPendingProject] = gTDS[s][tsProjectRows][listitem];
                return TDS_Leave(playerid,2);
            }
            if (listitem == gTDS[s][tsProjectRowCount] && gTDS[s][tsMore]) return TDSDB_List(playerid,gTDS[s][tsPage]+1);
            if (listitem == gTDS[s][tsProjectRowCount]+1 && gTDS[s][tsPage] > 0) return TDSDB_List(playerid,gTDS[s][tsPage]-1);
            TDSHost_Message(playerid,TDS_COLOR,"Других страниц в этом направлении нет.");
            return TDS_Main(playerid);
        }
        case TDS_DISCARD: return TDS_ContinueLeave(playerid);
        case TDS_RENAME:
        {
            if (!TDS_ValidName(input))
            {
                TDSHost_Message(playerid,ADMIN_ERROR,"Недопустимое название проекта.");
                return TDS_Main(playerid);
            }
            format(gTDS[s][tsName],TDS_NAME,"%s",input); gTDS[s][tsDirty] = true;
            TDS_HRecord(s, TDS_H_META);
            return TDS_Main(playerid);
        }
        case TDS_SETTINGS:
        {
            if (listitem == 0)
            {
                gTDS[s][tsInterface] = 1-gTDS[s][tsInterface]; gTDS[s][tsDirty] = true;
                TDS_HRecord(s, TDS_H_META);
                return TDS_Settings(playerid);
            }
            if (listitem == 1) return TDS_Dialog(playerid,TDS_DESC_DIALOG,DIALOG_STYLE_INPUT,"TD Studio | Описание", "До 128 печатных символов. Пустая строка удаляет описание.","Применить");
            return TDS_Main(playerid);
        }
        case TDS_DESC_DIALOG:
        {
            if (input[0] && !TDS_SafeText(input,TDS_DESCRIPTION-1)) return TDS_Settings(playerid);
            format(gTDS[s][tsDescription],TDS_DESCRIPTION,"%s",input); gTDS[s][tsDirty] = true;
            TDS_HRecord(s, TDS_H_META);
            return TDS_Settings(playerid);
        }
        case TDS_PROJECT_DELETE: return TDSDB_Delete(playerid);
        case TDS_EDIT_MENU:
        {
            if (listitem < 0 || listitem > 15) return TDS_Main(playerid);
            if (listitem == 0) return TDS_Main(playerid);
            if (listitem == 8) return TDS_ConfirmClose(playerid);
            if (!gTDS[s][tsProject])
            {
                TDSHost_Message(playerid, TDS_COLOR, "Сначала создайте проект.");
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
                case 7: return TDSInput_Start(playerid);
                case 9: return TDS_LayersMenu(playerid);
                case 10: return TDS_SelectionMenu(playerid);
                case 11: return TDS_GroupList(playerid);
                case 12:
                {
                    if (TDS_Duplicate(playerid) < 0) TDSHost_Message(playerid, ADMIN_ERROR, "Копия не создана: выберите элемент, проверьте лимит и ресурсы.");
                    return TDS_EditMenu(playerid);
                }
                case 13: return TDS_GeoMenu(playerid);
                case 14: return TDS_HMenu(playerid);
                case 15: return TDS_TplMenu(playerid);
            }
        }
        case TDS_NEW:
        {
            if (!TDS_NewProject(playerid, input))
                TDSHost_Message(playerid, ADMIN_ERROR, "Название: 1..32 печатных символа без { } ~.");
            return TDS_Main(playerid);
        }
        case TDS_ADD:
        {
            if (listitem < 0 || listitem > 3) return TDS_Main(playerid);
            if (TDS_AddElement(playerid, E_TDS_TYPE:listitem) < 0)
            {
                TDSHost_Message(playerid, ADMIN_ERROR, "Элемент не создан: лимит 64 или закончились PlayerTextDraw.");
                return TDS_Main(playerid);
            }
            return TDS_Props(playerid);
        }
        case TDS_LIST:
        {
            if (listitem < 0 || listitem >= gTDS[s][tsRowCount]) return TDS_List(playerid);
            new slot = gTDS[s][tsRows][listitem];
            if (!TDS_ElementValid(s, slot)) return TDS_List(playerid);
            TDS_Select(playerid, slot);
            return TDS_Props(playerid);
        }
        case TDS_PROPS:
        {
            if (listitem < 0 || listitem >= gTDS[s][tsRowCount]) return TDS_Props(playerid);
            new field = gTDS[s][tsRows][listitem];
            if (field == -1) return TDS_Dialog(playerid, TDS_DELETE, DIALOG_STYLE_MSGBOX,
                "TD Studio | Удалить элемент?", "Выбранный элемент будет удалён из проекта.", "Удалить");
            if (field == -2) return TDS_SetMode(playerid, TDS_EDIT_MODE);
            if (field == -3) return TDSInput_Start(playerid);
            if (field == -4)
            {
                if (TDS_Duplicate(playerid) < 0) TDSHost_Message(playerid, ADMIN_ERROR, "Копия не создана: лимит или нехватка хэндлов.");
                return TDS_Props(playerid);
            }
            if (field == -5) return TDS_GeoMenu(playerid);
            if (field < 0 || field > _:TDS_VEH_COLOURS) return TDS_Props(playerid);
            return TDS_Input(playerid, E_TDS_FIELD:field);
        }
        case TDS_INPUT:
        {
            if (!TDS_ApplyInput(playerid, gTDS[s][tsField], input))
            {
                if (TDS_Session(playerid) < 0)
                    return TDSHost_Message(playerid, ADMIN_ERROR, "Ресурс TextDraw недоступен. Сессия редактора закрыта.");
                TDSHost_Message(playerid, ADMIN_ERROR, "Неверное значение. Изменение не применено.");
                return TDS_Input(playerid, gTDS[s][tsField]);
            }
            return TDS_Props(playerid);
        }
        case TDS_STEP:
        {
            TDS_SetStep(playerid, listitem);
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
            gTDS[s][tsDirty] = true;
            TDS_HRecord(s, TDS_H_PROP+_:TDS_CONTENT, gTDSElement[s][e][tdId]);
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
    if (s >= 0 && gTDS[s][tsMode] == TDS_MOVE_MODE) return 1;
    if (s < 0 || !gTDS[s][tsMouse] || gTDS[s][tsMode] != TDS_EDIT_MODE) return 0;
    if (clicked == INVALID_PLAYER_TEXT_DRAW) return 1;
    for (new tool; tool < TDS_TOOLS; tool++)
    {
        if (clicked != gTDSTool[s][tool]) continue;
        if (!TDS_Action(s)) return 1;
        new Float:step = gTDSSteps[gTDS[s][tsStep]];
        switch (tool)
        {
            case 0: return TDSInput_Start(playerid);
            case 1: return TDS_Main(playerid);
            case 2: return TDS_List(playerid);
            case 3: TDS_Move(playerid, -step, 0.0);
            case 4: TDS_Move(playerid, step, 0.0);
            case 5: TDS_Move(playerid, 0.0, -step);
            case 6: TDS_Move(playerid, 0.0, step);
            case 7: return TDS_StepMenu(playerid);
            case 8: return TDS_SetMode(playerid, TDS_PREVIEW_MODE);
            case 9,10:
            {
                if (!TDS_HStep(playerid,tool == 10)) TDSHost_Message(playerid,TDS_COLOR,"Нет доступного действия или не хватает ресурсов.");
            }
        }
        return 1;
    }
    for (new e; e < TDS_ELEMENTS; e++)
    {
        if (!TDS_ElementValid(s, e) || clicked != gTDSDraw[s][e]) continue;
        if (!TDS_Action(s)) return 1;
        TDS_Select(playerid, e, gTDS[s][tsMultiPick]);
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
