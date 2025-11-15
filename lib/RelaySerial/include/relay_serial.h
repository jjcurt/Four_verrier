#pragma once
#include <Arduino.h>
#include "relay_pins.h"

namespace RelaySerial {
    void begin(unsigned long baud = RELAY_SERIAL_BAUD);
    void loop();
    void setRelay(uint8_t relay, bool state);
    // Set a PWM value (0-1023) for a relay capable of PWM control
    // Accepts 0..1023 (higher resolution). Backwards-compatible with 0..255 inputs.
    void setRelayPWM(uint8_t relay, uint16_t pwm);
    void requestState(uint8_t relay);
    uint8_t getRelayState(uint8_t relay);
}
