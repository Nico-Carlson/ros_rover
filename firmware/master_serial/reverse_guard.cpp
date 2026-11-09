#include "reverse_guard.h"

void reverse_guard_init(ReverseGuard* g) {
    g->relay_reverse = false;
    g->settling = false;
    g->settle_start_ms = 0;
}

void reverse_guard_step(ReverseGuard* g, const DriveOutput* want,
                        uint32_t now_ms, uint32_t settle_ms, DriveOutput* out) {
    bool moving = !want->brake;
    bool change = moving && (want->reverse != g->relay_reverse);

    if (!change) {
        // Nothing to switch (or the Pi wants a stop): pass the request through,
        // but the relay holds whatever it is doing now.
        g->settling = false;
        *out = *want;
        out->reverse = g->relay_reverse;
        return;
    }

    // The Pi wants the other direction: stop first, wait, then move the relay.
    if (!g->settling) {
        g->settling = true;
        g->settle_start_ms = now_ms;
    }
    if ((uint32_t)(now_ms - g->settle_start_ms) >= settle_ms) {
        g->relay_reverse = want->reverse;   // switch with the throttle at zero
        g->settling = false;
    }
    out->throttle_dac = 0;
    out->brake = true;
    out->reverse = g->relay_reverse;
}
