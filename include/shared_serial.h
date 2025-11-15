#pragma once
#include <Arduino.h>
#include <relay_pins.h>

// NOTE: This header belonged to a legacy, bit-banged SharedSerial implementation
// that has been superseded by the RelaySerial library in lib/RelaySerial.
// The implementation is retained for reference in src/shared_serial.cpp but
// is disabled to avoid accidental usage. Do not include this header in new code.

#if 0
namespace SharedSerial {
    // Initialize pins and timing
    void begin(uint32_t baud = RELAY_SERIAL_BAUD);

    // Send a byte (blocking)
    void write(uint8_t b);

    // Send a string (blocking)
    void print(const char* s);
    void println(const char* s);
    void println(const String &s);

    // Check if data available (non-blocking)
    bool available();

    // Read a byte if available (non-blocking)
    int read();

    // Acquire/release pins for MAX6675 use
    void acquireForMax6675();
    void releaseFromMax6675();
}
#endif

// Software serial implementation that can share pins with MAX6675
// Uses the CS pin as an enable line to switch between MAX6675 and serial modes
namespace SharedSerial
{
    // Initialize pins and timing
    void begin(uint32_t baud = RELAY_SERIAL_BAUD);

    // Send a byte (blocking)
    void write(uint8_t b);

    // Send a string (blocking)
    void print(const char *s);
    void println(const char *s);
    void println(const String &s);

    // Check if data available (non-blocking)
    bool available();

    // Read a byte if available (non-blocking)
    int read();

    // Acquire/release pins for MAX6675 use
    void acquireForMax6675();
    void releaseFromMax6675();
}