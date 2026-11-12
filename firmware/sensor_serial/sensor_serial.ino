#include "protocol.h"
#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include "tilt.h"
#include "safety_link.h"

// ===================== FILL IN =====================
// Every uncertain value lives here. Check against the real wiring.
#define PIN_LIFT_LOWER_LIMIT 13   // pre-ROS pin
#define PIN_LIFT_UPPER_LIMIT 14   // pre-ROS pin (GPIO 14 pulses at boot, harmless here)
#define LIMIT_PRESSED_LEVEL  LOW  // UNVERIFIED: assumes INPUT_PULLUP, switch to ground
#define LIFT_SEND_MS         100  // PLACEHOLDER 10 Hz, matches SPEC.md

#define PIN_BATTERY_ADC   34      // pre-ROS pin (input-only, fine for ADC)
#define BAT_DIVIDER_SCALE (39.8f / 2.39f)   // PLACEHOLDER from the old firmware; verify with a multimeter
#define BAT_SEND_MS       1000    // 1 Hz, matches SPEC.md
#define BAT_SAMPLES       16      // average to smooth ADC noise

// MPU6050 mounting. UNKNOWN: the old firmware's offsets suggested the board is mounted flipped.
// Level should read roll ~0, pitch ~0. If it reads roll ~180, set MPU_FLIP_Z to -1.
#define MPU_SIGN_X  1.0f
#define MPU_SIGN_Y  1.0f
#define MPU_SIGN_Z  1.0f
#define MPU_DEAD_MS 350      // no good reading for this long = danger (a dead sensor is not "level")
#define MPU_READ_MS 20       // 50 Hz

// Safety link to the master: one-way, sensor TX -> master RX, plus a shared ground.
#define PIN_LINK_RX    16   // unused (one-way), but the pins have to be given
#define PIN_LINK_TX    17   // wire to the master's pin 16
#define LINK_BAUD      115200   // PLACEHOLDER: must match the master
#define SAFETY_SEND_MS 100      // 10 Hz
// ===================================================

// NOTE: Serial (UART0) is the Pi link. Never print debug text on it.
uint32_t last_lift_ms = 0;
uint32_t last_bat_ms = 0;

Adafruit_MPU6050 mpu;
TiltState tilt;
bool mpu_ok = false;
uint32_t last_mpu_ms = 0;
uint32_t last_good_mpu_ms = 0;
uint32_t last_mpu_try_ms = 0;

uint8_t safety_seq = 0;
uint32_t last_safety_ms = 0;

void setup() {
  Serial.begin(115200);
  Serial1.begin(LINK_BAUD, SERIAL_8N1, PIN_LINK_RX, PIN_LINK_TX);
  pinMode(PIN_LIFT_LOWER_LIMIT, INPUT_PULLUP);
  pinMode(PIN_LIFT_UPPER_LIMIT, INPUT_PULLUP);

  tilt_init(&tilt);
  mpu_ok = mpu.begin();            // ONCE, not every loop like the old firmware
  last_good_mpu_ms = millis();
}

void loop() {
  uint32_t now = millis();

  // LIFT: send the switch LEVELS continuously, not just on a press.
  if (now - last_lift_ms >= LIFT_SEND_MS) {
    last_lift_ms = now;
    bool down = (digitalRead(PIN_LIFT_LOWER_LIMIT) == LIMIT_PRESSED_LEVEL);
    bool up   = (digitalRead(PIN_LIFT_UPPER_LIMIT) == LIMIT_PRESSED_LEVEL);
    char out[40];
    size_t n = protocol_make_lift(down, up, out, sizeof(out));
    if (n > 0) Serial.write((const uint8_t*)out, n);
  }

  // BAT: average a few samples, convert to volts, send at 1 Hz.
  if (now - last_bat_ms >= BAT_SEND_MS) {
    last_bat_ms = now;
    uint32_t sum = 0;
    for (int i = 0; i < BAT_SAMPLES; i++) sum += analogReadMilliVolts(PIN_BATTERY_ADC);
    float pin_volts = (sum / (float)BAT_SAMPLES) / 1000.0f;
    float volts = pin_volts * BAT_DIVIDER_SCALE;
    char out[40];
    size_t n = protocol_make_bat(volts, out, sizeof(out));
    if (n > 0) Serial.write((const uint8_t*)out, n);
  }

  // Retry the MPU every second if it is not up (begin() resets the chip, so never per loop).
  if (!mpu_ok && now - last_mpu_try_ms >= 1000) {
    last_mpu_try_ms = now;
    mpu_ok = mpu.begin();
  }

  // Tilt: read at 50 Hz, update flags. A dead sensor means danger.
  if (mpu_ok && now - last_mpu_ms >= MPU_READ_MS) {
    last_mpu_ms = now;
    sensors_event_t a, g, temp;
    mpu.getEvent(&a, &g, &temp);
    float roll, pitch;
    if (tilt_angles(a.acceleration.x * MPU_SIGN_X, a.acceleration.y * MPU_SIGN_Y,
                    a.acceleration.z * MPU_SIGN_Z, &roll, &pitch)) {
      tilt_update(&tilt, roll, pitch);
      last_good_mpu_ms = now;
    }
  }
  bool sensor_dead = (uint32_t)(now - last_good_mpu_ms) > MPU_DEAD_MS;
  bool tilt_danger = tilt.danger || sensor_dead;
  bool tilt_warning = tilt.warning; 

  // Safety packet to the master at 10 Hz.
  if (now - last_safety_ms >= SAFETY_SEND_MS) {
    last_safety_ms = now;
    uint8_t pkt[SAFETY_PACKET_LEN];
    safety_make_packet(safety_seq++, tilt_danger, tilt_warning, pkt);
    Serial1.write(pkt, SAFETY_PACKET_LEN);
  }
}
