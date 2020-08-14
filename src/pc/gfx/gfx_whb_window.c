#ifdef TARGET_WII_U

#include <macros.h>

#include <coreinit/foreground.h>
#include <gx2/display.h>
#include <gx2/event.h>
#include <proc_ui/procui.h>

#include <whb/gfx.h>

#include "gfx_window_manager_api.h"
#include "gfx_screen_config.h"
#include "gfx_whb.h"

static uint32_t running = 0;

static void gfx_whb_proc_ui_save_callback(void)
{
   OSSavesDone_ReadyToRelease();
}

static void gfx_whb_init(UNUSED const char *game_name, UNUSED bool start_in_fullscreen) {
    ProcUIInit(&gfx_whb_proc_ui_save_callback);
    running = 1;

    WHBGfxInit();
}

static void gfx_whb_set_keyboard_callbacks(UNUSED bool (*on_key_down)(int scancode), UNUSED bool (*on_key_up)(int scancode), UNUSED void (*on_all_keys_up)(void)) {
}

static void gfx_whb_set_fullscreen_changed_callback(UNUSED void (*on_fullscreen_changed)(bool is_now_fullscreen)) {
}

static void gfx_whb_set_fullscreen(UNUSED bool enable) {
}

static void gfx_whb_main_loop(void (*run_one_game_iter)(void)) {
    // Ensure we run at 30FPS
    // Fool-proof unless the Wii U is able to
    // execute `run_one_game_iter()` so fast
    // that it doesn't even stall for enough time for
    // the second `GX2WaitForVsync()` to register
    GX2WaitForVsync();
    run_one_game_iter();

    if (running == 0) {
        // Received exit status, no need to continue
        return;
    }

    GX2WaitForVsync();

    if (running == 2) {
        ProcUIDrawDoneRelease();
    }
}

static void gfx_whb_get_dimensions(uint32_t *width, uint32_t *height) {
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

static void gfx_whb_handle_events(void) {
    ProcUIStatus status = ProcUIProcessMessages(TRUE);
process:
    if (status == PROCUI_STATUS_EXITING) {
        running = 0;
    } else if (status == PROCUI_STATUS_RELEASE_FOREGROUND) {
        running = 2;
    } else if (status == PROCUI_STATUS_IN_BACKGROUND) {
        do {
            status = ProcUIProcessMessages(TRUE);
        } while (status == PROCUI_STATUS_IN_BACKGROUND);
        goto process; // No longer in background, process new state
    } else { // PROCUI_STATUS_IN_FOREGROUND
        running = 1;
    }
}

static bool gfx_whb_start_frame(void) {
    return running != 0;
}

static void gfx_whb_swap_buffers_begin(void) {
    WHBGfxFinishRender();
    whb_free_vbo();
}

static void gfx_whb_swap_buffers_end(void) {
}

static double gfx_whb_get_time(void) {
    return 0.0;
}

struct GfxWindowManagerAPI gfx_whb_window = {
    gfx_whb_init,
    gfx_whb_set_keyboard_callbacks,
    gfx_whb_set_fullscreen_changed_callback,
    gfx_whb_set_fullscreen,
    gfx_whb_main_loop,
    gfx_whb_get_dimensions,
    gfx_whb_handle_events,
    gfx_whb_start_frame,
    gfx_whb_swap_buffers_begin,
    gfx_whb_swap_buffers_end,
    gfx_whb_get_time
};

#endif
