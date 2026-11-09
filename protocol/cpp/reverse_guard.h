#pragma once
#include <stdint.h>
#include "drive.h"

#define REVERSE_SETTLE_MS 500  // PLACEHOLDER: how long to hold stopped before the relay moves

struct ReverseGuard {
    bool relay_reverse;         // what the relay is actually doing (false = forward)
    bool settling;              // true while waiting to change direction
    uint32_t settle_start_ms;   // when the wait began
};

// The relay comes up forward after a reset, so that is the starting state.
void reverse_guard_init(ReverseGuard* g);

// Call once per loop with what the Pi wants. `out` is what is safe to apply now.
void reverse_guard_step(ReverseGuard* g, const DriveOutput* want,
                        uint32_t now_ms, uint32_t settle_ms, DriveOutput* out);
