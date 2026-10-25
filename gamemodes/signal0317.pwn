#include <open.mp>
// Сторонние исходники зафиксированы в gamemodes/includes; бинарные компоненты ставятся отдельно.
#define SSCANF_NO_NICE_FEATURES
#include "Pawn.CMD.inc"
#include "sscanf2.inc"
#include "streamer.inc"

// «СИГНАЛ: 03:17». Файловая авторизация и сюжетная анкета персонажа.
// Технические идентификаторы и команды намеренно оставлены на английском.

#define COLOR_WHITE         0xFFFFFFFF
#define COLOR_AMBER         0xE6BB79FF
#define COLOR_GREY          0xA9A9A9FF
#define COLOR_RED           0xD97777FF

#define START_X             (-199.20)
#define START_Y             (1086.50)
#define START_Z             (19.60)
#define START_A             (90.0)
#define ACCOUNT_DIRECTORY   "signal_accounts"
#define ACCOUNT_FORMAT       "SIGNAL0317|5"
#define REGION_FORT_CARSON   (1)
#define REGION_LOS_SANTOS    (2)
#define REGION_PALOMINO      (3)
#define ACCOUNT_MAX_MONEY    (1000000000)
#define ACCOUNT_AUTOSAVE_MS  (120000)
#define PAYDAY_LEVEL_LIMIT    (1000)
#define PAYDAY_EXP_STEP       (4)
#define AUTH_COST           (12)
#define AUTH_LOCK_SECONDS   (300)
#define AUTH_FAILURE_WINDOW (600)
#define AUTH_LOCK_SLOTS     (128)
#define AUTH_MAX_PASSWORD   (64)

#define DIALOG_INTRO        (6100)
#define DIALOG_REGISTER     (6101)
#define DIALOG_CONFIRM      (6102)
#define DIALOG_BACKGROUND   (6103)
#define DIALOG_LOGIN        (6104)
#define DIALOG_SUCCESS      (6105)
#define DIALOG_REGION       (6106)

enum E_AUTH_STATE
{
    AUTH_NONE,
    AUTH_INTRO,
    AUTH_REGISTER_PASSWORD,
    AUTH_REGISTER_CONFIRM,
    AUTH_REGISTER_BACKGROUND,
    AUTH_REGISTER_REGION,
    AUTH_HASHING,
    AUTH_LOGIN,
    AUTH_VERIFYING,
    AUTH_SUCCESS,
    AUTH_READY
};

new E_AUTH_STATE:gAuthState[MAX_PLAYERS];
new gAuthToken[MAX_PLAYERS];
new gAccountName[MAX_PLAYERS][MAX_PLAYER_NAME];
new gPasswordPending[MAX_PLAYERS][AUTH_MAX_PASSWORD + 1];
new gAccountHash[MAX_PLAYERS][61];
new gBackground[MAX_PLAYERS];
new gStartRegion[MAX_PLAYERS];
new gAccountMoney[MAX_PLAYERS];
new gCharacterLevel[MAX_PLAYERS];
new gCharacterEXP[MAX_PLAYERS];
new gCharacterLastPaydayHour[MAX_PLAYERS];
new gStoryStage[MAX_PLAYERS];
// -1: нужна однократная инициализация исторически оплаченных услуг уборщика.
new gAccountCleanerPaid[MAX_PLAYERS];
// Driver career XP is distinct from collector's invariant xp=collected.
new gTruckDriverXP[MAX_PLAYERS];
new gAccountTxnSerial[MAX_PLAYERS];
new gAccountRaceStarts[MAX_PLAYERS];
new gAccountRaceFinishes[MAX_PLAYERS];
new gAccountRaceWins[MAX_PLAYERS];
new gAccountRaceBest[MAX_PLAYERS];
new gAccountRaceEarned[MAX_PLAYERS];
new gAccountRaceLastPaid[MAX_PLAYERS];
new Float:gAccountHealth[MAX_PLAYERS];
new Float:gAccountArmour[MAX_PLAYERS];
new bool:gAccountLegacy[MAX_PLAYERS];
new bool:gAccountSpawned[MAX_PLAYERS];
new bool:gAccountDead[MAX_PLAYERS];
new bool:gAuthDeathRespawnPending[MAX_PLAYERS];
new gAccountAutosaveTimer;
new gCleanerDumpNavTick[MAX_PLAYERS];
new gCleanerEditorOwner = 65535;
new bool:gAuthJustRegistered[MAX_PLAYERS];
new bool:gAuthDialogOpen[MAX_PLAYERS];
new gLockName[AUTH_LOCK_SLOTS][MAX_PLAYER_NAME];
new gLockFailures[AUTH_LOCK_SLOTS];
new gLockUntil[AUTH_LOCK_SLOTS];
new gLockTouched[AUTH_LOCK_SLOTS];

// API отдельного компонента samp-bcrypt.
// Нужен установленный плагин samp_bcrypt в pawn.legacy_plugins.
native bcrypt_hash(playerid, const callback[], const input[], cost, const args[] = "", {Float, _}:...);
native bcrypt_verify(playerid, const callback[], const input[], const hash[], const args[] = "", {Float, _}:...);
native bcrypt_get_hash(dest[], size = sizeof(dest));

forward Auth_OnPasswordHashed(playerid, token);
forward Auth_OnPasswordVerified(playerid, bool:success, token);
forward Auth_Autosave();
forward Auth_DeathRespawn(playerid, token);
forward bool:Auth_SaveAccount(playerid);
forward bool:Auth_Unsigned(const value[], &number, maximum);
forward bool:Economy_Credit(playerid, amount, const reason[]);
forward bool:Economy_Debit(playerid, amount, const reason[]);
forward bool:Economy_CleanerSettle(playerid, earned);
forward bool:Economy_CleanerInitialize(playerid, earned);
forward AdminTP_Reset(playerid);
forward AdminTP_Info(playerid);
forward CleanerDump_GetBin();
forward CleanerDump_NavReset(playerid);
forward CleanerDump_NavClear(playerid);
forward CleanerDump_Navigate(playerid, count);
forward CleanerEditor_List(playerid);
forward CleanerEditor_OnDialog(playerid, dialogid, response, listitem, const inputtext[]);
forward CleanerEditor_OnEdit(playerid, playerobject, objectid, EDIT_RESPONSE:response, Float:x, Float:y, Float:z, Float:rx, Float:ry, Float:rz);
forward CleanerDump_GetOverflow();
forward CleanerDump_GetBinCap();
forward CleanerDump_GetOverflowCap();
forward bool:CleanerDump_IsReady();
forward bool:CleanerDump_Near(playerid);
forward bool:CleanerDump_CanAccept(playerid, count);
forward bool:CleanerDump_Stage(playerid, count);
forward bool:CleanerDump_Commit();
forward CleanerDump_Rollback();
forward CleanerDump_Init();
forward CleanerDump_Shutdown();
forward CleanerDump_Service(playerid);
#if defined TARKENSO_DB_PRIMARY
forward bool:Auth_IsLocked(playerid);
forward bool:DBPrimary_AccountExists(playerid);
forward bool:DBPrimary_WriteAccount(playerid, bool:create = false);
forward bool:DBPrimary_LoadCleaner(playerid);
forward bool:DBPrimary_SaveCleaner(playerid);
forward bool:DBPrimary_ReadRole(playerid);
forward bool:DBPrimary_WriteRole(playerid, level, bool:eventRole);
forward bool:DBPrimary_IsBanned(playerid);
forward bool:DBPrimary_WriteBan(playerid, until, const reason[]);
forward bool:DBPrimary_BanExists(const name[]);
forward bool:DBPrimary_UnbanName(const name[]);
forward bool:DBWorld_LeaderSave(org);
forward bool:DBWorld_LeaderLoad(org);
forward bool:DBWorld_DistrictLoad();
forward bool:DBWorld_DistrictSave();
forward bool:DBWorld_TopLoad();
forward bool:DBWorld_TopSave();
forward bool:DBWorld_ZoneLoad(slot);
forward bool:DBWorld_ZoneSave(slot, bool:active, Float:x, Float:y, Float:z, Float:radius, world, interior, mode);
#endif

forward bool:AdminPunish_IsJailed(playerid);
forward bool:AdminPunish_Release(playerid, const actor[]);
forward bool:AdminPunish_WriteWarning(playerid, target, const reason[]);

#include "auth_camera.inc"
#include "auth_dialogs.inc"
#include "admin_system.inc"
#include "studio_system.inc"
#include "studio_ui.inc"
#include "economy.inc"
#include "admin_ui.inc"
#include "leader_system.inc"
#include "leader_ui.inc"
#include "orvelar_hud.inc"
#include "cleaner_job.inc"
#include "race_map.inc"
#include "race_event.inc"
#include "race_playlist.inc"
#include "radio_player.inc"
#include "race_audio.inc"
#include "race_progress.inc"
#include "race_core.inc"
#include "server_guard.inc"
#include "db_shadow.inc"
#include "db_primary.inc"
#include "payday.inc"
#include "story_prologue.inc"
#include "player_menu.inc"
#include "db_world.inc"
#include "cleaner_container.inc"
#include "cleaner_truck.inc"
#include "waste_sort.inc"
#include "cleaner_editor.inc"
#include "admin_teleport.inc"
#include "admin_vehicle.inc"
#include "admin_tools.inc"
#include "admin_help.inc"
#include "admin_tools2.inc"
#include "admin_punish.inc"
#include "commands.inc"

stock bool:Auth_SetAccountName(playerid)
{
    new original[MAX_PLAYER_NAME];
    GetPlayerName(playerid, original, sizeof(original));
    new length = strlen(original);
    if (length < 3 || length > 20) return false;

    for (new i = 0; i < length; i++)
    {
        new c = original[i];
        if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
              (c >= '0' && c <= '9') || c == '_')) return false;
        gAccountName[playerid][i] = c;
    }
    gAccountName[playerid][length] = EOS;
    return true;
}

// Пароли допускают только печатный ASCII, чтобы ввод был одинаков на любых клиентах.
stock bool:Auth_ValidPassword(const value[])
{
    new length = strlen(value);
    if (length < 8 || length > AUTH_MAX_PASSWORD) return false;
    for (new i = 0; i < length; i++)
    {
        if (value[i] < 33 || value[i] > 126) return false;
    }
    return true;
}

// Координаты появления по сохранённому региону, без смены выбора при релоге.
stock Auth_GetSpawnCoordinates(playerid, &Float:x, &Float:y, &Float:z, &Float:a)
{
    switch (gStartRegion[playerid])
    {
        case REGION_LOS_SANTOS:
        {
            x = 1481.0; y = -1736.0; z = 13.55; a = 90.0;
        }
        case REGION_PALOMINO:
        {
            x = 2246.0; y = -84.0; z = 26.5; a = 180.0;
        }
        default:
        {
            x = START_X; y = START_Y; z = START_Z; a = START_A;
        }
    }
    return 1;
}

stock Auth_AccountPath(playerid, path[], size)
{
    format(path, size, "%s/%s.acc", ACCOUNT_DIRECTORY, gAccountName[playerid]);
    return 1;
}

stock bool:Auth_AccountExists(playerid)
{
#if defined TARKENSO_DB_PRIMARY
    return DBPrimary_AccountExists(playerid);
#else
    new path[96];
    Auth_AccountPath(playerid, path, sizeof(path));
    return fexist(path) != 0;
#endif
}

stock Auth_StripLine(line[])
{
    new length = strlen(line);
    while (length > 0 && (line[length - 1] == '\n' || line[length - 1] == '\r'))
    {
        line[--length] = EOS;
    }
    return 1;
}

// Строгие числовые значения: повреждённые данные не превращаются в нули.
stock bool:Auth_Unsigned(const value[], &number, maximum)
{
    new length = strlen(value);
    if (length < 1 || length > 10) return false;
    for (new i = 0; i < length; i++)
        if (value[i] < '0' || value[i] > '9') return false;
    if (length == 10 && strcmp(value, "2147483647", false) > 0) return false;
    number = strval(value);
    return number >= 0 && number <= maximum;
}

stock bool:Auth_ParseVital(const value[], &Float:result)
{
    new length = strlen(value), dots;
    if (length < 1 || length > 12) return false;
    for (new i = 0; i < length; i++)
    {
        if (value[i] == '.')
        {
            if (++dots > 1 || i == 0 || i == length - 1) return false;
        }
        else if (value[i] < '0' || value[i] > '9') return false;
    }
    result = floatstr(value);
    return result >= 0.0 && result <= 100.0;
}

// Возвращает: 0 — новый аккаунт, 1 — существующий, -1 — повреждение.
stock Auth_ReadAccount(playerid)
{
    new path[96], backup[112], header[32], hash[96], traitLine[24];
    new muteText[24], moneyText[24], healthText[24], armourText[24];
    new paidText[24], serialText[24], extra[8];
    new startsText[24], finishesText[24], winsText[24];
    new bestText[24], earnedText[24], lastPaidText[24], regionText[24];
    Auth_AccountPath(playerid, path, sizeof(path));
    format(backup, sizeof(backup), "%s.bak", path);
    if (!fexist(path) && fexist(backup) && !frename(backup, path)) return -1;
    if (!fexist(path)) return 0;

    new File:handle = fopen(path, io_read);
    if (!handle) return -1;
    new bool:valid = true;
    if (!fread(handle, header, sizeof(header))) valid = false;
    if (!fread(handle, hash, sizeof(hash))) valid = false;
    if (!fread(handle, traitLine, sizeof(traitLine))) valid = false;
    if (!valid)
    {
        fclose(handle);
        return -1;
    }

    Auth_StripLine(header);
    Auth_StripLine(hash);
    Auth_StripLine(traitLine);
    new bool:v1 = !strcmp(header, "SIGNAL0317|1", false);
    new bool:v2 = !strcmp(header, "SIGNAL0317|2", false);
    new bool:v3 = !strcmp(header, "SIGNAL0317|3", false);
    new bool:v4 = !strcmp(header, "SIGNAL0317|4", false);
    if (!v1 && !v2 && !v3 && !v4 && strcmp(header, ACCOUNT_FORMAT, false))
    {
        fclose(handle);
        return -1;
    }
    new background, mutedUntil = 0, balance = 0, paid = -1, serial = 0;
    new Float:health = 100.0, Float:armour = 0.0;
    if (!Auth_Unsigned(traitLine, background, 3) || background < 1 ||
        strlen(hash) != 60 || hash[0] != '$' || hash[1] != '2')
        valid = false;
    if (!v1)
    {
        if (!fread(handle, muteText, sizeof(muteText)) ||
            !fread(handle, moneyText, sizeof(moneyText)) ||
            !fread(handle, healthText, sizeof(healthText)) ||
            !fread(handle, armourText, sizeof(armourText))) valid = false;
        if (valid)
        {
            Auth_StripLine(muteText);
            Auth_StripLine(moneyText);
            Auth_StripLine(healthText);
            Auth_StripLine(armourText);
            if (!Auth_Unsigned(muteText, mutedUntil, 2147483647) ||
                !Auth_Unsigned(moneyText, balance, ACCOUNT_MAX_MONEY) ||
                !Auth_ParseVital(healthText, health) ||
                !Auth_ParseVital(armourText, armour) || health < 1.0) valid = false;
        }
    }
    if (!v1 && !v2)
    {
        if (!fread(handle, paidText, sizeof(paidText)) ||
            !fread(handle, serialText, sizeof(serialText))) valid = false;
        if (valid)
        {
            Auth_StripLine(paidText);
            Auth_StripLine(serialText);
            if (strcmp(paidText, "-1", false))
            {
                if (!Auth_Unsigned(paidText, paid, CLEANER_MAX_TOTAL)) valid = false;
            }
            if (!Auth_Unsigned(serialText, serial, ACCOUNT_MAX_MONEY)) valid = false;
        }
    }
    new starts, finishes, wins, best, earned, lastPaid;
    if (!v1 && !v2 && !v3)
    {
        if (!fread(handle, startsText, sizeof(startsText)) ||
            !fread(handle, finishesText, sizeof(finishesText)) ||
            !fread(handle, winsText, sizeof(winsText)) ||
            !fread(handle, bestText, sizeof(bestText)) ||
            !fread(handle, earnedText, sizeof(earnedText)) ||
            !fread(handle, lastPaidText, sizeof(lastPaidText))) valid = false;
        if (valid)
        {
            Auth_StripLine(startsText);
            Auth_StripLine(finishesText);
            Auth_StripLine(winsText);
            Auth_StripLine(bestText);
            Auth_StripLine(earnedText);
            Auth_StripLine(lastPaidText);
            if (!Auth_Unsigned(startsText, starts, ACCOUNT_MAX_MONEY) ||
                !Auth_Unsigned(finishesText, finishes, ACCOUNT_MAX_MONEY) ||
                !Auth_Unsigned(winsText, wins, ACCOUNT_MAX_MONEY) ||
                !Auth_Unsigned(bestText, best, RACE_TIME_LIMIT) ||
                !Auth_Unsigned(earnedText, earned, ACCOUNT_MAX_MONEY) ||
                !Auth_Unsigned(lastPaidText, lastPaid, 2147483647) ||
                finishes > starts || wins > finishes ||
                (finishes == 0 && best != 0) || (finishes > 0 && best == 0) ||
                (earned == 0 && lastPaid != 0) ||
                (earned > 0 && lastPaid == 0)) valid = false;
        }
    }
    new region = REGION_FORT_CARSON;
    if (!v1 && !v2 && !v3 && !v4)
    {
        if (!fread(handle, regionText, sizeof(regionText))) valid = false;
        if (valid)
        {
            Auth_StripLine(regionText);
            if (!Auth_Unsigned(regionText, region, REGION_PALOMINO) ||
                region < REGION_FORT_CARSON) valid = false;
        }
    }
    if (fread(handle, extra, sizeof(extra))) valid = false;
    fclose(handle);
    if (!valid) return -1;
    format(gAccountHash[playerid], sizeof(gAccountHash[]), "%s", hash);
    gBackground[playerid] = background;
    gStartRegion[playerid] = region;
    gAdminMuteUntil[playerid] = mutedUntil > gettime() ? mutedUntil : 0;
    gAccountMoney[playerid] = balance;
    gAccountCleanerPaid[playerid] = paid;
    gAccountTxnSerial[playerid] = serial;
    gAccountRaceStarts[playerid] = starts;
    gAccountRaceFinishes[playerid] = finishes;
    gAccountRaceWins[playerid] = wins;
    gAccountRaceBest[playerid] = best;
    gAccountRaceEarned[playerid] = earned;
    gAccountRaceLastPaid[playerid] = lastPaid;
    gAccountHealth[playerid] = health;
    gAccountArmour[playerid] = armour;
    gAccountLegacy[playerid] = v1 || v2 || v3 || v4;
    return 1;
}

// Синхронизируем только реальные характеристики живого персонажа.
stock Auth_CaptureVitals(playerid)
{
    if (Race_IsParticipant(playerid) || !gAccountSpawned[playerid] || gAccountDead[playerid] ||
        GetPlayerState(playerid) == PLAYER_STATE_SPECTATING) return 0;
    new Float:health, Float:armour;
    GetPlayerHealth(playerid, health);
    GetPlayerArmour(playerid, armour);
    if (health >= 1.0 && health <= 100.0) gAccountHealth[playerid] = health;
    if (armour >= 0.0 && armour <= 100.0) gAccountArmour[playerid] = armour;
    return 1;
}

// Атомарная подготовка содержимого: .tmp -> .bak -> .acc.
stock bool:Auth_WriteData(playerid, bool:create = false)
{
#if defined TARKENSO_DB_PRIMARY
    return DBPrimary_WriteAccount(playerid, create);
#else
    if (gAccountName[playerid][0] == EOS || strlen(gAccountHash[playerid]) != 60 ||
        gBackground[playerid] < 1 || gBackground[playerid] > 3 ||
        gStartRegion[playerid] < REGION_FORT_CARSON || gStartRegion[playerid] > REGION_PALOMINO) return false;
    if (gAccountMoney[playerid] < 0 || gAccountMoney[playerid] > ACCOUNT_MAX_MONEY ||
        gAccountCleanerPaid[playerid] < -1 || gAccountCleanerPaid[playerid] > CLEANER_MAX_TOTAL ||
        gAccountTxnSerial[playerid] < 0 || gAccountTxnSerial[playerid] > ACCOUNT_MAX_MONEY ||
        gAccountRaceStarts[playerid] < 0 || gAccountRaceStarts[playerid] > ACCOUNT_MAX_MONEY ||
        gAccountRaceFinishes[playerid] < 0 ||
        gAccountRaceFinishes[playerid] > gAccountRaceStarts[playerid] ||
        gAccountRaceWins[playerid] < 0 ||
        gAccountRaceWins[playerid] > gAccountRaceFinishes[playerid] ||
        gAccountRaceBest[playerid] < 0 || gAccountRaceBest[playerid] > RACE_TIME_LIMIT ||
        (gAccountRaceFinishes[playerid] == 0 && gAccountRaceBest[playerid] != 0) ||
        (gAccountRaceFinishes[playerid] > 0 && gAccountRaceBest[playerid] == 0) ||
        gAccountRaceEarned[playerid] < 0 || gAccountRaceEarned[playerid] > ACCOUNT_MAX_MONEY ||
        gAccountRaceLastPaid[playerid] < 0 ||
        (gAccountRaceEarned[playerid] == 0 && gAccountRaceLastPaid[playerid] != 0) ||
        (gAccountRaceEarned[playerid] > 0 && gAccountRaceLastPaid[playerid] == 0))
        return false;
    new path[96], temp[112], backup[112], line[320];
    Auth_AccountPath(playerid, path, sizeof(path));
    format(temp, sizeof(temp), "%s.tmp", path);
    format(backup, sizeof(backup), "%s.bak", path);
    // The founder nickname must be provisioned offline, never via public registration.
    if (create && !strcmp(gAccountName[playerid], FOUNDER_NAME, true)) return false;
    if (create && (fexist(path) || fexist(backup))) return false;
    if (!create && !fexist(path))
    {
        if (!fexist(backup) || !frename(backup, path)) return false;
    }
    if (fexist(temp) && !fremove(temp)) return false;
    new File:file = fopen(temp, io_write);
    if (!file) return false;
    new bool:ok = fwrite(file, ACCOUNT_FORMAT "\n") > 0;
    format(line, sizeof(line), "%s\n%d\n%d\n%d\n%.2f\n%.2f\n%d\n%d\n%d\n%d\n%d\n%d\n%d\n%d\n%d\n",
        gAccountHash[playerid], gBackground[playerid], gAdminMuteUntil[playerid],
        gAccountMoney[playerid], gAccountHealth[playerid], gAccountArmour[playerid],
        gAccountCleanerPaid[playerid], gAccountTxnSerial[playerid],
        gAccountRaceStarts[playerid], gAccountRaceFinishes[playerid],
        gAccountRaceWins[playerid], gAccountRaceBest[playerid],
        gAccountRaceEarned[playerid], gAccountRaceLastPaid[playerid],
        gStartRegion[playerid]);
    if (fwrite(file, line) <= 0) ok = false;
    fclose(file);
    if (!ok) { fremove(temp); return false; }
    if (create)
    {
        if (fexist(path) || !frename(temp, path)) { fremove(temp); return false; }
    }
    else if (!Admin_ReplaceFile(path, temp, backup))
    {
        fremove(temp);
        return false;
    }
    gAccountLegacy[playerid] = false;
    return true;
#endif
}

stock bool:Auth_WriteAccount(playerid, const hash[])
{
    if (strlen(hash) != 60 || hash[0] != '$' || hash[1] != '2') return false;
    format(gAccountHash[playerid], sizeof(gAccountHash[]), "%s", hash);
    return Auth_WriteData(playerid, true);
}

stock bool:Auth_SaveAccount(playerid)
{
    if (gAuthState[playerid] != AUTH_READY) return false;
    Auth_CaptureVitals(playerid);
    return Auth_WriteData(playerid);
}

public Auth_Autosave()
{
    for (new playerid = 0; playerid < MAX_PLAYERS; playerid++)
        if (IsPlayerConnected(playerid) && !IsPlayerNPC(playerid) &&
            gAuthState[playerid] == AUTH_READY)
        {
            if (!Cleaner_SettlePending(playerid))
                printf("[ECONOMY] Failed to complete pending payout for player %d.", playerid);
            if (!Auth_SaveAccount(playerid))
                printf("[ACCOUNT] Autosave failed for player %d.", playerid);
        }
    return 1;
}

stock Auth_ShowIntro(playerid)
{
    gAuthState[playerid] = AUTH_INTRO;
    return AuthDialog_Intro(playerid);
}

stock Auth_ShowRegister(playerid, error = 0)
{
    gAuthState[playerid] = AUTH_REGISTER_PASSWORD;
    return AuthDialog_Register(playerid, error);
}

stock Auth_ShowConfirmation(playerid)
{
    gAuthState[playerid] = AUTH_REGISTER_CONFIRM;
    return AuthDialog_Confirm(playerid);
}

stock Auth_ShowBackground(playerid)
{
    gAuthState[playerid] = AUTH_REGISTER_BACKGROUND;
    return AuthDialog_Background(playerid);
}

stock Auth_ShowRegion(playerid)
{
    gAuthState[playerid] = AUTH_REGISTER_REGION;
    return AuthDialog_Region(playerid);
}

stock Auth_ShowLogin(playerid, failures = 0)
{
    gAuthState[playerid] = AUTH_LOGIN;
    return AuthDialog_Login(playerid, failures);
}

stock Auth_Finish(playerid)
{
    if (!IsPlayerConnected(playerid)) return 0;
    gAuthState[playerid] = AUTH_SUCCESS;
    return AuthDialog_Success(playerid, gAuthJustRegistered[playerid]);
}

stock Auth_EnterWorld(playerid)
{
    if (!IsPlayerConnected(playerid) || gAuthState[playerid] != AUTH_SUCCESS) return 0;
    gAuthState[playerid] = AUTH_READY;
    gAuthDialogOpen[playerid] = false;
    AuthCamera_Stop(playerid);
    if (!Admin_OnReady(playerid) || !AdminPunish_Load(playerid) ||
        (gAccountLegacy[playerid] && !Auth_SaveAccount(playerid)))
    {
        gAuthState[playerid] = AUTH_NONE;
        SendClientMessage(playerid, COLOR_RED, "Не удалось сохранить данные аккаунта.");
        Kick(playerid);
        return 0;
    }
    Leader_OnReady(playerid);
    Cleaner_OnReady(playerid);
    if (gTruckPersistenceReady && !TruckPersist_LoadProfile(playerid))
    {
        SendClientMessage(playerid, COLOR_RED,
            "Не удалось прочитать опыт муниципального водителя. Повторите вход.");
        Kick(playerid);
        return 0;
    }
    RaceProgress_OnReady(playerid);
    PayDay_OnReady(playerid);
    new Float:spawnX, Float:spawnY, Float:spawnZ, Float:spawnA;
    Auth_GetSpawnCoordinates(playerid, spawnX, spawnY, spawnZ, spawnA);
    SetSpawnInfo(playerid, NO_TEAM, 26, spawnX, spawnY, spawnZ, spawnA,
        WEAPON_FIST, 0, WEAPON_FIST, 0, WEAPON_FIST, 0);
    TogglePlayerSpectating(playerid, false);
    SetPlayerVirtualWorld(playerid, 0);
    SpawnPlayer(playerid);
    return 1;
}

// Истекшие записи освобождаются; активные блокировки не вытесняются.
stock Auth_FindLockSlot(const key[], bool:create = false)
{
    new freeSlot = -1, now = gettime();
    for (new i = 0; i < AUTH_LOCK_SLOTS; i++)
    {
        if (gLockName[i][0] != EOS)
        {
            if (gLockUntil[i] > now || gLockTouched[i] > now)
            {
                if (!strcmp(gLockName[i], key, false)) return i;
                continue;
            }
            gLockName[i][0] = EOS;
            gLockFailures[i] = 0;
            gLockUntil[i] = 0;
            gLockTouched[i] = 0;
        }
        if (freeSlot == -1) freeSlot = i;
    }
    if (!create || freeSlot < 0) return -1;

    format(gLockName[freeSlot], sizeof(gLockName[]), "%s", key);
    gLockFailures[freeSlot] = 0;
    gLockUntil[freeSlot] = 0;
    gLockTouched[freeSlot] = now + AUTH_FAILURE_WINDOW;
    return freeSlot;
}

stock bool:Auth_IsLocked(playerid)
{
    new slot = Auth_FindLockSlot(gAccountName[playerid]);
    if (slot < 0) return false;
    if (gLockUntil[slot] > gettime()) return true;
    // Истекшие блокировки уже удалены из таблицы при поиске.
    return false;
}

stock Auth_AuthFailure(playerid)
{
    new slot = Auth_FindLockSlot(gAccountName[playerid], true);
    if (slot < 0)
    {
        Kick(playerid);
        return 1;
    }
    gLockFailures[slot]++;
    gLockTouched[slot] = gettime() + AUTH_FAILURE_WINDOW;
    if (gLockFailures[slot] >= 3)
    {
        gLockUntil[slot] = gettime() + AUTH_LOCK_SECONDS;
        gLockTouched[slot] = gLockUntil[slot];
        SendClientMessage(playerid, COLOR_RED, "Архив заблокирован на 5 минут после трёх ошибок.");
        Kick(playerid);
        return 1;
    }
    Auth_ShowLogin(playerid, gLockFailures[slot]);
    return 1;
}

stock Auth_BeginAccount(playerid, bool:fromIntro = false)
{
#if defined TARKENSO_DB_PRIMARY
    return DBPrimary_BeginAccount(playerid, fromIntro);
#else
    new result = Auth_ReadAccount(playerid);
    if (result < 0)
    {
        SendClientMessage(playerid, COLOR_RED, "Личное дело повреждено или недоступно. Обратитесь к администратору.");
        Kick(playerid);
        return 0;
    }
    if (result == 0 && !strcmp(gAccountName[playerid], FOUNDER_NAME, true))
    {
        SendClientMessage(playerid, COLOR_RED, "Ник основателя зарезервирован. Регистрация недоступна.");
        printf("[FOUNDER] Registration with reserved founder name was rejected.");
        Kick(playerid);
        return 0;
    }
    if (result == 1)
    {
        DBShadow_CheckAccount(playerid);
        if (Auth_IsLocked(playerid))
        {
            SendClientMessage(playerid, COLOR_RED, "Доступ временно заблокирован после ошибок входа.");
            Kick(playerid);
            return 0;
        }
        Auth_ShowLogin(playerid);
    }
    else if (fromIntro) Auth_ShowRegister(playerid);
    else Auth_ShowIntro(playerid);
    return 1;
#endif
}

main()
{
    print("[Tarkenso RPG / SIGNAL 03:17] Starting game mode...");
}

public OnGameModeInit()
{
    SendRconCommand("hostname Tarkenso RPG");
    SetGameModeText("RPG | СИГНАЛ: 03:17");
    // GTA SA wheelie/jump/airtime payouts are disabled on this RP server.
    EnableStuntBonusForAll(false);
    ShowPlayerMarkers(PLAYER_MARKERS_MODE_OFF);
    ShowNameTags(true);
    DisableInteriorEnterExits();
    AddPlayerClass(26, START_X, START_Y, START_Z, START_A, WEAPON_FIST, 0, WEAPON_FIST, 0, WEAPON_FIST, 0);
    fcreatedir(ACCOUNT_DIRECTORY);
    gAccountAutosaveTimer = SetTimer("Auth_Autosave", ACCOUNT_AUTOSAVE_MS, true);
    DBShadow_Init();
    if (!DBPrimary_Init())
    {
        print("[DB-PRIMARY] MySQL unavailable; server startup aborted.");
        SendRconCommand("exit");
        return 0;
    }
    Guard_Init();


    PayDay_Init();
    Story_TapeInit();
    Cleaner_Init();
    CleanerDump_Init();
    Truck_Init();
    SortUI_Init();
    Race_Init();
    Admin_Init();
    AdminVeh_Init();
    gAdminJailTimer = SetTimer("AdminPunish_Tick", 5000, true);
    Studio_Init();
    Economy_Init();
    Leader_Init();
    OrvelarHUD_Init();
    print("[Tarkenso RPG / SIGNAL 03:17] Authentication initialized; samp-bcrypt required.");
    return 1;
}

public OnGameModeExit()
{
    if (gAccountAutosaveTimer) KillTimer(gAccountAutosaveTimer);
    gAccountAutosaveTimer = 0;
    Guard_Shutdown();
    if (gAdminJailTimer) KillTimer(gAdminJailTimer);
    gAdminJailTimer = 0;
    PayDay_Shutdown();
    Economy_Shutdown();
    Admin_ClockShutdown();
    Auth_Autosave();
    AdminVeh_Shutdown();
    OrvelarHUD_Shutdown();
    Race_Shutdown();
    SortUI_Shutdown();
    Truck_Shutdown();
    CleanerDump_Shutdown();
    Cleaner_Shutdown();
    Studio_Shutdown();
    DBShadow_Shutdown();
    DBPrimary_Shutdown();
    return 1;
}

public OnPlayerConnect(playerid)
{
    // Also apply at connection: default GTA stunt bonuses are enabled per player.
    EnableStuntBonusForPlayer(playerid, false);
    // No stock GTA checkpoints are used by the Tarkenso game mode.
    // Reset stale script checkpoints, not arbitrary map-icon slots.
    DisablePlayerCheckpoint(playerid);
    DisablePlayerRaceCheckpoint(playerid);
    Guard_OnConnect(playerid);
    Admin_OnConnect(playerid);
    AdminPunish_Reset(playerid);
    AdminVeh_Reset(playerid);
    CleanerDump_NavReset(playerid);
    AdminTP_Reset(playerid);
    StudioUI_Reset(playerid);
    Story_OnConnect(playerid);
    Cleaner_OnConnect(playerid);
    Truck_Reset(playerid);
    SortUI_Reset(playerid);
    Race_OnConnect(playerid);
    Radio_OnConnect(playerid);
    Leader_OnConnect(playerid);
    Leader_UI_Reset(playerid);
    Admin_UI_OnDisconnect(playerid);
    OrvelarHUD_OnConnect(playerid);
    gAuthToken[playerid]++;
    gAuthState[playerid] = AUTH_NONE;
    gAuthDialogOpen[playerid] = false;
    gAuthJustRegistered[playerid] = false;
    gPasswordPending[playerid][0] = EOS;
    gAccountHash[playerid][0] = EOS;
    gBackground[playerid] = 0;
    gStartRegion[playerid] = REGION_FORT_CARSON;
    gAccountMoney[playerid] = 0;
    gCharacterLevel[playerid] = 1;
    gCharacterEXP[playerid] = 0;
    gCharacterLastPaydayHour[playerid] = 0;
    SetPlayerScore(playerid, 0);
    gAccountCleanerPaid[playerid] = 0;
    gTruckDriverXP[playerid] = 0;
    gAccountTxnSerial[playerid] = 0;
    gAccountRaceStarts[playerid] = 0;
    gAccountRaceFinishes[playerid] = 0;
    gAccountRaceWins[playerid] = 0;
    gAccountRaceBest[playerid] = 0;
    gAccountRaceEarned[playerid] = 0;
    gAccountRaceLastPaid[playerid] = 0;
    gAccountHealth[playerid] = 100.0;
    gAccountArmour[playerid] = 0.0;
    gAccountLegacy[playerid] = false;
    gAccountSpawned[playerid] = false;
    gAccountDead[playerid] = false;

    if (!Auth_SetAccountName(playerid))
    {
        SendClientMessage(playerid, COLOR_RED, "Имя игрока: от 3 до 20 латинских букв, цифр и _. ");
        Kick(playerid);
        return 1;
    }

    DBShadow_OnConnect(playerid);
    DBPrimary_OnConnect(playerid);
    gAuthState[playerid] = AUTH_INTRO;
    AuthCamera_Start(playerid);
    Auth_BeginAccount(playerid);
    return 1;
}

public OnPlayerDisconnect(playerid, reason)
{
    Guard_OnDisconnect(playerid);
    if (gAuthState[playerid] == AUTH_READY && !Auth_SaveAccount(playerid))
        printf("[ACCOUNT] Failed to save account for player %d on disconnect.", playerid);
    DBShadow_OnDisconnect(playerid);
    DBPrimary_OnDisconnect(playerid);
    Radio_OnDisconnect(playerid);
    Race_OnDisconnect(playerid);
    if (gCleanerEditorOwner == playerid) CleanerEditor_Cancel();
    Studio_OnDisconnect(playerid);
    StudioUI_Reset(playerid);
    Story_OnDisconnect(playerid);
    Cleaner_OnDisconnect(playerid);
    SortUI_Reset(playerid);
    Truck_Reset(playerid);
    Admin_UI_OnDisconnect(playerid);
    AdminTP_Reset(playerid);
    AdminVeh_Reset(playerid);
    AdminPunish_Reset(playerid);
    Admin_OnDisconnect(playerid);
    Leader_OnDisconnect(playerid);
    Leader_UI_Reset(playerid);
    gAuthToken[playerid]++;
    gAuthState[playerid] = AUTH_NONE;
    gAuthDialogOpen[playerid] = false;
    gAuthJustRegistered[playerid] = false;
    AuthCamera_Stop(playerid);
    gPasswordPending[playerid][0] = EOS;
    gAccountHash[playerid][0] = EOS;
    gAccountName[playerid][0] = EOS;
    gBackground[playerid] = 0;
    gStartRegion[playerid] = REGION_FORT_CARSON;
    gAccountMoney[playerid] = 0;
    gCharacterLevel[playerid] = 1;
    gCharacterEXP[playerid] = 0;
    gCharacterLastPaydayHour[playerid] = 0;
    gAccountCleanerPaid[playerid] = 0;
    gTruckDriverXP[playerid] = 0;
    gAccountTxnSerial[playerid] = 0;
    gAccountRaceStarts[playerid] = 0;
    gAccountRaceFinishes[playerid] = 0;
    gAccountRaceWins[playerid] = 0;
    gAccountRaceBest[playerid] = 0;
    gAccountRaceEarned[playerid] = 0;
    gAccountRaceLastPaid[playerid] = 0;
    gAccountHealth[playerid] = 100.0;
    gAccountArmour[playerid] = 0.0;
    gAccountLegacy[playerid] = false;
    gAccountSpawned[playerid] = false;
    gAccountDead[playerid] = false;
    gAuthDeathRespawnPending[playerid] = false;
    return 1;
}

public OnPlayerRequestClass(playerid, classid)
{
    // Registered characters never enter SA-MP skin/class selection after death.
    if (gAuthState[playerid] == AUTH_READY && gAccountSpawned[playerid] &&
        gAccountDead[playerid])
    {
        // If the client enters class selection before the fallback timer,
        // force the saved class immediately; the timer becomes a no-op.
        SpawnPlayer(playerid);
        return 1;
    }
    if (gAuthState[playerid] != AUTH_READY && gAuthState[playerid] != AUTH_NONE)
    {
        AuthCamera_SetOpeningView(playerid);
        return 1;
    }
    new Float:spawnX, Float:spawnY, Float:spawnZ, Float:spawnA;
    Auth_GetSpawnCoordinates(playerid, spawnX, spawnY, spawnZ, spawnA);
    SetPlayerPos(playerid, spawnX, spawnY, spawnZ);
    SetPlayerCameraPos(playerid, spawnX + 4.0, spawnY + 6.0, spawnZ + 3.0);
    SetPlayerCameraLookAt(playerid, spawnX, spawnY, spawnZ);
    // Even the class-selection camera uses the live server environment.
    Admin_ApplyEvent(playerid);
    return 1;
}

public OnPlayerRequestSpawn(playerid)
{
    if (gAuthState[playerid] != AUTH_READY)
    {
        SendClientMessage(playerid, COLOR_RED, "Для появления необходимо открыть личное дело.");
        return 0;
    }
    return 1;
}

public OnPlayerSpawn(playerid)
{
    if (gAuthState[playerid] != AUTH_READY)
    {
        TogglePlayerControllable(playerid, false);
        TogglePlayerSpectating(playerid, true);
        AuthCamera_SetOpeningView(playerid);
        return 1;
    }

    if (!gAccountSpawned[playerid])
    {
        ResetPlayerMoney(playerid);
        if (gAccountMoney[playerid]) GivePlayerMoney(playerid, gAccountMoney[playerid]);
        SetPlayerHealth(playerid, gAccountHealth[playerid]);
        SetPlayerArmour(playerid, gAccountArmour[playerid]);
        gAccountSpawned[playerid] = true;
    }
    else if (gAccountDead[playerid])
    {
        gAccountHealth[playerid] = 100.0;
        gAccountArmour[playerid] = 0.0;
        SetPlayerHealth(playerid, 100.0);
        SetPlayerArmour(playerid, 0.0);
    }
    gAccountDead[playerid] = false;
    gAuthDeathRespawnPending[playerid] = false;
    // Clear inherited ordinary/race checkpoints on spawn. Our custom races
    // re-create their checkpoints only after a player has joined the event.
    DisablePlayerCheckpoint(playerid);
    DisablePlayerRaceCheckpoint(playerid);
    SortUI_Reset(playerid);
    Cleaner_OnSpawn(playerid);
    Truck_Reset(playerid);
    SetPlayerInterior(playerid, 0);
    SetPlayerVirtualWorld(playerid, 0);
    new Float:spawnX, Float:spawnY, Float:spawnZ, Float:spawnA;
    Auth_GetSpawnCoordinates(playerid, spawnX, spawnY, spawnZ, spawnA);
    SetPlayerPos(playerid, spawnX, spawnY, spawnZ);
    SetPlayerFacingAngle(playerid, spawnA);
    SetCameraBehindPlayer(playerid);
    TogglePlayerControllable(playerid, !gAdminFrozen[playerid]);
    OrvelarHUD_Show(playerid);
    Race_OnSpawn(playerid);
    CleanerDump_NavClear(playerid);
    Admin_ApplyEvent(playerid);
    if (!AdminPunish_OnSpawn(playerid)) Story_OnSpawn(playerid);
    return 1;
}

public OnPlayerDeath(playerid, killerid, WEAPON:reason)
{
    SortUI_Reset(playerid);
    if (Race_OnDeath(playerid)) return 1;
    if (gAuthState[playerid] == AUTH_READY)
    {
        gAccountDead[playerid] = true;
        gAccountHealth[playerid] = 100.0;
        gAccountArmour[playerid] = 0.0;
        if (!Auth_SaveAccount(playerid))
            printf("[ACCOUNT] Failed to persist death state for player %d.", playerid);
        if (!gAuthDeathRespawnPending[playerid])
        {
            gAuthDeathRespawnPending[playerid] = true;
            SetTimerEx("Auth_DeathRespawn", 1500, false, "ii", playerid, gAuthToken[playerid]);
        }
    }
    return 1;
}

public Auth_DeathRespawn(playerid, token)
{
    if (!IsPlayerConnected(playerid) || gAuthToken[playerid] != token ||
        gAuthState[playerid] != AUTH_READY || !gAccountDead[playerid]) return 1;
    gAuthDeathRespawnPending[playerid] = false;
    SpawnPlayer(playerid);
    return 1;
}

public OnPlayerKeyStateChange(playerid, KEY:newkeys, KEY:oldkeys)
{
    if (SortUI_OnKeyStateChange(playerid, newkeys, oldkeys)) return 1;
    if (Story_OnKeyStateChange(playerid, newkeys, oldkeys)) return 1;
    if (!Truck_OnKeyStateChange(playerid, newkeys, oldkeys))
        Cleaner_OnKeyStateChange(playerid, newkeys, oldkeys);
    return 1;
}

public OnPlayerClickPlayerTextDraw(playerid, PlayerText:playertextid)
{
    return SortUI_OnClick(playerid, playertextid);
}

public OnPlayerClickTextDraw(playerid, Text:clickedid)
{
    return SortUI_OnCancel(playerid, clickedid);
}

public OnPlayerEnterRaceCheckpoint(playerid)
{
    Race_OnCheckpoint(playerid);
    return 1;
}

public OnPlayerEnterVehicle(playerid, vehicleid, ispassenger)
{
    return Truck_OnEnterVehicle(playerid, vehicleid, ispassenger);
}

public OnPlayerStateChange(playerid, PLAYER_STATE:newstate, PLAYER_STATE:oldstate)
{
    Race_OnStateChange(playerid, newstate, oldstate);
    Truck_OnStateChange(playerid, newstate, oldstate);
    if (gCleanerDuty[playerid] && gCleanerBag[playerid] >= Cleaner_Capacity(playerid) &&
        gCleanerDumpNav[playerid])
    {
        CleanerDump_NavClear(playerid);
        CleanerDump_Navigate(playerid, gCleanerBag[playerid]);
    }
    return 1;
}

public OnPlayerEditObject(playerid, playerobject, objectid, EDIT_RESPONSE:response,
    Float:fX, Float:fY, Float:fZ, Float:fRotX, Float:fRotY, Float:fRotZ)
{
    return CleanerEditor_OnEdit(playerid, playerobject, objectid, response,
        fX, fY, fZ, fRotX, fRotY, fRotZ);
}

public OnPlayerClickMap(playerid, Float:fX, Float:fY, Float:fZ)
{
    return AdminTP_OnClickMap(playerid, fX, fY, fZ);
}

public OnVehicleDeath(vehicleid, killerid)
{
    #pragma unused killerid
    Truck_OnVehicleDeath(vehicleid);
    AdminVeh_OnVehicleDeath(vehicleid);
    return 1;
}

public OnDialogResponse(playerid, dialogid, response, listitem, inputtext[])
{
    if (Radio_OnDialog(playerid, dialogid, response, listitem)) return 1;
    if (Story_OnDialog(playerid, dialogid, response)) return 1;
    if (PlayerMenu_OnDialog(playerid, dialogid, response, listitem)) return 1;
    if (AdminTP_OnDialog(playerid, dialogid, response, inputtext)) return 1;
    if (CleanerEditor_OnDialog(playerid, dialogid, response, listitem, inputtext)) return 1;
    if (Truck_OnDialog(playerid, dialogid, response, listitem)) return 1;
    if (StudioUI_OnDialog(playerid, dialogid, response, listitem, inputtext)) return 1;
    if (Studio_OnDialog(playerid, dialogid, response, listitem)) return 1;
    if (AdminHelp_OnDialog(playerid, dialogid, response, listitem)) return 1;
    if (Admin_UI_OnDialog(playerid, dialogid, response, listitem, inputtext)) return 1;
    if (Leader_UI_OnDialog(playerid, dialogid, response, listitem, inputtext)) return 1;
    if (dialogid != DIALOG_INTRO && dialogid != DIALOG_REGISTER &&
        dialogid != DIALOG_CONFIRM && dialogid != DIALOG_BACKGROUND &&
        dialogid != DIALOG_LOGIN && dialogid != DIALOG_SUCCESS &&
        dialogid != DIALOG_REGION) return 0;
    if (!gAuthDialogOpen[playerid]) return 1;
    gAuthDialogOpen[playerid] = false;

    switch (dialogid)
    {
        case DIALOG_INTRO:
        {
            if (gAuthState[playerid] != AUTH_INTRO) return 1;
            if (!response)
            {
                Kick(playerid);
                return 1;
            }
            Auth_BeginAccount(playerid, true);
            return 1;
        }
        case DIALOG_REGISTER:
        {
            if (gAuthState[playerid] != AUTH_REGISTER_PASSWORD) return 1;
            if (!response)
            {
                gPasswordPending[playerid][0] = EOS;
                Auth_ShowIntro(playerid);
                return 1;
            }
            if (!Auth_ValidPassword(inputtext))
            {
                Auth_ShowRegister(playerid, 1);
                return 1;
            }
            format(gPasswordPending[playerid], sizeof(gPasswordPending[]), "%s", inputtext);
            Auth_ShowConfirmation(playerid);
            return 1;
        }
        case DIALOG_CONFIRM:
        {
            if (gAuthState[playerid] != AUTH_REGISTER_CONFIRM) return 1;
            if (!response)
            {
                gPasswordPending[playerid][0] = EOS;
                Auth_ShowRegister(playerid);
                return 1;
            }
            if (strcmp(gPasswordPending[playerid], inputtext, false) != 0)
            {
                gPasswordPending[playerid][0] = EOS;
                Auth_ShowRegister(playerid, 2);
                return 1;
            }
            Auth_ShowBackground(playerid);
            return 1;
        }
        case DIALOG_BACKGROUND:
        {
            if (gAuthState[playerid] != AUTH_REGISTER_BACKGROUND) return 1;
            if (!response)
            {
                gPasswordPending[playerid][0] = EOS;
                Auth_ShowRegister(playerid);
                return 1;
            }
            if (listitem < 0 || listitem > 2)
            {
                Auth_ShowBackground(playerid);
                return 1;
            }
            gBackground[playerid] = listitem + 1;
            Auth_ShowRegion(playerid);
            return 1;
        }
        case DIALOG_REGION:
        {
            if (gAuthState[playerid] != AUTH_REGISTER_REGION) return 1;
            if (!response)
            {
                Auth_ShowBackground(playerid);
                return 1;
            }
            if (listitem < 0 || listitem >= REGION_PALOMINO)
            {
                Auth_ShowRegion(playerid);
                return 1;
            }
            gStartRegion[playerid] = listitem + REGION_FORT_CARSON;
            if (Auth_AccountExists(playerid))
            {
                gPasswordPending[playerid][0] = EOS;
                SendClientMessage(playerid, COLOR_RED, "Это имя уже зарегистрировано.");
                Kick(playerid);
                return 1;
            }
            gAuthState[playerid] = AUTH_HASHING;
            SendClientMessage(playerid, COLOR_GREY, "Архив создаёт защищённое личное дело...");
            bcrypt_hash(playerid, "Auth_OnPasswordHashed", gPasswordPending[playerid],
                AUTH_COST, "d", gAuthToken[playerid]);
            gPasswordPending[playerid][0] = EOS;
            return 1;
        }
        case DIALOG_LOGIN:
        {
            if (gAuthState[playerid] != AUTH_LOGIN) return 1;
            if (!response)
            {
                Kick(playerid);
                return 1;
            }
            if (strlen(inputtext) < 1 || strlen(inputtext) > AUTH_MAX_PASSWORD)
            {
                Auth_AuthFailure(playerid);
                return 1;
            }
            if (Auth_IsLocked(playerid))
            {
                Kick(playerid);
                return 1;
            }
            gAuthState[playerid] = AUTH_VERIFYING;
            SendClientMessage(playerid, COLOR_GREY, "Проверяем пароль в архиве...");
            bcrypt_verify(playerid, "Auth_OnPasswordVerified", inputtext,
                gAccountHash[playerid], "d", gAuthToken[playerid]);
            return 1;
        }
        case DIALOG_SUCCESS:
        {
            if (gAuthState[playerid] != AUTH_SUCCESS) return 1;
            if (!response)
            {
                // ESC не позволяет пропустить подтверждение или обходить авторизацию.
                AuthDialog_Success(playerid, gAuthJustRegistered[playerid]);
                return 1;
            }
            Auth_EnterWorld(playerid);
            return 1;
        }
    }
    return 1;
}

public Auth_OnPasswordHashed(playerid, token)
{
    if (!IsPlayerConnected(playerid) || gAuthState[playerid] != AUTH_HASHING || gAuthToken[playerid] != token) return 1;

    new hash[61];
    bcrypt_get_hash(hash, sizeof(hash));
    if (strlen(hash) != 60 || hash[0] != '$' || hash[1] != '2' || !Auth_WriteAccount(playerid, hash))
    {
        SendClientMessage(playerid, COLOR_RED, "Не удалось сохранить аккаунт.");
        Kick(playerid);
        return 1;
    }

    format(gAccountHash[playerid], sizeof(gAccountHash[]), "%s", hash);
    gAuthJustRegistered[playerid] = true;
    Auth_Finish(playerid);
    return 1;
}

public Auth_OnPasswordVerified(playerid, bool:success, token)
{
    if (!IsPlayerConnected(playerid) || gAuthState[playerid] != AUTH_VERIFYING || gAuthToken[playerid] != token) return 1;
    if (!success)
    {
        Auth_AuthFailure(playerid);
        return 1;
    }
    new slot = Auth_FindLockSlot(gAccountName[playerid]);
    if (slot >= 0)
    {
        gLockFailures[slot] = 0;
        gLockUntil[slot] = 0;
        gLockTouched[slot] = 0;
        gLockName[slot][0] = EOS;
    }
    gAuthJustRegistered[playerid] = false;
    Auth_Finish(playerid);
    return 1;
}

public OnPlayerText(playerid, text[])
{
    if (gAuthState[playerid] != AUTH_READY) return 0;
    if (!Guard_AllowChat(playerid)) return 0;
    if (gAdminMuteUntil[playerid] > gettime())
    {
        SendClientMessage(playerid, ADMIN_ERROR, "Ваш чат временно заблокирован.");
        return 0;
    }
    // Собственный формат общего чата: игровой ник(ID): сообщение.
    // Фигурные скобки нейтрализуем, чтобы игрок не мог подменить цвет
    // чата через встроенную разметку {RRGGBB} SendClientMessage.
    new nickname[MAX_PLAYER_NAME], message[144], cleanText[116], n;
    GetPlayerName(playerid, nickname, sizeof(nickname));
    while (text[n] != EOS && n < sizeof(cleanText) - 1)
    {
        cleanText[n] = text[n];
        if (cleanText[n] == '{') cleanText[n] = '(';
        else if (cleanText[n] == '}') cleanText[n] = ')';
        n++;
    }
    cleanText[n] = EOS;
    format(message, sizeof(message), "%s(%d): %s", nickname, playerid, cleanText);
    SendClientMessageToAll(COLOR_WHITE, message);
    return 0;
}
