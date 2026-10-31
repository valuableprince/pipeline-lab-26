#if defined _tarkenso_rp_animations_included
    #endinput
#endif
#define _tarkenso_rp_animations_included

// RP-ANIM-01: explicitly whitelisted GTA SA gestures; no user-supplied clip names.
#define RP_ANIM_DIALOG (15010)
#define RP_ANIM_COLOR (0xD5B477FF)

enum E_RP_ANIM
{
    raCommand[16],
    raTitle[48],
    raLibrary[20],
    raClip[32]
};
new const gRPAnimations[][E_RP_ANIM] =
{
    {"sit",       "Сидеть",              "BEACH",      "ParkSit_M_loop"},
    {"handsup",   "Руки вверх",          "PED",        "handsup"},
    {"wave",      "Помахать рукой",      "ON_LOOKERS", "wave_loop"},
    {"crossarms", "Скрестить руки",     "PED",        "roadcross"},
    {"smoke",     "Курить",              "SMOKING",    "M_smkstnd_loop"},
    {"tired",     "Усталый вид",         "PED",        "IDLE_tired"},
    {"talk",      "Разговаривать",       "PED",        "IDLE_chat"},
    {"lie",       "Лежать",              "BEACH",      "bather"},
    {"cower",     "Пригнуться",          "PED",        "cower"}
};

// 0 = no player-owned RP animation; positive = one-based catalog index.
new gRPAnimSelected[MAX_PLAYERS];

stock RPAnim_Reset(playerid)
{
    // NEVER clear tasks here: jobs, story scenes, and vehicles own their animations.
    gRPAnimSelected[playerid] = 0;
    return 1;
}

stock bool:RPAnim_Active(playerid)
{
    return gRPAnimSelected[playerid] > 0;
}

stock bool:RPAnim_Allowed(playerid)
{
    return IsPlayerConnected(playerid) && !IsPlayerNPC(playerid) &&
        gAuthState[playerid] == AUTH_READY && gAccountSpawned[playerid] &&
        !gAccountDead[playerid] && !gAdminFrozen[playerid] &&
        !gAdminFly[playerid] && !TDS_Active(playerid) &&
        !Race_IsParticipant(playerid) && !gCleanerDuty[playerid] &&
        GetPlayerState(playerid) == PLAYER_STATE_ONFOOT &&
        GetPlayerSpecialAction(playerid) == SPECIAL_ACTION_NONE;
}

stock bool:RPAnim_IsCurrent(playerid)
{
    new selected = gRPAnimSelected[playerid] - 1;
    if (selected < 0 || selected >= sizeof(gRPAnimations)) return false;
    new index = GetPlayerAnimationIndex(playerid);
    // The client may not have synced the newly applied clip yet.
    if (!index) return true;
    new library[32], clip[32];
    GetAnimationName(index, library, sizeof(library), clip, sizeof(clip));
    return !strcmp(library, gRPAnimations[selected][raLibrary], true) &&
        !strcmp(clip, gRPAnimations[selected][raClip], true);
}

stock RPAnim_Stop(playerid)
{
    if (!RPAnim_Active(playerid)) return 0;
    // ClearAnimations can disrupt driving / gameplay. Only use for our own
    // tracked clip on foot, never when a different animation replaced it.
    if (IsPlayerConnected(playerid) &&
        GetPlayerState(playerid) == PLAYER_STATE_ONFOOT &&
        GetPlayerSpecialAction(playerid) == SPECIAL_ACTION_NONE &&
        RPAnim_IsCurrent(playerid))
        ClearAnimations(playerid, SYNC_ALL);
    RPAnim_Reset(playerid);
    return 1;
}

stock RPAnim_Play(playerid, index)
{
    if (index < 0 || index >= sizeof(gRPAnimations)) return 1;
    if (!RPAnim_Allowed(playerid))
        return SendClientMessage(playerid, COLOR_RED,
            "Анимация недоступна в транспорте, на работе, в гонке и при спецдействиях.");

    new Float:vx, Float:vy, Float:vz;
    GetPlayerVelocity(playerid, vx, vy, vz);
    if (vx * vx + vy * vy + vz * vz > 0.0025)
        return SendClientMessage(playerid, COLOR_AMBER,
            "Остановитесь, чтобы начать анимацию.");

    new current = GetPlayerAnimationIndex(playerid);
    if (RPAnim_Active(playerid) && !RPAnim_IsCurrent(playerid))
        RPAnim_Reset(playerid);
    if (!RPAnim_Active(playerid) && current)
    {
        new library[32], clip[32];
        GetAnimationName(current, library, sizeof(library), clip, sizeof(clip));
        // Do not overwrite unrelated game, work or cutscene tasks.
        if (strcmp(library, "PED", true) ||
            (strfind(clip, "IDLE_", true) != 0 &&
             strfind(clip, "idlestance", true) != 0))
            return SendClientMessage(playerid, COLOR_AMBER,
                "Сначала завершите текущее действие.");
    }

    gRPAnimSelected[playerid] = index + 1;
    ApplyAnimation(playerid, gRPAnimations[index][raLibrary],
        gRPAnimations[index][raClip], 4.1, true, false, false,
        false, 0, SYNC_NONE);
    SendClientMessage(playerid, RP_ANIM_COLOR,
        "Анимация включена. Для выхода нажмите ПРОБЕЛ (клавиша бега).");
    return 1;
}

stock RPAnim_Open(playerid)
{
    if (gAuthState[playerid] != AUTH_READY || !gAccountSpawned[playerid])
        return SendClientMessage(playerid, COLOR_RED, "Сначала войдите в игру.");
    new body[900] = "{FFD15C}Действие\t{FFD15C}Команда\n";
    new row[104];
    for (new i = 0; i < sizeof(gRPAnimations); i++)
    {
        format(row, sizeof(row), "{FFFFFF}%s\t{D5B477}/anim %s\n",
            gRPAnimations[i][raTitle], gRPAnimations[i][raCommand]);
        strcat(body, row, sizeof(body));
    }
    return ShowPlayerDialog(playerid, RP_ANIM_DIALOG, DIALOG_STYLE_TABLIST_HEADERS,
        "{FFD15C}TARKENSO | Анимации", body, "Выбрать", "Закрыть");
}

stock RPAnim_Command(playerid, const params[])
{
    if (isnull(params)) return RPAnim_Open(playerid);
    new command[20];
    if (strlen(params) >= sizeof(command) || strfind(params, " ") != -1 ||
        strfind(params, "\t") != -1 || sscanf(params, "s[20]", command))
        return SendClientMessage(playerid, COLOR_AMBER,
            "Использование: /anim название (без пробелов). /anim — список.");

    for (new i = 0; i < sizeof(gRPAnimations); i++)
        if (!strcmp(command, gRPAnimations[i][raCommand], true))
            return RPAnim_Play(playerid, i);
    return SendClientMessage(playerid, COLOR_AMBER,
        "Неизвестная анимация. Используйте /anim для выбора.");
}

stock RPAnim_Quick(playerid, const command[])
{
    for (new i = 0; i < sizeof(gRPAnimations); i++)
        if (!strcmp(command, gRPAnimations[i][raCommand], true))
            return RPAnim_Play(playerid, i);
    return 1;
}

stock RPAnim_OnDialog(playerid, dialogid, response, listitem)
{
    if (dialogid != RP_ANIM_DIALOG) return 0;
    if (!response) return 1;
    if (listitem < 0 || listitem >= sizeof(gRPAnimations)) return 1;
    return RPAnim_Play(playerid, listitem);
}

stock RPAnim_OnKeys(playerid, KEY:newkeys, KEY:oldkeys)
{
    // SPACE is KEY_SPRINT with standard GTA SA controls, not KEY_JUMP.
    // Only intercept a new press while a player-owned animation is running.
    if (!RPAnim_Active(playerid) ||
        !(newkeys & KEY_SPRINT) || (oldkeys & KEY_SPRINT)) return 0;
    if (RPAnim_Stop(playerid))
        SendClientMessage(playerid, RP_ANIM_COLOR, "Анимация завершена.");
    return 1;
}
