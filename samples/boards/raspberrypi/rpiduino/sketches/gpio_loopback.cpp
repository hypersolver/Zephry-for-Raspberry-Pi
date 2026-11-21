#include <rpiduino/sketch.hpp>
#include <zephyr/sys/printk.h>

namespace {
constexpr unsigned int outputPin = 17;
constexpr unsigned int inputPin = 27;
bool configured;
}

void setup()
{
    configured = pinMode(outputPin, OUTPUT) == 0 && pinMode(inputPin, INPUT_PULLDOWN) == 0;
    printk("RPiduino GPIO: GPIO17 -> GPIO27, setup=%s\n", configured ? "OK" : "FAIL");
}

void loop()
{
    if (configured) {
        digitalWrite(outputPin, LOW);
        delay(10);
        const int low = digitalRead(inputPin);
        digitalWrite(outputPin, HIGH);
        delay(10);
        const int high = digitalRead(inputPin);
        digitalWrite(outputPin, LOW);
        printk("RPiduino GPIO: low=%d high=%d %s\n", low, high,
               low == LOW && high == HIGH ? "PASS" : "FAIL");
    }
    delay(1000);
}
