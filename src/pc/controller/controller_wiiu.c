#include <stdint.h>
#include <stdbool.h>

#include <ultra64.h>

#include <vpad/input.h>
#include <padscore/wpad.h>
#include <padscore/kpad.h>

#include "controller_api.h"

static void controller_wiiu_init(void) {
    VPADInit();
    KPADInit();
    WPADEnableURCC(1);
    WPADEnableWiiRemote(1);
}

static void read_vpad(OSContPad *pad) {
    VPADStatus status;
    VPADReadError err;
    uint32_t v;

    VPADRead(VPAD_CHAN_0, &status, 1, &err);
    v = status.hold;

    if (v & VPAD_BUTTON_B || v & VPAD_BUTTON_A) pad->button |= A_BUTTON;
    if (v & VPAD_BUTTON_Y || v & VPAD_BUTTON_X) pad->button |= B_BUTTON;
    if (v & VPAD_BUTTON_ZL || v & VPAD_BUTTON_L) pad->button |= Z_TRIG;
    if (v & VPAD_BUTTON_R || v & VPAD_BUTTON_ZR) pad->button |= R_TRIG;
    if (v & VPAD_BUTTON_PLUS) pad->button |= START_BUTTON;
    if (v & VPAD_STICK_R_EMULATION_UP) pad->button |= U_CBUTTONS;
    if (v & VPAD_STICK_R_EMULATION_RIGHT) pad->button |= R_CBUTTONS;
    if (v & VPAD_STICK_R_EMULATION_DOWN) pad->button |= D_CBUTTONS;
    if (v & VPAD_STICK_R_EMULATION_LEFT) pad->button |= L_CBUTTONS;

    if (status.leftStick.x < 0)
        pad->stick_x = (s8) (status.leftStick.x * 128);
    else
        pad->stick_x = (s8) (status.leftStick.x * 127);

    if (status.leftStick.y < 0)
        pad->stick_y = (s8) (status.leftStick.y * 128);
    else
        pad->stick_y = (s8) (status.leftStick.y * 127);

}

static void read_wpad(OSContPad* pad) {
    // Disconnect any extra controllers
    for (int i = 1; i < 4; i++) {
        WPADExtensionType ext;
        int res = WPADProbe(i, &ext);
        if (res == 0) {
            WPADDisconnect(i);
        }
    }

    KPADStatus status;
    int err;
    int read = KPADReadEx(WPAD_CHAN_0, &status, 1, &err);
    if (read == 0)
        return;

    uint32_t wm = status.hold;
    KPADVec2D stick;
    bool disconnect = false;
    if (status.hold & WPAD_BUTTON_MINUS)
        disconnect = true;

    if (status.extensionType == WPAD_EXT_NUNCHUK || status.extensionType == WPAD_EXT_MPLUS_NUNCHUK) {
        uint32_t ext = status.nunchuck.hold;
        stick = status.nunchuck.stick;

        if (wm & WPAD_BUTTON_A) pad->button |= A_BUTTON;
        if (wm & WPAD_BUTTON_B) pad->button |= B_BUTTON;
        if (wm & WPAD_BUTTON_PLUS) pad->button |= START_BUTTON;
        if (wm & WPAD_BUTTON_UP) pad->button |= U_CBUTTONS;
        if (wm & WPAD_BUTTON_DOWN) pad->button |= D_CBUTTONS;
        if (wm & WPAD_BUTTON_LEFT) pad->button |= L_CBUTTONS;
        if (wm & WPAD_BUTTON_RIGHT) pad->button |= R_CBUTTONS;
        if (ext & WPAD_NUNCHUK_BUTTON_C) pad->button |= R_TRIG;
        if (ext & WPAD_NUNCHUK_BUTTON_Z) pad->button |= Z_TRIG;
    } else if (status.extensionType == WPAD_EXT_CLASSIC || status.extensionType == WPAD_EXT_MPLUS_CLASSIC) {
        uint32_t ext = status.classic.hold;
        stick = status.classic.leftStick;
        if (ext & (WPAD_CLASSIC_BUTTON_B | WPAD_CLASSIC_BUTTON_A)) pad->button |= A_BUTTON;
        if (ext & (WPAD_CLASSIC_BUTTON_Y | WPAD_CLASSIC_BUTTON_X)) pad->button |= B_BUTTON;
        if (ext & (WPAD_CLASSIC_BUTTON_ZL | WPAD_CLASSIC_BUTTON_L)) pad->button |= Z_TRIG;
        if (ext & (WPAD_CLASSIC_BUTTON_ZR | WPAD_CLASSIC_BUTTON_R)) pad->button |= R_TRIG;
        if (ext & WPAD_CLASSIC_STICK_R_EMULATION_UP) pad->button |= U_CBUTTONS;
        if (ext & WPAD_CLASSIC_STICK_R_EMULATION_DOWN) pad->button |= D_CBUTTONS;
        if (ext & WPAD_CLASSIC_STICK_R_EMULATION_LEFT) pad->button |= L_CBUTTONS;
        if (ext & WPAD_CLASSIC_STICK_R_EMULATION_RIGHT) pad->button |= R_CBUTTONS;
        if (ext & WPAD_CLASSIC_BUTTON_PLUS) pad->button |= START_BUTTON;
        if (ext & WPAD_CLASSIC_BUTTON_MINUS) disconnect = true;
    } else if (status.extensionType == WPAD_EXT_PRO_CONTROLLER) {
        uint32_t ext = status.pro.hold;
        stick = status.pro.leftStick;
        if (ext & (WPAD_PRO_BUTTON_B | WPAD_PRO_BUTTON_A)) pad->button |= A_BUTTON;
        if (ext & (WPAD_PRO_BUTTON_Y | WPAD_PRO_BUTTON_X)) pad->button |= B_BUTTON;
        if (ext & (WPAD_PRO_TRIGGER_ZL | WPAD_PRO_TRIGGER_L)) pad->button |= Z_TRIG;
        if (ext & (WPAD_PRO_TRIGGER_ZR | WPAD_PRO_TRIGGER_R)) pad->button |= R_TRIG;
        if (ext & WPAD_PRO_STICK_R_EMULATION_UP) pad->button |= U_CBUTTONS;
        if (ext & WPAD_PRO_STICK_R_EMULATION_DOWN) pad->button |= D_CBUTTONS;
        if (ext & WPAD_PRO_STICK_R_EMULATION_LEFT) pad->button |= L_CBUTTONS;
        if (ext & WPAD_PRO_STICK_R_EMULATION_RIGHT) pad->button |= R_CBUTTONS;
        if (ext & WPAD_PRO_BUTTON_PLUS) pad->button |= START_BUTTON;
        if (ext & WPAD_PRO_BUTTON_MINUS) disconnect = true;
    }

    if (stick.x < 0)
        pad->stick_x = (s8) (stick.x * 128);
    else
        pad->stick_x = (s8) (stick.x * 127);

    if (stick.y < 0)
        pad->stick_y = (s8) (stick.y * 128);
    else
        pad->stick_y = (s8) (stick.y * 127);

    if (disconnect)
        WPADDisconnect(WPAD_CHAN_0);
}

static void controller_wiiu_read(OSContPad* pad) {
    read_vpad(pad);
    read_wpad(pad);
}

struct ControllerAPI controller_wiiu = {
    controller_wiiu_init,
    controller_wiiu_read
};
