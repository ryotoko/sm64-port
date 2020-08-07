#ifdef TARGET_WII_U

#include <stdint.h>
#include <stdbool.h>

#ifndef _LANGUAGE_C
#define _LANGUAGE_C
#endif
#include <PR/gbi.h>

#include <gx2/draw.h>
#include <gx2/registers.h>
#include <gx2/swap.h>
#include <whb/gfx.h>

#include "gfx_cc.h"
#include "gfx_rendering_api.h"

struct ShaderProgram {
    uint32_t shader_id;
    WHBGfxShaderGroup group;
    uint8_t num_inputs;
    bool used_textures[2];
    bool used_noise;
    uint32_t frame_count_offset;
    uint32_t window_height_offset;
    uint32_t samplers_location[2];
};

static struct ShaderProgram shader_program_pool[64];
static uint8_t shader_program_pool_size = 0;

static struct ShaderProgram *current_shader_program = NULL;

static GX2Sampler samplers[2];
static uint8_t current_sampler = 0;

static uint32_t frame_count = 0;
static uint32_t current_height = 0;

static BOOL current_depth_test = FALSE;
static BOOL current_depth_write = FALSE;
static GX2CompareFunction current_depth_compare = GX2_COMPARE_FUNC_LEQUAL;

static bool gfx_whb_z_is_from_0_to_1(void) {
    return false;
}

static void gfx_whb_unload_shader(struct ShaderProgram *old_prg) {
    current_shader_program = NULL;
}

static void gfx_whb_set_uniforms(struct ShaderProgram *prg) {
    if (prg != NULL && prg->used_noise) {
        GX2SetPixelUniformReg(prg->frame_count_offset, 4, &frame_count);
        GX2SetPixelUniformReg(prg->window_height_offset, 4, &current_height);
    }
}

static void gfx_whb_load_shader(struct ShaderProgram *new_prg) {
    if (new_prg != NULL) {
        GX2SetFetchShader(&new_prg->group.fetchShader);
        GX2SetVertexShader(new_prg->group.vertexShader);
        GX2SetPixelShader(new_prg->group.pixelShader);

        gfx_whb_set_uniforms(new_prg);

        current_shader_program = new_prg;
    }
}

static struct ShaderProgram *gfx_whb_create_and_load_new_shader(uint32_t shader_id) {
    // TODO: read GFD shader (gsh), set uniforms and create ShaderProgram instance
    return NULL;
}

static struct ShaderProgram *gfx_whb_lookup_shader(uint32_t shader_id) {
    /*
    for (size_t i = 0; i < shader_program_pool_size; i++) {
        if (shader_program_pool[i].shader_id == shader_id) {
            return &shader_program_pool[i];
        }
    }
    */
    return NULL;
}

static void gfx_whb_shader_get_info(struct ShaderProgram *prg, uint8_t *num_inputs, bool used_textures[2]) {
    *num_inputs = 0;//prg->num_inputs;
    used_textures[0] = false;//prg->used_textures[0];
    used_textures[1] = false;//prg->used_textures[1];
}

static uint32_t gfx_whb_new_texture(void) {
    return 0;
}

static void gfx_whb_select_texture(int tile, uint32_t texture_id) {
    if (current_shader_program != NULL) {
        current_sampler = tile;
        GX2SetPixelSampler(&samplers[current_sampler], current_shader_program->samplers_location[current_sampler]);
    }
}

static void gfx_whb_upload_texture(const uint8_t *rgba32_buf, int width, int height) {
    if (current_shader_program != NULL) {
        // TODO: Do GX2Texture magic here
        //GX2SetPixelTexture(tex, current_shader_program->samplers_location[current_sampler]);
    }
}

static GX2TexClampMode gfx_cm_to_gx2(uint32_t val) {
    if (val & G_TX_CLAMP) {
        return GX2_TEX_CLAMP_MODE_CLAMP;
    }
    return (val & G_TX_MIRROR) ? GX2_TEX_CLAMP_MODE_MIRROR : GX2_TEX_CLAMP_MODE_WRAP;
}

static void gfx_whb_set_sampler_parameters(int tile, bool linear_filter, uint32_t cms, uint32_t cmt) {
    if (current_shader_program != NULL) {
        current_sampler = tile;

        GX2Sampler *sampler = &samplers[current_sampler];
        GX2InitSampler(sampler, GX2_TEX_CLAMP_MODE_CLAMP, linear_filter ? GX2_TEX_XY_FILTER_MODE_LINEAR : GX2_TEX_XY_FILTER_MODE_POINT);
        GX2InitSamplerClamping(sampler, gfx_cm_to_gx2(cms), gfx_cm_to_gx2(cmt), GX2_TEX_CLAMP_MODE_WRAP);

        GX2SetPixelSampler(sampler, current_shader_program->samplers_location[current_sampler]);
    }
}

static void gfx_whb_set_depth_test(bool depth_test) {
    current_depth_test = depth_test;
    GX2SetDepthOnlyControl(current_depth_test, current_depth_write, current_depth_compare);
}

static void gfx_whb_set_depth_mask(bool z_upd) {
    current_depth_write = z_upd;
    GX2SetDepthOnlyControl(current_depth_test, current_depth_write, current_depth_compare);
}

static void gfx_whb_set_zmode_decal(bool zmode_decal) {
    if (zmode_decal) {
        GX2SetPolygonControl(GX2_FRONT_FACE_CCW, FALSE, TRUE, FALSE,
                             GX2_POLYGON_MODE_TRIANGLE, GX2_POLYGON_MODE_TRIANGLE,
                             TRUE, FALSE, FALSE);
        GX2SetPolygonOffset(-2.0f, 1.0f, 0.0f, 0.0f, 0.0f);
    } else {
        GX2SetPolygonControl(GX2_FRONT_FACE_CCW, FALSE, TRUE, FALSE,
                             GX2_POLYGON_MODE_TRIANGLE, GX2_POLYGON_MODE_TRIANGLE,
                             FALSE, FALSE, FALSE);
        GX2SetPolygonOffset( 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
    }
}

static void gfx_whb_set_viewport(int x, int y, int width, int height) {
    GX2SetViewport(x, y, width, height, 0.0f, 1.0f);
    current_height = height;
}

static void gfx_whb_set_scissor(int x, int y, int width, int height) {
    GX2SetScissor(x, y, width, height);
}

static void gfx_whb_set_use_alpha(bool use_alpha) {
    if (use_alpha) {
        GX2SetBlendControl(GX2_RENDER_TARGET_0,
                           GX2_BLEND_MODE_SRC_ALPHA, GX2_BLEND_MODE_INV_SRC_ALPHA,
                           GX2_BLEND_COMBINE_MODE_ADD, FALSE,
                           GX2_BLEND_MODE_ZERO, GX2_BLEND_MODE_ZERO,
                           GX2_BLEND_COMBINE_MODE_ADD);
        GX2SetColorControl(GX2_LOGIC_OP_COPY, 1, FALSE, TRUE);
    } else {
        GX2SetBlendControl(GX2_RENDER_TARGET_0,
                           GX2_BLEND_MODE_ONE, GX2_BLEND_MODE_ZERO,
                           GX2_BLEND_COMBINE_MODE_ADD, FALSE,
                           GX2_BLEND_MODE_ZERO, GX2_BLEND_MODE_ZERO,
                           GX2_BLEND_COMBINE_MODE_ADD);
        GX2SetColorControl(GX2_LOGIC_OP_COPY, 0, FALSE, TRUE);
    }
}

static void gfx_whb_draw_triangles(float buf_vbo[], size_t buf_vbo_len, size_t buf_vbo_num_tris) {
    if (current_shader_program != NULL) {
        // TODO: Set other vertex attributes and invalidate data
        //GX2SetAttribBuffer(0, sizeof(float) * buf_vbo_len, sizeof(float), buf_vbo);

        //GX2DrawEx(GX2_PRIMITIVE_MODE_TRIANGLES, 3 * buf_vbo_num_tris, 0, 1);
    }
}

static void gfx_whb_init(void) {
}

static void gfx_whb_on_resize(void) {
}

static void gfx_whb_start_frame(void) {
    frame_count++;

    WHBGfxBeginRenderTV();
    WHBGfxClearColor(1.0f, 0.0f, 0.0f, 1.0f);
}

static void gfx_whb_end_frame(void) {
    WHBGfxFinishRenderTV();
    GX2CopyColorBufferToScanBuffer(WHBGfxGetTVColourBuffer(), GX2_SCAN_TARGET_DRC);
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
