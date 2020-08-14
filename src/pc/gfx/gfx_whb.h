#ifndef GFX_WHB_H
#define GFX_WHB_H

#include "gfx_window_manager_api.h"
#include "gfx_rendering_api.h"

extern struct GfxWindowManagerAPI gfx_whb_window;
extern struct GfxRenderingAPI gfx_whb_api;
void whb_free_vbo(void);
void whb_free(void);

#endif
