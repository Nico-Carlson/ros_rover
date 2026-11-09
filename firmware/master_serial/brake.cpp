#include "brake.h"

void brake_init(BrakeState* b) {
    b->applied = false;
    b->retracting = false;
    b->retract_start_ms = 0;
}

BrakeMotor brake_step(BrakeState* b, bool want_brake, bool limit_pressed, uint32_t now_ms) {
    if (want_brake) {
        b->retracting = false;
        if (limit_pressed) {
            b->applied = true;
            return BRAKE_MOTOR_STOP;       // fully applied, hold
        }
        b->applied = false;
        return BRAKE_MOTOR_EXTEND;
    }
    // Release: retract for a fixed time, then stop.
    b->applied = false;
    if (!b->retracting) {
        b->retracting = true;
        b->retract_start_ms = now_ms;
    }
    if ((uint32_t)(now_ms - b->retract_start_ms) < BRAKE_RETRACT_MS)
        return BRAKE_MOTOR_RETRACT;
    return BRAKE_MOTOR_STOP;
}
