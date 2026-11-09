#include "tilt.h"
#include <math.h>

void tilt_init(TiltState* t) { t->warning = false; t->danger = false; }

bool tilt_angles(float ax, float ay, float az, float* roll_deg, float* pitch_deg) {
    if (!isfinite(ax) || !isfinite(ay) || !isfinite(az)) return false;
    float mag = sqrtf(ax * ax + ay * ay + az * az);
    if (mag < 1e-3f) return false;
    const float k = 180.0f / 3.14159265f;
    *roll_deg  = atan2f(ay, az) * k;
    *pitch_deg = atan2f(-ax, sqrtf(ay * ay + az * az)) * k;
    return true;
}

static bool past(float v, float limit, bool already) {
    float a = fabsf(v);
    return already ? (a > limit - TILT_HYST_DEG) : (a >= limit);
}

void tilt_update(TiltState* t, float roll, float pitch) {
    t->warning = past(roll, TILT_WARN_ROLL_DEG, t->warning) || past(pitch, TILT_WARN_PITCH_DEG, t->warning);
    t->danger  = past(roll, TILT_DANGER_DEG, t->danger)     || past(pitch, TILT_DANGER_DEG, t->danger);
}
