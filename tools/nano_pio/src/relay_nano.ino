// Single canonical Nano firmware for burst-fire control of zero-cross SSRs.

#define USE_ZC 0          // 1 to use a hardware zero-cross input, 0 to use software timing
#define ENABLE_WATCHDOG 0 // 1 to enable a watchdog that turns relays off after timeout

#include <Arduino.h>

// Pin definitions
const int RELAY1_PIN = 9;  // SSR1 control (ON/OFF only)
const int RELAY2_PIN = 10; // SSR2 control (burst-fire PWM)
const int ZC_PIN = 2;      // Optional zero-cross detector input (INT0)

// SSR state tracking
volatile int relayState1 = 0; // SSR1 state (0=OFF, 1=ON)
volatile int relayState2 = 0; // SSR2 state (0=OFF, 1=ON)
volatile int relayPWM2 = 0;   // SSR2 power level (0..1023)

// Burst-fire control for SSR2 (timed window or ZC hardware)
volatile uint8_t windowCycles = 50; // Default: 50 cycles = 1 second window @50Hz
volatile uint8_t cycleIndex = 0;    // Current cycle in window (0..windowCycles-1)
volatile uint8_t cyclesOn = 0;      // Number of ON cycles in window
unsigned long lastCycleMs = 0;
volatile uint16_t cycleMs = 20; // Default: 20ms per cycle (50Hz)

// Safety watchdog
unsigned long lastCommandMs = 0;
const unsigned long WATCHDOG_TIMEOUT_MS = 5000; // 5 second timeout

// Runtime watchdog flag (can be toggled with serial D0/D1). Default follows compile-time macro.
bool watchdogEnabled = (ENABLE_WATCHDOG != 0);

// Command buffer
String buf;

// Optional ISR forward declaration (only used if USE_ZC==1)
#if USE_ZC
void onZeroCross();
#endif

void setup()
{
    // Configure pins
    pinMode(RELAY1_PIN, OUTPUT);
    pinMode(RELAY2_PIN, OUTPUT);
    digitalWrite(RELAY1_PIN, LOW);
    digitalWrite(RELAY2_PIN, LOW);

    // Initialize serial at 115200 baud to match hardware UART on ESP32
    // ESP32 uses UART0 (GPIO1/GPIO3, P1 connector) at 115200 for relay communication
    Serial.begin(115200);
    buf.reserve(64);

    // Initialize timers
    lastCommandMs = millis();
    lastCycleMs = millis();

#if USE_ZC
    // Attach zero-cross interrupt if hardware is available
    attachInterrupt(digitalPinToInterrupt(ZC_PIN), onZeroCross, RISING);
#endif
}

void checkWatchdog()
{
    if (watchdogEnabled)
    {
        if (millis() - lastCommandMs > WATCHDOG_TIMEOUT_MS)
        {
            digitalWrite(RELAY1_PIN, LOW);
            digitalWrite(RELAY2_PIN, LOW);
            relayState1 = 0;
            relayState2 = 0;
            cyclesOn = 0;
        }
    }
    else
    {
        (void)lastCommandMs;
    }
}

#if USE_ZC
void onZeroCross()
{
    // Called at each mains zero crossing; step the burst-fire window
    if (cycleIndex >= windowCycles)
        cycleIndex = 0;
    cyclesOn = map(relayPWM2, 0, 1023, 0, windowCycles);
    if (relayState2 && cycleIndex < cyclesOn)
        digitalWrite(RELAY2_PIN, HIGH);
    else
        digitalWrite(RELAY2_PIN, LOW);
    cycleIndex++;
}
#endif

void applyState(int relay, int val)
{
    lastCommandMs = millis();
    if (relay == 1)
    {
        relayState1 = val ? 1 : 0;
        digitalWrite(RELAY1_PIN, val ? HIGH : LOW);
    }
    else if (relay == 2)
    {
        relayState2 = val ? 1 : 0;
        if (!val)
        {
            digitalWrite(RELAY2_PIN, LOW);
            cyclesOn = 0;
        }
    }
}

void applyPWM(int relay, int pwm)
{
    lastCommandMs = millis();
    if (pwm < 0)
        pwm = 0;
    if (pwm > 1023)
        pwm = 1023;
    if (relay == 1)
        return;
    relayPWM2 = pwm;

    // IMPORTANT: Set relayState2 based on PWM value
    // relayState2 must be 1 for the burst-fire loop to output pulses
    if (pwm > 0)
    {
        relayState2 = 1;
    }
    else
    {
        relayState2 = 0;
        digitalWrite(RELAY2_PIN, LOW); // Ensure pin is LOW when PWM is 0
    }
}

void sendState(int relay)
{
    if (relay == 1)
    {
        Serial.print("K1,");
        Serial.print(relayState1 ? "1" : "0");
        Serial.print("\n");
    }
    else
    {
        Serial.print("K2,");
        Serial.print(relayState2 ? "1" : "0");
        Serial.print("\n");
    }
}

void parseLine(String &line)
{
    line.trim();
    if (line.length() == 0)
        return;
    char cmd = line.charAt(0);
    if (cmd == 'S' || cmd == 'P')
    {
        int comma = line.indexOf(',');
        if (comma > 1)
        {
            int n = line.substring(1, comma).toInt();
            if (n >= 1 && n <= 2)
            {
                if (cmd == 'S')
                {
                    int v = line.substring(comma + 1).toInt();
                    applyState(n, v != 0);
                    sendState(n);
                }
                else
                {
                    int pwm = line.substring(comma + 1).toInt();
                    if (pwm >= 0 && pwm <= 1023)
                    {
                        applyPWM(n, pwm);
                        sendState(n);
                    }
                }
            }
        }
    }
    else if (cmd == 'R')
    {
        int n = line.substring(1).toInt();
        if (n >= 1 && n <= 2)
            sendState(n);
    }
    else if (cmd == 'L')
    {
        lastCommandMs = millis(); // Reset watchdog timer
        int n = line.substring(1).toInt();
        if (n == 2)
        {
            int pct = (relayPWM2 * 100L) / 1023; // Use long for calculation
            Serial.print("L");
            Serial.print(n);
            Serial.print(",");
            Serial.print(pct);
            Serial.print("\n");
        }
    }
    else if (cmd == 'W')
    {
        int n = line.substring(1).toInt();
        if (n >= 1 && n <= 200)
        {
            noInterrupts();
            windowCycles = (uint8_t)n;
            if (cycleIndex >= windowCycles)
                cycleIndex = 0;
            interrupts();
            Serial.print("W,OK\n");
        }
    }
    else if (cmd == 'T')
    {
        int ms = line.substring(1).toInt();
        if (ms >= 5 && ms <= 1000)
        {
            noInterrupts();
            cycleMs = (uint16_t)ms;
            lastCycleMs = millis();
            interrupts();
            Serial.print("T,OK\n");
        }
    }
    else if (cmd == 'D')
    {
        int v = line.substring(1).toInt();
        watchdogEnabled = (v != 0);
        Serial.print("D,OK\n");
    }
}

void loop()
{
    checkWatchdog();
    while (Serial.available())
    {
        char c = Serial.read();
        if (c == '\r')
            continue;
        if (c == '\n')
        {
            if (buf.length() > 0)
            {
                parseLine(buf);
                buf = "";
            }
        }
        else
        {
            buf += c;
            if (buf.length() > 200)
                buf = buf.substring(buf.length() - 200);
        }
    }
#if !USE_ZC
    unsigned long now = millis();
    if (now - lastCycleMs >= cycleMs)
    {
        lastCycleMs += cycleMs;
        cyclesOn = map(relayPWM2, 0, 1023, 0, windowCycles);
        if (relayState2 && cycleIndex < cyclesOn)
            digitalWrite(RELAY2_PIN, HIGH);
        else
            digitalWrite(RELAY2_PIN, LOW);
        cycleIndex++;
        if (cycleIndex >= windowCycles)
            cycleIndex = 0;
    }
#endif
}
