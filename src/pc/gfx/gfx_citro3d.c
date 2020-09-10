#ifdef TARGET_N3DS

#include "macros.h"

#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>

#include "gfx_3ds.h"
#include "gfx_3ds_menu.h"

#include "gfx_cc.h"
#include "gfx_rendering_api.h"

#define TEXTURE_POOL_SIZE 4096

static Gfx3DSMode sCurrentGfx3DSMode = GFX_3DS_MODE_NORMAL;

#include "gfx_3ds_shaders.h"

struct ShaderProgram {
    uint32_t shader_id;
    uint8_t program_id;
    uint8_t buffer_id;
    struct CCFeatures cc_features;
    bool swap_input;
    C3D_TexEnv texenv0;
    C3D_TexEnv texenv1;
    C3D_TexEnv fog_texenv0;
};

struct video_buffer {
    uint8_t id;
    float *ptr;
    uint8_t stride;
    uint32_t offset;
    shaderProgram_s shader_program; // pica vertex shader
    C3D_AttrInfo attr_info;
    C3D_BufInfo buf_info;
};

static int uLoc_draw_fog;
static int uLoc_tex_scale;

static struct video_buffer *current_buffer;
static struct video_buffer video_buffers[16];
static uint8_t video_buffers_size;

static struct ShaderProgram sShaderProgramPool[32];
static uint8_t sShaderProgramPoolSize;

static u32 sTexBuf[16 * 1024] __attribute__((aligned(32)));
static C3D_Tex sTexturePool[TEXTURE_POOL_SIZE];
static float sTexturePoolScaleS[TEXTURE_POOL_SIZE];
static float sTexturePoolScaleT[TEXTURE_POOL_SIZE];
static u32 sTextureIndex;
static int sTexUnits[2];

static int sCurTex = 0;
static int sCurShader = 0;

static bool sDepthTestOn = false;
static bool sDepthUpdateOn = false;
static bool sDepthDecal = false;
static bool sUseBlend = false;

// calling FrameDrawOn resets viewport
static int viewport_x, viewport_y;
static int viewport_width, viewport_height;

static C3D_Mtx modelView, projection;

#ifdef ENABLE_N3DS_3D_MODE
static int original_offset;
static float iod; // determined by 3D slider position
static const float focalLen = 0.75f;
static const float fov = 54.0f*M_TAU/360.0f;
#endif

static bool gfx_citro3d_z_is_from_0_to_1(void)
{
    return true;
}

static void gfx_citro3d_vertex_array_set_attribs(UNUSED struct ShaderProgram *prg)
{
}

static void gfx_citro3d_unload_shader(UNUSED struct ShaderProgram *old_prg)
{
}

static GPU_TEVSRC getTevSrc(int input, bool swap_input)
{
    switch (input)
    {
        case SHADER_0:
            return GPU_CONSTANT;
        case SHADER_INPUT_1:
            return swap_input ? GPU_PREVIOUS : GPU_PRIMARY_COLOR;
        case SHADER_INPUT_2:
            return swap_input ? GPU_PRIMARY_COLOR : GPU_PREVIOUS;
        case SHADER_INPUT_3:
            return GPU_CONSTANT;
        case SHADER_INPUT_4:
            return GPU_CONSTANT;
        case SHADER_TEXEL0:
        case SHADER_TEXEL0A:
            return GPU_TEXTURE0;
        case SHADER_TEXEL1:
            return GPU_TEXTURE1;
    }
    return GPU_CONSTANT;
}

static void update_tex_env(struct ShaderProgram *prg, bool swap_input)
{
    if (prg->cc_features.num_inputs == 2)
    {
        C3D_TexEnvInit(&prg->texenv1);
        C3D_TexEnvColor(&prg->texenv1, 0);
        C3D_TexEnvFunc(&prg->texenv1, C3D_Both, GPU_REPLACE);
        C3D_TexEnvSrc(&prg->texenv1, C3D_Both, GPU_CONSTANT, 0, 0);
        C3D_TexEnvOpRgb(&prg->texenv1, GPU_TEVOP_RGB_SRC_COLOR, GPU_TEVOP_RGB_SRC_COLOR, GPU_TEVOP_RGB_SRC_COLOR);
        C3D_TexEnvOpAlpha(&prg->texenv1, GPU_TEVOP_A_SRC_ALPHA, GPU_TEVOP_A_SRC_ALPHA, GPU_TEVOP_A_SRC_ALPHA);
    }

    C3D_TexEnvInit(&prg->texenv0);
    C3D_TexEnvColor(&prg->texenv0, 0);
    if (prg->cc_features.opt_alpha && !prg->cc_features.color_alpha_same)
    {
        // RGB first
        if (prg->cc_features.do_single[0])
        {
            C3D_TexEnvFunc(&prg->texenv0, C3D_RGB, GPU_REPLACE);
            C3D_TexEnvSrc(&prg->texenv0, C3D_RGB, getTevSrc(prg->cc_features.c[0][3], swap_input), 0, 0);
            if (prg->cc_features.c[0][3] == SHADER_TEXEL0A)
                C3D_TexEnvOpRgb(&prg->texenv0, GPU_TEVOP_RGB_SRC_ALPHA, GPU_TEVOP_RGB_SRC_COLOR, GPU_TEVOP_RGB_SRC_COLOR);
            else
                C3D_TexEnvOpRgb(&prg->texenv0, GPU_TEVOP_RGB_SRC_COLOR, GPU_TEVOP_RGB_SRC_COLOR, GPU_TEVOP_RGB_SRC_COLOR);
        }
        else if (prg->cc_features.do_multiply[0])
        {
            C3D_TexEnvFunc(&prg->texenv0, C3D_RGB, GPU_MODULATE);
            C3D_TexEnvSrc(&prg->texenv0, C3D_RGB, getTevSrc(prg->cc_features.c[0][0], swap_input),
                                        getTevSrc(prg->cc_features.c[0][2], swap_input), 0);
            C3D_TexEnvOpRgb(&prg->texenv0,
                prg->cc_features.c[0][0] == SHADER_TEXEL0A ? GPU_TEVOP_RGB_SRC_ALPHA : GPU_TEVOP_RGB_SRC_COLOR,
                prg->cc_features.c[0][2] == SHADER_TEXEL0A ? GPU_TEVOP_RGB_SRC_ALPHA : GPU_TEVOP_RGB_SRC_COLOR,
                GPU_TEVOP_RGB_SRC_COLOR);
        }
        else if (prg->cc_features.do_mix[0])
        {
            C3D_TexEnvFunc(&prg->texenv0, C3D_RGB, GPU_INTERPOLATE);
            C3D_TexEnvSrc(&prg->texenv0, C3D_RGB, getTevSrc(prg->cc_features.c[0][0], swap_input),
                                        getTevSrc(prg->cc_features.c[0][1], swap_input),
                                        getTevSrc(prg->cc_features.c[0][2], swap_input));
            C3D_TexEnvOpRgb(&prg->texenv0,
                prg->cc_features.c[0][0] == SHADER_TEXEL0A ? GPU_TEVOP_RGB_SRC_ALPHA : GPU_TEVOP_RGB_SRC_COLOR,
                prg->cc_features.c[0][1] == SHADER_TEXEL0A ? GPU_TEVOP_RGB_SRC_ALPHA : GPU_TEVOP_RGB_SRC_COLOR,
                prg->cc_features.c[0][2] == SHADER_TEXEL0A ? GPU_TEVOP_RGB_SRC_ALPHA : GPU_TEVOP_RGB_SRC_COLOR);
        }
        // now Alpha
        C3D_TexEnvOpAlpha(&prg->texenv0,  GPU_TEVOP_A_SRC_ALPHA, GPU_TEVOP_A_SRC_ALPHA, GPU_TEVOP_A_SRC_ALPHA);
        if (prg->cc_features.do_single[1])
        {
            C3D_TexEnvFunc(&prg->texenv0, C3D_Alpha, GPU_REPLACE);
            C3D_TexEnvSrc(&prg->texenv0, C3D_Alpha, getTevSrc(prg->cc_features.c[1][3], swap_input), 0, 0);
        }
        else if (prg->cc_features.do_multiply[1])
        {
            C3D_TexEnvFunc(&prg->texenv0, C3D_Alpha, GPU_MODULATE);
            C3D_TexEnvSrc(&prg->texenv0, C3D_Alpha, getTevSrc(prg->cc_features.c[1][0], swap_input),
                                          getTevSrc(prg->cc_features.c[1][2], swap_input), 0);
        }
        else if (prg->cc_features.do_mix[1])
        {
            C3D_TexEnvFunc(&prg->texenv0, C3D_Alpha, GPU_INTERPOLATE);
            C3D_TexEnvSrc(&prg->texenv0, C3D_Alpha, getTevSrc(prg->cc_features.c[1][0], swap_input),
                                          getTevSrc(prg->cc_features.c[1][1], swap_input),
                                          getTevSrc(prg->cc_features.c[1][2], swap_input));
        }
    }
    else
    {
        // RBGA
        C3D_TexEnvOpAlpha(&prg->texenv0, GPU_TEVOP_A_SRC_ALPHA, GPU_TEVOP_A_SRC_ALPHA, GPU_TEVOP_A_SRC_ALPHA);
        if (prg->cc_features.do_single[0])
        {
            C3D_TexEnvFunc(&prg->texenv0, C3D_Both, GPU_REPLACE);
            C3D_TexEnvSrc(&prg->texenv0, C3D_Both, getTevSrc(prg->cc_features.c[0][3], swap_input), 0, 0);
            if (prg->cc_features.c[0][3] == SHADER_TEXEL0A)
                C3D_TexEnvOpRgb(&prg->texenv0, GPU_TEVOP_RGB_SRC_ALPHA, GPU_TEVOP_RGB_SRC_COLOR, GPU_TEVOP_RGB_SRC_COLOR);
            else
                C3D_TexEnvOpRgb(&prg->texenv0, GPU_TEVOP_RGB_SRC_COLOR, GPU_TEVOP_RGB_SRC_COLOR, GPU_TEVOP_RGB_SRC_COLOR);
        }
        else if (prg->cc_features.do_multiply[0])
        {
            C3D_TexEnvFunc(&prg->texenv0, C3D_Both, GPU_MODULATE);
            C3D_TexEnvSrc(&prg->texenv0, C3D_Both, getTevSrc(prg->cc_features.c[0][0], swap_input),
                                         getTevSrc(prg->cc_features.c[0][2], swap_input), 0);
            C3D_TexEnvOpRgb(&prg->texenv0,
                prg->cc_features.c[0][0] == SHADER_TEXEL0A ? GPU_TEVOP_RGB_SRC_ALPHA : GPU_TEVOP_RGB_SRC_COLOR,
                prg->cc_features.c[0][2] == SHADER_TEXEL0A ? GPU_TEVOP_RGB_SRC_ALPHA : GPU_TEVOP_RGB_SRC_COLOR,
                GPU_TEVOP_RGB_SRC_COLOR);
        }
        else if (prg->cc_features.do_mix[0])
        {
            C3D_TexEnvFunc(&prg->texenv0, C3D_Both, GPU_INTERPOLATE);
            C3D_TexEnvSrc(&prg->texenv0, C3D_Both, getTevSrc(prg->cc_features.c[0][0], swap_input),
                                         getTevSrc(prg->cc_features.c[0][1], swap_input),
                                         getTevSrc(prg->cc_features.c[0][2], swap_input));
            C3D_TexEnvOpRgb(&prg->texenv0,
                prg->cc_features.c[0][0] == SHADER_TEXEL0A ? GPU_TEVOP_RGB_SRC_ALPHA : GPU_TEVOP_RGB_SRC_COLOR,
                prg->cc_features.c[0][1] == SHADER_TEXEL0A ? GPU_TEVOP_RGB_SRC_ALPHA : GPU_TEVOP_RGB_SRC_COLOR,
                prg->cc_features.c[0][2] == SHADER_TEXEL0A ? GPU_TEVOP_RGB_SRC_ALPHA : GPU_TEVOP_RGB_SRC_COLOR);
        }
    }
    if (!prg->cc_features.opt_alpha)
    {
        C3D_TexEnvColor(&prg->texenv0, 0xFF000000);
        C3D_TexEnvFunc(&prg->texenv0, C3D_Alpha, GPU_REPLACE);
        C3D_TexEnvSrc(&prg->texenv0, C3D_Alpha, GPU_CONSTANT, 0, 0);
    }

    prg->swap_input = swap_input;

    if (prg->cc_features.opt_fog)
    {
        C3D_TexEnvInit(&prg->fog_texenv0);
        C3D_TexEnvColor(&prg->fog_texenv0, 0);
        C3D_TexEnvFunc(&prg->fog_texenv0, C3D_Both, GPU_REPLACE);
        C3D_TexEnvSrc(&prg->fog_texenv0, C3D_Both, GPU_PRIMARY_COLOR, 0, 0);
        C3D_TexEnvOpRgb(&prg->fog_texenv0, GPU_TEVOP_RGB_SRC_COLOR, GPU_TEVOP_RGB_SRC_COLOR, GPU_TEVOP_RGB_SRC_COLOR);
        C3D_TexEnvOpAlpha(&prg->fog_texenv0, GPU_TEVOP_A_SRC_ALPHA, GPU_TEVOP_A_SRC_ALPHA, GPU_TEVOP_A_SRC_ALPHA);
    }
}

static void update_shader(bool swap_input)
{
    struct ShaderProgram *prg = &sShaderProgramPool[sCurShader];

    // only Goddard
    if (prg->swap_input != swap_input)
    {
        update_tex_env(prg, swap_input);
    }

    if (prg->cc_features.num_inputs == 2)
    {
        C3D_SetTexEnv(0, &prg->texenv1);
        C3D_SetTexEnv(1, &prg->texenv0);
    } else {
        C3D_SetTexEnv(0, &prg->texenv0);
        C3D_TexEnvInit(C3D_GetTexEnv(1));
    }

    if (prg->cc_features.opt_texture_edge && prg->cc_features.opt_alpha)
        C3D_AlphaTest(true, GPU_GREATER, 77);
    else
        C3D_AlphaTest(true, GPU_GREATER, 0);
}

static void gfx_citro3d_load_shader(struct ShaderProgram *new_prg)
{
    sCurShader = new_prg->program_id;
    current_buffer = &video_buffers[new_prg->buffer_id];

    C3D_BindProgram(&current_buffer->shader_program);
    uLoc_projection = shaderInstanceGetUniformLocation((&current_buffer->shader_program)->vertexShader, "projection");
    uLoc_modelView = shaderInstanceGetUniformLocation((&current_buffer->shader_program)->vertexShader, "modelView");

    if (new_prg->cc_features.opt_fog)
        uLoc_draw_fog = shaderInstanceGetUniformLocation((&current_buffer->shader_program)->vertexShader, "draw_fog");
    if (new_prg->cc_features.used_textures[0] || new_prg->cc_features.used_textures[1])
        uLoc_tex_scale = shaderInstanceGetUniformLocation((&current_buffer->shader_program)->vertexShader, "tex_scale");

    // update buffer info
    C3D_SetBufInfo(&current_buffer->buf_info);
    C3D_SetAttrInfo(&current_buffer->attr_info);

    gfx_citro3d_vertex_array_set_attribs(new_prg);

    update_shader(false);
}

static uint8_t setup_new_buffer_etc(bool has_texture, bool has_fog, bool has_alpha,
                                    bool has_color, bool has_color2)
{
    // 1 => texture
    // 2 => fog
    // 4 => 1 color RGBA
    // 8 => 1 color RGB
    // 16 => 2 colors RGBA
    // 32 => 2 colors RGB

    u8 shader_code = 0;

    if (has_texture)
        shader_code += 1;
    if (has_fog)
        shader_code += 2;
    if (has_color)
        shader_code += has_alpha ? 4 : 8;
    if (has_color2)
        shader_code += has_alpha ? 16 : 32;

    for (int i = 0; i < video_buffers_size; i++)
    {
        if (shader_code == video_buffers[i].id)
            return i;
    }

    // not found, create new
    int id = video_buffers_size;
    struct video_buffer *cb = &video_buffers[video_buffers_size++];

    cb->id = shader_code;

    u8 *current_shader_shbin = 0;
    u32 current_shader_shbin_size = 0;

    switch(shader_code)
    {
        case 1:
            current_shader_shbin = shader_1_shbin;
            current_shader_shbin_size = shader_1_shbin_size;
            break;
        case 3:
            current_shader_shbin = shader_3_shbin;
            current_shader_shbin_size = shader_3_shbin_size;
            break;
        case 4:
            current_shader_shbin = shader_4_shbin;
            current_shader_shbin_size = shader_4_shbin_size;
            break;
        case 5:
            current_shader_shbin = shader_5_shbin;
            current_shader_shbin_size = shader_5_shbin_size;
            break;
        case 6:
            current_shader_shbin = shader_6_shbin;
            current_shader_shbin_size = shader_6_shbin_size;
            break;
        case 7:
            current_shader_shbin = shader_7_shbin;
            current_shader_shbin_size = shader_7_shbin_size;
            break;
        case 8:
            current_shader_shbin = shader_8_shbin;
            current_shader_shbin_size = shader_8_shbin_size;
            break;
        case 9:
            current_shader_shbin = shader_9_shbin;
            current_shader_shbin_size = shader_9_shbin_size;
            break;
        case 20:
            current_shader_shbin = shader_20_shbin;
            current_shader_shbin_size = shader_20_shbin_size;
            break;
        case 41:
            current_shader_shbin = shader_41_shbin;
            current_shader_shbin_size = shader_41_shbin_size;
            break;
        default:
            current_shader_shbin = shader_shbin;
            current_shader_shbin_size = shader_shbin_size;
            printf("Warning! Using default for %u\n", shader_code);
    }

    DVLB_s* sVShaderDvlb = DVLB_ParseFile((u32*)current_shader_shbin, current_shader_shbin_size);

    shaderProgramInit(&cb->shader_program);
    shaderProgramSetVsh(&cb->shader_program, &sVShaderDvlb->DVLE[0]);

    // Configure attributes for use with the vertex shader
    int attr = 0;
    uint32_t attr_mask = 0;
    cb->stride = 4;

    AttrInfo_Init(&cb->attr_info);
    AttrInfo_AddLoader(&cb->attr_info, attr++, GPU_FLOAT, 4);
    if (has_texture)
    {
        attr_mask += attr * (1 << 4 * attr);
        AttrInfo_AddLoader(&cb->attr_info, attr++, GPU_FLOAT, 2);
        cb->stride += 2;
    }
    if (has_fog)
    {
        attr_mask += attr * (1 << 4 * attr);
        AttrInfo_AddLoader(&cb->attr_info, attr++, GPU_FLOAT, 4);
        cb->stride += 4;
    }
    if (has_color)
    {
        attr_mask += attr * (1 << 4 * attr);
        AttrInfo_AddLoader(&cb->attr_info, attr++, GPU_FLOAT, has_alpha ? 4 : 3);
        cb->stride += has_alpha ? 4 : 3;
    }
    if (has_color2)
    {
        attr_mask += attr * (1 << 4 * attr);
        AttrInfo_AddLoader(&cb->attr_info, attr++, GPU_FLOAT, has_alpha ? 4 : 3);
        cb->stride += has_alpha ? 4 : 3;
    }

    // Create the VBO (vertex buffer object)
    cb->ptr = linearAlloc(256 * 1024); // sizeof(float) * 10000 vertexes * 10 floats per vertex?
    // Configure buffers
    BufInfo_Init(&cb->buf_info);
    BufInfo_Add(&cb->buf_info, cb->ptr, cb->stride * sizeof(float), attr, attr_mask);

    return id;
}

static struct ShaderProgram *gfx_citro3d_create_and_load_new_shader(uint32_t shader_id)
{
    int id = sShaderProgramPoolSize;
    struct ShaderProgram *prg = &sShaderProgramPool[sShaderProgramPoolSize++];

    prg->program_id = id;

    prg->shader_id = shader_id;
    gfx_cc_get_features(shader_id, &prg->cc_features);

    prg->buffer_id = setup_new_buffer_etc(prg->cc_features.used_textures[0] || prg->cc_features.used_textures[1],
                                          prg->cc_features.opt_fog,
                                          prg->cc_features.opt_alpha,
                                          prg->cc_features.num_inputs > 0,
                                          prg->cc_features.num_inputs > 1);

    update_tex_env(prg, false);

    gfx_citro3d_load_shader(prg);

    return prg;
}

static struct ShaderProgram *gfx_citro3d_lookup_shader(uint32_t shader_id)
{
    for (size_t i = 0; i < sShaderProgramPoolSize; i++)
    {
        if (sShaderProgramPool[i].shader_id == shader_id)
        {
            return &sShaderProgramPool[i];
        }
    }
    return NULL;
}

static void gfx_citro3d_shader_get_info(struct ShaderProgram *prg, uint8_t *num_inputs, bool used_textures[2])
{
    *num_inputs = prg->cc_features.num_inputs;
    used_textures[0] = prg->cc_features.used_textures[0];
    used_textures[1] = prg->cc_features.used_textures[1];
}

static uint32_t gfx_citro3d_new_texture(void)
{
    if (sTextureIndex == TEXTURE_POOL_SIZE)
    {
        printf("Out of textures!\n");
        return 0;
    }
    return sTextureIndex++;
}

static void gfx_citro3d_select_texture(int tile, uint32_t texture_id)
{
    if (sCurTex != texture_id)
    {
        C3D_TexBind(tile, &sTexturePool[texture_id]);
        sCurTex = texture_id;
        sTexUnits[tile] = texture_id;
    }
}

static int sTileOrder[] =
{
    0,  1,   4,  5,
    2,  3,   6,  7,

    8,  9,  12, 13,
    10, 11, 14, 15
};

static void performTexSwizzle(const u8* src, u32* dst, u32 w, u32 h)
{
    int offs = 0;
    for (u32 y = 0; y < h; y += 8)
    {
        for (u32 x = 0; x < w; x += 8)
        {
            for (int i = 0; i < 64; i++)
            {
                int x2 = i & 7;
                int y2 = i >> 3;
                int pos = sTileOrder[(x2 & 3) + ((y2 & 3) << 2)] + ((x2 >> 2) << 4) + ((y2 >> 2) << 5);
                u32 c = ((const u32*)src)[(y + y2) * w + x + x2];
                dst[offs + pos] = ((c & 0xFF) << 24) | (((c >> 8) & 0xFF) << 16) | (((c >> 16) & 0xFF) << 8) | (c >> 24);
            }
            dst += 64;
        }
    }
}

static void gfx_citro3d_upload_texture(const uint8_t *rgba32_buf, int width, int height)
{
    if (width < 8 || height < 8 || (width & (width - 1)) || (height & (height - 1)))
    {
        u32 newWidth = width < 8 ? 8 : (1 << (32 - __builtin_clz(width - 1)));
        u32 newHeight = height < 8 ? 8 : (1 << (32 - __builtin_clz(height - 1)));
        if (newWidth * newHeight * 4 > sizeof(sTexBuf))
        {
            printf("Tex buffer overflow!\n");
            return;
        }
        int offs = 0;
        for (u32 y = 0; y < newHeight; y += 8)
        {
            for (u32 x = 0; x < newWidth; x += 8)
            {
                for (int i = 0; i < 64; i++)
                {
                    int x2 = i % 8;
                    int y2 = i / 8;

                    int realX = x + x2;
                    if (realX >= width)
                        realX -= width;

                    int realY = y + y2;
                    if (realY >= height)
                        realY -= height;

                    int pos = sTileOrder[x2 % 4 + y2 % 4 * 4] + 16 * (x2 / 4) + 32 * (y2 / 4);
                    u32 c = ((u32*)rgba32_buf)[realY * width + realX];
                    ((u32*)sTexBuf)[offs + pos] = ((c & 0xFF) << 24) | (((c >> 8) & 0xFF) << 16) | (((c >> 16) & 0xFF) << 8) | (c >> 24);
                }
                offs += 64;
            }
        }
        sTexturePoolScaleS[sCurTex] = width / (float)newWidth;
        sTexturePoolScaleT[sCurTex] = height / (float)newHeight;
        width = newWidth;
        height = newHeight;
    }
    else
    {
        sTexturePoolScaleS[sCurTex] = 1.f;
        sTexturePoolScaleT[sCurTex] = 1.f;
        performTexSwizzle(rgba32_buf, sTexBuf, width, height);
    }
    C3D_TexInit(&sTexturePool[sCurTex], width, height, GPU_RGBA8);
    C3D_TexUpload(&sTexturePool[sCurTex], sTexBuf);
    C3D_TexFlush(&sTexturePool[sCurTex]);
}

static uint32_t gfx_cm_to_opengl(uint32_t val)
{
    if (val & G_TX_CLAMP)
        return GPU_CLAMP_TO_EDGE;
    return (val & G_TX_MIRROR) ? GPU_MIRRORED_REPEAT : GPU_REPEAT;
}

static void gfx_citro3d_set_sampler_parameters(int tile, bool linear_filter, uint32_t cms, uint32_t cmt)
{
    C3D_TexSetFilter(&sTexturePool[sTexUnits[tile]], linear_filter ? GPU_LINEAR : GPU_NEAREST, linear_filter ? GPU_LINEAR : GPU_NEAREST);
    C3D_TexSetWrap(&sTexturePool[sTexUnits[tile]], gfx_cm_to_opengl(cms), gfx_cm_to_opengl(cmt));
}

static void update_depth()
{
    C3D_DepthTest(sDepthTestOn, GPU_LEQUAL, sDepthUpdateOn ? GPU_WRITE_ALL : GPU_WRITE_COLOR);
    C3D_DepthMap(true, -1.0f, sDepthDecal ? -0.001f : 0);
}

static void gfx_citro3d_set_depth_test(bool depth_test)
{
    sDepthTestOn = depth_test;
    update_depth();
}

static void gfx_citro3d_set_depth_mask(bool z_upd)
{
    sDepthUpdateOn = z_upd;
    update_depth();
}

static void gfx_citro3d_set_zmode_decal(bool zmode_decal)
{
    sDepthDecal = zmode_decal;
    update_depth();
}

static void gfx_citro3d_set_viewport(int x, int y, int width, int height)
{
    if (gGfx3DSMode == GFX_3DS_MODE_AA_22 || gGfx3DSMode == GFX_3DS_MODE_WIDE_AA_12)
    {
        viewport_x = x * 2;
        viewport_y = y * 2;
        viewport_width = width * 2;
        viewport_height = height * 2;
    }
    else if (gGfx3DSMode == GFX_3DS_MODE_WIDE)
    {
        viewport_x = x * 2;
        viewport_y = y;
        viewport_width = width * 2;
        viewport_height = height;
    }
    else // gGfx3DSMode == GFX_3DS_MODE_NORMAL
    {
        viewport_x = x;
        viewport_y = y;
        viewport_width = width;
        viewport_height = height;
    }
}

static void gfx_citro3d_set_scissor(int x, int y, int width, int height)
{
    if (gGfx3DSMode == GFX_3DS_MODE_NORMAL)
        C3D_SetScissor(GPU_SCISSOR_NORMAL, y, x, y + height, x + width);
    else if (gGfx3DSMode == GFX_3DS_MODE_AA_22 || gGfx3DSMode == GFX_3DS_MODE_WIDE_AA_12)
        C3D_SetScissor(GPU_SCISSOR_NORMAL, y * 2, x * 2, (y + height) * 2, (x + width) * 2);
    else if (gGfx3DSMode == GFX_3DS_MODE_WIDE)
        C3D_SetScissor(GPU_SCISSOR_NORMAL, y, x * 2, y + height, (x + width) * 2);
}

static void applyBlend()
{
    if (sUseBlend)
        C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA, GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA);
    else
        C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, GPU_ONE, GPU_ZERO, GPU_ONE, GPU_ZERO);
}

static void gfx_citro3d_set_use_alpha(bool use_alpha)
{
    sUseBlend = use_alpha;
    applyBlend();
}

static u32 vec4ToU32Color(float r, float g, float b, float a)
{
    u8 r2 = MAX(0, MIN(255, r * 255));
    u8 g2 = MAX(0, MIN(255, g * 255));
    u8 b2 = MAX(0, MIN(255, b * 255));
    u8 a2 = MAX(0, MIN(255, a * 255));
    return (a2 << 24) | (b2 << 16) | (g2 << 8) | r2;
}

static void renderFog(float buf_vbo[], UNUSED size_t buf_vbo_len, size_t buf_vbo_num_tris)
{
    C3D_SetTexEnv(0, &sShaderProgramPool[sCurShader].fog_texenv0);
    C3D_TexEnvInit(C3D_GetTexEnv(1));

    C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA, GPU_ZERO, GPU_DST_ALPHA);
    C3D_DepthTest(sDepthTestOn, GPU_LEQUAL, GPU_WRITE_COLOR);

    C3D_BoolUnifSet(GPU_VERTEX_SHADER, uLoc_draw_fog, true);

    bool hasTex = sShaderProgramPool[sCurShader].cc_features.used_textures[0] || sShaderProgramPool[sCurShader].cc_features.used_textures[1];
    if (hasTex)
    {
        C3D_FVUnifSet(GPU_VERTEX_SHADER, uLoc_tex_scale,
            sTexturePoolScaleS[sCurTex], -sTexturePoolScaleT[sCurTex], 1, 1);
    }

    memcpy(current_buffer->ptr + current_buffer->offset * current_buffer->stride,
        buf_vbo,
        buf_vbo_num_tris * 3 * current_buffer->stride * sizeof(float));

    C3D_DrawArrays(GPU_TRIANGLES, current_buffer->offset, buf_vbo_num_tris * 3);
    current_buffer->offset += buf_vbo_num_tris * 3;

    update_shader(false);
    applyBlend();
    update_depth();
}

static void renderTwoColorTris(float buf_vbo[], size_t buf_vbo_len, size_t buf_vbo_num_tris)
{
    int offset = 0;
    bool hasTex = sShaderProgramPool[sCurShader].cc_features.used_textures[0] || sShaderProgramPool[sCurShader].cc_features.used_textures[1];
    bool hasAlpha = sShaderProgramPool[sCurShader].cc_features.opt_alpha;
    bool hasFog = sShaderProgramPool[sCurShader].cc_features.opt_fog;
    if (hasFog)
        C3D_TexEnvColor(C3D_GetTexEnv(2), vec4ToU32Color(buf_vbo[hasTex ? 6 : 4], buf_vbo[hasTex ? 7 : 5], buf_vbo[hasTex ? 8 : 6], buf_vbo[hasTex ? 9 : 7]));
    u32 firstColor0, firstColor1;
    bool color0Constant = true;
    bool color1Constant = true;
    //determine which color is constant over all vertices
    for (u32 i = 0; i < buf_vbo_num_tris * 3 && color0Constant && color1Constant; i++)
    {
        int vtxOffs = 4;
        if (hasTex)
            vtxOffs += 2;
        if (hasFog)
            vtxOffs += 4;
        u32 color0 = vec4ToU32Color(
            buf_vbo[offset + vtxOffs],
            buf_vbo[offset + vtxOffs + 1],
            buf_vbo[offset + vtxOffs + 2],
            hasAlpha ? buf_vbo[offset + vtxOffs + 3] : 1.0f);
        vtxOffs += hasAlpha ? 4 : 3;
        u32 color1 = vec4ToU32Color(
            buf_vbo[offset + vtxOffs],
            buf_vbo[offset + vtxOffs + 1],
            buf_vbo[offset + vtxOffs + 2],
            hasAlpha ? buf_vbo[offset + vtxOffs + 3] : 1.0f);
        if (i == 0)
        {
            firstColor0 = color0;
            firstColor1 = color1;
        }
        else
        {
            if (firstColor0 != color0)
                color0Constant = false;
            if (firstColor1 != color1)
                color1Constant = false;
        }
        offset += current_buffer->stride;
    }

    update_shader(!color1Constant);
    C3D_TexEnvColor(C3D_GetTexEnv(0), color1Constant ? firstColor1 : firstColor0);

    if (hasFog)
        C3D_BoolUnifSet(GPU_VERTEX_SHADER, uLoc_draw_fog, false);
    if (hasTex)
        C3D_FVUnifSet(GPU_VERTEX_SHADER, uLoc_tex_scale,
            sTexturePoolScaleS[sCurTex], -sTexturePoolScaleT[sCurTex], 1, 1);

    memcpy(current_buffer->ptr + current_buffer->offset * current_buffer->stride,
        buf_vbo,
        buf_vbo_num_tris * 3 * current_buffer->stride * sizeof(float));

    C3D_DrawArrays(GPU_TRIANGLES, current_buffer->offset, buf_vbo_num_tris * 3);
    current_buffer->offset += buf_vbo_num_tris * 3;
    //
    if (hasFog)
        renderFog(buf_vbo, buf_vbo_len, buf_vbo_num_tris);
}

static void gfx_citro3d_draw_triangles(float buf_vbo[], size_t buf_vbo_len, size_t buf_vbo_num_tris)
{
    if (current_buffer->offset * current_buffer->stride > 256 * 1024 / 4)
    {
        printf("vertex buffer full!\n");
        return;
    }

    if (sShaderProgramPool[sCurShader].cc_features.num_inputs > 1)
    {
        renderTwoColorTris(buf_vbo, buf_vbo_len, buf_vbo_num_tris);
        return;
    }

    bool hasFog = sShaderProgramPool[sCurShader].cc_features.opt_fog;

    if (hasFog)
        C3D_BoolUnifSet(GPU_VERTEX_SHADER, uLoc_draw_fog, false);

    if (sShaderProgramPool[sCurShader].cc_features.used_textures[0] || sShaderProgramPool[sCurShader].cc_features.used_textures[1])
        C3D_FVUnifSet(GPU_VERTEX_SHADER, uLoc_tex_scale,
            sTexturePoolScaleS[sCurTex], -sTexturePoolScaleT[sCurTex], 1, 1);

    memcpy(current_buffer->ptr + current_buffer->offset * current_buffer->stride,
        buf_vbo,
        buf_vbo_num_tris * 3 * current_buffer->stride * sizeof(float));

    C3D_DrawArrays(GPU_TRIANGLES, current_buffer->offset, buf_vbo_num_tris * 3);
    current_buffer->offset += buf_vbo_num_tris * 3;

    if (hasFog)
        renderFog(buf_vbo, buf_vbo_len, buf_vbo_num_tris);
}

#ifdef ENABLE_N3DS_3D_MODE
static bool sIs2D;
static void gfx_citro3d_is_2d(bool is_2d)
{
    sIs2D = is_2d;
}
#endif

static void gfx_citro3d_draw_triangles_helper(float buf_vbo[], size_t buf_vbo_len, size_t buf_vbo_num_tris)
{
    // reset model and projections
    Mtx_Identity(&modelView);
    Mtx_Identity(&projection);

    C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uLoc_modelView, &modelView);
    C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uLoc_projection, &projection);

#ifdef ENABLE_N3DS_3D_MODE
    if ((gGfx3DSMode == GFX_3DS_MODE_NORMAL || gGfx3DSMode == GFX_3DS_MODE_AA_22) && gSliderLevel > 0.0f)
    {
        original_offset = current_buffer->offset;
        iod = gSliderLevel / 4.0f;

        if (!sIs2D)
        {
            Mtx_PerspStereoTilt(&projection, fov, 1.0f , 0.1f, 10.0f, -iod, focalLen, false);
            // hacks
            (&projection)->r[2].z = 1.0f;
            (&projection)->r[3].w = 1.0f;
            C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uLoc_projection, &projection);
            // undo the rotation applied by tilt
            Mtx_RotateZ(&modelView, 0.25f*M_TAU, true);
            C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uLoc_modelView, &modelView);
        }
    }
#endif
    // left screen
    C3D_FrameDrawOn(gTarget);
    C3D_SetViewport(viewport_y, viewport_x, viewport_height, viewport_width);
    gfx_citro3d_draw_triangles(buf_vbo, buf_vbo_len, buf_vbo_num_tris);
#ifdef ENABLE_N3DS_3D_MODE
    if ((gGfx3DSMode == GFX_3DS_MODE_NORMAL || gGfx3DSMode == GFX_3DS_MODE_AA_22) && gSliderLevel > 0.0f)
    {
        // restore buffer index
        current_buffer->offset = original_offset;

        if (!sIs2D)
        {
            Mtx_PerspStereoTilt(&projection, fov, 1.0f, 0.1f, 10.0f, iod, focalLen, false);
            // hacks
            (&projection)->r[2].z = 1.0f;
            (&projection)->r[3].w = 1.0f;
            C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uLoc_projection, &projection);
        }
        // draw right screen
        C3D_FrameDrawOn(gTargetRight);
        C3D_SetViewport(viewport_y, viewport_x, viewport_height, viewport_width);
        gfx_citro3d_draw_triangles(buf_vbo, buf_vbo_len, buf_vbo_num_tris);
    }
#endif
}

static void gfx_citro3d_init(void)
{
    C3D_CullFace(GPU_CULL_NONE);
    C3D_DepthMap(true, -1.0f, 0);
    C3D_DepthTest(false, GPU_LEQUAL, GPU_WRITE_ALL);
    C3D_AlphaTest(true, GPU_GREATER, 0x00);

    C3D_FrameRate(30);
}

static void gfx_citro3d_start_frame(void)
{
    for (int i = 0; i < video_buffers_size; i++)
    {
        video_buffers[i].offset = 0;
    }

    C3D_FrameBegin(C3D_FRAME_SYNCDRAW);

    // reset viewport if video mode changed
    if (gGfx3DSMode != sCurrentGfx3DSMode)
    {
        gfx_citro3d_set_viewport(0, 0, 400, 240);
        sCurrentGfx3DSMode = gGfx3DSMode;
    }

    C3D_RenderTargetClear(gTarget, C3D_CLEAR_ALL, 0x000000FF, 0xFFFFFFFF);
    // C3D_RenderTargetClear(gTargetBottom, C3D_CLEAR_ALL, 0x000000FF, 0xFFFFFFFF);
#ifdef ENABLE_N3DS_3D_MODE
    if (gGfx3DSMode == GFX_3DS_MODE_NORMAL || gGfx3DSMode == GFX_3DS_MODE_AA_22)
        C3D_RenderTargetClear(gTargetRight, C3D_CLEAR_ALL, 0x000000FF, 0xFFFFFFFF);
#endif
}

static void gfx_citro3d_on_resize(void)
{
}

static int frame_count;
static void gfx_citro3d_end_frame(void)
{
    // TOOD: draw the minimap here
    // gfx_3ds_menu_draw(current_buffer->ptr, current_buffer->offset, gShowConfigMenu);

    // set the texenv back
    update_shader(false);

    C3D_FrameEnd(0);
}

static void gfx_citro3d_finish_render(void)
{
}

struct GfxRenderingAPI gfx_citro3d_api = {
    gfx_citro3d_z_is_from_0_to_1,
    gfx_citro3d_unload_shader,
    gfx_citro3d_load_shader,
    gfx_citro3d_create_and_load_new_shader,
    gfx_citro3d_lookup_shader,
    gfx_citro3d_shader_get_info,
    gfx_citro3d_new_texture,
    gfx_citro3d_select_texture,
    gfx_citro3d_upload_texture,
    gfx_citro3d_set_sampler_parameters,
    gfx_citro3d_set_depth_test,
    gfx_citro3d_set_depth_mask,
    gfx_citro3d_set_zmode_decal,
    gfx_citro3d_set_viewport,
    gfx_citro3d_set_scissor,
    gfx_citro3d_set_use_alpha,
    gfx_citro3d_draw_triangles_helper,
    gfx_citro3d_init,
    gfx_citro3d_on_resize,
    gfx_citro3d_start_frame,
    gfx_citro3d_end_frame,
#ifdef ENABLE_N3DS_3D_MODE
    gfx_citro3d_finish_render,
    gfx_citro3d_is_2d
#else
    gfx_citro3d_finish_render
#endif
};

#endif
