#if defined _orvelar_hud_included
    #endinput
#endif
#define _orvelar_hud_included

// ORVELAR HUD-01: лёгкий экранный знак в правой верхней зоне.
// Базовый TextDraw не умеет отображать произвольный PNG.
// Эта версия воспроизводит мотив кольца и диагонали средствами шрифтов GTA,
// не требует клиентских модов и не затрагивает авторизацию или админ-панель.
// Координаты заданы в стандартной области TextDraw 640 x 448.

new Text:gOrvelarHudRing = Text:INVALID_TEXT_DRAW;
new Text:gOrvelarHudStroke = Text:INVALID_TEXT_DRAW;
new Text:gOrvelarHudName = Text:INVALID_TEXT_DRAW;
new bool:gOrvelarHudVisible[MAX_PLAYERS];

stock OrvelarHUD_Init()
{
    // Выравнивание перед штатными индикаторами здоровья/оружия GTA.
    gOrvelarHudRing = TextDrawCreate(520.0, 7.0, "O");
    TextDrawFont(gOrvelarHudRing, TEXT_DRAW_FONT_3);
    TextDrawLetterSize(gOrvelarHudRing, 0.64, 2.5);
    TextDrawColour(gOrvelarHudRing, 0xDDE9FFFF);
    TextDrawSetOutline(gOrvelarHudRing, 1);
    TextDrawBackgroundColour(gOrvelarHudRing, 0x091527FF);
    TextDrawSetProportional(gOrvelarHudRing, true);

    gOrvelarHudStroke = TextDrawCreate(515.0, 6.0, "/");
    TextDrawFont(gOrvelarHudStroke, TEXT_DRAW_FONT_3);
    TextDrawLetterSize(gOrvelarHudStroke, 0.80, 2.55);
    TextDrawColour(gOrvelarHudStroke, 0x61B9FFFF);
    TextDrawSetOutline(gOrvelarHudStroke, 1);
    TextDrawBackgroundColour(gOrvelarHudStroke, 0x091527FF);
    TextDrawSetProportional(gOrvelarHudStroke, true);

    gOrvelarHudName = TextDrawCreate(457.0, 16.0, "Tarkenso RPG");
    TextDrawFont(gOrvelarHudName, TEXT_DRAW_FONT_2);
    TextDrawLetterSize(gOrvelarHudName, 0.26, 1.08);
    TextDrawColour(gOrvelarHudName, 0xE8F1FFFF);
    TextDrawSetOutline(gOrvelarHudName, 1);
    TextDrawBackgroundColour(gOrvelarHudName, 0x091527FF);
    TextDrawSetProportional(gOrvelarHudName, true);
    return 1;
}

stock OrvelarHUD_OnConnect(playerid)
{
    gOrvelarHudVisible[playerid] = true;
    return 1;
}

stock OrvelarHUD_Show(playerid)
{
    if (!gOrvelarHudVisible[playerid]) return 1;
    TextDrawShowForPlayer(playerid, gOrvelarHudRing);
    TextDrawShowForPlayer(playerid, gOrvelarHudStroke);
    TextDrawShowForPlayer(playerid, gOrvelarHudName);
    return 1;
}

stock OrvelarHUD_Hide(playerid)
{
    TextDrawHideForPlayer(playerid, gOrvelarHudRing);
    TextDrawHideForPlayer(playerid, gOrvelarHudStroke);
    TextDrawHideForPlayer(playerid, gOrvelarHudName);
    return 1;
}

stock bool:OrvelarHUD_Toggle(playerid)
{
    gOrvelarHudVisible[playerid] = !gOrvelarHudVisible[playerid];
    if (gOrvelarHudVisible[playerid])
        OrvelarHUD_Show(playerid);
    else
        OrvelarHUD_Hide(playerid);
    return gOrvelarHudVisible[playerid];
}

stock OrvelarHUD_Shutdown()
{
    TextDrawDestroy(gOrvelarHudRing);
    TextDrawDestroy(gOrvelarHudStroke);
    TextDrawDestroy(gOrvelarHudName);
    return 1;
}
