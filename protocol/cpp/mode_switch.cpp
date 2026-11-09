#include "mode_switch.h"

void mode_switch_init(ModeSwitch* m) {
    m->autonomous = false;
    m->count = 0;
}

void mode_switch_frame(ModeSwitch* m, int pulse_us) {
    bool wants_auto;
    if (pulse_us <= MODE_MANUAL_MAX_US) {
        wants_auto = false;
    } else if (pulse_us >= MODE_AUTO_MIN_US) {
        wants_auto = true;
    } else {
        m->autonomous = false;
        m->count = 0;   // dead zone: fall back to manual
        return;
    }

    if (wants_auto == m->autonomous) {
        m->count = 0;   // already in that mode, nothing pending
        return;
    }

    m->count++;
    if (m->count >= MODE_CONFIRM_FRAMES) {
        m->autonomous = wants_auto;
        m->count = 0;
    }
}

void mode_switch_stale(ModeSwitch* m) {
    m->autonomous = false;
    m->count = 0;
}
