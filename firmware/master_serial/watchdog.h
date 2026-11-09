#pragma once
#include <stdint.h>

struct CommandWatchdog {
    uint32_t last_ms;  // time of the last good command
    bool seen;         // false until the first command arrives
};

void watchdog_init(CommandWatchdog* w);

// Call when a valid CMD line arrives.
void watchdog_feed(CommandWatchdog* w, uint32_t now_ms);

// True only if a command has arrived and it is no older than timeout_ms.
bool watchdog_fresh(const CommandWatchdog* w, uint32_t now_ms, uint32_t timeout_ms);
