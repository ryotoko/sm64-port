#ifdef TARGET_WII_U

#include <malloc.h>
#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>

#include <vector>

#ifndef _LANGUAGE_C
#define _LANGUAGE_C
#endif
#include <PR/gbi.h>

#include <gx2/draw.h>
#include <gx2/mem.h>
#include <gx2/registers.h>
#include <gx2/swap.h>

#include <whb/log.h>
#include <whb/gfx.h>

#include "gfx_cc.h"
#include "gfx_rendering_api.h"

struct ShaderProgram {
    uint32_t shader_id;
    WHBGfxShaderGroup group;
    uint8_t num_inputs;
    bool used_textures[2];
    uint8_t num_floats;
    bool used_noise;
    uint32_t frame_count_offset;
    uint32_t window_height_offset;
    uint32_t samplers_location[2];
};

typedef struct _Texture
{
     GX2Texture texture;
     GX2Sampler sampler;
     bool textureUploaded;
     bool samplerSet;
} Texture;

static struct ShaderProgram shader_program_pool[64];
static uint8_t shader_program_pool_size = 0;

static struct ShaderProgram *current_shader_program = NULL;

static std::vector<Texture> whb_textures;
static uint8_t current_tile = 0;
static uint32_t current_texture_ids[2];

extern const uint8_t shader_wiiu[];

static uint32_t frame_count = 0;
static uint32_t current_height = 0;

static BOOL current_depth_test = FALSE;
static BOOL current_depth_write = FALSE;
static GX2CompareFunction current_depth_compare = GX2_COMPARE_FUNC_LEQUAL;

GX2SamplerVar *GX2GetPixelSamplerVar(const GX2PixelShader *shader, const char *name)
{
    for (uint32_t i = 0; i < shader->samplerVarCount; i++)
    {
       if (strcmp(shader->samplerVars[i].name, name) == 0)
           return &(shader->samplerVars[i]);
    }

    return NULL;
}

s32 GX2GetPixelSamplerVarLocation(const GX2PixelShader *shader, const char *name)
{
    GX2SamplerVar *sampler = GX2GetPixelSamplerVar(shader, name);
    if (!sampler)
        return -1;

    return sampler->location;
}

s32 GX2GetVertexUniformVarOffset(const GX2VertexShader *shader, const char *name)
{
    GX2UniformVar *uniform = GX2GetVertexUniformVar(shader, name);
    if (!uniform)
        return -1;

    return uniform->offset;
}

s32 GX2GetPixelUniformVarOffset(const GX2PixelShader *shader, const char *name)
{
    GX2UniformVar *uniform = GX2GetPixelUniformVar(shader, name);
    if (!uniform)
        return -1;

    return uniform->offset;
}

static bool gfx_whb_z_is_from_0_to_1(void) {
    return false;
}

static void gfx_whb_unload_shader(struct ShaderProgram *old_prg) {
    if (current_shader_program == old_prg) {
        current_shader_program = NULL;
    } else {
        // ??????????
    }
}

static void gfx_whb_set_uniforms(struct ShaderProgram *prg) {
    if (prg->used_noise) {
        uint32_t frame_count_array[4] = { frame_count, 0, 0, 0 };
        uint32_t window_height_array[4] = { current_height, 0, 0, 0 };

        GX2SetPixelUniformReg(prg->frame_count_offset, 4, frame_count_array);
        GX2SetPixelUniformReg(prg->window_height_offset, 4, window_height_array);
    }
}

static void gfx_whb_load_shader(struct ShaderProgram *new_prg) {
    GX2SetFetchShader(&new_prg->group.fetchShader);
    GX2SetVertexShader(new_prg->group.vertexShader);
    GX2SetPixelShader(new_prg->group.pixelShader);

    gfx_whb_set_uniforms(new_prg);

    current_shader_program = new_prg;
}

static struct ShaderProgram *gfx_whb_create_and_load_new_shader(uint32_t shader_id) {
    struct CCFeatures cc_features;
    gfx_cc_get_features(shader_id, &cc_features);

    struct ShaderProgram *prg = &shader_program_pool[shader_program_pool_size++];

    if (!WHBGfxLoadGFDShaderGroup(&prg->group, 0, shader_wiiu)) {
error:
        WHBLogPrint("Shader create failed!");
        shader_program_pool_size--;
        return NULL;
    }

    WHBLogPrint("Loaded GFD.");

    uint32_t pos = 0;
    prg->num_floats = 0;

    if (!WHBGfxInitShaderAttribute(&prg->group, "aVtxPos", 0, pos, GX2_ATTRIB_FORMAT_FLOAT_32_32_32_32)) {
        goto error;
    }

    pos += 4 * sizeof(float);
    prg->num_floats += 4;

    if (cc_features.used_textures[0] || cc_features.used_textures[1]) {
        if (!WHBGfxInitShaderAttribute(&prg->group, "aTexCoord", 0, pos, GX2_ATTRIB_FORMAT_FLOAT_32_32)) {
            goto error;
        }

        pos += 2 * sizeof(float);
        prg->num_floats += 2;
    }

    if (cc_features.opt_fog) {
        if (!WHBGfxInitShaderAttribute(&prg->group, "aFog", 0, pos, GX2_ATTRIB_FORMAT_FLOAT_32_32_32_32)) {
            goto error;
        }

        pos += 4 * sizeof(float);
        prg->num_floats += 4;
    }

    for (int i = 0; i < cc_features.num_inputs; i++) {
        char name[16];
        sprintf(name, "aInput%d", i + 1);
        if (!WHBGfxInitShaderAttribute(&prg->group, name, 0, pos, GX2_ATTRIB_FORMAT_FLOAT_32_32_32_32)) {
            goto error;
        }

        pos += 4 * sizeof(float);
        prg->num_floats += 4;
    }

    if (!WHBGfxInitFetchShader(&prg->group)) {
        goto error;
    }

    WHBLogPrint("Initiated Fetch Shader.");

    prg->shader_id = shader_id;
    prg->num_inputs = cc_features.num_inputs;
    prg->used_textures[0] = cc_features.used_textures[0];
    prg->used_textures[1] = cc_features.used_textures[1];

    gfx_whb_load_shader(prg);

    prg->samplers_location[0] = GX2GetPixelSamplerVarLocation(prg->group.pixelShader, "uTex0");
    prg->samplers_location[1] = GX2GetPixelSamplerVarLocation(prg->group.pixelShader, "uTex1");

    prg->frame_count_offset = GX2GetPixelUniformVarOffset(prg->group.pixelShader, "frame_count");
    prg->window_height_offset = GX2GetPixelUniformVarOffset(prg->group.pixelShader, "window_height");

    if (cc_features.opt_alpha && cc_features.opt_noise) {
        prg->used_noise = true;
    } else {
        prg->used_noise = false;
    }

    WHBLogPrint("Initiated Tex/Frame/Height uniforms.");

    int32_t  c_array[2][4] = { { cc_features.c[0][0], cc_features.c[0][1], cc_features.c[0][2], cc_features.c[0][3] }, { cc_features.c[1][0], cc_features.c[1][1], cc_features.c[1][2], cc_features.c[1][3] } };
    uint32_t alpha_used_array[4] = { cc_features.opt_alpha, 0, 0, 0 };
    uint32_t fog_used_array[4] = { cc_features.opt_fog, 0, 0, 0 };
    uint32_t texture_edge_array[4] = { cc_features.opt_texture_edge, 0, 0, 0 };
    uint32_t noise_used_array[4] = { cc_features.opt_noise, 0, 0, 0 };
    int32_t  tex_flags_array[4] = { cc_features.used_textures[0] | (cc_features.used_textures[1] << 1), 0, 0, 0 };
    int32_t  num_inputs_array[4] = { cc_features.num_inputs, 0, 0, 0 };
    uint32_t do_single_array[4] = { cc_features.do_single[0], cc_features.do_single[1], 0, 0 };
    uint32_t do_multiply_array[4] = { cc_features.do_multiply[0], cc_features.do_multiply[1], 0, 0 };
    uint32_t do_mix_array[4] = { cc_features.do_mix[0], cc_features.do_mix[1], 0, 0 };
    uint32_t color_alpha_same_array[4] = { cc_features.color_alpha_same, 0, 0, 0 };

    GX2SetVertexUniformReg(GX2GetVertexUniformVarOffset(prg->group.vertexShader, "fog_used"), 4, fog_used_array);
    GX2SetVertexUniformReg(GX2GetVertexUniformVarOffset(prg->group.vertexShader, "tex_flags"), 4, tex_flags_array);
    GX2SetVertexUniformReg(GX2GetVertexUniformVarOffset(prg->group.vertexShader, "num_inputs"), 4, num_inputs_array);

    GX2SetPixelUniformReg(GX2GetPixelUniformVarOffset(prg->group.pixelShader, "c"), 8, c_array);
    GX2SetPixelUniformReg(GX2GetPixelUniformVarOffset(prg->group.pixelShader, "alpha_used"), 4, alpha_used_array);
    GX2SetPixelUniformReg(GX2GetPixelUniformVarOffset(prg->group.pixelShader, "fog_used"), 4, fog_used_array);
    GX2SetPixelUniformReg(GX2GetPixelUniformVarOffset(prg->group.pixelShader, "texture_edge"), 4, texture_edge_array);
    GX2SetPixelUniformReg(GX2GetPixelUniformVarOffset(prg->group.pixelShader, "noise_used"), 4, noise_used_array);
    GX2SetPixelUniformReg(GX2GetPixelUniformVarOffset(prg->group.pixelShader, "tex_flags"), 4, tex_flags_array);
    GX2SetPixelUniformReg(GX2GetPixelUniformVarOffset(prg->group.pixelShader, "do_single"), 4, do_single_array);
    GX2SetPixelUniformReg(GX2GetPixelUniformVarOffset(prg->group.pixelShader, "do_multiply"), 4, do_multiply_array);
    GX2SetPixelUniformReg(GX2GetPixelUniformVarOffset(prg->group.pixelShader, "do_mix"), 4, do_mix_array);
    GX2SetPixelUniformReg(GX2GetPixelUniformVarOffset(prg->group.pixelShader, "color_alpha_same"), 4, color_alpha_same_array);

    WHBLogPrint("Initiated Shader.");

    return prg;
}

static struct ShaderProgram *gfx_whb_lookup_shader(uint32_t shader_id) {
    for (size_t i = 0; i < shader_program_pool_size; i++) {
        if (shader_program_pool[i].shader_id == shader_id) {
            return &shader_program_pool[i];
        }
    }
    return NULL;
}

static void gfx_whb_shader_get_info(struct ShaderProgram *prg, uint8_t *num_inputs, bool used_textures[2]) {
    *num_inputs = prg->num_inputs;
    used_textures[0] = prg->used_textures[0];
    used_textures[1] = prg->used_textures[1];
}

static uint32_t gfx_whb_new_texture(void) {
    whb_textures.resize(whb_textures.size() + 1);
    uint32_t texture_id = (uint32_t)(whb_textures.size() - 1);

    Texture& texture = whb_textures[texture_id];
    texture.textureUploaded = false;
    texture.samplerSet = false;

    return texture_id;
}

static void gfx_whb_select_texture(int tile, uint32_t texture_id) {
    current_tile = tile;
    current_texture_ids[tile] = texture_id;

    Texture& texture = whb_textures[texture_id];
    if (texture.textureUploaded)
        GX2SetPixelTexture(&texture.texture, current_shader_program->samplers_location[tile]);
    if (texture.samplerSet)
        GX2SetPixelSampler(&texture.sampler, current_shader_program->samplers_location[tile]);
}

static void gfx_whb_upload_texture(const uint8_t *rgba32_buf, int width, int height) {
    int tile = current_tile;
    GX2Texture& texture = whb_textures[current_texture_ids[tile]].texture;

    texture.surface.use =         GX2_SURFACE_USE_TEXTURE;
    texture.surface.dim =         GX2_SURFACE_DIM_TEXTURE_2D;
    texture.surface.width =       width;
    texture.surface.height =      height;
    texture.surface.depth =       1;
    texture.surface.mipLevels =   1;
    texture.surface.format =      GX2_SURFACE_FORMAT_UNORM_R8_G8_B8_A8;
    texture.surface.aa =          GX2_AA_MODE1X;
    texture.surface.tileMode =    GX2_TILE_MODE_LINEAR_ALIGNED;
    texture.viewFirstMip =        0;
    texture.viewNumMips =         1;
    texture.viewFirstSlice =      0;
    texture.viewNumSlices =       1;
    texture.surface.swizzle =     0;
    texture.surface.alignment =   0;
    texture.surface.pitch =       0;

    uint32_t i;
    for(i = 0; i < 13; i++) {
        texture.surface.mipLevelOffset[i] = 0;
    }
    texture.viewFirstMip = 0;
    texture.viewNumMips = 1;
    texture.viewFirstSlice = 0;
    texture.viewNumSlices = 1;
    texture.compMap = 0x00010203;
    for(i = 0; i < 5; i++){
        texture.regs[i] = 0;
    }

    GX2CalcSurfaceSizeAndAlignment(&texture.surface);
    GX2InitTextureRegs(&texture);

    texture.surface.image = memalign(texture.surface.alignment, texture.surface.imageSize);
    GX2Invalidate(GX2_INVALIDATE_MODE_CPU_TEXTURE, texture.surface.image, texture.surface.imageSize);

    GX2Surface surf;
    surf = texture.surface;
    surf.tileMode = GX2_TILE_MODE_LINEAR_SPECIAL;
    surf.image = (void *)rgba32_buf;
    GX2CalcSurfaceSizeAndAlignment(&surf);
    GX2Invalidate(GX2_INVALIDATE_MODE_CPU_TEXTURE, (void *)rgba32_buf, surf.imageSize);
    GX2CopySurface(&surf, 0, 0, &texture.surface, 0, 0);

    GX2SetPixelTexture(&texture, current_shader_program->samplers_location[tile]);
    whb_textures[current_texture_ids[tile]].textureUploaded = true;

    //WHBLogPrint("Texture set.");
}

static GX2TexClampMode gfx_cm_to_gx2(uint32_t val) {
    if (val & G_TX_CLAMP) {
        return GX2_TEX_CLAMP_MODE_CLAMP;
    }
    return (val & G_TX_MIRROR) ? GX2_TEX_CLAMP_MODE_MIRROR : GX2_TEX_CLAMP_MODE_WRAP;
}

static void gfx_whb_set_sampler_parameters(int tile, bool linear_filter, uint32_t cms, uint32_t cmt) {
    current_tile = tile;

    GX2Sampler *sampler = &whb_textures[current_texture_ids[tile]].sampler;
    GX2InitSampler(sampler, GX2_TEX_CLAMP_MODE_CLAMP, linear_filter ? GX2_TEX_XY_FILTER_MODE_LINEAR : GX2_TEX_XY_FILTER_MODE_POINT);
    GX2InitSamplerClamping(sampler, gfx_cm_to_gx2(cms), gfx_cm_to_gx2(cmt), GX2_TEX_CLAMP_MODE_WRAP);

    GX2SetPixelSampler(sampler, current_shader_program->samplers_location[tile]);
    whb_textures[current_texture_ids[tile]].samplerSet = true;
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
        GX2SetPolygonOffset(-2.0f, 1.0f, 0.0f, 0.0f, 0.0f );
    } else {
        GX2SetPolygonControl(GX2_FRONT_FACE_CCW, FALSE, TRUE, FALSE,
                             GX2_POLYGON_MODE_TRIANGLE, GX2_POLYGON_MODE_TRIANGLE,
                             FALSE, FALSE, FALSE);
        GX2SetPolygonOffset( 0.0f, 0.0f, 0.0f, 0.0f, 0.0f );
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
    GX2Invalidate(GX2_INVALIDATE_MODE_CPU_ATTRIBUTE_BUFFER, buf_vbo, sizeof(float) * buf_vbo_len);

    GX2SetAttribBuffer(0, sizeof(float) * buf_vbo_len, sizeof(float) * current_shader_program->num_floats, buf_vbo);
    GX2DrawEx(GX2_PRIMITIVE_MODE_TRIANGLES, 3 * buf_vbo_num_tris, 0, 1);
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

extern "C" void whb_free(void) {
    // Free our textures
    // TODO: free shaders
    for (uint32_t i = 0; i < whb_textures.size(); i++) {
        Texture& texture = whb_textures[i];
        if (texture.textureUploaded) {
            free(texture.texture.surface.image);
        }
    }

    whb_textures.clear();
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
