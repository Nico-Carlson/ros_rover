#include "watchdog.h"

void watchdog_init(CommandWatchdog* w) {
    w->last_ms = 0;
    w->seen = false;
}

void watchdog_feed(CommandWatchdog* w, uint32_t now_ms) {
    w->last_ms = now_ms;
    w->seen = true;
}

bool watchdog_fresh(const CommandWatchdog* w, uint32_t now_ms, uint32_t timeout_ms) {
    if (!w->seen) return false;                   // never heard from the Pi
    return (uint32_t)(now_ms - w->last_ms) <= timeout_ms;
}
