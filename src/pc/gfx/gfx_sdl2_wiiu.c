#ifdef TARGET_WII_U

#include <SDL2/SDL.h>
#include <gx2/display.h>
#include <gx2/event.h>
#include <whb/gfx.h>
#include <whb/log.h>

#include "gfx_window_manager_api.h"
#include "gfx_screen_config.h"

#define GFX_API_NAME "SDL2 - WHB"

static SDL_Window *wnd;
static unsigned int window_width;
static unsigned int window_height;
static bool fullscreen_state = false;
static void (*on_fullscreen_changed_callback)(bool is_now_fullscreen);
static bool (*on_key_down_callback)(int scancode);
static bool (*on_key_up_callback)(int scancode);
static void (*on_all_keys_up_callback)(void);

static void gfx_sdl_get_dimensions(uint32_t *width, uint32_t *height) {
    switch (GX2GetSystemTVScanMode()) {
        case GX2_TV_SCAN_MODE_480I:
        case GX2_TV_SCAN_MODE_480P:
            *width = 854;
            *height = 480;
            break;
        case GX2_TV_SCAN_MODE_1080I:
        case GX2_TV_SCAN_MODE_1080P:
            *width = 1920;
            *height = 1080;
            break;
        case GX2_TV_SCAN_MODE_720P:
        default:
            *width = 1280;
            *height = 720;
            break;
    }
}

static void set_fullscreen(bool on, bool call_callback) {
    if (fullscreen_state == on) {
        return;
    }
    fullscreen_state = on;

    gfx_sdl_get_dimensions(&window_width, &window_height);

    SDL_SetWindowSize(wnd, window_width, window_height);
    SDL_SetWindowFullscreen(wnd, SDL_WINDOW_FULLSCREEN);

    if (on_fullscreen_changed_callback != NULL && call_callback) {
        on_fullscreen_changed_callback(on);
    }
}

static void gfx_sdl_init(const char *game_name, bool start_in_fullscreen) {
    SDL_Init(SDL_INIT_VIDEO);

    char title[512];
    int len = sprintf(title, "%s (%s)", game_name, GFX_API_NAME);

    wnd = SDL_CreateWindow(title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
            0, 0, SDL_WINDOW_FULLSCREEN_DESKTOP);

    if (start_in_fullscreen) {
        set_fullscreen(true, false);
    }
}

static void gfx_sdl_set_fullscreen_changed_callback(void (*on_fullscreen_changed)(bool is_now_fullscreen)) {
    on_fullscreen_changed_callback = on_fullscreen_changed;
}

static void gfx_sdl_set_fullscreen(bool enable) {
    set_fullscreen(enable, true);
}

static void gfx_sdl_set_keyboard_callbacks(bool (*on_key_down)(int scancode), bool (*on_key_up)(int scancode), void (*on_all_keys_up)(void)) {
    on_key_down_callback = on_key_down;
    on_key_up_callback = on_key_up;
    on_all_keys_up_callback = on_all_keys_up;
}

static void gfx_sdl_main_loop(void (*run_one_game_iter)(void)) {
    // Ensure we run at 30FPS
    // Fool-proof unless the Wii U is able to
    // execute `run_one_game_iter()` so fast
    // that it doesn't even stall for enough time for
    // the second `GX2WaitForVsync()` to register
    GX2WaitForVsync();
    run_one_game_iter();
    GX2WaitForVsync();
}

static void gfx_sdl_onkeydown(int scancode) {
}

static void gfx_sdl_onkeyup(int scancode) {
}

static void gfx_sdl_handle_events(void) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
            case SDL_QUIT:
                SDL_DestroyWindow(wnd);
        }
    }
}

static bool gfx_sdl_start_frame(void) {
    return true;
}

static void gfx_sdl_swap_buffers_begin(void) {
    WHBGfxFinishRender();
}

static void gfx_sdl_swap_buffers_end(void) {
}

static double gfx_sdl_get_time(void) {
    return 0.0;
}

struct GfxWindowManagerAPI gfx_sdl = {
    gfx_sdl_init,
    gfx_sdl_set_keyboard_callbacks,
    gfx_sdl_set_fullscreen_changed_callback,
    gfx_sdl_set_fullscreen,
    gfx_sdl_main_loop,
    gfx_sdl_get_dimensions,
    gfx_sdl_handle_events,
    gfx_sdl_start_frame,
    gfx_sdl_swap_buffers_begin,
    gfx_sdl_swap_buffers_end,
    gfx_sdl_get_time
};

#endif
