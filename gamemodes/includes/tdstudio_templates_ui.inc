#if defined _tarkenso_tds_templates_ui
    #endinput
#endif
#define _tarkenso_tds_templates_ui

stock TDS_TplMenu(playerid)
{
    if (TDS_TplSession(playerid) < 0) return 0;
    return TDS_Dialog(playerid,TDS_TEMPLATE_MENU,DIALOG_STYLE_LIST,"TD Studio | Библиотека шаблонов",
        "Просмотреть / вставить шаблон\nСохранить выделение как шаблон\nВернуться в проект");
}

stock TDS_TplCategories(playerid, bool:creating)
{
    new body[160];
    for (new i; i < sizeof(gTDSTemplateCategories); i++) { strcat(body,gTDSTemplateCategories[i],sizeof(body)); strcat(body,"\n",sizeof(body)); }
    if (!creating) strcat(body,"Все категории",sizeof(body));
    return TDS_Dialog(playerid,creating ? TDS_TEMPLATE_NEW_CAT : TDS_TEMPLATE_CAT,DIALOG_STYLE_LIST,"TD Studio | Категории",body);
}

stock TDS_TplDetail(playerid)
{
    new s = TDS_TplSession(playerid); if (s < 0 || gTDS[s][tsTplId] <= 0) return TDS_TplMenu(playerid);
    new body[900], title[64];
    format(title,sizeof(title),"Шаблон #%d | %s",gTDS[s][tsTplId],gTDS[s][tsTplName]);
    format(body,sizeof(body),"Вставить шаблон\nПереименовать\nИзменить описание\nУдалить шаблон\nСоздатель %d | %s | категория %d | ревизия %d\nСоздан %s | изменён %s\n%s",
        gTDS[s][tsTplOwner],gTDS[s][tsTplVisibility] ? ("Общий") : ("Личный"),gTDS[s][tsTplCategory],gTDS[s][tsTplRevision],
        gTDS[s][tsTplCreated],gTDS[s][tsTplUpdated],gTDS[s][tsTplDescription]);
    return TDS_Dialog(playerid,TDS_TEMPLATE_DETAIL,DIALOG_STYLE_LIST,title,body);
}

stock TDS_TplConfirm(playerid)
{
    new s = TDS_TplSession(playerid); if (s < 0) return 0;
    new body[512]; format(body,sizeof(body),"Сохранить выделение как шаблон?\n%s\nКатегория %d | %s\n%s",
        gTDS[s][tsTplName],gTDS[s][tsTplCategory],gTDS[s][tsTplVisibility] ? ("Общий") : ("Личный"),gTDS[s][tsTplDescription]);
    return TDS_Dialog(playerid,TDS_TEMPLATE_CONFIRM,DIALOG_STYLE_MSGBOX,"TD Studio | Сохранить шаблон",body,"Сохранить");
}

stock TDS_OnTemplateDialog(playerid, dialog, response, row, const input[])
{
    new s = TDS_TplSession(playerid); if (s < 0) return 1;
    if (!response) return dialog == TDS_TEMPLATE_MENU ? TDS_Main(playerid) : TDS_TplMenu(playerid);
    switch (dialog)
    {
        case TDS_TEMPLATE_MENU:
        {
            if (row == 0) return TDS_TplCategories(playerid,false);
            if (row == 1)
            {
                if (!gTDS[s][tsProject] || !TDS_PickCount(s))
                    return TDSHost_Message(playerid,ADMIN_ERROR,"Сначала выделите элементы или группу текущего проекта.");
                gTDS[s][tsTplEdit] = 0;
                return TDS_Dialog(playerid,TDS_TEMPLATE_NAME,DIALOG_STYLE_INPUT,"TD Studio | Новый шаблон","Название: 1..32 печатных символа.","Продолжить");
            }
            return TDS_Main(playerid);
        }
        case TDS_TEMPLATE_CAT:
        {
            if (row < 0 || row > sizeof(gTDSTemplateCategories)) return TDS_TplMenu(playerid);
            gTDS[s][tsTplFilter] = row == sizeof(gTDSTemplateCategories) ? 0 : row+1; gTDS[s][tsTplPage] = 0;
            return TDS_TplRequest(playerid,TDS_DB_TEMPLATE_LIST);
        }
        case TDS_TEMPLATE_LIST:
        {
            if (row >= 0 && row < gTDS[s][tsTplRowCount]) return TDS_TplRequest(playerid,TDS_DB_TEMPLATE_INFO,gTDS[s][tsTplRows][row]);
            if (row == gTDS[s][tsTplRowCount] && gTDS[s][tsTplMore]) gTDS[s][tsTplPage]++;
            else if (row == gTDS[s][tsTplRowCount]+1 && gTDS[s][tsTplPage] > 0) gTDS[s][tsTplPage]--;
            else return TDS_TplMenu(playerid);
            return TDS_TplRequest(playerid,TDS_DB_TEMPLATE_LIST);
        }
        case TDS_TEMPLATE_DETAIL:
        {
            if (row == 0)
            {
                if (!gTDS[s][tsProject]) return TDSHost_Message(playerid,ADMIN_ERROR,"Для вставки сначала создайте или откройте проект.");
                return TDS_Dialog(playerid,TDS_TEMPLATE_PLACE,DIALOG_STYLE_LIST,"TD Studio | Вставить шаблон",
                    "По центру экрана\nУказать X Y левого верхнего угла\nСмещение от якоря активного элемента");
            }
            if (row >= 1 && row <= 3 && !TDS_TplOwned(s))
                return TDSHost_Message(playerid,ADMIN_ERROR,"Изменять шаблон может его создатель или подтверждённый основатель.");
            if (row == 1)
            {
                gTDS[s][tsTplEdit] = 1;
                return TDS_Dialog(playerid,TDS_TEMPLATE_NAME,DIALOG_STYLE_INPUT,"TD Studio | Переименовать шаблон","Новое название: 1..32 символа.","Применить");
            }
            if (row == 2)
            {
                gTDS[s][tsTplEdit] = 2;
                return TDS_Dialog(playerid,TDS_TEMPLATE_DESC,DIALOG_STYLE_INPUT,"TD Studio | Описание шаблона","До 128 печатных символов, можно оставить пустым.","Применить");
            }
            if (row == 3) return TDS_Dialog(playerid,TDS_TEMPLATE_DELETE,DIALOG_STYLE_MSGBOX,"TD Studio | Удалить шаблон?",
                "Шаблон и его состав будут удалены из библиотеки. Уже вставленные копии в проектах сохранятся.","Удалить");
        }
        case TDS_TEMPLATE_NAME:
        {
            if (!TDS_ValidName(input)) return TDSHost_Message(playerid,ADMIN_ERROR,"Недопустимое название шаблона.");
            format(gTDS[s][tsTplName],TDS_NAME,"%s",input);
            if (gTDS[s][tsTplEdit]) return TDS_TplRequest(playerid,TDS_DB_TEMPLATE_UPDATE,gTDS[s][tsTplId]);
            return TDS_TplCategories(playerid,true);
        }
        case TDS_TEMPLATE_NEW_CAT:
        {
            if (row < 0 || row >= sizeof(gTDSTemplateCategories)) return TDS_TplMenu(playerid);
            gTDS[s][tsTplCategory] = row+1;
            return TDS_Dialog(playerid,TDS_TEMPLATE_DESC,DIALOG_STYLE_INPUT,"TD Studio | Описание шаблона","До 128 печатных символов, можно оставить пустым.","Продолжить");
        }
        case TDS_TEMPLATE_DESC:
        {
            if (input[0] && !TDS_SafeText(input,TDS_DESCRIPTION-1)) return TDSHost_Message(playerid,ADMIN_ERROR,"Недопустимое описание.");
            format(gTDS[s][tsTplDescription],TDS_DESCRIPTION,"%s",input);
            if (gTDS[s][tsTplEdit]) return TDS_TplRequest(playerid,TDS_DB_TEMPLATE_UPDATE,gTDS[s][tsTplId]);
            return TDS_Dialog(playerid,TDS_TEMPLATE_VIS,DIALOG_STYLE_LIST,"TD Studio | Доступ к шаблону",
                "Личный\nОбщий (публикация только основателем)");
        }
        case TDS_TEMPLATE_VIS:
        {
            if (row < 0 || row > 1 || (row == 1 && !TDSHost_Founder(playerid))) return TDSHost_Message(playerid,ADMIN_ERROR,"Публиковать общие шаблоны может только подтверждённый основатель.");
            gTDS[s][tsTplVisibility] = row; return TDS_TplConfirm(playerid);
        }
        case TDS_TEMPLATE_CONFIRM: return TDS_TplRequest(playerid,TDS_DB_TEMPLATE_SAVE);
        case TDS_TEMPLATE_DELETE: return TDS_TplRequest(playerid,TDS_DB_TEMPLATE_DELETE,gTDS[s][tsTplId]);
        case TDS_TEMPLATE_PLACE:
        {
            if (row < 0 || row > 2) return TDS_TplMenu(playerid);
            gTDS[s][tsTplPlace] = row; gTDS[s][tsTplX] = 0.0; gTDS[s][tsTplY] = 0.0;
            if (row == 0) return TDS_Dialog(playerid,TDS_TEMPLATE_POS,DIALOG_STYLE_MSGBOX,"TD Studio | Вставить по центру?",
                "Компонент будет помещён по центру виртуального экрана. Состав и взаимное расположение сохранятся.","Вставить");
            return TDS_Dialog(playerid,TDS_TEMPLATE_POS,DIALOG_STYLE_INPUT,"TD Studio | Точка вставки",
                row == 1 ? ("Введите X Y левого верхнего угла компонента.") : ("Введите смещение X Y от якоря активного элемента, например 10 10."),"Вставить");
        }
        case TDS_TEMPLATE_POS:
        {
            if (gTDS[s][tsTplPlace] != 0)
            {
                new Float:v[2]; if (!TDS_Numbers(input,v,2) || !TDS_InRange(v[0],-1280.0,1280.0) || !TDS_InRange(v[1],-896.0,896.0))
                    return TDSHost_Message(playerid,ADMIN_ERROR,"Некорректная точка вставки.");
                gTDS[s][tsTplX] = v[0]; gTDS[s][tsTplY] = v[1];
            }
            return TDS_TplRequest(playerid,TDS_DB_TEMPLATE_INSERT,gTDS[s][tsTplId]);
        }
    }
    return TDS_TplMenu(playerid);
}
