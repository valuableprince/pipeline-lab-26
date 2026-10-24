#if defined _tarkenso_db_world_included
    #endinput
#endif
#define _tarkenso_db_world_included

// DB-WORLD-03: SQL primary for world state. File backend is compile-time fallback.
#if defined TARKENSO_DB_PRIMARY
    stock bool:DBWorld_Exec(const query[])
    {
        new changed;
        return DBPrimary_Execute(query, changed);
    }

    stock DBWorld_Rollback()
    {
        DBWorld_Exec("ROLLBACK");
        return 0;
    }

    stock bool:DBWorld_LeaderLoad(org)
    {
        if (!DBPrimary_IsReady() || org < 1 || org > FACTION_COUNT) return false;
        new o = org - 1, q[420], rows, start, end;
        mysql_format(gDBPrimary, q, sizeof(q),
            "SELECT term_start,term_end FROM factions WHERE faction_id=%d AND is_enabled=1 LIMIT 1", org);
        new Cache:c = mysql_query(gDBPrimary, q);
        if (c == Cache:0 || mysql_errno(gDBPrimary) || !cache_get_row_count(rows) || rows != 1)
        {
            if (c != Cache:0) cache_delete(c);
            return false;
        }
        new bool:ok = cache_get_value_name_int(0, "term_start", start) &&
            cache_get_value_name_int(0, "term_end", end);
        cache_delete(c);
        if (!ok || start < 0 || end < 0) return false;
        mysql_format(gDBPrimary, q, sizeof(q),
            "SELECT a.username,m.rank_id,m.joined_at,m.last_seen FROM faction_members m JOIN accounts a ON a.account_id=m.account_id WHERE m.faction_id=%d ORDER BY m.joined_at,m.account_id", org);
        c = mysql_query(gDBPrimary, q);
        if (c == Cache:0 || mysql_errno(gDBPrimary) || !cache_get_row_count(rows) ||
            rows > FACTION_ROSTER_MAX)
        {
            if (c != Cache:0) cache_delete(c);
            return false;
        }
        gFactionCount[o] = 0;
        gFactionTermStart[o] = start;
        gFactionTermEnd[o] = end;
        for (new i = 0; i < rows; i++)
        {
            new name[MAX_PLAYER_NAME], rank, joined, seen;
            if (!cache_get_value_name(i, "username", name, sizeof(name)) ||
                !cache_get_value_name_int(i, "rank_id", rank) ||
                !cache_get_value_name_int(i, "joined_at", joined) ||
                !cache_get_value_name_int(i, "last_seen", seen) ||
                !Admin_NormalizeName(name, gFactionMemberName[o][i], MAX_PLAYER_NAME) ||
                rank < 1 || rank > 10 || joined < 0 || seen < 0)
            {
                cache_delete(c);
                return false;
            }
            gFactionMemberRank[o][i] = rank;
            gFactionMemberJoined[o][i] = joined;
            gFactionMemberSeen[o][i] = seen;
            gFactionCount[o]++;
        }
        cache_delete(c);
        return Leader_ValidateRoster(org);
    }

    stock bool:DBWorld_LeaderSave(org)
    {
        if (!DBPrimary_IsReady() || !Leader_ValidateRoster(org)) return false;
        new o = org - 1, q[420], changed;
        if (!DBWorld_Exec("START TRANSACTION")) return false;
        mysql_format(gDBPrimary, q, sizeof(q),
            "UPDATE factions SET term_start=%d,term_end=%d WHERE faction_id=%d AND is_enabled=1",
            gFactionTermStart[o], gFactionTermEnd[o], org);
        if (!DBPrimary_Execute(q, changed)) { DBWorld_Rollback(); return false; }
        mysql_format(gDBPrimary, q, sizeof(q),
            "DELETE FROM faction_members WHERE faction_id=%d", org);
        if (!DBWorld_Exec(q)) { DBWorld_Rollback(); return false; }
        for (new i = 0; i < gFactionCount[o]; i++)
        {
            mysql_format(gDBPrimary, q, sizeof(q),
                "INSERT INTO faction_members(account_id,faction_id,rank_id,joined_at,last_seen) SELECT account_id,%d,%d,%d,%d FROM accounts WHERE username_key=LOWER('%e')",
                org, gFactionMemberRank[o][i], gFactionMemberJoined[o][i],
                gFactionMemberSeen[o][i], gFactionMemberName[o][i]);
            if (!DBPrimary_Execute(q, changed) || changed != 1)
            {
                DBWorld_Rollback();
                return false;
            }
        }
        if (!DBWorld_Exec("COMMIT")) { DBWorld_Rollback(); return false; }
        return true;
    }

    stock bool:DBWorld_DistrictLoad()
    {
        if (!DBPrimary_IsReady()) return false;
        new Cache:c = mysql_query(gDBPrimary,
            "SELECT district_id,pollution FROM cleaner_districts ORDER BY district_id");
        new rows;
        if (c == Cache:0 || mysql_errno(gDBPrimary) || !cache_get_row_count(rows) ||
            rows != CLEANER_DISTRICTS)
        {
            if (c != Cache:0) cache_delete(c);
            return false;
        }
        for (new i = 0; i < rows; i++)
        {
            new id, pollution;
            if (!cache_get_value_name_int(i, "district_id", id) ||
                !cache_get_value_name_int(i, "pollution", pollution) ||
                id != i || pollution < 0 || pollution > CLEANER_POLLUTION_MAX)
            {
                cache_delete(c);
                return false;
            }
            gCleanerDistrictDirt[i] = pollution;
        }
        cache_delete(c);
        return true;
    }

    stock bool:DBWorld_DistrictSave()
    {
        if (!DBPrimary_IsReady()) return false;
        for (new i = 0; i < CLEANER_DISTRICTS; i++)
            if (gCleanerDistrictDirt[i] < 0 ||
                gCleanerDistrictDirt[i] > CLEANER_POLLUTION_MAX) return false;
        new q[270];
        mysql_format(gDBPrimary, q, sizeof(q),
            "UPDATE cleaner_districts SET pollution=CASE district_id WHEN 0 THEN %d WHEN 1 THEN %d END WHERE district_id IN (0,1)",
            gCleanerDistrictDirt[0], gCleanerDistrictDirt[1]);
        return DBWorld_Exec(q);
    }

    stock bool:DBWorld_TopLoad()
    {
        if (!DBPrimary_IsReady()) return false;
        new Cache:c = mysql_query(gDBPrimary,
            "SELECT position_id,username,best_seconds FROM race_top ORDER BY position_id");
        new rows;
        if (c == Cache:0 || mysql_errno(gDBPrimary) || !cache_get_row_count(rows) ||
            rows > RACE_TOP_MAX)
        {
            if (c != Cache:0) cache_delete(c);
            return false;
        }
        new previous;
        for (new i = 0; i < rows; i++)
        {
            new position, seconds, username[MAX_PLAYER_NAME];
            if (!cache_get_value_name_int(i, "position_id", position) ||
                !cache_get_value_name_int(i, "best_seconds", seconds) ||
                !cache_get_value_name(i, "username", username, sizeof(username)) ||
                position != i + 1 || seconds < 1 || seconds > RACE_PROGRESS_MAX_TIME ||
                (i && seconds < previous) || !RaceProgress_ValidName(username))
            {
                cache_delete(c);
                return false;
            }
            format(gRaceTopName[i], MAX_PLAYER_NAME, "%s", username);
            gRaceTopSeconds[i] = seconds;
            previous = seconds;
            gRaceTopCount++;
        }
        cache_delete(c);
        return true;
    }

    stock bool:DBWorld_TopSave()
    {
        if (!DBPrimary_IsReady() || gRaceTopCount > RACE_TOP_MAX) return false;
        if (!DBWorld_Exec("START TRANSACTION")) return false;
        if (!DBWorld_Exec("DELETE FROM race_top")) { DBWorld_Rollback(); return false; }
        new q[240], changed;
        for (new i = 0; i < gRaceTopCount; i++)
        {
            if (!RaceProgress_ValidName(gRaceTopName[i]) ||
                gRaceTopSeconds[i] < 1 || gRaceTopSeconds[i] > RACE_PROGRESS_MAX_TIME)
            {
                DBWorld_Rollback();
                return false;
            }
            mysql_format(gDBPrimary, q, sizeof(q),
                "INSERT INTO race_top(position_id,username,best_seconds) VALUES (%d,'%e',%d)",
                i + 1, gRaceTopName[i], gRaceTopSeconds[i]);
            if (!DBPrimary_Execute(q, changed) || changed != 1)
            {
                DBWorld_Rollback();
                return false;
            }
        }
        if (!DBWorld_Exec("COMMIT")) { DBWorld_Rollback(); return false; }
        return true;
    }

    stock bool:DBWorld_ZoneLoad(slot)
    {
        if (!DBPrimary_IsReady() || slot < 0 || slot >= STUDIO_ZONE_COUNT) return false;
        gStudioZoneExists[slot] = false;
        gStudioZoneInvalid[slot] = false;
        gStudioZoneActive[slot] = false;
        new q[320], rows;
        mysql_format(gDBPrimary, q, sizeof(q),
            "SELECT is_active,x,y,z,radius,world_id,interior_id,zone_mode FROM studio_zones WHERE slot_id=%d",slot);
        new Cache:c = mysql_query(gDBPrimary, q);
        if (c == Cache:0 || mysql_errno(gDBPrimary) || !cache_get_row_count(rows))
        {
            if (c != Cache:0) cache_delete(c);
            gStudioZoneInvalid[slot] = true;
            return false;
        }
        if (rows == 0) { cache_delete(c); return true; }
        new active, world, interior, mode;
        new Float:x, Float:y, Float:z, Float:radius;
        new bool:ok = rows == 1 &&
            cache_get_value_name_int(0, "is_active", active) &&
            cache_get_value_name_int(0, "world_id", world) &&
            cache_get_value_name_int(0, "interior_id", interior) &&
            cache_get_value_name_int(0, "zone_mode", mode) &&
            cache_get_value_name_float(0, "x", x) &&
            cache_get_value_name_float(0, "y", y) &&
            cache_get_value_name_float(0, "z", z) &&
            cache_get_value_name_float(0, "radius", radius);
        cache_delete(c);
        if (!ok || (active != 0 && active != 1) || (mode != 0 && mode != 1) ||
            !StudioZone_Valid(x,y,z,radius,world,interior))
        {
            gStudioZoneInvalid[slot] = true;
            return false;
        }
        gStudioZoneExists[slot] = true;
        gStudioZoneActive[slot] = active == 1;
        gStudioZoneX[slot] = x;
        gStudioZoneY[slot] = y;
        gStudioZoneZ[slot] = z;
        gStudioZoneRadius[slot] = radius;
        gStudioZoneWorld[slot] = world;
        gStudioZoneInterior[slot] = interior;
        gStudioZoneMode[slot] = mode;
        return true;
    }

    stock bool:DBWorld_ZoneSave(slot, bool:active, Float:x, Float:y, Float:z,
        Float:radius, world, interior, mode)
    {
        if (!DBPrimary_IsReady() || slot < 0 || slot >= STUDIO_ZONE_COUNT ||
            (mode != 0 && mode != 1) || !StudioZone_Valid(x,y,z,radius,world,interior))
            return false;
        new q[560];
        mysql_format(gDBPrimary, q, sizeof(q),
            "INSERT INTO studio_zones(slot_id,is_active,x,y,z,radius,world_id,interior_id,zone_mode) VALUES (%d,%d,%.4f,%.4f,%.4f,%.3f,%d,%d,%d) ON DUPLICATE KEY UPDATE is_active=VALUES(is_active),x=VALUES(x),y=VALUES(y),z=VALUES(z),radius=VALUES(radius),world_id=VALUES(world_id),interior_id=VALUES(interior_id),zone_mode=VALUES(zone_mode)",
            slot, _:active, x, y, z, radius, world, interior, mode);
        return DBWorld_Exec(q);
    }
#endif
