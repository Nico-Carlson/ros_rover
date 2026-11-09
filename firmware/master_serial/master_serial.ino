#include "protocol.h"
#include "watchdog.h"
#include "drive.h"
#include "reverse_guard.h"
#include "mode_switch.h"

#define CMD_TIMEOUT_MS 500   // PLACEHOLDER: stop if no fresh CMD for this long
#define DEBUG_PRINT 1        // set to 0 for real runs

LineReader reader;
CommandWatchdog watchdog;
ReverseGuard guard;

float cmd_speed = 0, cmd_steer = 0, cmd_lift = 0;   // last good command
uint32_t last_print_ms = 0;

// ===================== FILL IN =====================
// Every uncertain value lives here. Check against the real wiring.
#define ENABLE_OUTPUTS 0        // 0 = never touch the motor pins (first test)

// RC inputs (pins from the pre-ROS firmware)
#define PIN_RC_STEER     35     // ch1
#define PIN_RC_THROTTLE  32     // ch2: throttle + brake
#define PIN_RC_LIFT       4     // ch6
#define PIN_RC_REVERSE   34     // ch7
#define PIN_RC_MODE      22     // manual/auto switch. Transmitter channel UNKNOWN

// Outputs
#define PIN_DAC_THROTTLE  25    // DAC1
#define PIN_REVERSE_RELAY 23
#define PIN_BRAKE_LPWM    18    // BTS7960, brake actuator
#define PIN_BRAKE_RPWM     2    // GPIO 2 is a strapping pin
#define PIN_BRAKE_L_EN    14
#define PIN_BRAKE_R_EN    19
#define PIN_BRAKE_LIMIT   13    // pullup, LOW = pressed
#define PIN_STEER_SERVO   33
#define PIN_LIFT_A        26    // both HIGH = stop
#define PIN_LIFT_B        27
// ===================================================

ModeSwitch mode;

#define RC_STALE_MS 100          // no new pulse for this long = receiver gone

volatile uint32_t mode_rise_us = 0;
volatile uint32_t mode_width_us = 0;
volatile uint32_t mode_new_at_us = 0;
volatile uint32_t mode_frames = 0;
portMUX_TYPE rcMux = portMUX_INITIALIZER_UNLOCKED;
uint32_t mode_frames_seen = 0;

void IRAM_ATTR mode_isr() {
  uint32_t t = micros();
  if (digitalRead(PIN_RC_MODE)) {
    mode_rise_us = t;                       // pulse started
  } else {
    uint32_t w = t - mode_rise_us;          // pulse ended
    if (w >= 500 && w <= 2500) {            // ignore garbage widths
      portENTER_CRITICAL_ISR(&rcMux);
      mode_width_us = w;
      mode_new_at_us = t;
      mode_frames++;
      portEXIT_CRITICAL_ISR(&rcMux);
    }
  }
}

void setup() {
  Serial.begin(115200);
  line_reader_init(&reader);
  watchdog_init(&watchdog);
  reverse_guard_init(&guard);

  mode_switch_init(&mode);
  pinMode(PIN_RC_MODE, INPUT);
  attachInterrupt(digitalPinToInterrupt(PIN_RC_MODE), mode_isr, CHANGE);
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

  // 1b. Mode switch: take a snapshot first, THEN read the clock.
  uint32_t w, at, frames;
  portENTER_CRITICAL(&rcMux);
  w = mode_width_us; at = mode_new_at_us; frames = mode_frames;
  portEXIT_CRITICAL(&rcMux);
  uint32_t now_us = micros();
  int32_t age_us = (int32_t)(now_us - at);

  if (frames == 0 || age_us > (int32_t)RC_STALE_MS * 1000) {
    mode_switch_stale(&mode);               // no signal = manual
  } else if (frames != mode_frames_seen) {
    mode_frames_seen = frames;
    mode_switch_frame(&mode, (int)w);       // one call per NEW pulse
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
    Serial.printf("# fresh=%d speed=%.2f dac=%u rev=%d brake=%d settling=%d auto=%d\n",
                  fresh, speed, safe.throttle_dac, safe.reverse, safe.brake,
                  guard.settling, mode.autonomous);
  }
#endif
}
