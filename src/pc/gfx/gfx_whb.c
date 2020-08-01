#ifdef TARGET_WII_U

#include <stdint.h>
#include <stdbool.h>

#ifndef _LANGUAGE_C
#define _LANGUAGE_C
#endif

#include <SDL2/SDL.h>

#include "gfx_rendering_api.h"

static uint32_t frame_count;
static uint32_t current_height;

static bool gfx_whb_z_is_from_0_to_1(void) {
    return false;
}

static void gfx_whb_unload_shader(struct ShaderProgram *old_prg) {
}

static void gfx_whb_load_shader(struct ShaderProgram *new_prg) {
}

static struct ShaderProgram *gfx_whb_create_and_load_new_shader(uint32_t shader_id) {
    return NULL;
}

static struct ShaderProgram *gfx_whb_lookup_shader(uint32_t shader_id) {
    return NULL;
}

static void gfx_whb_shader_get_info(struct ShaderProgram *prg, uint8_t *num_inputs, bool used_textures[2]) {
    *num_inputs = 0;
    used_textures[0] = false;
    used_textures[1] = false;
}

static uint32_t gfx_whb_new_texture(void) {
    return 0;
}

static void gfx_whb_select_texture(int tile, uint32_t texture_id) {
}

static void gfx_whb_upload_texture(const uint8_t *rgba32_buf, int width, int height) {
}

static void gfx_whb_set_sampler_parameters(int tile, bool linear_filter, uint32_t cms, uint32_t cmt) {
}

static void gfx_whb_set_depth_test(bool depth_test) {
}

static void gfx_whb_set_depth_mask(bool z_upd) {
}

static void gfx_whb_set_zmode_decal(bool zmode_decal) {
}

static void gfx_whb_set_viewport(int x, int y, int width, int height) {
    current_height = height;
}

static void gfx_whb_set_scissor(int x, int y, int width, int height) {
}

static void gfx_whb_set_use_alpha(bool use_alpha) {
}

static void gfx_whb_draw_triangles(float buf_vbo[], size_t buf_vbo_len, size_t buf_vbo_num_tris) {
}

static void gfx_whb_init(void) {
}

static void gfx_whb_on_resize(void) {
}

static void gfx_whb_start_frame(void) {
    frame_count++;
}

static void gfx_whb_end_frame(void) {
}

static void gfx_whb_finish_render(void) {
}

struct GfxRenderingAPI gfx_whb_api = {
    gfx_whb_z_is_from_0_to_1,
    gfx_whb_unload_shader,
    gfx_whb_load_shader,
    gfx_whb_create_and_load_new_shader,
    gfx_whb_lookup_shader,
    gfx_whb_shader_get_info,
    gfx_whb_new_texture,
    gfx_whb_select_texture,
    gfx_whb_upload_texture,
    gfx_whb_set_sampler_parameters,
    gfx_whb_set_depth_test,
    gfx_whb_set_depth_mask,
    gfx_whb_set_zmode_decal,
    gfx_whb_set_viewport,
    gfx_whb_set_scissor,
    gfx_whb_set_use_alpha,
    gfx_whb_draw_triangles,
    gfx_whb_init,
    gfx_whb_on_resize,
    gfx_whb_start_frame,
    gfx_whb_end_frame,
    gfx_whb_finish_render
};

#endif
