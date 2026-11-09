#pragma once
#include <stdint.h>

struct RcChannel {
  int pin;
  volatile uint32_t rise_us;
  volatile uint32_t width_us;
  volatile uint32_t new_at_us;
  volatile uint32_t frames;
};
