#include "protocol.h"
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
