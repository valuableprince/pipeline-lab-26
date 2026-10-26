#if defined _tarkenso_tds
    #endinput
#endif
#define _tarkenso_tds

// Host adapters: authenticated account, explicit grants, sorting UI, dialogs.
// No callbacks are hooked: the host routes events explicitly in signal0317.pwn.
#include "tdstudio_model.inc"
#include "tdstudio_host.inc"
#include "tdstudio_render.inc"
#include "tdstudio_core.inc"
#include "tdstudio_actions.inc"
#include "tdstudio_ui.inc"
