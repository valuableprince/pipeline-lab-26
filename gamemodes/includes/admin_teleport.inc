#if defined _tarkenso_admin_teleport_included
    #endinput
#endif
#define _tarkenso_admin_teleport_included

// ADMIN-TELEPORT-01. Map waypoint confirmation + exact-coordinate command.
// TARKENSO server is CP1251 for player-facing strings; admin console audit
// records use only ASCII coordinates and action names.
#define ADMIN_TP_DLG_CONFIRM  (6516)
#define ADMIN_TP_DLG_HEIGHT   (6517)
#define ADMIN_TP_DLG_INFO     (6518)
#define ADMIN_TP_MAP_Z_OFFSET (1.0)
#define ADMIN_TP_STAGE_NONE   (0)
#define ADMIN_TP_STAGE_HEIGHT (1)
#define ADMIN_TP_STAGE_CONFIRM (2)

new bool:gAdminTPPending[MAX_PLAYERS];
new gAdminTPStage[MAX_PLAYERS];
new gAdminTPAuthToken[MAX_PLAYERS];
new Float:gAdminTPX[MAX_PLAYERS];
new Float:gAdminTPY[MAX_PLAYERS];
new Float:gAdminTPZ[MAX_PLAYERS];

stock AdminTP_Reset(playerid)
{
    gAdminTPPending[playerid] = false;
    gAdminTPStage[playerid] = ADMIN_TP_STAGE_NONE;
    gAdminTPAuthToken[playerid] = 0;
    gAdminTPX[playerid] = 0.0;
    gAdminTPY[playerid] = 0.0;
    gAdminTPZ[playerid] = 0.0;
    return 1;
}

// Accept exactly N whitespace-separated numeric values, no hidden parameters.
stock bool:AdminTP_TokenCount(const input[], expected)
{
    new count, bool:word;
    if (isnull(input)) return false;
    for (new i = 0; input[i] != EOS; i++)
    {
        if (input[i] == ' ' || input[i] == '\t')
        {
            word = false;
            continue;
        }
        if (!word)
        {
            count++;
            if (count > expected) return false;
            word = true;
        }
    }
    return count == expected;
}

// Range tests also reject NaN: a NaN fails both >= and <= comparisons.
stock bool:AdminTP_Valid(Float:x, Float:y, Float:z)
{
    return x >= -3000.0 && x <= 3000.0 &&
        y >= -3000.0 && y <= 3000.0 &&
        z >= -150.0 && z <= 2000.0;
}

stock bool:AdminTP_CanUse(playerid)
{
    if (!Admin_Require(playerid, PERM_REPORT)) return false;
    if (Race_IsParticipant(playerid))
    {
        SendClientMessage(playerid, ADMIN_ERROR,
            "Телепортация недоступна во время гонки. Завершите заезд.");
        return false;
    }
    new PLAYER_STATE:playerState = GetPlayerState(playerid);
    if (playerState != PLAYER_STATE_ONFOOT && playerState != PLAYER_STATE_DRIVER)
    {
        SendClientMessage(playerid, ADMIN_ERROR,
            "Телепортация доступна пешком или за рулём без пассажиров.");
        return false;
    }
    if (playerState == PLAYER_STATE_DRIVER)
    {
        new vehicleid = GetPlayerVehicleID(playerid);
        if (vehicleid == INVALID_VEHICLE_ID)
            return false;
        // Do not move players who have not requested teleportation.
        for (new other = 0; other < MAX_PLAYERS; other++)
        {
            if (other == playerid || !IsPlayerConnected(other) ||
                !IsPlayerInAnyVehicle(other)) continue;
            if (GetPlayerVehicleID(other) == vehicleid)
            {
                SendClientMessage(playerid, ADMIN_ERROR,
                    "Попросите пассажиров выйти из автомобиля перед телепортацией.");
                return false;
            }
        }
    }
    return true;
}

// Called only after per-action permission checks. GTA map is overworld,
// therefore map and /tpcor always target virtual world 0 / interior 0.
stock AdminTP_Apply(playerid, Float:x, Float:y, Float:z, const action[])
{
    if (!AdminTP_CanUse(playerid)) return 1;
    if (!AdminTP_Valid(x, y, z))
        return SendClientMessage(playerid, ADMIN_ERROR,
            "Координаты за пределами San Andreas.");

    new detail[128];
    format(detail, sizeof(detail), "x=%.3f y=%.3f z=%.3f world=0 interior=0",
        x, y, z);
    // Fail closed if action audit is unavailable.
    if (!Admin_LogPlayer(playerid, action, detail))
        return SendClientMessage(playerid, ADMIN_ERROR,
            "Административный журнал недоступен. Телепортация отменена.");

    new bool:success;
    if (GetPlayerState(playerid) == PLAYER_STATE_DRIVER)
    {
        new vehicleid = GetPlayerVehicleID(playerid);
        LinkVehicleToInterior(vehicleid, 0);
        SetVehicleVirtualWorld(vehicleid, 0);
        SetPlayerInterior(playerid, 0);
        SetPlayerVirtualWorld(playerid, 0);
        success = SetVehiclePos(vehicleid, x, y, z);
    }
    else
    {
        SetPlayerInterior(playerid, 0);
        SetPlayerVirtualWorld(playerid, 0);
        success = SetPlayerPos(playerid, x, y, z);
    }
    if (!success)
        return SendClientMessage(playerid, ADMIN_ERROR,
            "Не удалось изменить позицию. Попробуйте ещё раз.");

    SetCameraBehindPlayer(playerid);
    new message[125];
    format(message, sizeof(message),
        "Телепортация выполнена: X %.2f, Y %.2f, Z %.2f (мир 0).",
        x, y, z);
    SendClientMessage(playerid, ADMIN_OK, message);
    return 1;
}

stock AdminTP_Coordinates(playerid, const params[])
{
    if (!AdminTP_CanUse(playerid)) return 1;
    new Float:x, Float:y, Float:z;
    if (!AdminTP_TokenCount(params, 3) || sscanf(params, "fff", x, y, z))
    {
        SendClientMessage(playerid, ADMIN_ERROR,
            "Использование: /tpcor X Y Z (например: /tpcor -199.2 1086.5 19.6).");
        return 1;
    }
    return AdminTP_Apply(playerid, x, y, z, "TP_COORDS");
}

stock AdminTP_Info(playerid)
{
    if (!AdminTP_CanUse(playerid)) return 1;
    ShowPlayerDialog(playerid, ADMIN_TP_DLG_INFO, DIALOG_STYLE_MSGBOX,
        "ADMIN | Телепортация",
        "По карте: откройте меню паузы (ESC), вкладку карты и\nпоставьте метку правой кнопкой мыши.\nПосле возврата в игру подтвердите координаты в диалоге.\n\nПо координатам: /tpcor X Y Z\nПример: /tpcor -199.2 1086.5 19.6\nТелепортация выполняется в обычном мире (0).",
        "Понятно", "");
    return 1;
}

stock AdminTP_ShowConfirmation(playerid)
{
    if (!gAdminTPPending[playerid] ||
        gAdminTPAuthToken[playerid] != gAuthToken[playerid] ||
        !AdminTP_CanUse(playerid))
    {
        AdminTP_Reset(playerid);
        return 1;
    }
    new message[260];
    format(message, sizeof(message),
        "Точка на карте:\nX: %.2f\nY: %.2f\nZ: %.2f\n\nМир: 0 | Интерьер: 0\nТелепортироваться в выбранную точку?\nУчтите: высота по карте может быть неточной.",
        gAdminTPX[playerid], gAdminTPY[playerid], gAdminTPZ[playerid]);
    gAdminTPStage[playerid] = ADMIN_TP_STAGE_CONFIRM;
    ShowPlayerDialog(playerid, ADMIN_TP_DLG_CONFIRM, DIALOG_STYLE_MSGBOX,
        "ADMIN | Подтверждение телепортации",
        message, "Телепорт", "Отмена");
    return 1;
}

stock AdminTP_OnClickMap(playerid, Float:x, Float:y, Float:z)
{
    // Do not change ordinary players' waypoints.
    if (!Admin_Allowed(playerid, PERM_REPORT)) return 0;
    if (!AdminTP_CanUse(playerid)) return 1;

    AdminTP_Reset(playerid);
    if (!(x >= -3000.0 && x <= 3000.0 && y >= -3000.0 && y <= 3000.0))
    {
        SendClientMessage(playerid, ADMIN_ERROR,
            "Выбранная точка вне границ карты San Andreas.");
        return 1;
    }
    gAdminTPPending[playerid] = true;
    gAdminTPAuthToken[playerid] = gAuthToken[playerid];
    gAdminTPX[playerid] = x;
    gAdminTPY[playerid] = y;

    // OnPlayerClickMap can report Z=0 for distant points. Never call
    // SetPlayerPosFindZ: open.mp warns it can also return underground Z=0
    // when the destination is far away.
    if (!(z >= -150.0 && z <= 1999.0) || floatabs(z) < 0.01)
    {
        gAdminTPStage[playerid] = ADMIN_TP_STAGE_HEIGHT;
        ShowPlayerDialog(playerid, ADMIN_TP_DLG_HEIGHT, DIALOG_STYLE_INPUT,
            "ADMIN | Укажите высоту Z",
            "Карта не смогла определить высоту выбранной точки.\nВведите Z поверхности вручную (например, 20.0).\nСервер добавит 1 метр, чтобы избежать проваливания.",
            "Далее", "Отмена");
        return 1;
    }
    gAdminTPZ[playerid] = z + ADMIN_TP_MAP_Z_OFFSET;
    return AdminTP_ShowConfirmation(playerid);
}

stock AdminTP_OnDialog(playerid, dialogid, response, const inputtext[])
{
    if (dialogid == ADMIN_TP_DLG_INFO) return 1;
    if (dialogid != ADMIN_TP_DLG_CONFIRM &&
        dialogid != ADMIN_TP_DLG_HEIGHT) return 0;

    if (!gAdminTPPending[playerid] ||
        gAdminTPAuthToken[playerid] != gAuthToken[playerid] ||
        (dialogid == ADMIN_TP_DLG_HEIGHT &&
            gAdminTPStage[playerid] != ADMIN_TP_STAGE_HEIGHT) ||
        (dialogid == ADMIN_TP_DLG_CONFIRM &&
            gAdminTPStage[playerid] != ADMIN_TP_STAGE_CONFIRM))
    {
        AdminTP_Reset(playerid);
        return 1;
    }
    if (!response)
    {
        AdminTP_Reset(playerid);
        return 1;
    }
    if (!AdminTP_CanUse(playerid))
    {
        AdminTP_Reset(playerid);
        return 1;
    }

    if (dialogid == ADMIN_TP_DLG_HEIGHT)
    {
        new Float:z;
        if (!AdminTP_TokenCount(inputtext, 1) ||
            sscanf(inputtext, "f", z) ||
            !AdminTP_Valid(gAdminTPX[playerid], gAdminTPY[playerid],
                z + ADMIN_TP_MAP_Z_OFFSET))
        {
            SendClientMessage(playerid, ADMIN_ERROR,
                "Введите корректную высоту Z в пределах игрового мира.");
            ShowPlayerDialog(playerid, ADMIN_TP_DLG_HEIGHT, DIALOG_STYLE_INPUT,
                "ADMIN | Высота Z",
                "Введите Z поверхности, например 20.0:", "Далее", "Отмена");
            return 1;
        }
        gAdminTPZ[playerid] = z + ADMIN_TP_MAP_Z_OFFSET;
        return AdminTP_ShowConfirmation(playerid);
    }

    new Float:x = gAdminTPX[playerid];
    new Float:y = gAdminTPY[playerid];
    new Float:z = gAdminTPZ[playerid];
    AdminTP_Reset(playerid);
    return AdminTP_Apply(playerid, x, y, z, "TP_MAP");
}
