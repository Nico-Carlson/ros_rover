#pragma once
#include <stdint.h>
#include <stddef.h>

// XOR of every byte in body (length len), same as the Python checksum().
uint8_t protocol_checksum(const char* body, size_t len);

// Writes the checksum as two uppercase hex digits plus a '\0' into out.
// out must have room for 3 chars.
void protocol_checksum_hex(const char* body, size_t len, char* out);

// Checks one complete line (no trailing newline, '\0'-terminated), e.g. "CMD,0.3,12.5,0.0*7D".
// Returns true if it has a '*' and the two hex digits after it match the checksum of
// everything before it. On success, *body_len is set to the length of the body (before the '*').
bool protocol_check_line(const char* line, size_t* body_len);

#define PROTOCOL_MAX_LINE 100

struct LineReader {
    char buf[PROTOCOL_MAX_LINE + 1];
    size_t len;
};

void line_reader_init(LineReader* r);

// Feed one byte. Returns true when a complete, valid line has just arrived.
// Then r->buf holds the body ('\0'-terminated, checksum removed) and
// *body_len is its length. The reader resets itself after every line.
bool line_reader_feed(LineReader* r, char c, size_t* body_len);
