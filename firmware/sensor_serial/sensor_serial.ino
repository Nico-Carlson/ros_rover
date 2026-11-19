#include "protocol.h"
#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <TinyGPSPlus.h>
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
#define MPU_SIGN_Y  -1.0f
#define MPU_SIGN_Z  -1.0f
#define MPU_DEAD_MS 350      // no good reading for this long = danger (a dead sensor is not "level")
#define MPU_READ_MS 20       // 50 Hz
#define GYRO_CAL_SAMPLES 200   // PLACEHOLDER: about 2 s at 10 ms each; rover must be STILL at power-up
#define MPU_ROLL_OFFSET_DEG  13.0f   // set to the roll reading at rest after step 1
#define MPU_PITCH_OFFSET_DEG -1.0f   // set to the pitch reading at rest after step 1

// Safety link to the master: one-way, sensor TX -> master RX, plus a shared ground.
#define PIN_LINK_RX    16   // unused (one-way), but the pins have to be given
#define PIN_LINK_TX    17   // wire to the master's pin 16
#define LINK_BAUD      115200   // PLACEHOLDER: must match the master
#define SAFETY_SEND_MS 100      // 10 Hz

#define PIN_GPS_RX  36     // GPS module TX -> ESP RX. Input-only pin is fine for RX. UNVERIFIED wiring
#define PIN_GPS_TX  33     // PLACEHOLDER: only needed to configure the GPS, we never send to it
#define GPS_BAUD    9600   // NEO-6M default (from memory, UNVERIFIED)
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
float gyro_bias_x = 0, gyro_bias_y = 0, gyro_bias_z = 0;   // rad/s, measured at boot

TinyGPSPlus gps;
TinyGPSCustom gga_quality(gps, "GPGGA", 6);   // fires once per GGA sentence, so once per fix

uint8_t safety_seq = 0;
uint32_t last_safety_ms = 0;

void calibrate_gyro() {
  float sx = 0, sy = 0, sz = 0;
  for (int i = 0; i < GYRO_CAL_SAMPLES; i++) {
    sensors_event_t a, g, temp;
    mpu.getEvent(&a, &g, &temp);
    sx += g.gyro.x * MPU_SIGN_X;
    sy += g.gyro.y * MPU_SIGN_Y;
    sz += g.gyro.z * MPU_SIGN_Z;
    delay(10);
  }
  gyro_bias_x = sx / GYRO_CAL_SAMPLES;
  gyro_bias_y = sy / GYRO_CAL_SAMPLES;
  gyro_bias_z = sz / GYRO_CAL_SAMPLES;
}

void setup() {
  Serial.begin(115200);
  Serial1.begin(LINK_BAUD, SERIAL_8N1, PIN_LINK_RX, PIN_LINK_TX);
  Serial2.begin(GPS_BAUD, SERIAL_8N1, PIN_GPS_RX, PIN_GPS_TX);
  pinMode(PIN_LIFT_LOWER_LIMIT, INPUT_PULLUP);
  pinMode(PIN_LIFT_UPPER_LIMIT, INPUT_PULLUP);

  tilt_init(&tilt);
  mpu_ok = mpu.begin();            // ONCE, not every loop like the old firmware

  if (mpu_ok) calibrate_gyro();

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
      roll  -= MPU_ROLL_OFFSET_DEG;
      pitch -= MPU_PITCH_OFFSET_DEG;
      tilt_update(&tilt, roll, pitch);
      last_good_mpu_ms = now;
    }

    float ax = a.acceleration.x * MPU_SIGN_X;
    float ay = a.acceleration.y * MPU_SIGN_Y;
    float az = a.acceleration.z * MPU_SIGN_Z;
    float gx = g.gyro.x * MPU_SIGN_X - gyro_bias_x;
    float gy = g.gyro.y * MPU_SIGN_Y - gyro_bias_y;
    float gz = g.gyro.z * MPU_SIGN_Z - gyro_bias_z;
    char imu_out[80];
    size_t imu_n = protocol_make_imu(ax, ay, az, gx, gy, gz, imu_out, sizeof(imu_out));
    if (imu_n > 0) Serial.write((const uint8_t*)imu_out, imu_n);
  }
  bool sensor_dead = (uint32_t)(now - last_good_mpu_ms) > MPU_DEAD_MS;
  bool tilt_danger = tilt.danger || sensor_dead;
  bool tilt_warning = tilt.warning; 

  // Safety packet to the master at 10 Hz.
  if (now - last_safety_ms >= SAFETY_SEND_MS) {
    last_safety_ms = now;
    bool lift_down = (digitalRead(PIN_LIFT_LOWER_LIMIT) == LIMIT_PRESSED_LEVEL);
    uint8_t pkt[SAFETY_PACKET_LEN];
    safety_make_packet(safety_seq++, tilt_danger, tilt_warning, pkt, !lift_down);
    Serial1.write(pkt, SAFETY_PACKET_LEN);
  }

  // GPS: feed NMEA bytes to the parser, send one line per new GGA sentence.
  while (Serial2.available()) gps.encode(Serial2.read());
  if (gga_quality.isUpdated()) {
    gga_quality.value();   // reading it clears the flag; without this we would send every loop
    bool fix = gps.location.isValid();
    int sats = gps.satellites.isValid() ? (int)gps.satellites.value() : 0;
    double lat = fix ? gps.location.lat() : 0.0;
    double lon = fix ? gps.location.lng() : 0.0;
    float alt = (fix && gps.altitude.isValid()) ? (float)gps.altitude.meters() : 0.0f;
    float hdop = gps.hdop.isValid() ? (float)gps.hdop.hdop() : 99.0f;
    char gps_out[96];
    size_t gps_n = protocol_make_gps(fix, sats, lat, lon, alt, hdop, gps_out, sizeof(gps_out));
    if (gps_n > 0) Serial.write((const uint8_t*)gps_out, gps_n);
  }

  // TEMP lift limit debug: delete after the bench check
  // static uint32_t last_dbg_ms = 0;
  // if (now - last_dbg_ms >= 200) {
  //   last_dbg_ms = now;
  //   Serial.printf("# TEMP raw13=%d raw14=%d down=%d up=%d\n",
  //                 digitalRead(PIN_LIFT_LOWER_LIMIT), digitalRead(PIN_LIFT_UPPER_LIMIT),
  //                 digitalRead(PIN_LIFT_LOWER_LIMIT) == LIMIT_PRESSED_LEVEL,
  //                 digitalRead(PIN_LIFT_UPPER_LIMIT) == LIMIT_PRESSED_LEVEL);
  // }
  // END TEMP

}
