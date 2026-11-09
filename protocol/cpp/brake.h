#pragma once
#include <stdint.h>

#define BRAKE_RETRACT_MS 2000   // PLACEHOLDER: old firmware's fixed retract time

enum BrakeMotor { BRAKE_MOTOR_STOP = 0, BRAKE_MOTOR_EXTEND = 1, BRAKE_MOTOR_RETRACT = 2 };

struct BrakeState {
    bool applied;          // true once the limit switch has been hit
    bool retracting;
    uint32_t retract_start_ms;
};

void brake_init(BrakeState* b);

// want_brake: true = apply. limit_pressed: switch level (true = pressed).
// Returns what the actuator should do now.
BrakeMotor brake_step(BrakeState* b, bool want_brake, bool limit_pressed, uint32_t now_ms);
