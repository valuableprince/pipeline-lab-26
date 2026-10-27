#if defined _tarkenso_tds_geometry_ui
    #endinput
#endif
#define _tarkenso_tds_geometry_ui

stock TDS_GeoMenu(playerid)
{
    new s = TDS_LayoutSession(playerid);
    if (s < 0) return 0;
    new body[1600];
    format(body, sizeof(body), "Параметры и границы выделения\nИзменить размер области активного элемента\nИзменить размер букв (текст)\nМасштабировать выделение / группу\nТочные X Y W H LetterX LetterY (активный)\nТочная позиция опорной точки выделения\nТочный масштаб X Y\nВыровнять выбранные элементы\nРаспределить по горизонтали\nРаспределить по вертикали\nПозиция относительно экрана\nСетка и Snap\nСохранять пропорции: %s\nОпора: %s\nШаг изменения размера: %.1f\nОсь изменения: %d (0=обе, 1=W, 2=H)\nОриентир выравнивания: %s\nВыровнять ранее выбранную группу по активному\nНа холст",
        gTDS[s][tsKeepRatio] ? ("ВКЛ") : ("ВЫКЛ"), gTDS[s][tsAnchorCenter] ? ("Центр") : ("Левый верх"),
        gTDSSteps[gTDS[s][tsResizeStep]], gTDS[s][tsGeoAxis], gTDS[s][tsAlignActive] ? ("Активный элемент") : ("Границы выделения"));
    return TDS_Dialog(playerid, TDS_GEOMETRY, DIALOG_STYLE_LIST, "TD Studio | Геометрия", body);
}

stock TDS_GeoInfo(playerid)
{
    new s = TDS_LayoutSession(playerid), slots[TDS_ELEMENTS];
    if (s < 0) return 0;
    new count = TDS_Targets(s, slots), Float:l, Float:t, Float:r, Float:b, Float:px, Float:py;
    if (!TDS_GeoBounds(s, slots, count, l, t, r, b)) return TDS_GeoMenu(playerid);
    TDS_GeoPivot(s, slots, count, px, py);
    new e = gTDS[s][tsSelected], body[1100], line[512], g = TDS_GroupSlot(s, gTDS[s][tsSelectedGroup]);
    format(body, sizeof(body), "Активный ID %d (%s)\nX %.3f  Y %.3f\nОбласть W %.3f  H %.3f\n",
        gTDSElement[s][e][tdId], gTDSTypes[_:gTDSElement[s][e][tdType]],
        gTDSElement[s][e][tdX], gTDSElement[s][e][tdY], gTDSElement[s][e][tdW], gTDSElement[s][e][tdH]);
    if (gTDSElement[s][e][tdType] == TDS_TEXT_TYPE)
    {
        format(line, sizeof(line), "LetterSize %.3f %.3f | alignment %d\nТекст: границы области, не измерение глифов/переносов.\n",
            gTDSElement[s][e][tdLetterX], gTDSElement[s][e][tdLetterY], gTDSElement[s][e][tdAlign]);
        strcat(body, line, sizeof(body));
    }
    if (g >= 0)
    {
        format(line, sizeof(line), "Группа G%d: %s\n", gTDSGroup[s][g][tgId], gTDSGroup[s][g][tgName]);
        strcat(body, line, sizeof(body));
    }
    format(line, sizeof(line), "Выбрано %d | границы %.3f %.3f — %.3f %.3f\nРазмер набора %.3f x %.3f\nОпора: %s | %.3f %.3f\nШаг движения %.1f | размера %.1f\nСетка %.1f | Snap %d | пропорции %d\nЭкран 640x480; якоря формата 2: X 0..640, Y 0..448.",
        count, l, t, r, b, r-l, b-t, gTDS[s][tsAnchorCenter] ? ("Центр") : ("Левый верх"),
        px, py,
        gTDSSteps[gTDS[s][tsStep]], gTDSSteps[gTDS[s][tsResizeStep]], gTDSGridSteps[gTDS[s][tsGridStep]], _:gTDS[s][tsSnap], _:gTDS[s][tsKeepRatio]);
    strcat(body, line, sizeof(body));
    return TDS_Dialog(playerid, TDS_GEO_INFO, DIALOG_STYLE_MSGBOX, "TD Studio | Геометрия и ограничения", body, "Назад", "Закрыть");
}

stock TDS_GeoInput(playerid, field)
{
    new s = TDS_LayoutSession(playerid);
    if (s < 0) return 0;
    gTDS[s][tsGeoField] = field;
    new body[384];
    switch (field)
    {
        case 0: format(body, sizeof(body), "Введите 6 чисел: X Y W H LetterX LetterY.\nТочные размеры без автоматического сохранения пропорций.\nSnap применяется к X/Y, если включён. Для не-текста LetterSize не меняется.");
        case 1: format(body, sizeof(body), "Введите X Y выбранной опоры группы/набора.\nДля одиночного элемента — его якорь TextDraw.\nПри Snap координаты округляются к сетке. Изменение применяется ко всему выделению.");
        case 2: format(body, sizeof(body), "Введите множители X Y, например 1.5 1.5 или 0.5 0.5.\nОпора фиксируется перед применением. При включённых пропорциях используется X для обеих осей.\nТекст: область и LetterSize масштабируются; контур/тень/Zoom не меняются.");
        case 3: format(body, sizeof(body), "Отступ от границ виртуального экрана 640x480: 0..224.\nЯкорные ограничения формата 2 сохраняются; невозможное выравнивание отклоняется.");
        default: return TDS_GeoMenu(playerid);
    }
    return TDS_Dialog(playerid, TDS_GEO_INPUT, DIALOG_STYLE_INPUT, "TD Studio | Точная геометрия", body, "Применить");
}

stock TDS_GeoAlignMenu(playerid)
{
    return TDS_Dialog(playerid, TDS_GEO_ALIGN, DIALOG_STYLE_LIST, "TD Studio | Выравнивание",
        "По левому краю\nПо правому краю\nПо верхнему краю\nПо нижнему краю\nПо горизонтальному центру\nПо вертикальному центру");
}

stock TDS_GeoScreenMenu(playerid)
{
    return TDS_Dialog(playerid, TDS_GEO_SCREEN, DIALOG_STYLE_LIST, "TD Studio | Экран 640x480 (виртуальный)",
        "Центр экрана\nСлева\nСправа\nСверху\nСнизу\nЦентр сверху\nЦентр снизу\nНастроить отступ");
}

stock TDS_GeoGridMenu(playerid)
{
    new s = TDS_LayoutSession(playerid);
    if (s < 0) return 0;
    new body[384];
    format(body, sizeof(body), "2 единицы\n5 единиц\n10 единиц\n20 единиц\nSnap: %s\nЛокальная сетка (6 направляющих): %s\nСейчас шаг %.1f. Сетка и Snap независимы; включение не меняет проект.",
        gTDS[s][tsSnap] ? ("ВКЛ") : ("ВЫКЛ"), gTDS[s][tsGridShow] ? ("ВКЛ") : ("ВЫКЛ"), gTDSGridSteps[gTDS[s][tsGridStep]]);
    return TDS_Dialog(playerid, TDS_GEO_GRID, DIALOG_STYLE_LIST, "TD Studio | Grid / Snap", body);
}

stock TDS_GeoError(playerid)
{
    return SendClientMessage(playerid, ADMIN_ERROR, "Геометрия не изменена. Проверьте выделение, размеры и пределы X 0..640 / Y 0..448; распределение требует свободных промежутков.");
}

stock TDS_OnGeoDialog(playerid, dialog, response, row, const input[])
{
    new s = TDS_LayoutSession(playerid);
    if (s < 0) return 1;
    if (!response || dialog == TDS_GEO_INFO) return TDS_GeoMenu(playerid);
    switch (dialog)
    {
        case TDS_GEOMETRY:
        {
            switch (row)
            {
                case 0: return TDS_GeoInfo(playerid);
                case 1: return TDSInput_Start(playerid, TDS_TRANS_RESIZE);
                case 2:
                {
                    new e = gTDS[s][tsSelected];
                    if (!TDS_ElementValid(s, e) || gTDSElement[s][e][tdType] != TDS_TEXT_TYPE) TDS_GeoError(playerid);
                    else return TDSInput_Start(playerid, TDS_TRANS_RESIZE, true);
                }
                case 3: return TDSInput_Start(playerid, TDS_TRANS_SCALE);
                case 4,5,6: return TDS_GeoInput(playerid, row-4);
                case 7: { gTDS[s][tsAlignGroup] = false; return TDS_GeoAlignMenu(playerid); }
                case 8,9: { if (!TDS_GeoDistribute(playerid, row == 8)) TDS_GeoError(playerid); }
                case 10: return TDS_GeoScreenMenu(playerid);
                case 11: return TDS_GeoGridMenu(playerid);
                case 12: gTDS[s][tsKeepRatio] = !gTDS[s][tsKeepRatio];
                case 13: { gTDS[s][tsAnchorCenter] = !gTDS[s][tsAnchorCenter]; gTDSGridCached[s] = false; }
                case 14: return TDS_Dialog(playerid, TDS_GEO_STEP, DIALOG_STYLE_LIST, "TD Studio | Шаг изменения размера", "0.1\n0.5\n1.0\n5.0\n10.0");
                case 15: gTDS[s][tsGeoAxis] = (gTDS[s][tsGeoAxis]+1)%3;
                case 16: gTDS[s][tsAlignActive] = !gTDS[s][tsAlignActive];
                case 17:
                {
                    gTDS[s][tsAlignGroup] = true;
                    SendClientMessage(playerid, TDS_COLOR, "Группа берётся из последнего выбора группы; активный ориентир должен быть вне неё.");
                    return TDS_GeoAlignMenu(playerid);
                }
                case 18: return TDS_SetMode(playerid, TDS_EDIT_MODE);
            }
        }
        case TDS_GEO_INPUT:
        {
            new bool:ok, Float:v[2];
            switch (gTDS[s][tsGeoField])
            {
                case 0: ok = TDS_GeoExact(playerid, input);
                case 1: { if (TDS_Numbers(input, v, 2)) ok = TDS_GeoPosition(playerid, v[0], v[1]); }
                case 2: { if (TDS_Numbers(input, v, 2)) ok = TDS_GeoScale(playerid, v[0], v[1]); }
                case 3:
                {
                    if (TDS_Numbers(input, v, 1) && TDS_InRange(v[0], 0.0, 224.0))
                    { gTDS[s][tsMargin] = v[0]; ok = true; }
                }
            }
            if (!ok) TDS_GeoError(playerid);
        }
        case TDS_GEO_ALIGN:
        {
            if (row < 0 || row > 5 || (gTDS[s][tsAlignGroup] && TDS_GroupSlot(s, gTDS[s][tsGroupFocus]) < 0) ||
                !TDS_GeoAlign(playerid, E_TDS_ALIGN_OP:row, gTDS[s][tsAlignActive],
                gTDS[s][tsAlignGroup] ? gTDS[s][tsGroupFocus] : 0)) TDS_GeoError(playerid);
        }
        case TDS_GEO_SCREEN:
        {
            if (row == 7) return TDS_GeoInput(playerid, 3);
            if (!TDS_GeoScreen(playerid, row)) TDS_GeoError(playerid);
        }
        case TDS_GEO_GRID:
        {
            if (row >= 0 && row < sizeof(gTDSGridSteps)) TDS_GridStep(playerid, row);
            else if (row == 4) { gTDS[s][tsSnap] = !gTDS[s][tsSnap]; TDS_RenderStatus(s); }
            else if (row == 5 && !TDS_GridSet(playerid, !gTDS[s][tsGridShow]))
                SendClientMessage(playerid, ADMIN_ERROR, "Не хватает хэндлов для направляющих. Математический Snap остаётся доступен.");
            return TDS_GeoGridMenu(playerid);
        }
        case TDS_GEO_STEP:
        {
            if (row >= 0 && row < sizeof(gTDSSteps)) gTDS[s][tsResizeStep] = row;
        }
    }
    return TDS_GeoMenu(playerid);
}
