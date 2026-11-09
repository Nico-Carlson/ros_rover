#include "protocol.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

uint8_t protocol_checksum(const char* body, size_t len) {
    uint8_t value = 0;
    for (size_t i = 0; i < len; i++) {
        value ^= (uint8_t)body[i];
    }
    return value;
}

void protocol_checksum_hex(const char* body, size_t len, char* out) {
    const char* digits = "0123456789ABCDEF";
    uint8_t value = protocol_checksum(body, len);
    out[0] = digits[value >> 4];
    out[1] = digits[value & 0x0F];
    out[2] = '\0';
}

bool protocol_check_line(const char* line, size_t* body_len) {
    size_t len = strlen(line);

    // find the LAST '*', same as rsplit in Python
    size_t star = len;
    for (size_t i = len; i > 0; i--) {
        if (line[i - 1] == '*') {
            star = i - 1;
            break;
        }
    }
    if (star == len) return false;           // no '*'
    if (len - star - 1 != 2) return false;   // need exactly 2 chars after it

    char expected[3];
    protocol_checksum_hex(line, star, expected);
    if (line[star + 1] != expected[0] || line[star + 2] != expected[1]) return false;

    *body_len = star;
    return true;
}

void line_reader_init(LineReader* r) {
    r->len = 0;
    r->buf[0] = '\0';
}

bool line_reader_feed(LineReader* r, char c, size_t* body_len) {
    if (c == '\n') {
        r->buf[r->len] = '\0';
        size_t n = 0;
        bool ok = protocol_check_line(r->buf, &n);
        if (ok) {
            r->buf[n] = '\0';   // cut off "*CS", leaving just the body
            *body_len = n;
        }
        r->len = 0;
        return ok;
    }
    if (r->len >= PROTOCOL_MAX_LINE) {
        r->len = 0;             // overflow: throw the line away
        return false;
    }
    r->buf[r->len++] = c;
    return false;
}

size_t protocol_make_line(const char* body, char* out, size_t out_size) {
    size_t len = strlen(body);
    size_t needed = len + 1 + 2 + 1 + 1;  // body, '*', two hex digits, '\n', '\0'
    if (needed > out_size) return 0;

    memcpy(out, body, len);
    out[len] = '*';
    protocol_checksum_hex(body, len, out + len + 1);  // writes 2 digits plus '\0'
    out[len + 3] = '\n';
    out[len + 4] = '\0';
    return len + 4;
}

size_t protocol_make_lift(bool down, bool up, char* out, size_t out_size) {
    char body[16];
    snprintf(body, sizeof(body), "LIFT,%d,%d", down ? 1 : 0, up ? 1 : 0);
    return protocol_make_line(body, out, out_size);
}

size_t protocol_make_bat(float volts, char* out, size_t out_size) {
    char body[24];
    snprintf(body, sizeof(body), "BAT,%.1f", volts);
    return protocol_make_line(body, out, out_size);
}

size_t protocol_make_mode(bool autonomous, bool danger, bool warning, bool brake,
                          char* out, size_t out_size) {
    char body[24];
    snprintf(body, sizeof(body), "MODE,%d,%d,%d,%d",
             autonomous ? 1 : 0, danger ? 1 : 0, warning ? 1 : 0, brake ? 1 : 0);
    return protocol_make_line(body, out, out_size);
}

bool protocol_parse_cmd(const char* body, float* speed, float* steer, float* lift) {
    if (strncmp(body, "CMD,", 4) != 0) return false;
    const char* p = body + 4;
    char* end;
    float v[3];
    for (int i = 0; i < 3; i++) {
        v[i] = strtof(p, &end);
        if (end == p) return false;            // no number found
        if (!isfinite(v[i])) return false;     // reject nan and inf
        if (i < 2) {
            if (*end != ',') return false;     // a comma must follow the first two
            p = end + 1;
        } else if (*end != '\0') {
            return false;                      // nothing may follow the third
        }
    }
    *speed = v[0];
    *steer = v[1];
    *lift = v[2];
    return true;
}
