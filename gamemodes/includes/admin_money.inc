#if defined _tarkenso_admin_money_included
    #endinput
#endif
#define _tarkenso_admin_money_included

// ADMIN-MONEY-01: intentional administrative cash mint, not a treasury transfer.
// Reuse the authoritative, save-before-HUD Economy_Credit path on BOTH file
// and MySQL account backends. Every completed grant is recorded in economy
// ledger and the administration audit log; do not use GivePlayerMoney directly.
#define ADMIN_MONEY_MAX_GRANT (1000000)

stock AdminMoney_Give(playerid,const params[])
{
    if(!Admin_Require(playerid,PERM_MANAGE_STAFF))return 1;

    new at=0,targetText[16],amountText[16],extra[8];
    if(!Admin_Next(params,at,targetText,sizeof(targetText)) ||
        !Admin_Next(params,at,amountText,sizeof(amountText)) ||
        Admin_Next(params,at,extra,sizeof(extra)) ||
        !Admin_UnsignedNumber(targetText) ||
        !Admin_UnsignedNumber(amountText))
        return AdminHelp_Usage(playerid,"givemoney");

    new target=strval(targetText),amount=strval(amountText);
    if(target<0 || target>=MAX_PLAYERS || IsPlayerNPC(target) ||
       !Admin_Ready(target))
        return SendClientMessage(playerid,ADMIN_ERROR,
            "Игрок не найден или ещё не авторизован.");
    if(amount<1 || amount>ADMIN_MONEY_MAX_GRANT)
        return SendClientMessage(playerid,ADMIN_ERROR,
            "Сумма должна быть от $1 до $1 000 000.");
#if defined TARKENSO_DB_PRIMARY
    if(!gDBPrimaryLoaded[target] || gDBPrimaryAccountId[target]<=0 ||
       gGovTransferBusy[target])
        return SendClientMessage(playerid,ADMIN_ERROR,
            "Денежная операция игрока уже выполняется либо MySQL недоступен.");
#endif
    if(gAccountMoney[target]<0 ||
       amount>ACCOUNT_MAX_MONEY-gAccountMoney[target] ||
       gAccountTxnSerial[target]>=ACCOUNT_MAX_MONEY)
        return SendClientMessage(playerid,ADMIN_ERROR,
            "Превышен максимальный баланс игрока или лимит транзакций.");

    new detail[128],message[180];
    format(detail,sizeof(detail),"to=%s playerid=%d amount=%d",
        gAccountName[target],target,amount);
    // No award without a durable administration request audit.
    if(!Admin_LogPlayer(playerid,"GIVEMONEY_REQUEST",detail))
        return SendClientMessage(playerid,ADMIN_ERROR,
            "Журнал администрации недоступен. Выдача отменена.");
    // Writes authenticated account balance + txn_serial before changing HUD.
    // A MySQL version conflict or failed save rolls back in-memory values.
    if(!Economy_Credit(target,amount,"admin_givemoney"))
    {
        Admin_LogPlayer(playerid,"GIVEMONEY_FAILED",detail);
        return SendClientMessage(playerid,ADMIN_ERROR,
            "Не удалось сохранить баланс игрока. Выдача отменена.");
    }

    Admin_LogPlayer(playerid,"GIVEMONEY",detail);
    format(message,sizeof(message),
        "Вы выдали игроку %s (ID %d) $%d.",gAccountName[target],target,amount);
    SendClientMessage(playerid,ADMIN_OK,message);
    format(message,sizeof(message),
        "Администрация выдала вам $%d. Баланс обновлён.",amount);
    SendClientMessage(target,ADMIN_OK,message);
    return 1;
}
