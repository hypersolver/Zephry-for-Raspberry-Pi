#include <rpiduino/sketch.hpp>
#include <zephyr/sys/printk.h>
#ifdef CONFIG_SOC_BCM2837
#include <rpiduino/uart_control.hpp>
#endif

void setup()
{
    printk("RPiduino Zephyr: setup complete\n");
}

void loop()
{
    printk("RPiduino Zephyr: uptime=%u ms\n", static_cast<unsigned int>(millis()));
#ifdef CONFIG_SOC_BCM2837
    const uint32_t start = millis();
    do {
        pollUartControl();
        k_sleep(K_TICKS(1));
    } while (millis() - start < 1000);
#else
    delay(1000);
#endif
}
