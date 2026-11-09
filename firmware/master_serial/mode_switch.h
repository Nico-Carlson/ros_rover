#pragma once

// Numbers carried over from the old ROS firmware. The transmitter channel is still unknown.
#define MODE_MANUAL_MAX_US 1400   // at or below this pulse = manual
#define MODE_AUTO_MIN_US   1600   // at or above this pulse = autonomous
#define MODE_CONFIRM_FRAMES 3     // fresh readings in a row needed to change mode

struct ModeSwitch {
    bool autonomous;   // the decided mode
    int  count;        // consecutive frames asking for the other mode
};

// Starts in manual.
void mode_switch_init(ModeSwitch* m);

// Call once for each NEW pulse measured from the receiver.
void mode_switch_frame(ModeSwitch* m, int pulse_us);

// Call when the RC signal is stale or missing: forces manual at once.
void mode_switch_stale(ModeSwitch* m);
