#if defined _orvelar_economy_included
    #endinput
#endif
#define _orvelar_economy_included

// AUDIT-FIX-02: авторитетный кошелёк в .acc v3.
// Журнал только диагностический: деньги и маркеры записи сохраняются вместе.
#define ECONOMY_DIRECTORY "signal_economy"
#define ECONOMY_CLEANER_LIMIT (1000000)

stock Economy_Init()
{
    fcreatedir(ECONOMY_DIRECTORY);
    return 1;
}

stock Economy_Log(playerid, amount, const reason[])
{
    new File:file = fopen(ECONOMY_DIRECTORY "/ledger.log", io_append);
    if (!file)
    {
        print("[ECONOMY] Journal unavailable; account was already saved.");
        return 0;
    }
    new action[48], line[192];
    format(action, sizeof(action), "%s", reason);
    Admin_SafeText(action, sizeof(action));
    format(line, sizeof(line), "%d|%s|%d|%d|%d|%s\n",
        gettime(), gAccountName[playerid], gAccountTxnSerial[playerid],
        amount, gAccountMoney[playerid], action);
    new written = fwrite(file, line);
    fclose(file);
    return written > 0;
}

stock Economy_SyncHUD(playerid)
{
    if (!gAccountSpawned[playerid] || gAccountDead[playerid] ||
        gAuthState[playerid] != AUTH_READY) return 0;
    ResetPlayerMoney(playerid);
    if (gAccountMoney[playerid] > 0)
        GivePlayerMoney(playerid, gAccountMoney[playerid]);
    return 1;
}

// Пишем на диск прежде чем показывать деньги клиенту.
// Ошибка сохранения возвращает весь объём транзакции в памяти.
stock bool:Economy_Change(playerid, amount, const reason[])
{
    if (gAuthState[playerid] != AUTH_READY || amount == 0 ||
        gAccountTxnSerial[playerid] >= ACCOUNT_MAX_MONEY) return false;
    if (amount > 0)
    {
        if (amount > ACCOUNT_MAX_MONEY - gAccountMoney[playerid]) return false;
    }
    else if (amount < -gAccountMoney[playerid]) return false;

    new oldBalance = gAccountMoney[playerid];
    gAccountMoney[playerid] += amount;
    gAccountTxnSerial[playerid]++;
    if (!Auth_SaveAccount(playerid))
    {
        gAccountMoney[playerid] = oldBalance;
        gAccountTxnSerial[playerid]--;
        return false;
    }
    Economy_SyncHUD(playerid);
    Economy_Log(playerid, amount, reason);
    return true;
}

stock bool:Economy_Credit(playerid, amount, const reason[])
{
    if (amount <= 0) return false;
    return Economy_Change(playerid, amount, reason);
}

stock bool:Economy_Debit(playerid, amount, const reason[])
{
    if (amount <= 0 || amount > gAccountMoney[playerid]) return false;
    return Economy_Change(playerid, -amount, reason);
}

// На первом запуске v3 старые заработки не платим второй раз.
stock bool:Economy_CleanerInitialize(playerid, earned)
{
    if (gAuthState[playerid] != AUTH_READY ||
        earned < 0 || earned > ECONOMY_CLEANER_LIMIT) return false;
    if (gAccountCleanerPaid[playerid] >= 0) return true;
    gAccountCleanerPaid[playerid] = earned;
    if (!Auth_SaveAccount(playerid))
    {
        gAccountCleanerPaid[playerid] = -1;
        return false;
    }
    return true;
}

// Ровно один раз по монотонному earned: баланс и watermark в одном .acc.
stock bool:Economy_CleanerSettle(playerid, earned)
{
    if (gAuthState[playerid] != AUTH_READY ||
        earned < 0 || earned > ECONOMY_CLEANER_LIMIT ||
        gAccountCleanerPaid[playerid] < 0 ||
        earned < gAccountCleanerPaid[playerid]) return false;
    new amount = earned - gAccountCleanerPaid[playerid];
    if (amount == 0) return true;
    if (amount > ACCOUNT_MAX_MONEY - gAccountMoney[playerid] ||
        gAccountTxnSerial[playerid] >= ACCOUNT_MAX_MONEY) return false;
    new oldPaid = gAccountCleanerPaid[playerid];
    gAccountCleanerPaid[playerid] = earned;
    gAccountMoney[playerid] += amount;
    gAccountTxnSerial[playerid]++;
    if (!Auth_SaveAccount(playerid))
    {
        gAccountTxnSerial[playerid]--;
        gAccountMoney[playerid] -= amount;
        gAccountCleanerPaid[playerid] = oldPaid;
        return false;
    }
    Economy_SyncHUD(playerid);
    Economy_Log(playerid, amount, "cleaner_settle");
    return true;
}
