#include "protocol.h"

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
// ===================================================

// NOTE: Serial (UART0) is the Pi link. Never print debug text on it.
uint32_t last_lift_ms = 0;
uint32_t last_bat_ms = 0;

void setup() {
  Serial.begin(115200);
  pinMode(PIN_LIFT_LOWER_LIMIT, INPUT_PULLUP);
  pinMode(PIN_LIFT_UPPER_LIMIT, INPUT_PULLUP);
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
}
