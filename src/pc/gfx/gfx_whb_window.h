#ifndef GFX_WIIU_H
#define GFX_WIIU_H

#ifdef TARGET_WII_U

#include "gfx_window_manager_api.h"

extern struct GfxWindowManagerAPI gfx_whb_window;
bool gfx_whb_window_is_running();

#endif
#endif
