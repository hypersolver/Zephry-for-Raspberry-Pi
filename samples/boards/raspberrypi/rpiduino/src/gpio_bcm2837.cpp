#include <rpiduino/gpio.hpp>

#include <errno.h>
#include <zephyr/devicetree.h>
#include <zephyr/irq.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/sys_io.h>

namespace {
constexpr uintptr_t gpioBase = DT_REG_ADDR(DT_NODELABEL(gpio));
constexpr uintptr_t gpioSet = gpioBase + 0x1c;
constexpr uintptr_t gpioClear = gpioBase + 0x28;
constexpr uintptr_t gpioLevel = gpioBase + 0x34;
constexpr uintptr_t pullControl = gpioBase + 0x94;
constexpr uintptr_t pullClock = gpioBase + 0x98;

bool isHeaderGpio(unsigned int pin)
{
    return pin >= 2 && pin <= 27 && pin != 14 && pin != 15;
}
}

int pinMode(unsigned int pin, PinMode mode)
{
    if (!isHeaderGpio(pin)) {
        return -EINVAL;
    }

    uint32_t pull;
    switch (mode) {
    case INPUT:
    case OUTPUT:
        pull = 0;
        break;
    case INPUT_PULLDOWN:
        pull = 1;
        break;
    case INPUT_PULLUP:
        pull = 2;
        break;
    default:
        return -EINVAL;
    }

    const uintptr_t functionSelect = gpioBase + 4U * (pin / 10U);
    const unsigned int shift = 3U * (pin % 10U);
    const unsigned int key = irq_lock();
    const uint32_t function = mode == OUTPUT ? 1U : 0U;
    const uint32_t previous = sys_read32(functionSelect);
    if (mode == OUTPUT) {
        sys_write32(BIT(pin), gpioClear);
    }
    sys_write32((previous & ~(7U << shift)) | (function << shift), functionSelect);

    // BCM2837 latches the pull setting with the legacy GPPUD/GPPUDCLK sequence.
    sys_write32(pull, pullControl);
    k_busy_wait(5);
    sys_write32(BIT(pin), pullClock);
    k_busy_wait(5);
    sys_write32(0, pullControl);
    sys_write32(0, pullClock);
    irq_unlock(key);
    return 0;
}

int digitalWrite(unsigned int pin, PinValue value)
{
    if (!isHeaderGpio(pin) || (value != LOW && value != HIGH)) {
        return -EINVAL;
    }
    sys_write32(BIT(pin), value == HIGH ? gpioSet : gpioClear);
    return 0;
}

int digitalRead(unsigned int pin)
{
    if (!isHeaderGpio(pin)) {
        return -EINVAL;
    }
    return (sys_read32(gpioLevel) & BIT(pin)) != 0 ? HIGH : LOW;
}
