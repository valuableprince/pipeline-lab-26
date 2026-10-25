#if defined _tarkenso_admin_tools_included
    #endinput
#endif
#define _tarkenso_admin_tools_included

// ADMIN-TOOLS-01: six duty-gated commands; all sensitive actions are audited.
// This module uses the existing ADMIN-01 hierarchy and vehicle rate limiter.

stock bool:AdminTools_ReadTarget(const params[], &target)
{
    new offset, value[24], extra[12];
    if (!Admin_Next(params, offset, value, sizeof(value)) ||
        !Admin_UnsignedNumber(value) ||
        Admin_Next(params, offset, extra, sizeof(extra))) return false;
    target = strval(value);
    return target >= 0 && target < MAX_PLAYERS;
}

stock bool:AdminTools_CanEditPlayer(playerid, target)
{
    if (target < 0 || target >= MAX_PLAYERS || !Admin_Ready(target))
    {
        SendClientMessage(playerid, ADMIN_ERROR, "Игрок не найден или не авторизован.");
        return false;
    }
    // Personal testing is permitted. Other staff members retain rank/founder protection.
    if (target != playerid && !Admin_ValidTarget(playerid, target)) return false;
    if (!gAccountSpawned[target] || gAccountDead[target] ||
        GetPlayerState(target) == PLAYER_STATE_SPECTATING ||
        Race_IsParticipant(target))
    {
        SendClientMessage(playerid, ADMIN_ERROR, "Персонаж ещё не появился, погиб, наблюдает или участвует в гонке.");
        return false;
    }
    return true;
}

stock AdminTools_Help(playerid)
{
    if (!Admin_Require(playerid, PERM_REPORT)) return 1;
    SendClientMessage(playerid, ADMIN_COLOR,
        "Администрация: /aduty, /admin, /admins, /a, /reports, /check ID.");
    if (Admin_Allowed(playerid, PERM_MODERATE))
    {
        SendClientMessage(playerid, ADMIN_COLOR,
            "Уровень 2+: /heal ID, /armour ID 0-100, /fixveh [ID], /flip [ID].");
        SendClientMessage(playerid, ADMIN_COLOR,
            "Уровень 2+: /spec, /warn, /mute, /kick, /freeze, /weather ID.");
    }
    if (Admin_Allowed(playerid, PERM_BAN_DAY))
        SendClientMessage(playerid, ADMIN_COLOR, "Уровень 3+: /goto, /gethere, /ban.");
    if (Admin_Allowed(playerid, PERM_BAN_MONTH))
        SendClientMessage(playerid, ADMIN_COLOR, "Уровень 4+: /alog.");
    if (Admin_Allowed(playerid, PERM_MANAGE_STAFF))
        SendClientMessage(playerid, ADMIN_COLOR, "Уровень 5+: /unban, /setadmin, /setevent.");
    if (Admin_Allowed(playerid, PERM_EVENT))
        SendClientMessage(playerid, ADMIN_COLOR, "Event Team: /event fog|night|clear.");
    if (Admin_IsFounder(playerid))
        SendClientMessage(playerid, ADMIN_COLOR, "Founder: /founder, /studio, /trashservice.");
    return 1;
}

stock AdminTools_Check(playerid, const params[])
{
    if (!Admin_Require(playerid, PERM_REPORT)) return 1;
    new target;
    if (!AdminTools_ReadTarget(params, target))
        return SendClientMessage(playerid, ADMIN_ERROR, "Использование: /check ID.");
    if (!Admin_Ready(target))
        return SendClientMessage(playerid, ADMIN_ERROR, "Игрок не найден или не авторизован.");

    new detail[72];
    format(detail, sizeof(detail), "target=%s id=%d", gAccountName[target], target);
    if (!Admin_LogPlayer(playerid, "CHECK", detail))
        return SendClientMessage(playerid, ADMIN_ERROR, "Журнал недоступен. Просмотр отменён.");

    new Float:health = gAccountHealth[target], Float:armour = gAccountArmour[target];
    if (gAccountSpawned[target] && !gAccountDead[target] &&
        GetPlayerState(target) != PLAYER_STATE_SPECTATING &&
        !Race_IsParticipant(target))
    {
        GetPlayerHealth(target, health);
        GetPlayerArmour(target, armour);
    }
    new org = Leader_PlayerOrg(target), orgName[48], msg[144];
    if (org) Leader_OrgTitle(org, orgName, sizeof(orgName));
    else format(orgName, sizeof(orgName), "Нет");
    format(msg, sizeof(msg), "Игрок %s (ID %d) | Уровень %d | EXP %d",
        gAccountName[target], target, gCharacterLevel[target], gCharacterEXP[target]);
    SendClientMessage(playerid, ADMIN_COLOR, msg);
    format(msg, sizeof(msg), "Здоровье: %.0f | Броня: %.0f | Деньги: $%d | Админ: %d",
        health, armour, gAccountSpawned[target] ? GetPlayerMoney(target) : gAccountMoney[target],
        gAdminLevel[target]);
    SendClientMessage(playerid, ADMIN_COLOR, msg);
    format(msg, sizeof(msg), "Организация: %s (ранг %d) | Мир: %d | Интерьер: %d",
        orgName, Leader_PlayerRank(target), GetPlayerVirtualWorld(target), GetPlayerInterior(target));
    SendClientMessage(playerid, ADMIN_COLOR, msg);
    new muted = gAdminMuteUntil[target] - gettime();
    if (muted > 0)
    {
        format(msg, sizeof(msg), "Блокировка чата: осталось %d мин.", (muted + 59) / 60);
        SendClientMessage(playerid, ADMIN_COLOR, msg);
    }
    return 1;
}

stock AdminTools_ChangeVitals(playerid, const params[], bool:heal)
{
    if (!Admin_Require(playerid, PERM_MODERATE)) return 1;
    new offset, first[24], second[24], extra[12], target, amount = 100;
    if (!Admin_Next(params, offset, first, sizeof(first)) ||
        !Admin_UnsignedNumber(first))
        return SendClientMessage(playerid, ADMIN_ERROR,
            heal ? "Использование: /heal ID." : "Использование: /armour ID 0-100.");
    target = strval(first);
    if (!heal)
    {
        if (!Admin_Next(params, offset, second, sizeof(second)) ||
            !Admin_UnsignedNumber(second))
            return SendClientMessage(playerid, ADMIN_ERROR, "Использование: /armour ID 0-100.");
        amount = strval(second);
    }
    if (Admin_Next(params, offset, extra, sizeof(extra)) ||
        target < 0 || target >= MAX_PLAYERS || amount < 0 || amount > 100)
        return SendClientMessage(playerid, ADMIN_ERROR,
            heal ? "Использование: /heal ID." : "Использование: /armour ID 0-100.");
    if (!AdminTools_CanEditPlayer(playerid, target)) return 1;

    new detail[84];
    format(detail, sizeof(detail), "target=%s id=%d value=%d",
        gAccountName[target], target, amount);
    if (!Admin_LogPlayer(playerid, heal ? "HEAL" : "ARMOUR", detail))
        return SendClientMessage(playerid, ADMIN_ERROR, "Журнал недоступен. Действие отменено.");

    new Float:priorHealth, Float:priorArmour;
    GetPlayerHealth(target, priorHealth);
    GetPlayerArmour(target, priorArmour);
    if (heal) SetPlayerHealth(target, 100.0);
    else SetPlayerArmour(target, float(amount));
    // Save is synchronous for the current MariaDB implementation; rollback if it fails.
    if (!Auth_SaveAccount(target))
    {
        SetPlayerHealth(target, priorHealth);
        SetPlayerArmour(target, priorArmour);
        gAccountHealth[target] = priorHealth;
        gAccountArmour[target] = priorArmour;
        return SendClientMessage(playerid, ADMIN_ERROR, "Не удалось сохранить аккаунт. Изменение отменено.");
    }
    new msg[96];
    if (heal)
        format(msg, sizeof(msg), "Здоровье игрока %d восстановлено.", target);
    else
        format(msg, sizeof(msg), "Игроку %d установлена броня: %d.", target, amount);
    SendClientMessage(playerid, ADMIN_OK, msg);
    if (target != playerid)
        SendClientMessage(target, ADMIN_COLOR,
            heal ? "Администратор восстановил ваше здоровье." :
                "Администратор изменил вашу броню.");
    return 1;
}

stock bool:AdminTools_ReadVehicle(playerid, const params[], &vehicleid)
{
    new offset, first[24], extra[12];
    if (!Admin_Next(params, offset, first, sizeof(first)))
    {
        vehicleid = GetPlayerVehicleID(playerid);
    }
    else
    {
        if (!Admin_UnsignedNumber(first) ||
            Admin_Next(params, offset, extra, sizeof(extra))) return false;
        vehicleid = strval(first);
    }
    return vehicleid >= 1 && vehicleid <= MAX_VEHICLES && IsValidVehicle(vehicleid);
}

stock AdminTools_Vehicle(playerid, const params[], bool:flip)
{
    if (!Admin_Require(playerid, PERM_MODERATE)) return 1;
    if (Race_IsParticipant(playerid) ||
        GetPlayerState(playerid) == PLAYER_STATE_SPECTATING)
        return SendClientMessage(playerid, ADMIN_ERROR, "Команда недоступна во время гонки или наблюдения.");

    new vehicleid;
    if (!AdminTools_ReadVehicle(playerid, params, vehicleid))
        return SendClientMessage(playerid, ADMIN_ERROR,
            flip ? "Использование: /flip [ID автомобиля]." :
                "Использование: /fixveh [ID автомобиля].");
    if (GetVehicleVirtualWorld(vehicleid) != GetPlayerVirtualWorld(playerid))
        return SendClientMessage(playerid, ADMIN_ERROR, "Транспорт находится в другом виртуальном мире.");

    // Do not interfere with any racing participant, even when acting from outside a race.
    for (new i = 0; i < MAX_PLAYERS; i++)
        if (Admin_Ready(i) && Race_IsParticipant(i) &&
            GetPlayerVehicleID(i) == vehicleid)
            return SendClientMessage(playerid, ADMIN_ERROR,
                "Транспорт участника гонки изменять нельзя.");

    if (!AdminVeh_CooldownReady(playerid)) return 1;
    new detail[60], msg[96];
    format(detail, sizeof(detail), "vehicleid=%d world=%d",
        vehicleid, GetVehicleVirtualWorld(vehicleid));
    if (!Admin_LogPlayer(playerid, flip ? "VEH_FLIP" : "VEH_FIX", detail))
        return SendClientMessage(playerid, ADMIN_ERROR, "Журнал недоступен. Действие отменено.");

    if (flip)
    {
        new Float:x, Float:y, Float:z, Float:angle;
        GetVehiclePos(vehicleid, x, y, z);
        GetVehicleZAngle(vehicleid, angle);
        SetVehicleVelocity(vehicleid, 0.0, 0.0, 0.0);
        SetVehiclePos(vehicleid, x, y, z + 0.5);
        SetVehicleZAngle(vehicleid, angle);
    }
    else
    {
        RepairVehicle(vehicleid);
        SetVehicleHealth(vehicleid, 1000.0);
    }
    gAdminVehNextAction[playerid] = gettime() + ADMIN_VEH_COOLDOWN_SEC;
    format(msg, sizeof(msg), flip ? "Автомобиль ID %d поставлен на колёса." :
        "Автомобиль ID %d полностью отремонтирован.", vehicleid);
    return SendClientMessage(playerid, ADMIN_OK, msg);
}
