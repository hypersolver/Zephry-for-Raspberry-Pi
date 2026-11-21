#pragma once

#include <stdint.h>
#include <zephyr/kernel.h>

// Match Arduino's 32-bit millisecond counter and wraparound behavior.
inline uint32_t millis()
{
    return k_uptime_get_32();
}

inline void delay(uint32_t milliseconds)
{
    k_sleep(K_MSEC(milliseconds));
}

void setup();
void loop();
