#include <stdio.h>
#include <math.h>
#include <string.h>
#include "protocol.h"
#include "watchdog.h"
#include "drive.h"
#include "reverse_guard.h"
#include "mode_switch.h"
#include "brake.h"
#include "tilt.h"

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

static void check_make_bat(float volts, const char* expected) {
    char out[PROTOCOL_MAX_LINE + 1];
    protocol_make_bat(volts, out, sizeof(out));
    if (strcmp(out, expected) == 0) {
        printf("PASS  make_bat %.2f\n", volts);
    } else {
        printf("FAIL  make_bat %.2f -> \"%s\" (expected \"%s\")\n", volts, out, expected);
        failures++;
    }
}

static void check_make_mode(bool a, bool d, bool w, bool b, const char* expected) {
    char out[PROTOCOL_MAX_LINE + 1];
    protocol_make_mode(a, d, w, b, out, sizeof(out));
    if (strcmp(out, expected) == 0) {
        printf("PASS  make_mode %d,%d,%d,%d\n", a, d, w, b);
    } else {
        printf("FAIL  make_mode %d,%d,%d,%d -> \"%s\" (expected \"%s\")\n", a, d, w, b, out, expected);
        failures++;
    }
}

static void check_parse_cmd(const char* body, bool expect_ok,
                            float s, float t, float l) {
    float rs = 99, rt = 99, rl = 99;   // 99 = "untouched" marker
    bool ok = protocol_parse_cmd(body, &rs, &rt, &rl);
    bool pass;
    if (expect_ok) {
        pass = ok && fabsf(rs - s) < 1e-4f && fabsf(rt - t) < 1e-4f && fabsf(rl - l) < 1e-4f;
    } else {
        pass = !ok && rs == 99 && rt == 99 && rl == 99;
    }
    if (pass) {
        printf("PASS  parse_cmd \"%s\" -> %s\n", body, ok ? "ok" : "rejected");
    } else {
        printf("FAIL  parse_cmd \"%s\" -> %s (%.2f, %.2f, %.2f)\n", body, ok ? "ok" : "rejected", rs, rt, rl);
        failures++;
    }
}

static void check_fresh(const char* name, const CommandWatchdog* w,
                        uint32_t now, uint32_t timeout, bool expect) {
    bool got = watchdog_fresh(w, now, timeout);
    if (got == expect) {
        printf("PASS  watchdog %s\n", name);
    } else {
        printf("FAIL  watchdog %s (got %d, expected %d)\n", name, got, expect);
        failures++;
    }
}

static void check_drive(float speed, int dac, bool rev, bool brake) {
    DriveOutput o;
    drive_from_speed(speed, &o);
    if (o.throttle_dac == dac && o.reverse == rev && o.brake == brake) {
        printf("PASS  drive %.2f -> dac %d, reverse %d, brake %d\n", speed, o.throttle_dac, o.reverse, o.brake);
    } else {
        printf("FAIL  drive %.2f -> dac %d, reverse %d, brake %d (expected %d, %d, %d)\n",
               speed, o.throttle_dac, o.reverse, o.brake, dac, rev, brake);
        failures++;
    }
}

// Sweeps -2.00 to +2.00 m/s in 0.01 steps and checks the outputs never contradict.
static void check_drive_sweep() {
    int bad = 0;
    for (int i = -200; i <= 200; i++) {
        float s = i / 100.0f;
        DriveOutput o;
        drive_from_speed(s, &o);
        if (o.brake && o.throttle_dac != 0) bad++;                 // brake and throttle together
        if (o.throttle_dac > 0 && o.reverse != (s < 0)) bad++;     // relay disagrees with direction
        if (o.brake && o.reverse) bad++;                           // relay on while stopped
    }
    if (bad == 0) {
        printf("PASS  drive sweep -2.00 to 2.00\n");
    } else {
        printf("FAIL  drive sweep (%d bad speeds)\n", bad);
        failures++;
    }
}

// Runs one loop of the guard with a speed command and checks the safe outputs.
static void check_guard(const char* name, ReverseGuard* g, float speed, uint32_t now,
                        int dac, bool rev, bool brake) {
    DriveOutput want, out;
    drive_from_speed(speed, &want);
    reverse_guard_step(g, &want, now, REVERSE_SETTLE_MS, &out);
    if (out.throttle_dac == dac && out.reverse == rev && out.brake == brake) {
        printf("PASS  guard %s\n", name);
    } else {
        printf("FAIL  guard %s -> dac %d, reverse %d, brake %d (expected %d, %d, %d)\n",
               name, out.throttle_dac, out.reverse, out.brake, dac, rev, brake);
        failures++;
    }
}

static void feed_mode(ModeSwitch* m, int pulse, int times) {
    for (int i = 0; i < times; i++) mode_switch_frame(m, pulse);
}

static void check_mode(const char* name, const ModeSwitch* m, bool expect_auto) {
    if (m->autonomous == expect_auto) {
        printf("PASS  mode %s\n", name);
    } else {
        printf("FAIL  mode %s (autonomous %d, expected %d)\n", name, m->autonomous, expect_auto);
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

static void check_brake(const char* name, BrakeMotor got, BrakeMotor expect) {
    if (got == expect) {
        printf("PASS  brake %s\n", name);
    } else {
        printf("FAIL  brake %s (got %d, expected %d)\n", name, got, expect);
        failures++;
    }
}

static void check_tilt(const char* name, float ax, float ay, float az,
                       float exp_roll, float exp_pitch, bool exp_warn, bool exp_danger) {
    float roll = 0, pitch = 0;
    TiltState t;
    tilt_init(&t);
    bool ok = tilt_angles(ax, ay, az, &roll, &pitch);
    if (ok) tilt_update(&t, roll, pitch);
    bool pass = ok && fabsf(roll - exp_roll) < 0.5f && fabsf(pitch - exp_pitch) < 0.5f
                && t.warning == exp_warn && t.danger == exp_danger;
    if (pass) {
        printf("PASS  tilt %s\n", name);
    } else {
        printf("FAIL  tilt %s (roll %.1f, pitch %.1f, warn %d, danger %d)\n",
               name, roll, pitch, t.warning, t.danger);
        failures++;
    }
}

static void check_tilt_rejected(const char* name, float ax, float ay, float az) {
    float roll, pitch;
    if (!tilt_angles(ax, ay, az, &roll, &pitch)) {
        printf("PASS  tilt %s\n", name);
    } else {
        printf("FAIL  tilt %s (should be rejected)\n", name);
        failures++;
    }
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

    check_make_bat(12.4f, "BAT,12.4*62\n");
    check_make_bat(12.46f, "BAT,12.5*63\n");
    check_make_bat(0.0f, "BAT,0.0*55\n");

    check_make_mode(true, false, false, false, "MODE,1,0,0,0*02\n");
    check_make_mode(false, false, false, true, "MODE,0,0,0,1*02\n");
    check_make_mode(false, true, true, false, "MODE,0,1,1,0*03\n");

    check_parse_cmd("CMD,0.30,12.5,0.00", true, 0.30f, 12.5f, 0.0f);
    check_parse_cmd("CMD,-0.45,-30.0,0.00", true, -0.45f, -30.0f, 0.0f);
    check_parse_cmd("CMD,0.3,12.5", false, 0, 0, 0);           // missing field
    check_parse_cmd("CMD,0.3,12.5,0.0,9", false, 0, 0, 0);     // extra field
    check_parse_cmd("CMD,abc,1,2", false, 0, 0, 0);            // not a number
    check_parse_cmd("CMD,0.3,12.5,0.0x", false, 0, 0, 0);      // junk after the number
    check_parse_cmd("CMD,nan,1,2", false, 0, 0, 0);            // nan rejected
    check_parse_cmd("BAT,12.4", false, 0, 0, 0);               // wrong type

    CommandWatchdog w;
    watchdog_init(&w);
    check_fresh("never fed is stale", &w, 100, 500, false);
    watchdog_feed(&w, 1000);
    check_fresh("fresh just after feed", &w, 1000, 500, true);
    check_fresh("fresh at 400 ms", &w, 1400, 500, true);
    check_fresh("fresh at exactly timeout", &w, 1500, 500, true);
    check_fresh("stale at 501 ms", &w, 1501, 500, false);
    watchdog_feed(&w, 0xFFFFFF00u);
    check_fresh("fresh across counter wrap", &w, 0x00000064u, 500, true);   // 356 ms later
    check_fresh("stale across counter wrap", &w, 0x000001FFu, 500, false);  // 767 ms later

    check_drive(0.0f, 0, false, true);       // stopped
    check_drive(0.05f, 0, false, true);      // exactly the stop band: still stopped
    check_drive(0.06f, 10, false, false);    // just above: slow forward
    check_drive(0.3f, 51, false, false);
    check_drive(0.75f, 128, false, false);   // half scale
    check_drive(1.5f, 255, false, false);    // full scale
    check_drive(3.0f, 255, false, false);    // over full scale clamps
    check_drive(-0.07f, 12, true, false);    // the old bug: small reverse stays reverse
    check_drive(-0.3f, 51, true, false);
    check_drive(-3.0f, 255, true, false);
    check_drive(NAN, 0, false, true);        // garbage in, stopped out
    check_drive_sweep();

    ReverseGuard g;
    reverse_guard_init(&g);
    check_guard("forward passes through", &g, 0.3f, 0, 51, false, false);
    check_guard("reverse asked: stop first", &g, -0.3f, 100, 0, false, true);
    check_guard("still waiting at 200 ms", &g, -0.3f, 300, 0, false, true);
    check_guard("relay flips at settle time", &g, -0.3f, 600, 0, true, true);
    check_guard("reverse throttle after flip", &g, -0.3f, 700, 51, true, false);
    check_guard("forward asked: stop first", &g, 0.3f, 800, 0, true, true);
    check_guard("stop request cancels wait", &g, 0.0f, 900, 0, true, true);
    check_guard("reverse again passes at once", &g, -0.3f, 1000, 51, true, false);
    check_guard("stopped holds the relay", &g, 0.0f, 1100, 0, true, true);
    check_guard("new change starts a new timer", &g, 0.3f, 1200, 0, true, true);
    check_guard("still waiting at 400 ms", &g, 0.3f, 1600, 0, true, true);
    check_guard("flips back to forward", &g, 0.3f, 1700, 0, false, true);
    check_guard("forward throttle after flip", &g, 0.3f, 1800, 51, false, false);

    ModeSwitch m;
    mode_switch_init(&m);
    check_mode("starts manual", &m, false);
    feed_mode(&m, 1700, 2);
    check_mode("2 auto frames: not yet", &m, false);
    feed_mode(&m, 1700, 1);
    check_mode("3rd auto frame switches", &m, true);
    feed_mode(&m, 1500, 1);
    check_mode("dead zone drops to manual", &m, false);
    feed_mode(&m, 1700, 3);
    check_mode("auto again after 3 frames", &m, true);
    feed_mode(&m, 1300, 2);
    feed_mode(&m, 1700, 1);
    feed_mode(&m, 1300, 2);
    check_mode("interrupted count restarts", &m, true);
    feed_mode(&m, 1300, 1);
    check_mode("3rd manual frame switches", &m, false);
    feed_mode(&m, 1700, 2);
    feed_mode(&m, 1500, 1);
    feed_mode(&m, 1700, 2);
    check_mode("dead zone resets count", &m, false);
    feed_mode(&m, 1700, 1);
    check_mode("then 3 in a row switches", &m, true);
    mode_switch_stale(&m);
    check_mode("stale forces manual", &m, false);
    feed_mode(&m, 1700, 2);
    mode_switch_stale(&m);
    feed_mode(&m, 1700, 1);
    check_mode("stale clears the count", &m, false);

    mode_switch_init(&m);
    feed_mode(&m, 1599, 5);
    check_mode("1599 is dead zone", &m, false);
    feed_mode(&m, 1600, 3);
    check_mode("1600 counts as auto", &m, true);
    feed_mode(&m, 1401, 1);
    check_mode("1401 is dead zone, drops to manual", &m, false);
    feed_mode(&m, 1600, 3);
    feed_mode(&m, 1400, 3);
    check_mode("1400 counts as manual", &m, false);

    BrakeState bk;
    brake_init(&bk);
    check_brake("want, switch open: extend", brake_step(&bk, true, false, 0), BRAKE_MOTOR_EXTEND);
    check_brake("want, switch pressed: stop", brake_step(&bk, true, true, 10), BRAKE_MOTOR_STOP);
    check_brake("applied flag set", (BrakeMotor)bk.applied, (BrakeMotor)1);
    check_brake("release at 0 ms: retract", brake_step(&bk, false, false, 1000), BRAKE_MOTOR_RETRACT);
    check_brake("release at 1999 ms: retract", brake_step(&bk, false, false, 2999), BRAKE_MOTOR_RETRACT);
    check_brake("release at 2000 ms: stop", brake_step(&bk, false, false, 3000), BRAKE_MOTOR_STOP);
    check_brake("release later: stop", brake_step(&bk, false, false, 9000), BRAKE_MOTOR_STOP);

    brake_init(&bk);
    brake_step(&bk, false, false, 100);
    check_brake("brake wanted mid-retract: extend", brake_step(&bk, true, false, 500), BRAKE_MOTOR_EXTEND);

    brake_init(&bk);
    brake_step(&bk, false, false, 0xFFFFFF00u);
    check_brake("wrap +100 ms: retract", brake_step(&bk, false, false, 0xFFFFFF00u + 100), BRAKE_MOTOR_RETRACT);
    check_brake("wrap +2100 ms: stop", brake_step(&bk, false, false, 0xFFFFFF00u + 2100), BRAKE_MOTOR_STOP);

    // roll = atan2(ay, az), so ay = az * tan(angle). tan 16 = 0.2867, tan 46 = 1.0355.
    check_tilt("level", 0, 0, 9.8f, 0, 0, false, false);
    check_tilt("roll 16: warning only", 0, 9.8f * 0.2867f, 9.8f, 16, 0, true, false);
    check_tilt("roll 46: warning and danger", 0, 9.8f * 1.0355f, 9.8f, 46, 0, true, true);
    check_tilt_rejected("NaN input", NAN, 0, 9.8f);
    check_tilt_rejected("zero vector", 0, 0, 0);

    TiltState th;
    tilt_init(&th);
    tilt_update(&th, 16, 0);
    bool on16 = th.warning;
    tilt_update(&th, 14, 0);
    bool on14 = th.warning;
    tilt_update(&th, 12, 0);
    bool on12 = th.warning;
    if (on16 && on14 && !on12) {
        printf("PASS  tilt hysteresis\n");
    } else {
        printf("FAIL  tilt hysteresis (16: %d, 14: %d, 12: %d)\n", on16, on14, on12);
        failures++;
    }

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
