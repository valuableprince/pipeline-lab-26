# GOV-00 — Atomic player ↔ treasury settlement (TARKENSO)

## Scope and safe rollout

GOV-00 is the **first phase** of the approved government bonds roadmap.
It establishes a reusable asynchronous, idempotent MySQL settlement primitive
between a registered player account and the one state treasury.

This PR does **not** add bonds, fund reserves, taxes, PayDay transfers,
admin commands, or scheduled cash movements. No money moves automatically.

Migration **018** is additive and is applied **once** after the already-installed
migration 017. Do NOT rerun or replace 017 and do NOT run test payments on
production. Backup the database first.

If 018 is missing, `GovTransfer_Request` returns false; account/treasury
systems continue operating as before. The feature requires
`TARKENSO_DB_PRIMARY`; no file-mode transfer fallback is allowed.

## SQL contract

`gov_transfer_post(event_key, account_id, treasury_delta, source, expected_account_version)`

- Positive `treasury_delta`: debit **account**, credit **state**.
- Negative `treasury_delta`: debit **state**, credit **account**.
- Amount must be nonzero, absolute value ≤ 1,000,000,000.
- `event_key` is unique, case-sensitive, 7–64 ASCII chars with `gov00:` prefix.
- `source` is a stable ASCII machine token, 1–32 chars.
- `account_id` is the persistent database account ID, never the session player ID.
- A new operation compares `accounts.row_version` with the expected version.
- Treasury limits, available balance, enabled flags and `minimum_reserve` are enforced.
- Atomic changes: `accounts.balance/txn_serial/row_version`,
  `treasury_accounts.balance/row_version`, `treasury_ledger`, `gov_transfers`.
- Transaction obtains InnoDB row locks in **treasury → account** order.
- On SQL exception, rollback preserves both wallets and both ledgers.
- A repeat with the same key, account, amount and source returns current balances
  with `replayed=1`, without double booking. Reuse with differing data fails.

The existing `treasury_post` is unchanged. Its transaction must **never** be
nested inside a `gov_transfer_post` call; both are independent top-level operations.

## Async Pawn adapter

`stock bool:GovTransfer_Request(playerid, amount, bool:toTreasury, const eventKey[], const source[])`

The caller must generate the event key server-side, validate the gameplay cause,
and retain the **same** key on retries. The bool return means the request
was queued, **not** that the payment succeeded. In GOV-00 no player/admin command
calls this API yet.

`mysql_tquery` issues the procedure asynchronously. Pending requests are
limited to 16 concurrently and one per player. While outstanding,
`Economy_Change`, `Economy_CleanerSettle` and `DBPrimary_WriteAccount`
refuse parallel wallet mutations/saves.

A matching completion callback reads the authoritative SQL balances,
account `txn_serial` and row versions, then updates the player's session wallet
and the treasury snapshot. On an unknown result (possibly committed, acknowledgement
lost), it **fails closed**: prevents saving stale money and disconnects the player,
so the next login reloads the authoritative database value.

Callback tokens also bind the request to `gAuthToken` and the persistent
account ID, preventing a late callback from crediting a different session
after a player slot was reused. SQL ledger/event keys are the final authority.

**Operational limitation:** Until a durable operation-result callback/inbox
and all other direct wallet mutation paths are audited, this primitive must
remain **unexposed to players**. The next implementation step may add an
outcome handler and a safe in-game feature only after live UAT. Avoid mixing
this with broader wallet refactoring in the same release.

## Minimal runtime SQL permissions

Runtime account needs:
- `SELECT` and `UPDATE (balance,row_version)` on `treasury_accounts`.
- `SELECT,INSERT` on `treasury_ledger`.
- `SELECT` and `UPDATE (balance,row_version,txn_serial)` on `accounts`
  (existing account saving may already need additional column UPDATE permissions).
- `SELECT,INSERT` on `gov_transfers`.
- `EXECUTE` on `gov_transfer_post`.

Procedure is `SQL SECURITY INVOKER`; these grants must be checked against
the existing runtime user and never copied verbatim to a public account.

## Staging/UAT checklist (real MySQL required)

Use isolated test accounts and an isolated copy of the 017 treasury.
Do **not** execute these transfers against real player accounts.

1. Backup MySQL; apply `018_gov_atomic_transfers.sql` after 017.
2. Verify `SELECT version FROM schema_migrations WHERE version=18`.
3. Verify `SELECT COUNT(*) FROM gov_transfers` = 0.
4. Seed a test account with a safe balance and note its `row_version`.
5. Seed treasury **only on staging** via an audited test credit.
6. Call `gov_transfer_post('gov00:uat:1', <account_id>, 100, 'uat', <version>)`.
7. Check **account -100**, **treasury +100**, and exactly one row in
   `gov_transfers` and exactly one matching `treasury_ledger` entry.
8. Repeat same call with same key/version. Expect `replayed=1`, no movement.
9. Reuse key with another amount or account. Expect SQL error, no movement.
10. Try stale version, insufficient balance, disabled treasury, transaction
    above limit, and treasury debit below reserve. Expect full rollback.
11. Test `-100` direction with a **new event key** and new account version;
    verify account +100, treasury -100.
12. Run concurrent purchase/replay stress tests: exactly-once posting.
13. Test disconnect/reconnect during async transfer and lost response;
    no stale-save overwrites, SQL totals unchanged.
14. Test account wallet cap, row version cap, transaction serial cap and
    total treasury cap.
15. Reload game mode; confirm balances and authoritative versions persist.
16. Review all ledger/event keys; verify no repeated commits.

Static `tests/test_gov_transfer.py` exercises structural contracts but does
**not** replace live SQL or open.mp testing.

## Next phases

GOV-01 adds protected fund reserve (F), committed guarantees (E), fund ledger
and reserve-aware debits; existing migration 018 remains immutable after applying.
GOV-02 registers issues/holdings; GOV-03 performs first bond placements using
this settlement primitive.
