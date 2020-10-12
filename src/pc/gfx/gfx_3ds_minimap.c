#ifdef TARGET_N3DS

#include <stdlib.h>

#include "gfx_3ds_minimap.h"
#include "gfx_3ds_menu.h"
#include "gfx_3ds.h"

static C3D_Mtx modelView, projBottom;

static uint8_t *current_texture;
static size_t current_texture_size;

static uint32_t minimap_color;      // flood-fill background color
static C3D_Tex minimap_mario_tex;   // texture for mario location
static C3D_Tex minimap_arrow_tex;   // texture for heading arrow
static C3D_Tex minimap_tex;         // texture for map background

static float mario_x, mario_y, mario_direction = 0.0f;
// 320 - 240 = 80, so add 40 to shift map into middle of the screen
static float x_offset = 40.0f;
// FIXME: just chop bottom 16px of texture for now
static float y_offset = 0.0f; //-16.0f;

static uint32_t rgb_to_abgr(uint32_t rgb)
{
    // 0xRRGGBB to 0xFFBBGGRR
    return (0x0000ff & rgb >> 16)
        | (0x00ff00 & rgb)
        | (0xff0000 & rgb << 16)
        | 0xff000000;
}

static void minimap_load_new_minimap_texture()
{
    minimap_get_current_texture(&current_texture, &current_texture_size, &minimap_color);
    load_t3x_texture(&minimap_tex, NULL, current_texture, current_texture_size);
    C3D_TexSetFilter(&minimap_tex, GPU_LINEAR, GPU_NEAREST);
}

static void gfx_3ds_minimap_draw_background_color()
{
    Mtx_Identity(&modelView);
    Mtx_OrthoTilt(&projBottom, 0.0, 320.0, 0.0, 240.0, 0.0, 1.0, true);

    C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uLoc_modelView, &modelView);
    C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uLoc_projection, &projBottom);

    C3D_TexEnv* env = C3D_GetTexEnv(0);
    C3D_TexEnvInit(env);
    C3D_TexEnvColor(env, rgb_to_abgr(minimap_color));
    C3D_TexEnvSrc(env, C3D_Both, GPU_CONSTANT, 0, 0);
    C3D_TexEnvFunc(env, C3D_Both, GPU_REPLACE);

    gfx_3ds_immediate_render(vertex_list_color, 2);

}

static void gfx_3ds_minimap_draw_background()
{
    Mtx_Identity(&modelView);
    Mtx_Translate(&modelView, x_offset, y_offset, 0.0f, false);

    Mtx_OrthoTilt(&projBottom, 0.0, 320.0, 0.0, 240.0, 0.0, 1.0, true);

    C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uLoc_modelView, &modelView);
    C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uLoc_projection, &projBottom);

    C3D_TexBind(0, &minimap_tex);
    C3D_TexFlush(&minimap_tex);

    C3D_TexEnv* env = C3D_GetTexEnv(0);
    C3D_TexEnvInit(env);
    C3D_TexEnvColor(env, 0);
    C3D_TexEnvSrc(env, C3D_Both, GPU_TEXTURE0, 0, 0);
    C3D_TexEnvFunc(env, C3D_Both, GPU_REPLACE);

    gfx_3ds_immediate_render(vertex_list_background, 2);
}

static void gfx_3ds_minimap_draw_mario()
{
    Mtx_Identity(&modelView);
    // subtract y from 240 to flip y-axis
    Mtx_Translate(&modelView, mario_x + x_offset, 240.0f - mario_y + y_offset, 0.0f, false);

    Mtx_OrthoTilt(&projBottom, 0.0, 320.0, 0.0, 240.0, 0.0, 1.0, true);

    C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uLoc_modelView, &modelView);
    C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uLoc_projection, &projBottom);

    C3D_TexBind(0, &minimap_mario_tex);
    C3D_TexFlush(&minimap_mario_tex);

    C3D_TexEnv* env = C3D_GetTexEnv(0);
    C3D_TexEnvInit(env);
    C3D_TexEnvColor(env, 0);
    C3D_TexEnvFunc(env, C3D_Both, GPU_REPLACE);
    C3D_TexEnvSrc(env, C3D_Both, GPU_TEXTURE0, 0, 0);

    gfx_3ds_immediate_render(vertex_list_mario, 2);
}

static void gfx_3ds_minimap_draw_heading()
{
    float angle = C3D_Angle(mario_direction);

    Mtx_Identity(&modelView);
    Mtx_RotateZ(&modelView, angle, false);

    float arrow_x_offset = 16.0f * sin(angle);
    float arrow_y_offset = -16.0f * cos(angle);

    Mtx_Translate(&modelView, mario_x + x_offset + arrow_x_offset,
                  240.0f - mario_y + y_offset + arrow_y_offset, 0.0f, false);

    Mtx_OrthoTilt(&projBottom, 0.0, 320.0, 0.0, 240.0, 0.0, 1.0, true);

    C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uLoc_modelView, &modelView);
    C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uLoc_projection, &projBottom);

    C3D_TexBind(0, &minimap_arrow_tex);
    C3D_TexFlush(&minimap_arrow_tex);

    C3D_TexEnv* env = C3D_GetTexEnv(0);
    C3D_TexEnvInit(env);
    C3D_TexEnvColor(env, 0);
    C3D_TexEnvFunc(env, C3D_Both, GPU_REPLACE);
    C3D_TexEnvSrc(env, C3D_Both, GPU_TEXTURE0, 0, 0);

    gfx_3ds_immediate_render(vertex_list_arrow, 2);
}

void gfx_3ds_init_minimap()
{
    load_t3x_texture(&minimap_mario_tex, NULL, mario_t3x, mario_t3x_size);
    C3D_TexSetFilter(&minimap_mario_tex, GPU_LINEAR, GPU_NEAREST);

    load_t3x_texture(&minimap_arrow_tex, NULL, arrow_t3x, arrow_t3x_size);
    C3D_TexSetFilter(&minimap_arrow_tex, GPU_LINEAR, GPU_NEAREST);
}

bool show_minimap = false;

void gfx_3ds_draw_minimap()
{
    if(minimap_has_level_or_area_changed())
    {
        show_minimap = minimap_load_level_and_area();
        minimap_load_new_minimap_texture();
    }

    if (show_minimap && minimap_get_mario_position(&mario_x, &mario_y, &mario_direction))
    {
        gfx_3ds_minimap_draw_background_color();
        gfx_3ds_minimap_draw_background();
        gfx_3ds_minimap_draw_mario();
        gfx_3ds_minimap_draw_heading();
    }
}

#endif
