#if defined _tarkenso_big_ear_included
    #endinput
#endif
#define _tarkenso_big_ear_included

// BIG-EAR-01: vanilla GTA:SA model 16613 is ONE rigid mesh.
// This controller rotates the entire installation about Z; a separate mesh
// is required for a moving reflector with a fixed support structure.
#define DISH_MODEL_ID          (16613)
#define DISH_X                 (-346.67)
#define DISH_Y                 (1595.08)
#define DISH_Z                 (79.6641)
#define DISH_ROT_LIMIT         (140.0)
#define DISH_AUTO_STEP         (10.0)
#define DISH_MANUAL_SPEED      (0.5)   // angular degrees per second
#define DISH_Z_TRAVEL          (0.01)  // MoveObject requires nonzero travel
#define DISH_COLOR             (0xB6D9EFFF)

stock Dish_Message(playerid, const message[], bool:error = false)
{
    new output[200];
    format(output, sizeof(output), "{D9AA55}[TARKENSO] {FFFFFF}%s", message);
    return SendClientMessage(playerid, error ? ADMIN_ERROR : -1, output);
}

stock Dish_Usage(playerid)
{
    Dish_Message(playerid, "Синтаксис: {B6E4B3}/dish status|auto|stop|angle|left|right");
    Dish_Message(playerid, "{9FAAB8}Пример: {B6E4B3}/dish angle 45 {9FAAB8}| Подробнее: /ahelp dish");
    return 1;
}

new gDishObject = INVALID_OBJECT_ID;
new bool:gDishAutomatic = true;
new bool:gDishMoving;
new gDishDirection = 1;
new Float:gDishDestination;

stock bool:Dish_Ready()
{
    return gDishObject != INVALID_OBJECT_ID && IsValidObject(gDishObject);
}

stock Float:Dish_CurrentAngle()
{
    if (!Dish_Ready()) return 0.0;
    new Float:rx, Float:ry, Float:rz;
    GetObjectRot(gDishObject, rx, ry, rz);
    // Do not normalize via unbounded loops: bad rotations can stall the
    // main server thread, preventing chat messages and driving CPU to 100%.
    if (rz < -360.0 || rz > 360.0) return gDishDestination;
    if (rz > 180.0) rz -= 360.0;
    else if (rz < -180.0) rz += 360.0;
    return rz;
}

stock bool:Dish_MoveTo(Float:target)
{
    if (!Dish_Ready() || target < -DISH_ROT_LIMIT || target > DISH_ROT_LIMIT)
        return false;

    // Stop the current interpolation before sampling its position and angle.
    StopObject(gDishObject);
    gDishMoving = false;
    new Float:current = Dish_CurrentAngle();
    new Float:difference = floatabs(target - current);
    if (difference < 0.05)
    {
        gDishDestination = current;
        return true;
    }

    new Float:x, Float:y, Float:z;
    GetObjectPos(gDishObject, x, y, z);
    new Float:nextZ = (z > DISH_Z + DISH_Z_TRAVEL / 2.0) ? DISH_Z : (DISH_Z + DISH_Z_TRAVEL);
    new Float:distance = floatabs(nextZ - z);
    if (distance < 0.0001)
    {
        nextZ = z + DISH_Z_TRAVEL;
        distance = DISH_Z_TRAVEL;
    }

    new Float:seconds = difference / DISH_MANUAL_SPEED;
    if (seconds < 1.0) seconds = 1.0;
    new Float:speed = distance / seconds;
    new duration = MoveObject(gDishObject, DISH_X, DISH_Y, nextZ, speed, 0.0, 0.0, target);
    if (duration <= 0)
    {
        printf("[BIG-EAR] MoveObject failed (target %.1f).", target);
        return false;
    }
    gDishMoving = true;
    gDishDestination = target;
    return true;
}

stock Dish_AdvanceAutomatic()
{
    if (!gDishAutomatic || !Dish_Ready()) return 0;

    new Float:current = Dish_CurrentAngle();
    new Float:next = current + float(gDishDirection) * DISH_AUTO_STEP;
    if (next > DISH_ROT_LIMIT || next < -DISH_ROT_LIMIT)
    {
        gDishDirection = -gDishDirection;
        next = current + float(gDishDirection) * DISH_AUTO_STEP;
    }
    if (!Dish_MoveTo(next))
    {
        gDishAutomatic = false;
        print("[BIG-EAR] Automatic movement stopped: could not move object.");
        return 0;
    }
    return 1;
}

stock Dish_Init()
{
    gDishObject = CreateObject(DISH_MODEL_ID, DISH_X, DISH_Y, DISH_Z,
        0.0, 0.0, 0.0, 300.0);
    if (!Dish_Ready())
    {
        gDishObject = INVALID_OBJECT_ID;
        gDishAutomatic = false;
        print("[BIG-EAR] Object allocation failed; vanilla map will not be hidden.");
        return 0;
    }

    // Safety: automatic sweeping is temporarily disabled until measured.
    // Operator may start it explicitly with /dish auto after CPU verification.
    gDishAutomatic = false;
    gDishMoving = false;
    gDishDirection = 1;
    gDishDestination = 0.0;
    print("[BIG-EAR] Vanilla dish replacement initialized.");
    return 1;
}

stock Dish_OnConnect(playerid)
{
    if (!Dish_Ready()) return 0;
    // Radius is deliberately small: only IPL entries sharing the dish origin,
    // including the matching distant LOD, should be affected.
    RemoveBuildingForPlayer(playerid, -1, DISH_X, DISH_Y, DISH_Z, 1.0);
    return 1;
}

stock Dish_OnObjectMoved(objectid)
{
    if (objectid != gDishObject) return 0;
    gDishMoving = false;
    if (gDishAutomatic) Dish_AdvanceAutomatic();
    return 1;
}

stock Dish_Shutdown()
{
    gDishAutomatic = false;
    gDishMoving = false;
    if (Dish_Ready())
    {
        StopObject(gDishObject);
        DestroyObject(gDishObject);
    }
    gDishObject = INVALID_OBJECT_ID;
    return 1;
}

stock Dish_Status(playerid)
{
    if (!Dish_Ready())
        return Dish_Message(playerid, "Радиотелескоп недоступен: объект не создан.", true);

    new message[144];
    format(message, sizeof(message),
        "The Big Ear: %s | угол %.1f | цель %.1f | %s",
        gDishAutomatic ? ("АВТО") : ("РУЧНОЙ"),
        Dish_CurrentAngle(), gDishDestination,
        gDishMoving ? ("движение") : ("остановлен"));
    Dish_Message(playerid, message);
    return 1;
}

stock Dish_Command(playerid, const params[])
{
    new operation[16];
    if (isnull(params))
    {
        Dish_Status(playerid);
        Dish_Usage(playerid);
        return 1;
    }
    if (sscanf(params, "s[16]", operation))
        return Dish_Usage(playerid);

    if (!strcmp(operation, "status", true)) return Dish_Status(playerid);
    if (!Admin_Require(playerid, PERM_MANAGE_STAFF)) return 1;
    if (!Dish_Ready()) return Dish_Status(playerid);

    if (!strcmp(operation, "stop", true))
    {
        gDishAutomatic = false;
        StopObject(gDishObject);
        gDishMoving = false;
        gDishDestination = Dish_CurrentAngle();
        return Dish_Message(playerid, "The Big Ear: привод остановлен. Режим ручного управления.");
    }
    if (!strcmp(operation, "auto", true))
    {
        if (gDishAutomatic) return Dish_Status(playerid);
        gDishAutomatic = true;
        if (!Dish_AdvanceAutomatic())
            return Dish_Message(playerid, "Не удалось запустить автоматический режим.", true);
        return Dish_Message(playerid, "The Big Ear: автоматическое сканирование включено.");
    }

    new operationCopy[16];
    new Float:value;
    if (sscanf(params, "s[16]f", operationCopy, value))
        return Dish_Message(playerid, "Укажите угол: /dish angle 45, /dish left 10 или /dish right 10.", true);

    new Float:target;
    if (!strcmp(operation, "angle", true))
    {
        if (value < -DISH_ROT_LIMIT || value > DISH_ROT_LIMIT)
            return Dish_Message(playerid, "Допустимый угол: от -140 до 140 градусов.", true);
        target = value;
    }
    else if (!strcmp(operation, "left", true) || !strcmp(operation, "right", true))
    {
        if (value < 1.0 || value > 90.0)
            return Dish_Message(playerid, "Шаг ручного поворота: от 1 до 90 градусов.", true);
        target = Dish_CurrentAngle() +
            (!strcmp(operation, "left", true) ? -value : value);
        if (target < -DISH_ROT_LIMIT || target > DISH_ROT_LIMIT)
            return Dish_Message(playerid, "Достигнут предел поворота (+/-140 градусов).", true);
    }
    else
        return Dish_Usage(playerid);

    gDishAutomatic = false;
    if (!Dish_MoveTo(target))
        return Dish_Message(playerid, "Не удалось повернуть антенну.", true);
    new message[100];
    format(message, sizeof(message), "The Big Ear: поворот к %.1f градуса.", target);
    return Dish_Message(playerid, message);
}
