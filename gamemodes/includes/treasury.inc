#if defined _tarkenso_treasury_included
    #endinput
#endif
#define _tarkenso_treasury_included

// TREASURY-01: government budget is independent of player cash.
// SQL is authoritative; this snapshot is only for status/read APIs.
// No income, tax or PayDay integration is enabled at this stage.
#define TREASURY_STATE_ID       (1)
#define TREASURY_MAX_BALANCE    (2000000000)
#define TREASURY_MAX_CHANGE     (1000000000)

new bool:gTreasuryReady;
new gTreasuryBalance;
new gTreasuryVersion;
new bool:gTreasuryEnabled;
new bool:gTreasuryIncomeEnabled;
new bool:gTreasuryExpenseEnabled;
new gTreasuryMinimumReserve;
new gTreasuryMaxTransaction;
new gTreasuryIncomeTaxBasisPoints;
new gTreasuryConfigVersion;
new bool:gTreasuryLastReplayed;

stock bool:Treasury_IsReady()
{
#if defined TARKENSO_DB_PRIMARY
    return gTreasuryReady && DBPrimary_IsReady();
#else
    return false;
#endif
}

stock Treasury_GetBalance()
{
    if (!Treasury_IsReady()) return -1;
    return gTreasuryBalance;
}

// Event and source are stable ASCII machine identifiers, not player text.
stock bool:Treasury_ValidToken(const value[], maximum)
{
    new length = strlen(value);
    if (length < 1 || length > maximum) return false;
    for (new i = 0; i < length; i++)
    {
        new c = value[i];
        if ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') ||
            (c >= 'A' && c <= 'Z') || c == '_' || c == '-' ||
            c == ':' || c == '.' || c == '/') continue;
        return false;
    }
    return true;
}

stock Treasury_Shutdown()
{
    gTreasuryReady = false;
    gTreasuryBalance = 0;
    gTreasuryVersion = 0;
    gTreasuryEnabled = false;
    gTreasuryIncomeEnabled = false;
    gTreasuryExpenseEnabled = false;
    gTreasuryMinimumReserve = 0;
    gTreasuryMaxTransaction = 0;
    gTreasuryIncomeTaxBasisPoints = 0;
    gTreasuryConfigVersion = 0;
    gTreasuryLastReplayed = false;
    return 1;
}

// Refresh from the single source of truth after a controlled settings update.
// Missing 017 disables treasury only; it must NEVER prevent existing gameplay.
stock bool:Treasury_Reload()
{
    gTreasuryReady = false;
#if defined TARKENSO_DB_PRIMARY
    if (!DBPrimary_IsReady()) return false;

    new Cache:c = mysql_query(gDBPrimary,
        "SELECT version FROM schema_migrations WHERE version=17 LIMIT 1");
    if (c == Cache:0) return false;
    new rows = 0;
    new bool:found = mysql_errno(gDBPrimary) == 0 &&
        cache_get_row_count(rows) && rows == 1;
    cache_delete(c);
    if (!found)
    {
        print("[TREASURY] Schema v17 not installed. Treasury is disabled.");
        return false;
    }

    c = mysql_query(gDBPrimary,
        "SELECT balance,enabled,row_version,income_enabled,expense_enabled,minimum_reserve,max_transaction,income_tax_basis_points,config_version FROM treasury_accounts WHERE treasury_id=1 LIMIT 1");
    if (c == Cache:0) return false;

    new balance, enabled, version, income, expense, reserve, cap, tax, configVersion;
    rows = 0;
    new bool:valid = mysql_errno(gDBPrimary) == 0 &&
        cache_get_row_count(rows) && rows == 1 &&
        cache_get_value_name_int(0, "balance", balance) &&
        cache_get_value_name_int(0, "enabled", enabled) &&
        cache_get_value_name_int(0, "row_version", version) &&
        cache_get_value_name_int(0, "income_enabled", income) &&
        cache_get_value_name_int(0, "expense_enabled", expense) &&
        cache_get_value_name_int(0, "minimum_reserve", reserve) &&
        cache_get_value_name_int(0, "max_transaction", cap) &&
        cache_get_value_name_int(0, "income_tax_basis_points", tax) &&
        cache_get_value_name_int(0, "config_version", configVersion);
    cache_delete(c);

    if (!valid || balance < 0 || balance > TREASURY_MAX_BALANCE ||
        version < 0 || version > 2147483646 ||
        (enabled != 0 && enabled != 1) || (income != 0 && income != 1) ||
        (expense != 0 && expense != 1) || reserve < 0 ||
        reserve > TREASURY_MAX_BALANCE || cap <= 0 || cap > TREASURY_MAX_CHANGE ||
        tax < 0 || tax > 10000 || configVersion < 0 || configVersion > 2147483646)
    {
        print("[TREASURY] Invalid state/settings row; treasury is disabled.");
        return false;
    }

    gTreasuryBalance = balance;
    gTreasuryVersion = version;
    gTreasuryEnabled = enabled != 0;
    gTreasuryIncomeEnabled = income != 0;
    gTreasuryExpenseEnabled = expense != 0;
    gTreasuryMinimumReserve = reserve;
    gTreasuryMaxTransaction = cap;
    gTreasuryIncomeTaxBasisPoints = tax;
    gTreasuryConfigVersion = configVersion;
    gTreasuryLastReplayed = false;
    gTreasuryReady = true;
    return true;
#else
    return false;
#endif
}

stock Treasury_Init()
{
    Treasury_Shutdown();
    if (Treasury_Reload())
        printf("[TREASURY] Loaded state treasury, balance=%d, revision=%d.",
            gTreasuryBalance, gTreasuryVersion);
    else
        print("[TREASURY] Not active (file backend, unavailable DB, or migration 017 missing).");
    return 1;
}

// Exactly one atomic SQL procedure call for balance + append-only ledger.
// The event key is stable across retries. A matching replay succeeds without
// posting a second entry; a reused key with different data is rejected.
// No client or actor input is trusted as an amount/balance.
stock bool:Treasury_Post(delta, const eventKey[], const source[], const note[], actorAccountId = 0)
{
#if defined TARKENSO_DB_PRIMARY
    gTreasuryLastReplayed = false;
    if (!Treasury_IsReady() || delta == 0 ||
        delta < -TREASURY_MAX_CHANGE || delta > TREASURY_MAX_CHANGE ||
        actorAccountId < 0 || !Treasury_ValidToken(eventKey, 64) ||
        !Treasury_ValidToken(source, 32) || strlen(note) > 120)
        return false;

    new query[768];
    mysql_format(gDBPrimary, query, sizeof(query),
        "CALL treasury_post(%d,'%e',%d,'%e',NULLIF(%d,0),'%e')",
        TREASURY_STATE_ID, eventKey, delta, source, actorAccountId, note);
    new Cache:c = mysql_query(gDBPrimary, query);
    if (c == Cache:0)
    {
        print("[TREASURY] Posting failed; transaction not confirmed.");
        return false;
    }

    new balance, version, replayed, rows = 0;
    new bool:valid = mysql_errno(gDBPrimary) == 0 &&
        cache_get_row_count(rows) && rows == 1 &&
        cache_get_value_name_int(0, "balance_after", balance) &&
        cache_get_value_name_int(0, "row_version", version) &&
        cache_get_value_name_int(0, "replayed", replayed);
    cache_delete(c);
    if (!valid)
    {
        print("[TREASURY] Posting refused or acknowledgement unavailable.");
        // A write MAY have committed if the acknowledgement was lost.
        // Do not trust the local snapshot until an explicit database reload.
        gTreasuryReady = false;
        return false;
    }

    if (balance < 0 || balance > TREASURY_MAX_BALANCE ||
        version < 0 || version > 2147483646 ||
        (replayed != 0 && replayed != 1))
    {
        gTreasuryReady = false;
        print("[TREASURY] Invalid operation acknowledgement.");
        return false;
    }
    gTreasuryBalance = balance;
    gTreasuryVersion = version;
    gTreasuryLastReplayed = replayed != 0;
    return true;
#else
    #pragma unused delta, eventKey, source, note, actorAccountId
    return false;
#endif
}

stock bool:Treasury_Credit(amount, const eventKey[], const source[], const note[], actorAccountId = 0)
{
    if (amount <= 0 || amount > TREASURY_MAX_CHANGE) return false;
    return Treasury_Post(amount, eventKey, source, note, actorAccountId);
}

stock bool:Treasury_Debit(amount, const eventKey[], const source[], const note[], actorAccountId = 0)
{
    if (amount <= 0 || amount > TREASURY_MAX_CHANGE) return false;
    return Treasury_Post(-amount, eventKey, source, note, actorAccountId);
}
