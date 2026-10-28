#if defined _tarkenso_bond_fund_included
    #endinput
#endif
#define _tarkenso_bond_fund_included

// GOV-01: protected bond-fund reserve inside the SINGLE state treasury.
// No issuing, purchase, tax routing, insurance payouts or admin commands yet.
// Authoritative state is InnoDB. These variables are read-only snapshots.
new bool:gBondFundReady;
new bool:gBondFundBusy;
new gBondFundRequestSerial;
new gBondFundReserved;
new gBondFundCommitted;
new bool:gBondFundEnabled;
new gBondFundCoverageBps;
new gBondFundPremiumBps;
new gBondFundUtilizationBps;
new gBondFundTaxAllocationBps;
new gBondFundGraceSeconds;
new gBondFundConfigVersion;

forward BondFund_OnComplete(requestSerial);

stock bool:BondFund_IsReady()
{
#if defined TARKENSO_DB_PRIMARY
    return gBondFundReady && DBPrimary_IsReady() && Treasury_IsReady();
#else
    return false;
#endif
}

stock BondFund_GetReserved()
{
    if (!BondFund_IsReady()) return -1;
    return gBondFundReserved;
}

stock BondFund_GetCommitted()
{
    if (!BondFund_IsReady()) return -1;
    return gBondFundCommitted;
}

// Avoid 32-bit Pawn overflow: reserved * utilization can reach 20 trillion.
stock BondFund_GetAvailableCapacity()
{
    if (!BondFund_IsReady()) return -1;
    new limit = (gBondFundReserved / 10000) * gBondFundUtilizationBps +
        ((gBondFundReserved % 10000) * gBondFundUtilizationBps) / 10000;
    if (limit < gBondFundCommitted) return 0;
    return limit - gBondFundCommitted;
}

stock BondFund_GetFreeBudget()
{
    if (!BondFund_IsReady()) return -1;
    if (gTreasuryBalance < gTreasuryMinimumReserve ||
        gTreasuryBalance - gTreasuryMinimumReserve < gBondFundReserved)
        return 0;
    return gTreasuryBalance - gTreasuryMinimumReserve - gBondFundReserved;
}

stock BondFund_Shutdown()
{
    gBondFundReady = false;
    gBondFundBusy = false;
    gBondFundRequestSerial++;
    gBondFundReserved = 0;
    gBondFundCommitted = 0;
    gBondFundEnabled = false;
    gBondFundCoverageBps = 0;
    gBondFundPremiumBps = 0;
    gBondFundUtilizationBps = 0;
    gBondFundTaxAllocationBps = 0;
    gBondFundGraceSeconds = 0;
    gBondFundConfigVersion = 0;
    return 1;
}

stock bool:BondFund_Reload()
{
    gBondFundReady = false;
#if defined TARKENSO_DB_PRIMARY
    if (gBondFundBusy || !Treasury_IsReady()) return false;
    new Cache:c = mysql_query(gDBPrimary,
        "SELECT version FROM schema_migrations WHERE version=19 LIMIT 1");
    if (c == Cache:0) return false;
    new rows = 0;
    new bool:found = mysql_errno(gDBPrimary) == 0 &&
        cache_get_row_count(rows) && rows == 1;
    cache_delete(c);
    if (!found) return false;

    c = mysql_query(gDBPrimary,
        "SELECT balance,row_version,bond_fund_reserved,bond_fund_committed,bond_fund_enabled,bond_fund_coverage_bps,bond_fund_premium_bps,bond_fund_utilization_bps,bond_fund_tax_allocation_bps,bond_fund_grace_seconds,bond_fund_config_version FROM treasury_accounts WHERE code='state' LIMIT 1");
    if (c == Cache:0) return false;

    new balance, version, reserved, committed, enabled;
    new coverage, premium, utilization, tax, grace, config;
    rows = 0;
    new bool:valid = mysql_errno(gDBPrimary) == 0 &&
        cache_get_row_count(rows) && rows == 1 &&
        cache_get_value_name_int(0, "balance", balance) &&
        cache_get_value_name_int(0, "row_version", version) &&
        cache_get_value_name_int(0, "bond_fund_reserved", reserved) &&
        cache_get_value_name_int(0, "bond_fund_committed", committed) &&
        cache_get_value_name_int(0, "bond_fund_enabled", enabled) &&
        cache_get_value_name_int(0, "bond_fund_coverage_bps", coverage) &&
        cache_get_value_name_int(0, "bond_fund_premium_bps", premium) &&
        cache_get_value_name_int(0, "bond_fund_utilization_bps", utilization) &&
        cache_get_value_name_int(0, "bond_fund_tax_allocation_bps", tax) &&
        cache_get_value_name_int(0, "bond_fund_grace_seconds", grace) &&
        cache_get_value_name_int(0, "bond_fund_config_version", config);
    cache_delete(c);

    if (!valid || balance < 0 || balance > TREASURY_MAX_BALANCE ||
        version < 0 || version > 2147483646 || reserved < 0 ||
        reserved > balance || committed < 0 || committed > reserved ||
        (enabled != 0 && enabled != 1) || coverage < 1 || coverage > 10000 ||
        premium < 0 || premium > 10000 || utilization < 1 ||
        utilization > 10000 || tax < 0 || tax > 10000 ||
        grace < 60 || grace > 2592000 || config < 0 ||
        config > 2147483646)
    {
        print("[GOV-01] Invalid bond fund state or configuration.");
        return false;
    }

    // SQL uses 64-bit for capacity. In Pawn this division avoids overflow.
    new capacity = (reserved / 10000) * utilization +
        ((reserved % 10000) * utilization) / 10000;
    if (committed > capacity)
    {
        print("[GOV-01] Bond fund guarantee commitment exceeds capacity.");
        return false;
    }

    gBondFundReserved = reserved;
    gBondFundCommitted = committed;
    gBondFundEnabled = enabled != 0;
    gBondFundCoverageBps = coverage;
    gBondFundPremiumBps = premium;
    gBondFundUtilizationBps = utilization;
    gBondFundTaxAllocationBps = tax;
    gBondFundGraceSeconds = grace;
    gBondFundConfigVersion = config;
    if (version >= gTreasuryVersion)
    {
        gTreasuryBalance = balance;
        gTreasuryVersion = version;
    }
    gBondFundReady = true;
    return true;
#else
    return false;
#endif
}

stock BondFund_Init()
{
    BondFund_Shutdown();
    if (BondFund_Reload())
    {
        printf("[GOV-01] Fund ready. Reserved=%d, committed=%d, enabled=%d.",
            gBondFundReserved,gBondFundCommitted,_:gBondFundEnabled);
        printf("[GOV-01] coverage_bp=%d premium_bp=%d tax_bp=%d grace=%d config_rev=%d.",
            gBondFundCoverageBps,gBondFundPremiumBps,gBondFundTaxAllocationBps,
            gBondFundGraceSeconds,gBondFundConfigVersion);
    }
    else
        print("[GOV-01] Fund inactive (migration 019 absent, file backend or DB issue).");
    return 1;
}

// Internal async dispatch; the return value confirms QUEUING, not settlement.
stock bool:BondFund_Queue(bool:commitment, amount, const eventKey[],
    const issueCode[], actorAccountId)
{
#if defined TARKENSO_DB_PRIMARY
    if (!BondFund_IsReady() || gBondFundBusy || amount <= 0 ||
        amount > TREASURY_MAX_CHANGE || actorAccountId < 0 ||
        !Treasury_ValidToken(eventKey, 64) ||
        strlen(eventKey) < 6 || strcmp(eventKey, "fund:", false, 5) != 0)
        return false;

    new query[480];
    if (commitment)
    {
        if (!gBondFundEnabled || !Treasury_ValidToken(issueCode, 64) ||
            amount > BondFund_GetAvailableCapacity()) return false;
        mysql_format(gDBPrimary, query, sizeof(query),
            "CALL bond_fund_commit('%e','%e',%d,NULLIF(%d,0))",
            eventKey, issueCode, amount, actorAccountId);
    }
    else
    {
        if (amount > BondFund_GetFreeBudget()) return false;
        mysql_format(gDBPrimary, query, sizeof(query),
            "CALL bond_fund_capitalize('%e',%d,NULLIF(%d,0))",
            eventKey, amount, actorAccountId);
    }

    gBondFundBusy = true;
    gBondFundRequestSerial++;
    if (!mysql_tquery(gDBPrimary, query, "BondFund_OnComplete", "i",
        gBondFundRequestSerial))
    {
        gBondFundBusy = false;
        return false;
    }
    return true;
#else
    #pragma unused commitment,amount,eventKey,issueCode,actorAccountId
    return false;
#endif
}

// Reclassify EXISTING treasury money. It does not credit the total cash balance.
stock bool:BondFund_Capitalize(amount, const eventKey[], actorAccountId = 0)
{
    return BondFund_Queue(false,amount,eventKey,"",actorAccountId);
}

// Reserve capacity for a future issued bond series (no player calls in GOV-01).
stock bool:BondFund_Commit(const issueCode[], amount, const eventKey[],
    actorAccountId = 0)
{
    return BondFund_Queue(true,amount,eventKey,issueCode,actorAccountId);
}

public BondFund_OnComplete(requestSerial)
{
#if defined TARKENSO_DB_PRIMARY
    if (!gBondFundBusy || requestSerial != gBondFundRequestSerial) return 1;
    gBondFundBusy = false;

    new rows = 0, balance, reserved, committed, version, replayed;
    new bool:valid = mysql_errno(gDBPrimary) == 0 &&
        cache_get_row_count(rows) && rows == 1 &&
        cache_get_value_name_int(0, "balance", balance) &&
        cache_get_value_name_int(0, "fund_reserved", reserved) &&
        cache_get_value_name_int(0, "fund_committed", committed) &&
        cache_get_value_name_int(0, "row_version", version) &&
        cache_get_value_name_int(0, "replayed", replayed);

    if (!valid || balance < 0 || balance > TREASURY_MAX_BALANCE ||
        reserved < 0 || reserved > balance || committed < 0 ||
        version < 0 || version > 2147483646 ||
        (replayed != 0 && replayed != 1))
    {
        // An ACK may have been lost after SQL COMMIT. Never trust stale snapshots.
        gBondFundReady = false;
        gTreasuryReady = false;
        print("[GOV-01] Fund response uncertain; safe reinitialization required.");
        return 1;
    }

    gBondFundReserved = reserved;
    gBondFundCommitted = committed;
    if (version >= gTreasuryVersion)
    {
        gTreasuryBalance = balance;
        gTreasuryVersion = version;
    }
    if (replayed) print("[GOV-01] Replayed fund operation; nothing charged twice.");
#else
    #pragma unused requestSerial
#endif
    return 1;
}
