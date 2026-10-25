#if defined _tarkenso_race_progress_included
    #endinput
#endif
#define _tarkenso_race_progress_included

// RACE-PROGRESS-01: авторитетные награды и статистика в .acc v4.
// Топ-10 — вспомогательный кэш; отсутствие кэша не блокирует личные данные.
#define RACE_PROGRESS_MAX_TIME (300)
#define RACE_PROGRESS_COLOR    (0xE6BB79FF)
#define RACE_PAY_SOLO          (150)
#define RACE_PAY_FIRST         (600)
#define RACE_PAY_SECOND        (350)
#define RACE_PAY_FINISH        (200)
#define RACE_PAY_COOLDOWN      (900)
#define RACE_MIN_REWARD_TIME   (45)
#define RACE_TOP_MAX           (10)
#define RACE_TOP_DIRECTORY     "signal_races"
#define RACE_TOP_FILE          "signal_races/top10.dat"
#define RACE_TOP_TEMP          "signal_races/top10.dat.tmp"
#define RACE_TOP_BACKUP        "signal_races/top10.dat.bak"
#define RACE_TOP_HEADER        "TARKENSO_RACE_TOP|1"

new gRaceTopName[RACE_TOP_MAX][MAX_PLAYER_NAME];
new gRaceTopSeconds[RACE_TOP_MAX];
new gRaceTopCount;
new bool:gRaceTopWritable;

stock RaceProgress_ResetTop()
{
    gRaceTopCount = 0;
    for (new i = 0; i < RACE_TOP_MAX; i++)
    {
        gRaceTopName[i][0] = EOS;
        gRaceTopSeconds[i] = 0;
    }
    return 1;
}

stock bool:RaceProgress_ValidName(const value[])
{
    new len = strlen(value);
    if (len < 3 || len > 20) return false;
    for (new i = 0; i < len; i++)
        if (!((value[i] >= 'A' && value[i] <= 'Z') ||
              (value[i] >= 'a' && value[i] <= 'z') ||
              (value[i] >= '0' && value[i] <= '9') ||
               value[i] == '_')) return false;
    return true;
}

stock bool:RaceProgress_LoadTop()
{
    RaceProgress_ResetTop();
#if defined TARKENSO_DB_PRIMARY
    return DBWorld_TopLoad();
#else
    if (!fexist(RACE_TOP_FILE) && fexist(RACE_TOP_BACKUP))
    {
        if (!frename(RACE_TOP_BACKUP, RACE_TOP_FILE)) return false;
    }
    if (!fexist(RACE_TOP_FILE)) return true;
    new File:file = fopen(RACE_TOP_FILE, io_read);
    if (!file) return false;
    new line[96], bool:ok = true;
    if (!fread(file, line, sizeof(line))) ok = false;
    if (ok)
    {
        Auth_StripLine(line);
        if (strcmp(line, RACE_TOP_HEADER, false)) ok = false;
    }
    while (ok && fread(file, line, sizeof(line)))
    {
        if (gRaceTopCount >= RACE_TOP_MAX) { ok = false; break; }
        Auth_StripLine(line);
        new sep = strfind(line, "|"), len = strlen(line);
        if (sep < 3 || sep > 20 || sep >= len - 1) { ok = false; break; }
        new name[MAX_PLAYER_NAME], number[16], seconds;
        strmid(name, line, 0, sep, sizeof(name));
        strmid(number, line, sep + 1, len, sizeof(number));
        if (!RaceProgress_ValidName(name) ||
            !Auth_Unsigned(number, seconds, RACE_PROGRESS_MAX_TIME) ||
            seconds < 1) { ok = false; break; }
        if (gRaceTopCount && seconds < gRaceTopSeconds[gRaceTopCount - 1])
        {
            ok = false;
            break;
        }
        for (new i = 0; i < gRaceTopCount; i++)
        {
            if (!strcmp(name, gRaceTopName[i], true)) ok = false;
        }
        if (!ok) break;
        format(gRaceTopName[gRaceTopCount], MAX_PLAYER_NAME, "%s", name);
        gRaceTopSeconds[gRaceTopCount++] = seconds;
    }
    fclose(file);
    if (!ok) RaceProgress_ResetTop();
    return ok;
#endif
}

stock bool:RaceProgress_SaveTop()
{
    if (!gRaceTopWritable) return false;
#if defined TARKENSO_DB_PRIMARY
    return DBWorld_TopSave();
#else
    if (fexist(RACE_TOP_TEMP) && !fremove(RACE_TOP_TEMP)) return false;
    new File:file = fopen(RACE_TOP_TEMP, io_write);
    if (!file) return false;
    new bool:ok = fwrite(file, RACE_TOP_HEADER "\n") > 0;
    new line[96];
    for (new i = 0; i < gRaceTopCount; i++)
    {
        format(line, sizeof(line), "%s|%d\n",
            gRaceTopName[i], gRaceTopSeconds[i]);
        if (fwrite(file, line) <= 0) ok = false;
    }
    fclose(file);
    if (!ok) { fremove(RACE_TOP_TEMP); return false; }
    if (!fexist(RACE_TOP_FILE))
    {
        if (!frename(RACE_TOP_TEMP, RACE_TOP_FILE))
        {
            fremove(RACE_TOP_TEMP);
            return false;
        }
        return true;
    }
    if (!Admin_ReplaceFile(RACE_TOP_FILE, RACE_TOP_TEMP, RACE_TOP_BACKUP))
    {
        fremove(RACE_TOP_TEMP);
        return false;
    }
    return true;
#endif
}

// Кэш обновляется только ПОСЛЕ успешного сохранения личного дела.
stock bool:RaceProgress_UpdateTop(playerid)
{
    if (!gRaceTopWritable || gAccountRaceBest[playerid] < 1) return false;
    new seconds = gAccountRaceBest[playerid];
    new index = -1;
    for (new i = 0; i < gRaceTopCount; i++)
        if (!strcmp(gRaceTopName[i], gAccountName[playerid], true))
        {
            index = i;
            break;
        }
    if (index >= 0 && gRaceTopSeconds[index] <= seconds) return true;
    if (index < 0 && gRaceTopCount == RACE_TOP_MAX &&
        gRaceTopSeconds[RACE_TOP_MAX - 1] <= seconds) return true;

    new snapshotNames[RACE_TOP_MAX][MAX_PLAYER_NAME], snapshotSeconds[RACE_TOP_MAX];
    new oldCount = gRaceTopCount;
    for (new i = 0; i < RACE_TOP_MAX; i++)
    {
        format(snapshotNames[i], MAX_PLAYER_NAME, "%s", gRaceTopName[i]);
        snapshotSeconds[i] = gRaceTopSeconds[i];
    }
    if (index < 0)
    {
        index = gRaceTopCount;
        if (index < RACE_TOP_MAX) gRaceTopCount++;
        else index = RACE_TOP_MAX - 1;
    }
    format(gRaceTopName[index], MAX_PLAYER_NAME, "%s", gAccountName[playerid]);
    gRaceTopSeconds[index] = seconds;
    for (new i = gRaceTopCount - 1; i > 0; i--)
    {
        if (gRaceTopSeconds[i] >= gRaceTopSeconds[i - 1]) break;
        new holder[MAX_PLAYER_NAME], saved = gRaceTopSeconds[i - 1];
        format(holder, sizeof(holder), "%s", gRaceTopName[i - 1]);
        format(gRaceTopName[i - 1], MAX_PLAYER_NAME, "%s", gRaceTopName[i]);
        format(gRaceTopName[i], MAX_PLAYER_NAME, "%s", holder);
        gRaceTopSeconds[i - 1] = gRaceTopSeconds[i];
        gRaceTopSeconds[i] = saved;
    }
    if (RaceProgress_SaveTop()) return true;
    gRaceTopCount = oldCount;
    for (new i = 0; i < RACE_TOP_MAX; i++)
    {
        format(gRaceTopName[i], MAX_PLAYER_NAME, "%s", snapshotNames[i]);
        gRaceTopSeconds[i] = snapshotSeconds[i];
    }
    return false;
}

stock RaceProgress_Init()
{
    fcreatedir(RACE_TOP_DIRECTORY);
    gRaceTopWritable = RaceProgress_LoadTop();
    if (!gRaceTopWritable)
        print("[RACE-PROGRESS] Top-10 file corrupt; writes disabled until restored.");
    return 1;
}

stock RaceProgress_OnReady(playerid)
{
    if (gAccountRaceBest[playerid] > 0 && !RaceProgress_UpdateTop(playerid))
        printf("[RACE-PROGRESS] Failed to update Top-10 for account %d.", playerid);
    return 1;
}

stock RaceProgress_OnStart(playerid)
{
    if (gAccountRaceStarts[playerid] >= ACCOUNT_MAX_MONEY ||
        gAuthState[playerid] != AUTH_READY) return 0;
    gAccountRaceStarts[playerid]++;
    if (!Auth_SaveAccount(playerid))
    {
        gAccountRaceStarts[playerid]--;
        return 0;
    }
    return 1;
}

stock RaceProgress_Reward(playerid, place, runners, seconds)
{
    if (seconds < RACE_MIN_REWARD_TIME || seconds > RACE_PROGRESS_MAX_TIME ||
        gAccountRaceLastPaid[playerid] > gettime() ||
        (gAccountRaceLastPaid[playerid] &&
         gettime() - gAccountRaceLastPaid[playerid] < RACE_PAY_COOLDOWN))
        return 0;
    if (runners <= 1) return RACE_PAY_SOLO;
    if (place == 1) return RACE_PAY_FIRST;
    if (place == 2) return RACE_PAY_SECOND;
    return RACE_PAY_FINISH;
}

// Один Auth_SaveAccount фиксирует одновременно результат, кошелёк,
// серийный номер экономической транзакции и время последней выплаты.
stock RaceProgress_OnFinish(playerid, place, runners, seconds)
{
    if (seconds < 1 || seconds > RACE_PROGRESS_MAX_TIME ||
        gAuthState[playerid] != AUTH_READY ||
        gAccountRaceFinishes[playerid] >= ACCOUNT_MAX_MONEY)
        return 0;
    new bool:won = runners > 1 && place == 1;
    if (won && gAccountRaceWins[playerid] >= ACCOUNT_MAX_MONEY) return 0;
    new oldFinishes = gAccountRaceFinishes[playerid];
    new oldWins = gAccountRaceWins[playerid];
    new oldBest = gAccountRaceBest[playerid];
    new oldMoney = gAccountMoney[playerid];
    new oldEarned = gAccountRaceEarned[playerid];
    new oldPaid = gAccountRaceLastPaid[playerid];
    new oldSerial = gAccountTxnSerial[playerid];
    new reward = RaceProgress_Reward(playerid, place, runners, seconds);

    if (reward > ACCOUNT_MAX_MONEY - gAccountMoney[playerid] ||
        reward > ACCOUNT_MAX_MONEY - gAccountRaceEarned[playerid] ||
        (reward > 0 && gAccountTxnSerial[playerid] >= ACCOUNT_MAX_MONEY))
        reward = 0;

    gAccountRaceFinishes[playerid]++;
    if (won) gAccountRaceWins[playerid]++;
    if (oldBest == 0 || seconds < oldBest) gAccountRaceBest[playerid] = seconds;
    if (reward > 0)
    {
        gAccountMoney[playerid] += reward;
        gAccountRaceEarned[playerid] += reward;
        gAccountTxnSerial[playerid]++;
        gAccountRaceLastPaid[playerid] = gettime();
    }
    if (!Auth_SaveAccount(playerid))
    {
        gAccountRaceFinishes[playerid] = oldFinishes;
        gAccountRaceWins[playerid] = oldWins;
        gAccountRaceBest[playerid] = oldBest;
        gAccountMoney[playerid] = oldMoney;
        gAccountRaceEarned[playerid] = oldEarned;
        gAccountRaceLastPaid[playerid] = oldPaid;
        gAccountTxnSerial[playerid] = oldSerial;
        SendClientMessage(playerid, COLOR_RED, "Результат не сохранён. Награда не начислена.");
        return 0;
    }
    if (reward)
    {
        Economy_SyncHUD(playerid);
        Economy_Log(playerid, reward, "race_finish");
        new message[96];
        format(message, sizeof(message), "Финишная награда: $%d. Следующая выплата через 15 минут.", reward);
        SendClientMessage(playerid, RACE_PROGRESS_COLOR, message);
    }
    else SendClientMessage(playerid, COLOR_GREY,
        "Результат сохранён. Выплата сейчас недоступна (лимит/кулдаун/время).");

    if (!RaceProgress_UpdateTop(playerid))
        print("[RACE-PROGRESS] Top-10 update failed; personal result was saved.");
    return 1;
}

stock RaceProgress_Stats(playerid)
{
    new line[144];
    format(line, sizeof(line), "Гонки: стартов %d, финишей %d, побед %d.",
        gAccountRaceStarts[playerid], gAccountRaceFinishes[playerid],
        gAccountRaceWins[playerid]);
    SendClientMessage(playerid, RACE_PROGRESS_COLOR, line);
    if (gAccountRaceBest[playerid] > 0)
        format(line, sizeof(line), "Лучший результат: %d с; заработано $%d.",
            gAccountRaceBest[playerid], gAccountRaceEarned[playerid]);
    else format(line, sizeof(line), "Рекордов пока нет; заработано $%d.",
        gAccountRaceEarned[playerid]);
    SendClientMessage(playerid, RACE_PROGRESS_COLOR, line);
    return 1;
}

stock RaceProgress_Leaders(playerid)
{
    if (!gRaceTopWritable)
        return SendClientMessage(playerid, COLOR_RED,
            "Таблица рекордов недоступна: требуется восстановление файла.");
    if (!gRaceTopCount)
        return SendClientMessage(playerid, COLOR_GREY,
            "Пока нет зафиксированных рекордов трассы.");
    SendClientMessage(playerid, RACE_PROGRESS_COLOR, "Tarkenso RPG | Топ-10: лучшие времена трассы.");
    new message[96];
    for (new i = 0; i < gRaceTopCount; i++)
    {
        format(message, sizeof(message), "%d. %s — %d с",
            i + 1, gRaceTopName[i], gRaceTopSeconds[i]);
        SendClientMessage(playerid, RACE_PROGRESS_COLOR, message);
    }
    return 1;
}
