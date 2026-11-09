#include "drive.h"
#include <math.h>

void drive_from_speed(float speed_mps, DriveOutput* out) {
    float mag = fabsf(speed_mps);

    // "Not clearly moving" includes NaN, because any comparison with NaN is false.
    if (!(mag > DRIVE_STOP_BAND_MPS)) {
        out->throttle_dac = 0;
        out->reverse = false;
        out->brake = true;
        return;
    }

    if (mag > DRIVE_FULL_SCALE_MPS) mag = DRIVE_FULL_SCALE_MPS;  // clamp

    out->throttle_dac = (uint8_t)(mag / DRIVE_FULL_SCALE_MPS * 255.0f + 0.5f);  // +0.5 rounds
    out->reverse = speed_mps < 0;
    out->brake = false;
}
