#if defined _tarkenso_tds
    #endinput
#endif
#define _tarkenso_tds

// Host adapters: authenticated account, explicit grants, sorting UI, dialogs.
// No callbacks are hooked: the host routes events explicitly in signal0317.pwn.
#include "tdstudio_model.inc"
#include "tdstudio_host.inc"
#include "tdstudio_codec.inc"
#include "tdstudio_render.inc"
#include "tdstudio_core.inc"
#include "tdstudio_layout.inc"
#include "tdstudio_geometry.inc"
#include "tdstudio_grid.inc"
#include "tdstudio_input.inc"
#include "tdstudio_actions.inc"
#include "tdstudio_db.inc"
#include "tdstudio_export.inc"
#include "tdstudio_layout_ui.inc"
#include "tdstudio_geometry_ui.inc"
#include "tdstudio_ui.inc"
