#if defined _tarkenso_tds_history_ui
    #endinput
#endif
#define _tarkenso_tds_history_ui

stock TDS_HMenu(playerid)
{
    new s = TDS_LayoutSession(playerid); if (s < 0) return TDS_Main(playerid);
    new body[256];
    format(body,sizeof(body),"Отменить действие (%d)\nПовторить действие (%d)\nПросмотреть последние действия\nОчистить историю",TDS_HCount(s),TDS_HCount(s,true));
    return TDS_Dialog(playerid,TDS_HISTORY_MENU,DIALOG_STYLE_LIST,"TD Studio | История действий",body);
}

stock TDS_OnHistoryDialog(playerid, dialog, response, row)
{
    new s = TDS_LayoutSession(playerid); if (s < 0) return 1;
    if (!response) return dialog == TDS_HISTORY_MENU ? TDS_Main(playerid) : TDS_HMenu(playerid);
    if (dialog == TDS_HISTORY_CLEAR) { TDS_HClear(playerid); return TDS_HMenu(playerid); }
    if (dialog == TDS_HISTORY_MENU) switch (row)
    {
        case 0,1:
        {
            if (!TDS_HStep(playerid,row == 1)) SendClientMessage(playerid,TDS_COLOR,"Действие истории недоступно или не удалось выделить ресурсы.");
        }
        case 2:
        {
            new body[1800], line[96];
            for (new i = gTDSHistoryCount[s]; i > 0; i--)
            {
                new at = (gTDSHistoryHead[s]+i)%TDS_H_STATES;
                format(line,sizeof(line),"%s %d. %s\n",i <= gTDSHistoryAt[s] ? ("[+]") : ("[-]"),i,gTDSHistoryLabel[s][at]); strcat(body,line,sizeof(body));
            }
            if (!body[0]) format(body,sizeof(body),"История пуста.");
            return TDS_Dialog(playerid,TDS_HISTORY_LIST,DIALOG_STYLE_MSGBOX,"TD Studio | [+] применено, [-] отменено",body);
        }
        case 3: return TDS_Dialog(playerid,TDS_HISTORY_CLEAR,DIALOG_STYLE_MSGBOX,"TD Studio | Очистить историю?",
            "Undo/Redo текущего проекта станут недоступны. Сам проект и контрольная точка сохранения остаются.","Очистить");
    }
    return TDS_HMenu(playerid);
}
