#if defined _tarkenso_cleaner_container_included
    #endinput
#endif
#define _tarkenso_cleaner_container_included

// JOB-CLEANER-04. Server-wide, persisted refuse container. The 60-unit
// dumpster NEVER exceeds capacity; the adjoining 40-unit pile is separate.
#define CLEANER_DUMP_X              (-227.1175)
#define CLEANER_DUMP_Y              (1066.0662)
#define CLEANER_DUMP_Z              (19.7422)
#define CLEANER_DUMP_CAP            (60)
#define CLEANER_DUMP_OVERFLOW_CAP   (40)
#define CLEANER_DUMP_MODEL          (1344) // CJ_Dumpster2
#define CLEANER_DUMP_BAG_MODEL      (1264)
#define CLEANER_DUMP_BAG_OBJECTS    (10)
#define CLEANER_DUMP_INTERACT_SQ    (16.0)

new bool:gCleanerDumpReady;
new gCleanerDumpBin;
new gCleanerDumpOverflow;
new bool:gCleanerDumpStaged;
new gCleanerDumpNextBin;
new gCleanerDumpNextOverflow;
new gCleanerDumpObject = INVALID_OBJECT_ID;
new Text3D:gCleanerDumpLabel = Text3D:INVALID_3DTEXT_ID;
new gCleanerDumpBags[CLEANER_DUMP_BAG_OBJECTS];

// Points are deliberately adjacent to the container, not atop the entrance.
// Placement and collision are subject to live Fort Carson UAT.
new const Float:gCleanerDumpBagOffsets[CLEANER_DUMP_BAG_OBJECTS][2] =
{
    {-2.5, 0.5}, {-3.3, 0.6}, {-2.5, -0.6}, {-3.3, -0.6},
    {-4.1, 0.5}, {-4.1, -0.6}, {-2.5, -1.6}, {-3.3, -1.6},
    {-4.1, -1.6}, {-4.8, -0.6}
};

stock CleanerDump_GetBin() { return gCleanerDumpBin; }
stock CleanerDump_GetOverflow() { return gCleanerDumpOverflow; }
stock CleanerDump_GetBinCap() { return CLEANER_DUMP_CAP; }
stock CleanerDump_GetOverflowCap() { return CLEANER_DUMP_OVERFLOW_CAP; }

stock bool:CleanerDump_IsReady()
{
    return gCleanerDumpReady;
}

stock bool:CleanerDump_Near(playerid)
{
    if (!Cleaner_InWorld(playerid) ||
        GetPlayerState(playerid) != PLAYER_STATE_ONFOOT) return false;
    new Float:x, Float:y, Float:z;
    GetPlayerPos(playerid, x, y, z);
    x -= CLEANER_DUMP_X;
    y -= CLEANER_DUMP_Y;
    z -= CLEANER_DUMP_Z;
    return x * x + y * y + z * z <= CLEANER_DUMP_INTERACT_SQ;
}

stock CleanerDump_VisualBagCount(overflow)
{
    if (overflow > 30) return 10;
    if (overflow >= 20) return 4;
    if (overflow >= 10) return 2;
    return 0;
}

stock CleanerDump_Refresh()
{
    new title[125];
    format(title, sizeof(title),
        "{A7D6B5}Мусорный контейнер %d/%d\n{FFFFFF}/trashdrop — сдать мусор",
        gCleanerDumpBin, CLEANER_DUMP_CAP);
    if (gCleanerDumpLabel != Text3D:INVALID_3DTEXT_ID)
        Update3DTextLabelText(gCleanerDumpLabel, CLEANER_COLOR, title);
    new needed = CleanerDump_VisualBagCount(gCleanerDumpOverflow);
    for (new i = 0; i < CLEANER_DUMP_BAG_OBJECTS; i++)
    {
        if (i >= needed)
        {
            if (gCleanerDumpBags[i] != INVALID_OBJECT_ID)
            {
                DestroyObject(gCleanerDumpBags[i]);
                gCleanerDumpBags[i] = INVALID_OBJECT_ID;
            }
        }
        else if (gCleanerDumpBags[i] == INVALID_OBJECT_ID)
        {
            gCleanerDumpBags[i] = CreateObject(CLEANER_DUMP_BAG_MODEL,
                CLEANER_DUMP_X + gCleanerDumpBagOffsets[i][0],
                CLEANER_DUMP_Y + gCleanerDumpBagOffsets[i][1],
                CLEANER_DUMP_Z - 0.87, 0.0, 0.0, float(i * 31), 80.0);
        }
    }
    return 1;
}

stock bool:CleanerDump_Load()
{
#if defined TARKENSO_DB_PRIMARY
    if (!DBPrimary_IsReady()) return false;
    new Cache:c = mysql_query(gDBPrimary,
        "SELECT bin_units,overflow_units FROM cleaner_container WHERE container_id=1 LIMIT 1");
    new rows, bin, overflow;
    if (c == Cache:0 || mysql_errno(gDBPrimary) ||
        !cache_get_row_count(rows) || rows != 1)
    {
        if (c != Cache:0) cache_delete(c);
        return false;
    }
    new bool:ok = cache_get_value_name_int(0, "bin_units", bin) &&
        cache_get_value_name_int(0, "overflow_units", overflow);
    cache_delete(c);
    if (!ok) return false;
#else
    new path[] = CLEANER_DIRECTORY "/container.env";
    new backup[] = CLEANER_DIRECTORY "/container.bak";
    if (!fexist(path) && fexist(backup) && !frename(backup, path))
        return false;
    if (!fexist(path))
    {
        gCleanerDumpBin = 0;
        gCleanerDumpOverflow = 0;
        return true;
    }
    new File:f = fopen(path, io_read);
    if (!f) return false;
    new header[32], a[16], b[16];
    new bool:ok = fread(f, header, sizeof(header)) &&
        fread(f, a, sizeof(a)) && fread(f, b, sizeof(b));
    fclose(f);
    if (!ok) return false;
    Auth_StripLine(header);
    Auth_StripLine(a);
    Auth_StripLine(b);
    new bin, overflow;
    if (strcmp(header, "TARKENSODUMP|1", false) ||
        !Cleaner_Number(a, bin) || !Cleaner_Number(b, overflow))
        return false;
#endif
    if (bin < 0 || bin > CLEANER_DUMP_CAP ||
        overflow < 0 || overflow > CLEANER_DUMP_OVERFLOW_CAP ||
        (overflow > 0 && bin != CLEANER_DUMP_CAP)) return false;
    gCleanerDumpBin = bin;
    gCleanerDumpOverflow = overflow;
    return true;
}

#if !defined TARKENSO_DB_PRIMARY
stock bool:CleanerDump_SaveFile(bin, overflow)
{
    new temp[] = CLEANER_DIRECTORY "/container.tmp";
    new path[] = CLEANER_DIRECTORY "/container.env";
    new backup[] = CLEANER_DIRECTORY "/container.bak";
    if (fexist(temp)) fremove(temp);
    new File:f = fopen(temp, io_write);
    if (!f) return false;
    new bool:ok = fwrite(f, "TARKENSODUMP|1\n") > 0;
    new line[36];
    format(line, sizeof(line), "%d\n%d\n", bin, overflow);
    if (fwrite(f, line) <= 0) ok = false;
    fclose(f);
    if (!ok) { fremove(temp); return false; }
    new bool:hadOld = fexist(path) != 0;
    if (hadOld)
    {
        if (fexist(backup)) fremove(backup);
        if (!frename(path, backup)) { fremove(temp); return false; }
    }
    if (!frename(temp, path))
    {
        if (hadOld) frename(backup, path);
        fremove(temp);
        return false;
    }
    if (hadOld && fexist(backup)) fremove(backup);
    return true;
}
#endif

stock bool:CleanerDump_CanAccept(count)
{
    return gCleanerDumpReady && !gCleanerDumpStaged &&
        count > 0 && count <= 15 &&
        gCleanerDumpBin >= 0 && gCleanerDumpBin <= CLEANER_DUMP_CAP &&
        gCleanerDumpOverflow >= 0 && gCleanerDumpOverflow <= CLEANER_DUMP_OVERFLOW_CAP &&
        gCleanerDumpBin + gCleanerDumpOverflow + count <=
            CLEANER_DUMP_CAP + CLEANER_DUMP_OVERFLOW_CAP;
}

// Stage/Commit wraps Cleaner_Save, so SQL balance changes and bin changes
// are ONE InnoDB transaction. Failed reports do not consume capacity.
stock bool:CleanerDump_Stage(count)
{
    if (!CleanerDump_CanAccept(count)) return false;
    new freeSpace = CLEANER_DUMP_CAP - gCleanerDumpBin;
    new inBin = count;
    if (inBin > freeSpace) inBin = freeSpace;
    gCleanerDumpNextBin = gCleanerDumpBin + inBin;
    gCleanerDumpNextOverflow = gCleanerDumpOverflow + count - inBin;
#if defined TARKENSO_DB_PRIMARY
    if (!DBWorld_Exec("START TRANSACTION")) return false;
    new query[250], changed;
    mysql_format(gDBPrimary, query, sizeof(query),
        "UPDATE cleaner_container SET bin_units=%d,overflow_units=%d WHERE container_id=1 AND bin_units=%d AND overflow_units=%d",
        gCleanerDumpNextBin, gCleanerDumpNextOverflow,
        gCleanerDumpBin, gCleanerDumpOverflow);
    if (!DBPrimary_Execute(query, changed) || changed != 1)
    {
        DBWorld_Rollback();
        return false;
    }
#endif
    gCleanerDumpStaged = true;
    return true;
}

stock CleanerDump_Rollback()
{
#if defined TARKENSO_DB_PRIMARY
    if (gCleanerDumpStaged) DBWorld_Rollback();
#endif
    gCleanerDumpStaged = false;
    return 1;
}

stock bool:CleanerDump_Commit()
{
    if (!gCleanerDumpStaged) return false;
#if defined TARKENSO_DB_PRIMARY
    if (!DBWorld_Exec("COMMIT"))
    {
        CleanerDump_Rollback();
        gCleanerDumpReady = false; // uncertain transaction state: fail closed
        return false;
    }
#else
    if (!CleanerDump_SaveFile(gCleanerDumpNextBin, gCleanerDumpNextOverflow))
    {
        CleanerDump_Rollback();
        return false;
    }
#endif
    gCleanerDumpBin = gCleanerDumpNextBin;
    gCleanerDumpOverflow = gCleanerDumpNextOverflow;
    gCleanerDumpStaged = false;
    CleanerDump_Refresh();
    return true;
}

stock CleanerDump_Init()
{
    gCleanerDumpBin = 0;
    gCleanerDumpOverflow = 0;
    gCleanerDumpStaged = false;
    gCleanerDumpReady = CleanerDump_Load();
    if (!gCleanerDumpReady)
        print("[Cleaner] ERROR: container storage unavailable. Apply SQL migration 006 on SQL-first.");
    for (new i = 0; i < CLEANER_DUMP_BAG_OBJECTS; i++)
        gCleanerDumpBags[i] = INVALID_OBJECT_ID;
    gCleanerDumpObject = CreateObject(CLEANER_DUMP_MODEL,
        CLEANER_DUMP_X, CLEANER_DUMP_Y, CLEANER_DUMP_Z - 0.85,
        0.0, 0.0, 90.0, 160.0);
    new title[115];
    if (gCleanerDumpReady)
        format(title, sizeof(title),
            "{A7D6B5}Мусорный контейнер %d/%d\n{FFFFFF}/trashdrop — сдать мусор",
            gCleanerDumpBin, CLEANER_DUMP_CAP);
    else format(title, sizeof(title),
            "{FF8888}Мусорный контейнер: недоступен");
    gCleanerDumpLabel = Create3DTextLabel(title, CLEANER_COLOR,
        CLEANER_DUMP_X, CLEANER_DUMP_Y, CLEANER_DUMP_Z + 1.4,
        18.0, 0, false);
    if (gCleanerDumpReady) CleanerDump_Refresh();
    return 1;
}

stock CleanerDump_Shutdown()
{
    if (gCleanerDumpStaged) CleanerDump_Rollback();
    for (new i = 0; i < CLEANER_DUMP_BAG_OBJECTS; i++)
    {
        if (gCleanerDumpBags[i] != INVALID_OBJECT_ID)
            DestroyObject(gCleanerDumpBags[i]);
        gCleanerDumpBags[i] = INVALID_OBJECT_ID;
    }
    if (gCleanerDumpObject != INVALID_OBJECT_ID)
        DestroyObject(gCleanerDumpObject);
    gCleanerDumpObject = INVALID_OBJECT_ID;
    if (gCleanerDumpLabel != Text3D:INVALID_3DTEXT_ID)
        Delete3DTextLabel(gCleanerDumpLabel);
    gCleanerDumpLabel = Text3D:INVALID_3DTEXT_ID;
    gCleanerDumpReady = false;
    return 1;
}

// Temporary municipal service action, until truck logistics become a game job.
// Only the verified founder on duty can empty the dump.
stock CleanerDump_Service(playerid)
{
    if (!Admin_IsFounder(playerid) || !gAdminDuty[playerid])
        return SendClientMessage(playerid, ADMIN_ERROR,
            "Очистка доступна только основателю в режиме /aduty.");
    if (!gCleanerDumpReady || gCleanerDumpStaged)
        return SendClientMessage(playerid, ADMIN_ERROR,
            "Хранилище контейнера недоступно.");
    new info[100];
    format(info, sizeof(info), "bin=%d overflow=%d",
        gCleanerDumpBin, gCleanerDumpOverflow);
    if (!Admin_LogPlayer(playerid, "DUMP_SERVICE", info))
        return SendClientMessage(playerid, ADMIN_ERROR,
            "Журнал администрации недоступен. Очистка отменена.");
#if defined TARKENSO_DB_PRIMARY
    new q[190], changed;
    mysql_format(gDBPrimary, q, sizeof(q),
        "UPDATE cleaner_container SET bin_units=0,overflow_units=0 WHERE container_id=1 AND bin_units=%d AND overflow_units=%d",
        gCleanerDumpBin, gCleanerDumpOverflow);
    if (!DBPrimary_Execute(q, changed) || changed != 1)
        return SendClientMessage(playerid, ADMIN_ERROR,
            "Не удалось обновить контейнер в MariaDB.");
#else
    if (!CleanerDump_SaveFile(0, 0))
        return SendClientMessage(playerid, ADMIN_ERROR,
            "Не удалось сохранить очистку контейнера.");
#endif
    gCleanerDumpBin = 0;
    gCleanerDumpOverflow = 0;
    CleanerDump_Refresh();
    SendClientMessage(playerid, CLEANER_COLOR,
        "Муниципальный вывоз: контейнер и площадка очищены.");
    return 1;
}
