#pragma once
#include <Arduino.h>

// Simple protocol for relay control over software serial
// Commands:
// S1,1 - Set relay 1 on
// S1,0 - Set relay 1 off
// S2,1 - Set relay 2 on
// S2,0 - Set relay 2 off
// R1   - Request relay 1 state
// R2   - Request relay 2 state
// Response:
// K1,1 - Relay 1 is on
// K1,0 - Relay 1 is off
// K2,1 - Relay 2 is on
// K2,0 - Relay 2 off

namespace RelaySerial {
    void begin(unsigned long baud);
    void loop();
    
    // Control interface
    void setRelay(uint8_t relay, bool state);
    // Set PWM value for a relay. Accepts 0..1023 for higher resolution PWM.
    void setRelayPWM(uint8_t relay, uint16_t pwm);  // PWM value 0-1023
    void requestState(uint8_t relay);
    uint8_t getRelayState(uint8_t relay);
}