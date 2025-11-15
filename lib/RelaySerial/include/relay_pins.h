#pragma once
// Shared pin macros used by the RelaySerial library and the main project
// These were moved here so the library can be self-contained.

// Hardware Serial configuration (P1 connector on ESP32-2432S028R)
// TX = GPIO1 (P1 connector TX pin)
// RX = GPIO3 (P1 connector RX pin)
// NOTE: This uses UART0 (Serial) - debug logs must be disabled in production!
//       Use conditional compilation (#ifdef DEBUG) to separate debug from production.

#ifndef RELAY_SERIAL_TX
#define RELAY_SERIAL_TX 1  // GPIO1 - Hardware UART0 TX (P1 connector)
#endif

#ifndef RELAY_SERIAL_RX
#define RELAY_SERIAL_RX 3  // GPIO3 - Hardware UART0 RX (P1 connector)
#endif

// Default baud for hardware serial used to talk to the Nano
// Can be higher than SoftwareSerial (115200 recommended for production)
#ifndef RELAY_SERIAL_BAUD
#define RELAY_SERIAL_BAUD 115200
#endif

// Use hardware Serial instead of SoftwareSerial
#ifndef USE_HARDWARE_SERIAL
#define USE_HARDWARE_SERIAL 1
#endif
