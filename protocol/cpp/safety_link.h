// safety_link.h
#pragma once
#include <stdint.h>
#include <stddef.h>
#include "watchdog.h"

uint8_t safety_crc8(const uint8_t *data, size_t len);

#define SAFETY_MAGIC        0xA5
#define SAFETY_FLAG_DANGER  0x01
#define SAFETY_FLAG_WARNING 0x02
#define SAFETY_PACKET_LEN   4
#define SAFETY_TIMEOUT_MS 350   // PLACEHOLDER, from the old firmware

struct SafetyReader {
  uint8_t buf[SAFETY_PACKET_LEN];
  uint8_t count;   // bytes collected so far
};

struct SafetyLink {
    SafetyReader reader;
    CommandWatchdog wd;   // reused: "last good packet" time
    bool danger;          // flags from the last good packet
    bool warning;
};

void safety_link_init(SafetyLink* s);

// Feed one received byte. A good packet refreshes the timer and the flags.
void safety_link_push(SafetyLink* s, uint8_t byte, uint32_t now_ms);

// danger/warning are the flags from the last good packet (false until one
// arrives) and HOLD until a later packet changes them. stale is for the
// debug line only: true if no good packet within timeout_ms, or none ever.
void safety_link_status(const SafetyLink* s, uint32_t now_ms, uint32_t timeout_ms,
                        bool* danger, bool* warning, bool* stale);

void safety_make_packet(uint8_t seq, bool danger, bool warning,
                        uint8_t out[SAFETY_PACKET_LEN]);


void safety_reader_init(SafetyReader* r);

// Feed ONE byte. Returns true only when a complete packet with a good CRC
// has just arrived; then danger/warning are filled in. On false the outputs
// are untouched.
bool safety_reader_push(SafetyReader* r, uint8_t byte, bool* danger, bool* warning);
