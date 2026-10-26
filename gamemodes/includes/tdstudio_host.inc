#if defined _tarkenso_tds_host
    #endinput
#endif
#define _tarkenso_tds_host

// The only project-specific adapter. Replace this file to extract the editor.
stock bool:TDSHost_Ready(playerid)
{
    if (!TDS_PlayerValid(playerid) || !Admin_Ready(playerid)) return false;
#if defined TARKENSO_DB_PRIMARY
    if (!gDBPrimaryLoaded[playerid] || gDBPrimaryAccountId[playerid] <= 0) return false;
#endif
    return true;
}

stock TDSHost_Account(playerid)
{
#if defined TARKENSO_DB_PRIMARY
    return gDBPrimaryAccountId[playerid];
#else
    #pragma unused playerid
    return 0;
#endif
}

stock TDSHost_Token(playerid)
{
    return gAuthToken[playerid];
}

stock bool:TDSHost_Founder(playerid)
{
    return TDS_PlayerValid(playerid) && Studio_Allowed(playerid);
}

stock bool:TDSHost_CanOpen(playerid)
{
    return !gSortOpen[playerid] && GetPlayerDialogID(playerid) == INVALID_DIALOG_ID &&
        GetPlayerState(playerid) == PLAYER_STATE_ONFOOT && gAccountSpawned[playerid] &&
        !gAccountDead[playerid] && !Race_IsParticipant(playerid) && !AdminPunish_IsJailed(playerid);
}

stock bool:TDSHost_LogGrant(playerid, target, bool:allow)
{
    new detail[80];
    format(detail, sizeof(detail), "target=%d account=%d allow=%d", target, TDSHost_Account(target), _:allow);
    return bool:Admin_LogPlayer(playerid, "TDSTUDIO_ACCESS", detail);
}
