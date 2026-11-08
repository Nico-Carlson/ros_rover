#include <stdio.h>
#include <string.h>
#include "protocol.h"

static int failures = 0;

static void check_checksum(const char* body, const char* expected) {
    char out[3];
    protocol_checksum_hex(body, strlen(body), out);
    if (strcmp(out, expected) == 0) {
        printf("PASS  %s -> %s\n", body, out);
    } else {
        printf("FAIL  %s -> %s (expected %s)\n", body, out, expected);
        failures++;
    }
}

static void check_line(const char* line, bool expect_ok, size_t expect_len) {
    size_t body_len = 0;
    bool ok = protocol_check_line(line, &body_len);
    if (ok == expect_ok && (!ok || body_len == expect_len)) {
        printf("PASS  \"%s\" -> %s\n", line, ok ? "ok" : "dropped");
    } else {
        printf("FAIL  \"%s\" -> %s, len %zu\n", line, ok ? "ok" : "dropped", body_len);
        failures++;
    }
}

static void check_make_line(const char* body, const char* expected) {
    char out[PROTOCOL_MAX_LINE + 1];
    size_t n = protocol_make_line(body, out, sizeof(out));
    if (n == strlen(expected) && strcmp(out, expected) == 0) {
        printf("PASS  make_line %s\n", body);
    } else {
        printf("FAIL  make_line %s -> \"%s\" (expected \"%s\")\n", body, out, expected);
        failures++;
    }
}

static void check_make_lift(bool down, bool up, const char* expected) {
    char out[PROTOCOL_MAX_LINE + 1];
    protocol_make_lift(down, up, out, sizeof(out));
    if (strcmp(out, expected) == 0) {
        printf("PASS  make_lift %d,%d\n", down, up);
    } else {
        printf("FAIL  make_lift %d,%d -> \"%s\" (expected \"%s\")\n", down, up, out, expected);
        failures++;
    }
}

// Feeds every char of s. Returns how many valid lines arrived; last_body gets the latest body.
static int feed_all(LineReader* r, const char* s, char* last_body) {
    int count = 0;
    for (size_t i = 0; s[i] != '\0'; i++) {
        size_t n = 0;
        if (line_reader_feed(r, s[i], &n)) {
            count++;
            strcpy(last_body, r->buf);
        }
    }
    return count;
}

static void check_reader(const char* name, int got, int expect,
                         const char* body, const char* expect_body) {
    bool ok = (got == expect) && (expect == 0 || strcmp(body, expect_body) == 0);
    if (ok) {
        printf("PASS  %s\n", name);
    } else {
        printf("FAIL  %s (got %d lines, body \"%s\")\n", name, got, body);
        failures++;
    }
}

static void run_vectors(const char* path) {
    FILE* f = fopen(path, "r");
    if (!f) {
        printf("FAIL  cannot open %s\n", path);
        failures++;
        return;
    }
    char raw[256];
    while (fgets(raw, sizeof(raw), f)) {
        raw[strcspn(raw, "\r\n")] = '\0';
        if (raw[0] == '\0' || raw[0] == '#') continue;

        // split at '|' (at most 3 parts)
        char* parts[3] = {0, 0, 0};
        int count = 0;
        char* p = raw;
        parts[count++] = p;
        while (*p && count < 3) {
            if (*p == '|') {
                *p = '\0';
                parts[count++] = p + 1;
            }
            p++;
        }

        bool expect_ok = (strcmp(parts[0], "ok") == 0);
        const char* line = parts[1];
        const char* expect_body = (count > 2) ? parts[2] : "";

        // feed the line plus a newline through the reader, byte by byte
        LineReader r;
        line_reader_init(&r);
        char got_body[PROTOCOL_MAX_LINE + 1] = "";
        int lines = feed_all(&r, line, got_body);
        lines += feed_all(&r, "\n", got_body);

        bool pass;
        if (expect_ok) pass = (lines == 1) && strcmp(got_body, expect_body) == 0;
        else           pass = (lines == 0);

        if (pass) {
            printf("PASS  vector %s|%s\n", parts[0], line);
        } else {
            printf("FAIL  vector %s|%s (got %d lines, body \"%s\")\n", parts[0], line, lines, got_body);
            failures++;
        }
    }
    fclose(f);
}

int main() {
    check_checksum("CMD,0.30,12.5,0.0", "4D");
    check_checksum("CMD,0.3,12.5,0.0", "7D");
    check_checksum("", "00");

    check_line("CMD,0.3,12.5,0.0*7D", true, 16);   // good line
    check_line("CMD,0.3,12.5,0.0*7E", false, 0);   // wrong checksum
    check_line("CMD,0.3,12.5,0.0", false, 0);      // no checksum
    check_line("CMD,0.3,12.5,0.0*7", false, 0);    // checksum cut short
    check_line("CMD,0.3,12.5,0.0*7D1", false, 0);  // extra char after checksum

    check_make_line("BAT,12.4", "BAT,12.4*62\n");
    check_make_line("CMD,0.3,12.5,0.0", "CMD,0.3,12.5,0.0*7D\n");
    check_make_lift(true, false, "LIFT,1,0*16\n");
    check_make_lift(false, false, "LIFT,0,0*17\n");
    check_make_lift(false, true, "LIFT,0,1*16\n");

    LineReader r;
    line_reader_init(&r);
    char body[PROTOCOL_MAX_LINE + 1] = "";
    int n;

    n = feed_all(&r, "CMD,0.3,12.", body);
    check_reader("half line, first part", n, 0, body, "");
    n = feed_all(&r, "5,0.0*7D\n", body);
    check_reader("half line, rest", n, 1, body, "CMD,0.3,12.5,0.0");

    n = feed_all(&r, "CMD,0.3,12.5,0.0*7D\nBAT,12.4*62\n", body);
    check_reader("two lines in one chunk", n, 2, body, "BAT,12.4");

    n = feed_all(&r, "garbageCMD,0.3,12.5,0.0*7D\n", body);
    check_reader("junk before a good line", n, 0, body, "");
    n = feed_all(&r, "CMD,0.3,12.5,0.0*7D\n", body);
    check_reader("recovers after junk", n, 1, body, "CMD,0.3,12.5,0.0");

    n = feed_all(&r, "CMD,0.4,12.5,0.0*7D\n", body);
    check_reader("corrupted line", n, 0, body, "");

    char long_line[153];
    memset(long_line, 'x', 150);
    long_line[150] = '\n';
    long_line[151] = '\0';
    n = feed_all(&r, long_line, body);
    check_reader("over-long line", n, 0, body, "");
    n = feed_all(&r, "CMD,0.3,12.5,0.0*7D\n", body);
    check_reader("recovers after overflow", n, 1, body, "CMD,0.3,12.5,0.0");

    run_vectors("../vectors.txt");

    return failures;
}
