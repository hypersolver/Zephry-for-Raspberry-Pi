#include <rpiduino/sketch.hpp>
#include <zephyr/sys/printk.h>

void setup()
{
    printk("RPiduino Zephyr: setup complete\n");
}

void loop()
{
    printk("RPiduino Zephyr: uptime=%u ms\n", static_cast<unsigned int>(millis()));
    delay(1000);
}
