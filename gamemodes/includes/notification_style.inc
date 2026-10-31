#if defined _tarkenso_notification_style_included
    #endinput
#endif
#define _tarkenso_notification_style_included

// NOTIFY-01: one presentation contract for gameplay SYSTEM notices.
// This is NOT a replacement for RP speech, /r chat or raw moderation logs.
// Accent identifies the system; semantic color identifies the outcome.
// Each outgoing SA-MP chat message is bounded to 144 characters.
#define TNOTIFY_GENERAL   (0)
#define TNOTIFY_ADMIN     (1)
#define TNOTIFY_FACTION   (2)
#define TNOTIFY_FINANCE   (3)
#define TNOTIFY_CRIME     (4)
#define TNOTIFY_WORK      (5)
#define TNOTIFY_RACE      (6)
#define TNOTIFY_SHIP      (7)
#define TNOTIFY_STORY     (8)

#define TNOTIFY_INFO      (0)
#define TNOTIFY_SUCCESS   (1)
#define TNOTIFY_WARNING   (2)
#define TNOTIFY_HINT      (3)

#define TNOTIFY_MAX_CHAT (145)

stock TNotify_Theme(system, label[], labelSize, accent[], accentSize)
{
    switch(system)
    {
        case TNOTIFY_ADMIN:
        {
            format(label,labelSize,"Администрация");
            format(accent,accentSize,"92B8FF");
        }
        case TNOTIFY_FACTION:
        {
            format(label,labelSize,"Организация");
            format(accent,accentSize,"9DD4B4");
        }
        case TNOTIFY_FINANCE:
        {
            format(label,labelSize,"Госзаймы");
            format(accent,accentSize,"D9AA55");
        }
        case TNOTIFY_CRIME:
        {
            format(label,labelSize,"Криминальная система");
            format(accent,accentSize,"FFD15C");
        }
        case TNOTIFY_WORK:
        {
            format(label,labelSize,"Работа");
            format(accent,accentSize,"A7D6B5");
        }
        case TNOTIFY_RACE:
        {
            format(label,labelSize,"Гонки");
            format(accent,accentSize,"E6BB79");
        }
        case TNOTIFY_SHIP:
        {
            format(label,labelSize,"Судоходство");
            format(accent,accentSize,"87D9E5");
        }
        case TNOTIFY_STORY:
        {
            format(label,labelSize,"История");
            format(accent,accentSize,"E6BB79");
        }
        default:
        {
            format(label,labelSize,"TARKENSO");
            format(accent,accentSize,"D9AA55");
        }
    }
    return 1;
}

stock TNotify_Semantic(severity, color[], size)
{
    switch(severity)
    {
        case TNOTIFY_SUCCESS: format(color,size,"B6E4B3");
        case TNOTIFY_WARNING: format(color,size,"E27E82");
        case TNOTIFY_HINT: format(color,size,"9FAAB8");
        default: format(color,size,"FFFFFF");
    }
    return 1;
}

// The first line always has a coloured [system] label. A message's actual
// content can include trusted inline colour tokens, but the base is white.
stock TNotify_Format(system,severity,const body[],line[],size)
{
    new label[32],accent[7],color[7];
    TNotify_Theme(system,label,sizeof(label),accent,sizeof(accent));
    TNotify_Semantic(severity,color,sizeof(color));
    format(line,size,"{%s}[%s] {%s}%s",accent,label,color,body);
    return 1;
}

stock TNotify_Send(playerid,system,severity,const body[])
{
    if(!IsPlayerConnected(playerid) || IsPlayerNPC(playerid)) return 0;
    new line[TNOTIFY_MAX_CHAT];
    TNotify_Format(system,severity,body,line,sizeof(line));
    return SendClientMessage(playerid,0xFFFFFFFF,line);
}

stock TNotify_All(system,severity,const body[])
{
    new line[TNOTIFY_MAX_CHAT];
    TNotify_Format(system,severity,body,line,sizeof(line));
    for(new playerid=0;playerid<MAX_PLAYERS;playerid++)
        if(IsPlayerConnected(playerid) && !IsPlayerNPC(playerid))
            SendClientMessage(playerid,0xFFFFFFFF,line);
    return 1;
}

// Continuation lines omit a repeated prefix; all such lines have a
// semantic base color. Formatting tokens in trusted content remain usable.
stock TNotify_Detail(playerid,severity,const body[])
{
    if(!IsPlayerConnected(playerid) || IsPlayerNPC(playerid)) return 0;
    new color[7],line[TNOTIFY_MAX_CHAT];
    TNotify_Semantic(severity,color,sizeof(color));
    format(line,sizeof(line),"{%s}%s",color,body);
    return SendClientMessage(playerid,0xFFFFFFFF,line);
}
