#if defined _tarkenso_tds_layout_ui
    #endinput
#endif
#define _tarkenso_tds_layout_ui

stock TDS_LayersMenu(playerid)
{
    return TDS_Dialog(playerid, TDS_LAYERS, DIALOG_STYLE_LIST, "TD Studio | Слои выделения",
        "Список: от заднего к переднему\nПоднять на один слой\nОпустить на один слой\nНа передний план\nНа задний план");
}

stock TDS_SelectionMenu(playerid)
{
    new s = TDS_LayoutSession(playerid);
    if (s < 0) return 0;
    new title[96], body[384];
    format(title, sizeof(title), "Выделено: %d / %d | активный ID — для свойств", TDS_PickCount(s), gTDS[s][tsCount]);
    format(body, sizeof(body), "Список: добавить / убрать\nМножественный выбор мышью: %s\nВыделить все\nСнять выделение\nНа холст",
        gTDS[s][tsMultiPick] ? ("ВКЛ") : ("ВЫКЛ"));
    return TDS_Dialog(playerid, TDS_SELECTION, DIALOG_STYLE_LIST, title, body);
}

stock TDS_PickList(playerid, bool:members = false)
{
    new s = TDS_LayoutSession(playerid);
    if (s < 0) return 0;
    if (members && TDS_GroupSlot(s, gTDS[s][tsGroupFocus]) < 0) return TDS_GroupList(playerid);
    new body[4096], line[96], slots[TDS_ELEMENTS], count = TDS_Order(s, slots);
    if (!count) return TDS_EditMenu(playerid);
    for (new i; i < count; i++)
    {
        new e = slots[i]; gTDS[s][tsRows][i] = e;
        format(line, sizeof(line), "%s L%d ID%d %s G%d\n",
            (members ? (gTDSElement[s][e][tdGroup] == gTDS[s][tsGroupFocus]) : gTDSPicked[s][e]) ? ("[+]") : ("[ ]"),
            i, gTDSElement[s][e][tdId], gTDSTypes[_:gTDSElement[s][e][tdType]], gTDSElement[s][e][tdGroup]);
        strcat(body, line, sizeof(body));
    }
    gTDS[s][tsRowCount] = count;
    return TDS_Dialog(playerid, members ? TDS_GROUP_MEMBERS : TDS_PICK_LIST, DIALOG_STYLE_LIST,
        members ? ("Группа: нажмите элемент для включения/исключения") : ("Выделение: нажмите элемент для переключения"), body);
}

stock TDS_GroupList(playerid)
{
    new s = TDS_LayoutSession(playerid);
    if (s < 0) return 0;
    new body[2048], line[96], rows;
    format(body, sizeof(body), "Создать из выделения (%d элементов)\n", TDS_PickCount(s));
    gTDS[s][tsRows][rows++] = 0;
    for (new g; g < TDS_GROUPS; g++) if (gTDSGroup[s][g][tgId])
    {
        new id = gTDSGroup[s][g][tgId]; gTDS[s][tsRows][rows++] = id;
        format(line, sizeof(line), "#%d | %s | %d элементов\n", id, gTDSGroup[s][g][tgName], TDS_GroupMembers(s, id));
        strcat(body, line, sizeof(body));
    }
    gTDS[s][tsRowCount] = rows;
    return TDS_Dialog(playerid, TDS_GROUP_LIST, DIALOG_STYLE_LIST, "TD Studio | Группы (до 16)", body);
}

stock TDS_GroupMenu(playerid)
{
    new s = TDS_LayoutSession(playerid);
    if (s < 0) return 0;
    new id = gTDS[s][tsGroupFocus], g = TDS_GroupSlot(s, id);
    if (g < 0) return TDS_GroupList(playerid);
    new title[100];
    format(title, sizeof(title), "G%d %s | %d | шаг %.1f", id, gTDSGroup[s][g][tgName],
        TDS_GroupMembers(s, id), gTDSSteps[gTDS[s][tsStep]]);
    return TDS_Dialog(playerid, TDS_GROUP_MENU, DIALOG_STYLE_LIST, title,
        "Выделить всю группу\nПереименовать\nСостав: посмотреть / добавить / исключить\nРасформировать\nПереместить группу\nСлои группы");
}

stock TDS_GroupNameDialog(playerid, bool:rename)
{
    new s = TDS_LayoutSession(playerid);
    if (s < 0) return 0;
    if (rename && TDS_GroupSlot(s, gTDS[s][tsGroupFocus]) < 0) return TDS_GroupList(playerid);
    gTDS[s][tsRenameGroup] = rename;
    return TDS_Dialog(playerid, TDS_GROUP_NAME, DIALOG_STYLE_INPUT,
        rename ? ("TD Studio | Имя группы") : ("TD Studio | Создать группу"),
        "Уникальное имя: 1..32 символа. Например TARKENSO_LOGO.\nДля создания нужны хотя бы 2 выделенных элемента без других групп.", "Применить");
}

stock TDS_OnLayoutDialog(playerid, dialog, response, row, const input[])
{
    new s = TDS_LayoutSession(playerid);
    if (s < 0) return 1;
    if (!response)
    {
        switch (dialog)
        {
            case TDS_PICK_LIST: return TDS_SelectionMenu(playerid);
            case TDS_GROUP_MENU, TDS_GROUP_NAME: return TDS_GroupList(playerid);
            case TDS_GROUP_MEMBERS, TDS_GROUP_DELETE: return TDS_GroupMenu(playerid);
            default: return TDS_EditMenu(playerid);
        }
    }
    switch (dialog)
    {
        case TDS_LAYERS:
        {
            if (row == 0) return TDS_List(playerid);
            if (row < 1 || row > 4) return TDS_LayersMenu(playerid);
            if (!TDS_Layer(playerid, E_TDS_LAYER:(row-1)))
                SendClientMessage(playerid, ADMIN_ERROR, "Выберите элементы. При нехватке хэндлов прежние слои сохранены.");
            return TDS_LayersMenu(playerid);
        }
        case TDS_SELECTION:
        {
            switch (row)
            {
                case 0: return TDS_PickList(playerid);
                case 1: { gTDS[s][tsMultiPick] = !gTDS[s][tsMultiPick]; return TDS_SetMode(playerid, TDS_EDIT_MODE); }
                case 2: TDS_SelectAll(playerid, true);
                case 3: TDS_SelectAll(playerid, false);
                case 4: return TDS_SetMode(playerid, TDS_EDIT_MODE);
            }
            return TDS_SelectionMenu(playerid);
        }
        case TDS_PICK_LIST, TDS_GROUP_MEMBERS:
        {
            if (row < 0 || row >= gTDS[s][tsRowCount]) return TDS_EditMenu(playerid);
            new e = gTDS[s][tsRows][row];
            if (!TDS_ElementValid(s, e)) return TDS_EditMenu(playerid);
            if (dialog == TDS_PICK_LIST) { TDS_Select(playerid, e, true); return TDS_PickList(playerid); }
            new id = gTDS[s][tsGroupFocus];
            if (!TDS_GroupAssign(playerid, id, e, gTDSElement[s][e][tdGroup] != id))
                SendClientMessage(playerid, ADMIN_ERROR, "Элемент уже в другой группе. Сначала исключите его из неё.");
            return TDS_PickList(playerid, true);
        }
        case TDS_GROUP_LIST:
        {
            if (row < 0 || row >= gTDS[s][tsRowCount]) return TDS_GroupList(playerid);
            new id = gTDS[s][tsRows][row];
            if (!id) return TDS_GroupNameDialog(playerid, false);
            if (TDS_GroupSlot(s, id) < 0) return TDS_GroupList(playerid);
            gTDS[s][tsGroupFocus] = id;
            return TDS_GroupMenu(playerid);
        }
        case TDS_GROUP_NAME:
        {
            new bool:ok;
            if (gTDS[s][tsRenameGroup]) ok = TDS_GroupRename(playerid, gTDS[s][tsGroupFocus], input);
            else ok = TDS_GroupCreate(playerid, input) > 0;
            if (!ok)
            {
                SendClientMessage(playerid, ADMIN_ERROR, "Группа не изменена: проверьте имя, выделение, принадлежность и лимит 16 групп.");
                return TDS_GroupNameDialog(playerid, gTDS[s][tsRenameGroup]);
            }
            return TDS_GroupMenu(playerid);
        }
        case TDS_GROUP_MENU:
        {
            new id = gTDS[s][tsGroupFocus];
            if (TDS_GroupSlot(s, id) < 0) return TDS_GroupList(playerid);
            switch (row)
            {
                case 0: { TDS_GroupSelect(playerid, id); return TDS_SetMode(playerid, TDS_EDIT_MODE); }
                case 1: return TDS_GroupNameDialog(playerid, true);
                case 2: return TDS_PickList(playerid, true);
                case 3: return TDS_Dialog(playerid, TDS_GROUP_DELETE, DIALOG_STYLE_MSGBOX, "TD Studio | Расформировать группу?",
                    "Все TextDraw останутся на холсте с прежними свойствами и слоями.", "Расформировать");
                case 4: { if (TDS_GroupSelect(playerid, id)) return TDSInput_Start(playerid); }
                case 5: { TDS_GroupSelect(playerid, id); return TDS_LayersMenu(playerid); }
            }
            return TDS_GroupMenu(playerid);
        }
        case TDS_GROUP_DELETE: { TDS_GroupDissolve(playerid, gTDS[s][tsGroupFocus]); return TDS_GroupList(playerid); }
    }
    return 1;
}
