#if defined _tarkenso_admin_vehicle_included
    #endinput
#endif
#define _tarkenso_admin_vehicle_included

// ADMIN-VEH-01. One temporary vehicle per on-duty administrator.
// No database persistence; vehicle is removed on replacement, death,
// disconnect or game-mode shutdown.
new gAdminVehVehicle[MAX_PLAYERS];

stock AdminVeh_Reset(playerid)
{
    gAdminVehVehicle[playerid] = INVALID_VEHICLE_ID;
    return 1;
}

stock AdminVeh_Destroy(playerid)
{
    new vehicleid = gAdminVehVehicle[playerid];
    gAdminVehVehicle[playerid] = INVALID_VEHICLE_ID;
    if (vehicleid != INVALID_VEHICLE_ID && IsValidVehicle(vehicleid))
        DestroyVehicle(vehicleid);
    return 1;
}

stock AdminVeh_OnVehicleDeath(vehicleid)
{
    for (new playerid = 0; playerid < MAX_PLAYERS; playerid++)
    {
        if (gAdminVehVehicle[playerid] != vehicleid) continue;
        gAdminVehVehicle[playerid] = INVALID_VEHICLE_ID;
        if (IsValidVehicle(vehicleid)) DestroyVehicle(vehicleid);
        return 1;
    }
    return 0;
}

stock AdminVeh_Shutdown()
{
    for (new playerid = 0; playerid < MAX_PLAYERS; playerid++)
        AdminVeh_Destroy(playerid);
    return 1;
}

stock AdminVeh_Command(playerid, const params[])
{
    // Even administrator level 1 requires the explicit /aduty switch.
    if (!Admin_Require(playerid, PERM_REPORT)) return 1;
    if (isnull(params))
        return SendClientMessage(playerid, ADMIN_COLOR,
            "Использование: /veh ID [цвет1] [цвет2] | /veh del. Пример: /veh 411 1 1.");

    if (!strcmp(params, "del", true) || !strcmp(params, "delete", true))
    {
        if (gAdminVehVehicle[playerid] == INVALID_VEHICLE_ID ||
            !IsValidVehicle(gAdminVehVehicle[playerid]))
        {
            AdminVeh_Reset(playerid);
            return SendClientMessage(playerid, ADMIN_ERROR,
                "У вас нет созданного административного транспорта.");
        }
        new detail[64];
        format(detail, sizeof(detail), "vehicleid=%d",
            gAdminVehVehicle[playerid]);
        if (!Admin_LogPlayer(playerid, "VEH_DELETE", detail))
            return SendClientMessage(playerid, ADMIN_ERROR,
                "Журнал администрации недоступен: действие отменено.");
        AdminVeh_Destroy(playerid);
        return SendClientMessage(playerid, ADMIN_OK,
            "Административный транспорт удалён.");
    }

    new modelid, color1 = -1, color2 = -1;
    if (sscanf(params, "iI(-1)I(-1)", modelid, color1, color2) ||
        modelid < 400 || modelid > 611 || color1 < -1 || color1 > 255 ||
        color2 < -1 || color2 > 255)
        return SendClientMessage(playerid, ADMIN_ERROR,
            "Укажите модель 400..611 и цвета 0..255 (-1 = случайный). Пример: /veh 411 1 1.");

    // Rail vehicles cannot be placed safely at arbitrary coordinates.
    if (modelid == 449 || modelid == 537 || modelid == 538 ||
        modelid == 569 || modelid == 570 || modelid == 590)
        return SendClientMessage(playerid, ADMIN_ERROR,
            "Железнодорожный транспорт здесь не поддерживается.");

    if (Race_IsParticipant(playerid) ||
        GetPlayerState(playerid) != PLAYER_STATE_ONFOOT ||
        gAdminSpectate[playerid] != INVALID_PLAYER_ID)
        return SendClientMessage(playerid, ADMIN_ERROR,
            "Создание транспорта доступно только пешком, вне гонки и наблюдения.");

    new Float:x, Float:y, Float:z, Float:angle;
    GetPlayerPos(playerid, x, y, z);
    GetPlayerFacingAngle(playerid, angle);
    x += floatsin(-angle, degrees) * 5.0;
    y += floatcos(-angle, degrees) * 5.0;

    new world = GetPlayerVirtualWorld(playerid);
    new interior = GetPlayerInterior(playerid), detail[144];
    format(detail, sizeof(detail),
        "model=%d colors=%d/%d x=%.2f y=%.2f z=%.2f world=%d interior=%d",
        modelid, color1, color2, x, y, z, world, interior);
    if (!Admin_LogPlayer(playerid, "VEH_CREATE", detail))
        return SendClientMessage(playerid, ADMIN_ERROR,
            "Журнал администрации недоступен: действие отменено.");

    new vehicleid = CreateVehicle(modelid, x, y, z + 0.5, angle,
        color1, color2, -1);
    if (vehicleid == INVALID_VEHICLE_ID)
        return SendClientMessage(playerid, ADMIN_ERROR,
            "Не удалось создать транспорт: проверьте лимит машин сервера.");

    SetVehicleVirtualWorld(vehicleid, world);
    LinkVehicleToInterior(vehicleid, interior);

    // Only discard the old temporary vehicle after the replacement exists.
    AdminVeh_Destroy(playerid);
    gAdminVehVehicle[playerid] = vehicleid;
    PutPlayerInVehicle(playerid, vehicleid, 0);

    new message[100];
    format(message, sizeof(message),
        "Создан транспорт: модель %d, ID %d. Удалить: /veh del.",
        modelid, vehicleid);
    return SendClientMessage(playerid, ADMIN_OK, message);
}
