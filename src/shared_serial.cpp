// NOTE: This legacy SharedSerial bit-banged implementation is kept for reference
// but is not used in the current project. The RelaySerial library provides a
// dedicated implementation and is the active code path. To avoid duplicate
// functionality and accidental use, the implementation below is disabled.

#if 0
#include "shared_serial.h"
#include "pins.h"

namespace SharedSerial {
    static uint32_t _bitTime;  // Microseconds per bit
    static bool _acquired = false;  // True when MAX6675 has control

    void begin(uint32_t baud) {
        _bitTime = 1000000 / baud;
        pinMode(SOFT_SERIAL_TX, OUTPUT);
        pinMode(SOFT_SERIAL_RX, INPUT);
        pinMode(SOFT_SERIAL_EN, OUTPUT);
        digitalWrite(SOFT_SERIAL_EN, LOW);  // Start in serial mode
        digitalWrite(SOFT_SERIAL_TX, HIGH); // Idle state
    }

    void write(uint8_t b) {
        if (_acquired) return;  // Don't transmit during MAX6675 reads

        noInterrupts();
        // Start bit
        digitalWrite(SOFT_SERIAL_TX, LOW);
        delayMicroseconds(_bitTime);

        // Data bits
        for (uint8_t i = 0; i < 8; i++) {
            digitalWrite(SOFT_SERIAL_TX, (b & 1) ? HIGH : LOW);
            delayMicroseconds(_bitTime);
            b >>= 1;
        }

        // Stop bit
        digitalWrite(SOFT_SERIAL_TX, HIGH);
        delayMicroseconds(_bitTime);
        interrupts();
    }

    void print(const char* s) {
        while (*s) write(*s++);
    }

    void println(const char* s) {
        print(s);
        write('\r');
        write('\n');
    }

    void println(const String &s) {
        println(s.c_str());
    }

    bool available() {
        return !_acquired && (digitalRead(SOFT_SERIAL_RX) == LOW);  // Start bit detected
    }

    int read() {
        if (_acquired) return -1;
        if (!available()) return -1;

        uint8_t b = 0;
        noInterrupts();
        
        // Wait for middle of start bit
        delayMicroseconds(_bitTime / 2);
        
        // Read 8 data bits
        for (uint8_t i = 0; i < 8; i++) {
            delayMicroseconds(_bitTime);
            if (digitalRead(SOFT_SERIAL_RX))
                b |= (1 << i);
        }

        // Wait for stop bit
        delayMicroseconds(_bitTime);
        interrupts();
        return b;
    }

    void acquireForMax6675() {
        _acquired = true;
        digitalWrite(SOFT_SERIAL_EN, HIGH);  // Enable MAX6675 CS
        delayMicroseconds(100);  // Let lines settle
    }

    void releaseFromMax6675() {
        digitalWrite(SOFT_SERIAL_EN, LOW);   // Back to serial mode
        _acquired = false;
        delayMicroseconds(100);  // Let lines settle
    }
}
#endif