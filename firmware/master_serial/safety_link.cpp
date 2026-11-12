// safety_link.cpp
#include "safety_link.h"

uint8_t safety_crc8(const uint8_t *data, size_t len) {
    uint8_t crc = 0x00;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int bit = 0; bit < 8; bit++) {
            if (crc & 0x80) crc = (uint8_t)((crc << 1) ^ 0x07);
            else            crc = (uint8_t)(crc << 1);
        }
    }
    return crc;
}

void safety_make_packet(uint8_t seq, bool danger, bool warning,
                        uint8_t out[SAFETY_PACKET_LEN]) {
    uint8_t flags = 0;
    if (danger)  flags |= SAFETY_FLAG_DANGER;
    if (warning) flags |= SAFETY_FLAG_WARNING;

    out[0] = SAFETY_MAGIC;
    out[1] = seq;
    out[2] = flags;
    out[3] = safety_crc8(&out[1], 2);   // covers seq + flags, not the magic
}

void safety_reader_init(SafetyReader* r) {
    r->count = 0;
}

bool safety_reader_push(SafetyReader* r, uint8_t byte, bool* danger, bool* warning) {
    // Not inside a packet yet: ignore everything until the magic byte.
    if (r->count == 0 && byte != SAFETY_MAGIC) return false;

    r->buf[r->count++] = byte;
    if (r->count < SAFETY_PACKET_LEN) return false;   // still collecting

    // We have 4 bytes. Good CRC = valid packet.
    if (safety_crc8(&r->buf[1], 2) == r->buf[3]) {
        *danger  = (r->buf[2] & SAFETY_FLAG_DANGER)  != 0;
        *warning = (r->buf[2] & SAFETY_FLAG_WARNING) != 0;
        r->count = 0;
        return true;
    }

    // Bad CRC. Drop the first byte, then look for another magic byte
    // inside what we already have, because a real packet may have started
    // there. Keep from that magic byte onward and carry on collecting.
    uint8_t start = 1;
    while (start < SAFETY_PACKET_LEN && r->buf[start] != SAFETY_MAGIC) start++;

    uint8_t n = 0;
    for (uint8_t i = start; i < SAFETY_PACKET_LEN; i++) r->buf[n++] = r->buf[i];
    r->count = n;
    return false;
}

void safety_link_init(SafetyLink* s) {
    safety_reader_init(&s->reader);
    watchdog_init(&s->wd);
    s->danger  = false;
    s->warning = false;
}

void safety_link_push(SafetyLink* s, uint8_t byte, uint32_t now_ms) {
    bool d, w;
    if (safety_reader_push(&s->reader, byte, &d, &w)) {
        s->danger  = d;
        s->warning = w;
        watchdog_feed(&s->wd, now_ms);
    }
}

void safety_link_status(const SafetyLink* s, uint32_t now_ms, uint32_t timeout_ms,
                        bool* danger, bool* warning, bool* stale) {
    *danger  = s->danger;
    *warning = s->warning;
    *stale   = !watchdog_fresh(&s->wd, now_ms, timeout_ms);
}
