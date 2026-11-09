#include "protocol.h"
#include "watchdog.h"
#include "drive.h"
#include "reverse_guard.h"
#include "mode_switch.h"
#include "rc_channel.h"
#include "brake.h"

#define CMD_TIMEOUT_MS 500   // PLACEHOLDER: stop if no fresh CMD for this long
#define DEBUG_PRINT 1        // set to 0 for real runs



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

// Manual throttle scale. The old firmware used 0.75 in normal mode (and 1.0 in warning).
// 1.0 = full stick gives full DAC. Applied when the manual path is written.
#define MANUAL_THROTTLE_SCALE 1.0f

// RC pulse ranges in microseconds. PLACEHOLDERS: measure each channel on the bench
// (min, neutral, max) and fix these numbers.
#define THROTTLE_PULSE_MIN 980    // pre-ROS firmware value; stick pulled = LOWER pulse = more throttle
#define THROTTLE_PULSE_MAX 1517
#define STEER_PULSE_MIN   1000
#define STEER_PULSE_MAX   2000

// RC brake request from ch2. PLACEHOLDER threshold: the old firmware used > 2000 us,
// which sits at the edge of a normal channel. Measure your transmitter's real max.
#define BRAKE_PULSE_MIN 2000

// Loss of signal = brake ON (the old firmware released it, which coasts).
bool manual_brake_wanted(bool have_signal, int pulse_us) {
  if (!have_signal) return true;
  return pulse_us > BRAKE_PULSE_MIN;
}

// Which BTS7960 PWM pin EXTENDS the brake actuator (applies the brake)? UNKNOWN.
#define BRAKE_EXTEND_PIN  PIN_BRAKE_LPWM
#define BRAKE_RETRACT_PIN PIN_BRAKE_RPWM

#define REVERSE_PULSE_MIN 2000   // PLACEHOLDER: old firmware toggled when ch7 pulse > 2000 us
// ===================================================

float cmd_speed = 0, cmd_steer = 0, cmd_lift = 0;   // last good command
uint32_t last_print_ms = 0;
uint32_t last_mode_ms = 0;

bool manual_reverse = false;     // direction chosen with the ch7 switch
bool rev_prev_pressed = false;

LineReader reader;
CommandWatchdog watchdog;
ReverseGuard guard;
ModeSwitch mode;
BrakeState brake;

#define RC_STALE_MS 100          // no new pulse for this long = receiver gone
portMUX_TYPE rcMux = portMUX_INITIALIZER_UNLOCKED;
uint32_t mode_frames_seen = 0;

RcChannel rc_steer    = {PIN_RC_STEER};
RcChannel rc_throttle = {PIN_RC_THROTTLE};
RcChannel rc_lift     = {PIN_RC_LIFT};
RcChannel rc_reverse  = {PIN_RC_REVERSE};
RcChannel rc_mode = {PIN_RC_MODE};

void IRAM_ATTR rc_isr(void* arg) {
  RcChannel* c = (RcChannel*)arg;
  uint32_t t = micros();
  if (digitalRead(c->pin)) {
    c->rise_us = t;
  } else {
    uint32_t w = t - c->rise_us;
    if (w >= 500 && w <= 2500) {
      portENTER_CRITICAL_ISR(&rcMux);
      c->width_us = w;
      c->new_at_us = t;
      c->frames++;
      portEXIT_CRITICAL_ISR(&rcMux);
    }
  }
}

// Returns true and fills *width if the channel has a fresh pulse.
bool rc_read(RcChannel* c, int* width) {
  uint32_t w, at, frames;
  portENTER_CRITICAL(&rcMux);
  w = c->width_us; at = c->new_at_us; frames = c->frames;
  portEXIT_CRITICAL(&rcMux);
  uint32_t now_us = micros();                // clock AFTER the snapshot
  int32_t age = (int32_t)(now_us - at);
  if (frames == 0 || age > (int32_t)RC_STALE_MS * 1000) return false;
  *width = (int)w;
  return true;
}

// Linear map with clamping (no cliff: out-of-range pulses saturate, not zero).
int rc_clamp_map(int w, int in_min, int in_max, int out_min, int out_max) {
  if (in_min == in_max) return out_min;
  if (w < in_min) w = in_min;
  if (w > in_max) w = in_max;
  return out_min + (long)(w - in_min) * (out_max - out_min) / (in_max - in_min);
}

// Manual throttle from the RC pulse: pulled stick (low pulse) = more throttle.
// Clamps, so past-the-end pulses saturate instead of dropping to 0.
int manual_throttle_dac(int pulse_us) {
  int dac = rc_clamp_map(pulse_us, THROTTLE_PULSE_MIN, THROTTLE_PULSE_MAX, 255, 0);
  dac = (int)(dac * MANUAL_THROTTLE_SCALE);
  if (dac < 0) dac = 0;
  if (dac > 255) dac = 255;
  return dac;
}

void setup() {
  Serial.begin(115200);
  line_reader_init(&reader);
  watchdog_init(&watchdog);
  reverse_guard_init(&guard);

  mode_switch_init(&mode);
  pinMode(PIN_RC_MODE, INPUT);
  pinMode(PIN_RC_STEER, INPUT);
  pinMode(PIN_RC_THROTTLE, INPUT);
  pinMode(PIN_RC_LIFT, INPUT);
  pinMode(PIN_RC_REVERSE, INPUT);
  attachInterruptArg(digitalPinToInterrupt(PIN_RC_STEER),    rc_isr, &rc_steer,    CHANGE);
  attachInterruptArg(digitalPinToInterrupt(PIN_RC_THROTTLE), rc_isr, &rc_throttle, CHANGE);
  attachInterruptArg(digitalPinToInterrupt(PIN_RC_LIFT),     rc_isr, &rc_lift,     CHANGE);
  attachInterruptArg(digitalPinToInterrupt(PIN_RC_REVERSE),  rc_isr, &rc_reverse,  CHANGE);
  attachInterruptArg(digitalPinToInterrupt(PIN_RC_MODE), rc_isr, &rc_mode, CHANGE);

  brake_init(&brake);
#if ENABLE_OUTPUTS
  // UNTESTED ON HARDWARE. Brake H-bridge: enables high, both PWMs low = stopped.
  pinMode(PIN_BRAKE_L_EN, OUTPUT);  digitalWrite(PIN_BRAKE_L_EN, HIGH);
  pinMode(PIN_BRAKE_R_EN, OUTPUT);  digitalWrite(PIN_BRAKE_R_EN, HIGH);
  pinMode(PIN_BRAKE_LPWM, OUTPUT);  digitalWrite(PIN_BRAKE_LPWM, LOW);
  pinMode(PIN_BRAKE_RPWM, OUTPUT);  digitalWrite(PIN_BRAKE_RPWM, LOW);
  pinMode(PIN_BRAKE_LIMIT, INPUT_PULLUP);
  pinMode(PIN_REVERSE_RELAY, OUTPUT); digitalWrite(PIN_REVERSE_RELAY, LOW);
  dacWrite(PIN_DAC_THROTTLE, 0);
#endif
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

  // 1b. Mode switch
  int mode_w;
  if (!rc_read(&rc_mode, &mode_w)) {
    mode_switch_stale(&mode);               // no signal = manual
  } else if (rc_mode.frames != mode_frames_seen) {
    mode_frames_seen = rc_mode.frames;
    mode_switch_frame(&mode, mode_w);       // one call per NEW pulse
  }

  // 1c. Manual throttle (computed only; nothing drives a pin yet)
  int thr_w = 0;
  bool thr_ok = rc_read(&rc_throttle, &thr_w);
  int manual_dac = thr_ok ? manual_throttle_dac(thr_w) : 0;
  bool manual_brake = manual_brake_wanted(thr_ok, thr_w);
  if (manual_brake) manual_dac = 0;          // brake always beats throttle

  // 1d. Manual reverse: each NEW press of the ch7 switch flips the direction.
  int rev_w = 0;
  bool rev_ok = rc_read(&rc_reverse, &rev_w);
  bool rev_pressed = rev_ok && rev_w > REVERSE_PULSE_MIN;
  if (rev_pressed && !rev_prev_pressed) manual_reverse = !manual_reverse;
  rev_prev_pressed = rev_pressed;

  // 2. Choose the source, then run it through the ONE reverse guard.
  bool fresh = watchdog_fresh(&watchdog, now, CMD_TIMEOUT_MS);
  float speed = fresh ? cmd_speed : 0.0f;
  bool use_auto = mode.autonomous && fresh;
  DriveOutput want, safe;
  if (use_auto) {
    drive_from_speed(speed, &want);
  } else {
    want.throttle_dac = (uint8_t)manual_dac;
    want.reverse = manual_reverse;
    want.brake = manual_brake;
  }
  if (manual_brake) { want.throttle_dac = 0; want.brake = true; }   // brake always wins
  reverse_guard_step(&guard, &want, now, REVERSE_SETTLE_MS, &safe);

  // 3. Tell the Pi our state at a steady 10 Hz.
  if (now - last_mode_ms >= 100) {
    last_mode_ms = now;
    char out[40];
    size_t n = protocol_make_mode(mode.autonomous, false, false, safe.brake != 0,
                                  out, sizeof(out));
    if (n > 0) Serial.write((const uint8_t*)out, n);
  }

  // 4. The guard output is final.
  int final_dac = safe.throttle_dac;
  bool final_rev = safe.reverse;
  bool final_brake = safe.brake;

  // 5. Drive the outputs. UNTESTED ON HARDWARE.
#if ENABLE_OUTPUTS
  dacWrite(PIN_DAC_THROTTLE, final_dac);
  digitalWrite(PIN_REVERSE_RELAY, final_rev ? HIGH : LOW);
  bool limit_pressed = (digitalRead(PIN_BRAKE_LIMIT) == LOW);
  BrakeMotor bm = brake_step(&brake, final_brake, limit_pressed, now);
  digitalWrite(BRAKE_EXTEND_PIN,  bm == BRAKE_MOTOR_EXTEND  ? HIGH : LOW);
  digitalWrite(BRAKE_RETRACT_PIN, bm == BRAKE_MOTOR_RETRACT ? HIGH : LOW);
#endif

#if DEBUG_PRINT
  if (now - last_print_ms >= 200) {
    last_print_ms = now;
    Serial.printf("# fresh=%d speed=%.2f dac=%u rev=%d brake=%d settling=%d auto=%d\n manual=%d manual_dac mbrake=%d manual_brake",
                  fresh, speed, safe.throttle_dac, safe.reverse, safe.brake,
                  guard.settling, mode.autonomous);
  }
#endif
}
