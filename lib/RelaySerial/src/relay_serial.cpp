#include "relay_serial.h"
#include "relay_pins.h"  // library-local shared pin definitions

#if USE_HARDWARE_SERIAL
// Use hardware Serial (UART0) for relay communication
// NOTE: This means Serial.print() debug logs must be disabled in production!
#define relaySerial Serial
#else
#include "SoftwareSerial.h"
// Software serial instance (legacy, for boards with dedicated GPIOs)
static SoftwareSerial relaySerialSoft(RELAY_SERIAL_RX, RELAY_SERIAL_TX);
#define relaySerial relaySerialSoft
#endif

namespace RelaySerial {
    // State tracking
    static uint8_t relayStates[2] = {0, 0};  // 0=unknown, 1=off, 2=on
    static char rxBuffer[16];
    static uint8_t rxIndex = 0;
    
    void begin(unsigned long baud) {
#if !USE_HARDWARE_SERIAL
        // SoftwareSerial: configure enable pin if used
        #ifdef SOFT_SERIAL_EN
        pinMode(SOFT_SERIAL_EN, OUTPUT);
        digitalWrite(SOFT_SERIAL_EN, LOW);  // Enable serial mode
        #endif
#endif
        relaySerial.begin(baud);
    }
    
    void loop() {
        while (relaySerial.available()) {
            char c = relaySerial.read();
            if (c == '\n' || c == '\r') {
                if (rxIndex > 0) {
                    rxBuffer[rxIndex] = 0;  // null terminate
                    // Parse response: K1,1 or K2,0 etc.
                    if (rxBuffer[0] == 'K' && rxIndex >= 3) {
                        uint8_t relay = rxBuffer[1] - '0';
                        if (relay >= 1 && relay <= 2) {
                            relayStates[relay-1] = (rxBuffer[3] == '1') ? 2 : 1;
                        }
                    }
                    rxIndex = 0;
                }
            } else if (rxIndex < sizeof(rxBuffer)-1) {
                rxBuffer[rxIndex++] = c;
            }
        }
    }
    
    void setRelay(uint8_t relay, bool state) {
        if (relay < 1 || relay > 2) return;
        char cmd[8];
        snprintf(cmd, sizeof(cmd), "S%d,%d\n", relay, state ? 1 : 0);
        relaySerial.write(cmd);
        relayStates[relay-1] = 0; // unknown until confirmed
    }

    void setRelayPWM(uint8_t relay, uint16_t pwm) {
        if (relay < 1 || relay > 2) return;
        // Clamp to accepted range (0..1023)
        if (pwm > 1023) pwm = 1023;
        // Send a PWM command in the form: P<relay>,<value>\n  (value 0-1023)
        char cmd[20];
        snprintf(cmd, sizeof(cmd), "P%u,%u\n", (unsigned)relay, (unsigned)pwm);
        relaySerial.write(cmd);
        // Track non-zero pwm as "on" for basic state reporting
        relayStates[relay-1] = (pwm == 0) ? 1 : 2;
    }
    
    void requestState(uint8_t relay) {
        if (relay < 1 || relay > 2) return;
        relaySerial.print("R");
        relaySerial.print(relay);
        relaySerial.print("\n");
    }
    
    uint8_t getRelayState(uint8_t relay) {
        if (relay < 1 || relay > 2) return 0;
        return relayStates[relay-1];
    }
}