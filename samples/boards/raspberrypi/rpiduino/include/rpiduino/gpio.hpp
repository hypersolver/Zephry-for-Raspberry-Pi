#pragma once

enum PinMode { INPUT, OUTPUT, INPUT_PULLUP, INPUT_PULLDOWN };
enum PinValue { LOW, HIGH };

// Use BCM GPIO numbers, not physical header pin numbers.
// Return 0 on success, or a negative errno for an invalid pin or argument.
int pinMode(unsigned int pin, PinMode mode);
int digitalWrite(unsigned int pin, PinValue value);

// Return LOW, HIGH, or a negative errno.
int digitalRead(unsigned int pin);
