#if defined _tarkenso_admin_vehicle_included
    #endinput
#endif
#define _tarkenso_admin_vehicle_included

// ADMIN-VEH-02. Administrators can create many temporary vehicles.
// /dl is the built-in client debug-label command showing vehicle IDs.
// Only /veh del ID (or /delveh ID) can remove an admin-created vehicle.
// Never destroy unrelated cars by ID. Vehicles remain after admin logout.
#define ADMIN_VEH_COOLDOWN_SEC (3)
new bool:gAdminVehCreated[MAX_VEHICLES + 1];
new gAdminVehNextAction[MAX_PLAYERS];

stock AdminVeh_Reset(playerid)
{
    // Player slot reset does NOT destroy their previously created vehicles.
    gAdminVehNextAction[playerid] = 0;
    return 1;
}

stock AdminVeh_Init()
{
    for (new vehicleid = 0; vehicleid <= MAX_VEHICLES; vehicleid++)
        gAdminVehCreated[vehicleid] = false;
    for (new playerid = 0; playerid < MAX_PLAYERS; playerid++)
        AdminVeh_Reset(playerid);
    return 1;
}

stock bool:AdminVeh_IsTracked(vehicleid)
{
    return vehicleid > 0 && vehicleid <= MAX_VEHICLES &&
        gAdminVehCreated[vehicleid] && IsValidVehicle(vehicleid);
}

stock AdminVeh_DestroyById(vehicleid)
{
    if (!AdminVeh_IsTracked(vehicleid)) return 0;
    // Clear registry first, before ID can be reused by other modules.
    gAdminVehCreated[vehicleid] = false;
    DestroyVehicle(vehicleid);
    return 1;
}

stock AdminVeh_OnVehicleDeath(vehicleid)
{
    // Clean up destroyed temporary vehicles; regular vehicles are untouched.
    return AdminVeh_DestroyById(vehicleid);
}

stock AdminVeh_Shutdown()
{
    for (new vehicleid = 1; vehicleid <= MAX_VEHICLES; vehicleid++)
        if (gAdminVehCreated[vehicleid])
        {
            if (IsValidVehicle(vehicleid)) DestroyVehicle(vehicleid);
            gAdminVehCreated[vehicleid] = false;
        }
    return 1;
}

stock bool:AdminVeh_CooldownReady(playerid)
{
    if (gettime() >= gAdminVehNextAction[playerid]) return true;
    SendClientMessage(playerid, ADMIN_ERROR,
        "Антифлуд: подождите 3 секунды между операциями с транспортом.");
    return false;
}

stock AdminVeh_DeleteById(playerid, vehicleid)
{
    if (!Admin_Require(playerid, PERM_REPORT)) return 1;
    if (!AdminVeh_IsTracked(vehicleid))
        return SendClientMessage(playerid, ADMIN_ERROR,
            "Машина не найдена или не создана администрацией через /veh.");
    if (!AdminVeh_CooldownReady(playerid)) return 1;

    new detail[64];
    format(detail, sizeof(detail), "vehicleid=%d", vehicleid);
    if (!Admin_LogPlayer(playerid, "VEH_DELETE", detail))
        return SendClientMessage(playerid, ADMIN_ERROR,
            "Журнал администрации недоступен: удаление отменено.");

    AdminVeh_DestroyById(vehicleid);
    gAdminVehNextAction[playerid] = gettime() + ADMIN_VEH_COOLDOWN_SEC;
    new message[90];
    format(message, sizeof(message),
        "Административный транспорт ID %d удалён.", vehicleid);
    return SendClientMessage(playerid, ADMIN_OK, message);
}

stock AdminVeh_DeleteCommand(playerid, const params[])
{
    if (!Admin_Require(playerid, PERM_REPORT)) return 1;
    new vehicleid;
    if (sscanf(params, "i", vehicleid))
        return SendClientMessage(playerid, ADMIN_ERROR,
            "Использование: /delveh ID. ID машины можно увидеть через /dl.");
    return AdminVeh_DeleteById(playerid, vehicleid);
}

stock AdminVeh_Command(playerid, const params[])
{
    if (!Admin_Require(playerid, PERM_REPORT)) return 1;
    if (isnull(params))
        return SendClientMessage(playerid, ADMIN_COLOR,
            "Использование: /veh модель [цвет1] [цвет2] | /veh del ID. Узнать ID: /dl.");

    if (!strcmp(params, "del", true) || !strcmp(params, "delete", true))
        return SendClientMessage(playerid, ADMIN_ERROR,
            "Укажите ID: /veh del ID. Посмотреть номера машин: /dl.");

    new verb[8], deleteId;
    if (!sscanf(params, "s[8]i", verb, deleteId) &&
        (!strcmp(verb, "del", true) || !strcmp(verb, "delete", true)))
        return AdminVeh_DeleteById(playerid, deleteId);

    new modelid, color1 = -1, color2 = -1;
    if (sscanf(params, "iI(-1)I(-1)", modelid, color1, color2) ||
        modelid < 400 || modelid > 611 || color1 < -1 || color1 > 255 ||
        color2 < -1 || color2 > 255)
        return SendClientMessage(playerid, ADMIN_ERROR,
            "Модель 400..611, цвета 0..255 (-1 случайный). Пример: /veh 411 1 1.");

    if (modelid == 449 || modelid == 537 || modelid == 538 ||
        modelid == 569 || modelid == 570 || modelid == 590)
        return SendClientMessage(playerid, ADMIN_ERROR,
            "Железнодорожный транспорт здесь не поддерживается.");

    new PLAYER_STATE:state = GetPlayerState(playerid);
    if (Race_IsParticipant(playerid) ||
        (state != PLAYER_STATE_ONFOOT && state != PLAYER_STATE_DRIVER) ||
        gAdminSpectate[playerid] != INVALID_PLAYER_ID)
        return SendClientMessage(playerid, ADMIN_ERROR,
            "Создание возможно пешком или за рулём, вне гонки и наблюдения.");

    if (!AdminVeh_CooldownReady(playerid)) return 1;
    new Float:x, Float:y, Float:z, Float:angle;
    GetPlayerPos(playerid, x, y, z);
    if (state == PLAYER_STATE_DRIVER)
        GetVehicleZAngle(GetPlayerVehicleID(playerid), angle);
    else
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
            "Журнал администрации недоступен: создание отменено.");

    new vehicleid = CreateVehicle(modelid, x, y, z + 0.5, angle,
        color1, color2, -1);
    if (vehicleid == INVALID_VEHICLE_ID)
        return SendClientMessage(playerid, ADMIN_ERROR,
            "Не удалось создать транспорт: исчерпан лимит машин сервера.");
    if (vehicleid < 1 || vehicleid > MAX_VEHICLES)
    {
        DestroyVehicle(vehicleid);
        return SendClientMessage(playerid, ADMIN_ERROR,
            "Сервер вернул некорректный ID транспорта.");
    }

    SetVehicleVirtualWorld(vehicleid, world);
    LinkVehicleToInterior(vehicleid, interior);
    gAdminVehCreated[vehicleid] = true;
    gAdminVehNextAction[playerid] = gettime() + ADMIN_VEH_COOLDOWN_SEC;

    // Do not automatically seat the admin: they may place several cars.
    new message[116];
    format(message, sizeof(message),
        "Создан транспорт: модель %d, ID %d. Удалить: /veh del %d (ID через /dl).",
        modelid, vehicleid, vehicleid);
    return SendClientMessage(playerid, ADMIN_OK, message);
}
