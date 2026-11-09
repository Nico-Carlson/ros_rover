#pragma once
#include <stdint.h>

// PLACEHOLDERS, never measured on the real rover.
#define DRIVE_FULL_SCALE_MPS 1.5f   // speed that maps to full throttle (DAC 255)
#define DRIVE_STOP_BAND_MPS  0.05f  // at or below this speed = stop (brake on)

struct DriveOutput {
    uint8_t throttle_dac;  // 0 to 255
    bool reverse;          // reverse relay on
    bool brake;            // brake applied
};

// Turns a speed command (m/s, negative = reverse) into the three outputs.
// One function decides all three, so they can never contradict each other.
void drive_from_speed(float speed_mps, DriveOutput* out);
