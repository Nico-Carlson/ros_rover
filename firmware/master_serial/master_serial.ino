#include "protocol.h"
#include "watchdog.h"
#include "drive.h"
#include "reverse_guard.h"

#define CMD_TIMEOUT_MS 500   // PLACEHOLDER: stop if no fresh CMD for this long
#define DEBUG_PRINT 1        // set to 0 for real runs

LineReader reader;
CommandWatchdog watchdog;
ReverseGuard guard;

float cmd_speed = 0, cmd_steer = 0, cmd_lift = 0;   // last good command
uint32_t last_print_ms = 0;

void setup() {
  Serial.begin(115200);
  line_reader_init(&reader);
  watchdog_init(&watchdog);
  reverse_guard_init(&guard);
}

void loop() {
  uint32_t now = millis();

  // 1. Read whatever bytes are waiting. Never wait for more.
  while (Serial.available() > 0) {
    char c = (char)Serial.read();
    size_t len = 0;
    if (line_reader_feed(&reader, c, &len)) {
      float s, t, l;
      if (protocol_parse_cmd(reader.buf, &s, &t, &l)) {
        cmd_speed = s;
        cmd_steer = t;
        cmd_lift = l;
        watchdog_feed(&watchdog, now);
      }
    }
  }

  // 2. Decide. A stale command means speed 0, which means brake on.
  bool fresh = watchdog_fresh(&watchdog, now, CMD_TIMEOUT_MS);
  float speed = fresh ? cmd_speed : 0.0f;
  DriveOutput want, safe;
  drive_from_speed(speed, &want);
  reverse_guard_step(&guard, &want, now, REVERSE_SETTLE_MS, &safe);

  // (Later: apply `safe` to the DAC, relay and brake pins here.)

#if DEBUG_PRINT
  if (now - last_print_ms >= 200) {
    last_print_ms = now;
    Serial.printf("# fresh=%d speed=%.2f dac=%u rev=%d brake=%d settling=%d\n",
                  fresh, speed, safe.throttle_dac, safe.reverse, safe.brake,
                  guard.settling);
  }
#endif
}
