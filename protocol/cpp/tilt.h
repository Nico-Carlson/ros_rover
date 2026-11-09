#pragma once

// PLACEHOLDERS. Warning = old firmware's 15/20 deg. Danger = flip-over only (decision 2026-11-09).
#define TILT_WARN_ROLL_DEG   15.0f
#define TILT_WARN_PITCH_DEG  20.0f
#define TILT_DANGER_DEG      45.0f   // either axis past this = danger
#define TILT_HYST_DEG         2.0f   // must fall this far below a threshold to clear

struct TiltState {
    bool warning;
    bool danger;
};

void tilt_init(TiltState* t);

// ax, ay, az in m/s^2 (or any unit, only ratios matter). Mounting: roll from ay/az, pitch from ax.
// Writes roll and pitch in degrees. Returns false if the input is not finite or has no gravity.
bool tilt_angles(float ax, float ay, float az, float* roll_deg, float* pitch_deg);

// Updates the flags with hysteresis.
void tilt_update(TiltState* t, float roll_deg, float pitch_deg);
