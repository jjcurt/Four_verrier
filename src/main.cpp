/*
 * Glass Furnace Controller for ESP32-2432S028R
 * Features:
 * - MAX6675 temperature sensor (CS=27, SO=35, SCK=22)
 * - Dual SSR control for heating elements via Arduino Nano (UART0: GPIO1/GPIO3)
 * - SD card support (CS=5) for program storage and logging
 * - Web interface for remote control
 * - 2.8" TFT display with touch (CYD - Cheap Yellow Display)
 *
 * IMPORTANT: Hardware Serial (UART0) is used for Nano relay communication in production.
 * To enable debug logging via USB/Serial, define DEBUG_SERIAL below (development only).
 * In production (furnace installation), USB is inaccessible and UART0 is dedicated to relay comms.
 */

// Uncomment to enable debug logging via Serial (development only)
// WARNING: This will conflict with relay communication! Use only during development.
// #define DEBUG_SERIAL

// Firmware version
const char *FIRMWARE_VERSION = "1.0.0";

// Debug logging macros
#ifdef DEBUG_SERIAL
#define DEBUG_PRINT(x) Serial.print(x)
#define DEBUG_PRINTLN(x) Serial.println(x)
#define DEBUG_PRINTF(fmt, ...) Serial.printf(fmt, ##__VA_ARGS__)
#else
#define DEBUG_PRINT(x)
#define DEBUG_PRINTLN(x)
#define DEBUG_PRINTF(fmt, ...)
#endif

#include <Arduino.h>
#include <SPI.h>
#include <WiFi.h>
#include <time.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <max6675.h>
#include <PID_v1.h>
#include "TFT_eSPI.h"
#include <SD.h>
#include <FS.h>
#include <ArduinoJson.h>
#include <Update.h>
#include "pins.h"
#include "config.h"
#include "wifi_manager.h"
#include "furnace_types.h"
// RelaySerial public header and shared pin macros
// Raw thermocouple reading before applying offset
double rawTemp = 0.0;
// User calibration offset added to rawTemp (Celsius)
double tempOffset = 0.0;
#include <relay_serial.h>
#include <relay_pins.h>

/*  Install the "TFT_eSPI" library by Bodmer to interface with the TFT Display - https://github.com/Bodmer/TFT_eSPI
    *** IMPORTANT: User_Setup.h available on the internet will probably NOT work with the examples available at Raspberryme.com ***
    *** YOU MUST USE THE User_Setup.h FILE PROVIDED IN THE LINK BELOW IN ORDER TO USE THE EXAMPLES FROM RANDOM NERD TUTORIALS ***
    FULL INSTRUCTIONS AVAILABLE ON HOW CONFIGURE THE LIBRARY: https://Raspberryme.com/cyd/ or https://Raspberryme.com/esp32-tft/   */

// #include <font_Arial.h> // from ILI9341_t3
//  Global objects
TFT_eSPI tft = TFT_eSPI(240, 320);
MAX6675 thermocouple(TEMP_SCK_PIN, TEMP_CS_PIN, TEMP_SO_PIN);
AsyncWebServer server(HTTP_PORT);
AsyncWebSocket ws("/ws");
// SD card will use default SPI (VSPI peripheral with standard pins via SPI global)
// Global state variables
double currentTemp = 0;
double targetTemp = 0;
double pidOutput = 0;
bool programRunning = false;
unsigned long lastTempRead = 0;
unsigned long lastPIDCompute = 0;
// Track SSR2 PWM value for display/telemetry (0..1023)
uint16_t ssr2Pwm = 0;
uint16_t lastSsr2Pwm = 0;
bool lastSsr1State = false;

// Configurable update interval (ms) - defaults to TEMP_READ_INTERVAL
unsigned long tempReadInterval = TEMP_READ_INTERVAL;
// PID tuning parameters (configurable via web)
double pidKp = 40.0;
double pidKi = 0.1;
double pidKd = 4.0;

// Program display/state placeholders
String currentProgramName = "Idle";
int programStep = 0;
unsigned long programStartMs = 0;

// Current firing program data
FiringProgram currentProgram = {};

// Program execution state
bool programLoaded = false;
enum ProgramPhase
{
    PHASE_IDLE,
    PHASE_RAMP,
    PHASE_HOLD
};
ProgramPhase currentPhase = PHASE_IDLE;
unsigned long phaseStartMs = 0;
float rampStartTemp = 0;

// Data logging for PID tuning and analysis
File dataLogFile;
bool dataLogActive = false;
unsigned long lastDataLogMs = 0;
const unsigned long DATA_LOG_INTERVAL = 5000; // Log every 5 seconds

// Temperature graph buffer (simple circular buffer)
const int GRAPH_W = 300; // pixels / samples
const int GRAPH_H = 60;  // pixels
float tempSamples[GRAPH_W];
int tempSampleIdx = 0;
bool tempSamplesFilled = false;

// PID controller setup
PID tempPID(&currentTemp, &pidOutput, &targetTemp, pidKp, pidKi, pidKd, DIRECT);

// Safe WebSocket broadcast - only sends to clients that can handle messages
void safeBroadcast(const String &message)
{
    // Iterate through all clients and send only if their queue isn't full
    for (auto &client : ws.getClients())
    {
        if (client.queueLen() < 8)
        { // Only send if queue has space (max is typically 32)
            client.text(message);
        }
        else
        {
            // Queue is getting full, skip this client (will catch up or disconnect)
            DEBUG_PRINTF("[WS] Client #%u queue full (%d), skipping message\n",
                         client.id(), client.queueLen());
        }
    }
}

// Load settings from SD card (/config/settings.json)
bool loadSettings()
{
    if (!SD.exists("/config/settings.json"))
    {
        DEBUG_PRINTLN("No settings file found, using defaults");
        return false;
    }

    File file = SD.open("/config/settings.json", FILE_READ);
    if (!file)
    {
        DEBUG_PRINTLN("Failed to open settings file");
        return false;
    }

    StaticJsonDocument<256> doc;
    DeserializationError error = deserializeJson(doc, file);
    file.close();

    if (error)
    {
        DEBUG_PRINTF("Failed to parse settings: %s\n", error.c_str());
        return false;
    }

    // Load values with current defaults as fallback
    tempReadInterval = doc["updateInterval"] | tempReadInterval;
    pidKp = doc["pidKp"] | pidKp;
    pidKi = doc["pidKi"] | pidKi;
    pidKd = doc["pidKd"] | pidKd;
    tempOffset = doc["tempOffset"] | tempOffset; // default 0 if absent

    // Update PID controller with loaded tunings
    tempPID.SetTunings(pidKp, pidKi, pidKd);

    DEBUG_PRINTF("Settings loaded: interval=%lu, Kp=%.2f, Ki=%.3f, Kd=%.2f, offset=%.2f\n",
                 tempReadInterval, pidKp, pidKi, pidKd, tempOffset);
    return true;
}

// Save settings to SD card (/config/settings.json)
bool saveSettings()
{
    StaticJsonDocument<256> doc;
    doc["updateInterval"] = tempReadInterval;
    doc["pidKp"] = pidKp;
    doc["pidKi"] = pidKi;
    doc["pidKd"] = pidKd;
    doc["tempOffset"] = tempOffset;

    File file = SD.open("/config/settings.json", FILE_WRITE);
    if (!file)
    {
        DEBUG_PRINTLN("Failed to create settings file");
        return false;
    }

    if (serializeJson(doc, file) == 0)
    {
        DEBUG_PRINTLN("Failed to write settings");
        file.close();
        return false;
    }

    file.close();
    DEBUG_PRINTLN("Settings saved to SD");
    return true;
}

// Load a firing program from SD card
bool loadProgramFromSD(const String &programName)
{
    String path = "/programs/" + programName;
    if (!path.endsWith(".json"))
        path += ".json";

    if (!SD.exists(path))
    {
        DEBUG_PRINTF("Program file not found: %s\n", path.c_str());
        return false;
    }

    File file = SD.open(path, FILE_READ);
    if (!file)
    {
        DEBUG_PRINTF("Failed to open program file: %s\n", path.c_str());
        return false;
    }

    DynamicJsonDocument doc(2048);
    DeserializationError error = deserializeJson(doc, file);
    file.close();
    if (error)
    {
        DEBUG_PRINTF("Failed to parse program JSON: %s\n", error.c_str());
        return false;
    }

    // Parse program structure
    const char *name = doc["name"] | "Unknown";
    strncpy(currentProgram.name, name, sizeof(currentProgram.name) - 1);
    currentProgram.name[sizeof(currentProgram.name) - 1] = '\0';

    currentProgram.numSteps = doc["numSteps"] | 0;
    if (currentProgram.numSteps == 0 || currentProgram.numSteps > 10)
    {
        DEBUG_PRINTLN("Invalid number of steps");
        return false;
    }

    // Parse steps array
    JsonArray stepsArray = doc["steps"];
    if (!stepsArray || stepsArray.size() != currentProgram.numSteps)
    {
        DEBUG_PRINTLN("Steps array missing or size mismatch");
        return false;
    }

    for (uint8_t i = 0; i < currentProgram.numSteps; i++)
    {
        JsonObject stepObj = stepsArray[i];
        currentProgram.steps[i].targetTemp = stepObj["targetTemp"] | 0.0f;
        currentProgram.steps[i].rampRate = stepObj["rampRate"] | 0.0f;
        currentProgram.steps[i].holdTime = stepObj["holdTime"] | 0;
        currentProgram.steps[i].withRamp = stepObj["withRamp"] | false;
    }

    // Parse optional enableDataLog field (default to false)
    currentProgram.enableDataLog = doc["enableDataLog"] | false;

    currentProgram.active = false;
    currentProgram.currentStep = 0;

    DEBUG_PRINTF("Program loaded: %s with %d steps (dataLog=%s)\n",
                 currentProgram.name, currentProgram.numSteps,
                 currentProgram.enableDataLog ? "ON" : "OFF");
    return true;
}

// Start data logging to CSV file
bool startDataLog(const String &programName)
{
    if (dataLogActive)
    {
        dataLogFile.close();
        dataLogActive = false;
    }

    // Create log filename with timestamp
    time_t now = time(nullptr);
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);
    char timestamp[32];
    strftime(timestamp, sizeof(timestamp), "%Y%m%d_%H%M%S", &timeinfo);

    // Sanitize program name (replace spaces and special chars with underscore)
    String safeName = programName;
    safeName.replace(" ", "_");
    safeName.replace("/", "_");
    safeName.replace("\\", "_");

    String logPath = String("/logs/") + safeName + "_" + timestamp + ".csv";

    DEBUG_PRINTF("Starting data log: %s\n", logPath.c_str());

    dataLogFile = SD.open(logPath, FILE_WRITE);
    if (!dataLogFile)
    {
        DEBUG_PRINTF("Failed to create data log file: %s\n", logPath.c_str());
        return false;
    }

    // Write CSV header
    dataLogFile.println("Timestamp,ElapsedSeconds,CurrentTemp,RawTemp,TargetTemp,PidOutput,SSR2_PWM,Phase,Step");
    dataLogFile.flush();

    dataLogActive = true;
    lastDataLogMs = millis();

    DEBUG_PRINTLN("Data logging started");
    return true;
}

// Stop data logging and close file
void stopDataLog()
{
    if (dataLogActive)
    {
        dataLogFile.close();
        dataLogActive = false;
        DEBUG_PRINTLN("Data logging stopped");
    }
}

// Write a data point to the log file
void logDataPoint()
{
    if (!dataLogActive || !dataLogFile)
        return;

    // Calculate elapsed time
    unsigned long elapsedMs = millis() - programStartMs;
    float elapsedSec = elapsedMs / 1000.0f;

    // Get current timestamp
    time_t now = time(nullptr);
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);
    char timestamp[32];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", &timeinfo);

    // Determine phase string
    const char *phaseStr = (currentPhase == PHASE_RAMP) ? "RAMP" : (currentPhase == PHASE_HOLD) ? "HOLD"
                                                                                                : "IDLE";

    // Write CSV line: Timestamp,ElapsedSeconds,CurrentTemp,RawTemp,TargetTemp,PidOutput,SSR2_PWM,Phase,Step
    dataLogFile.printf("%s,%.1f,%.2f,%.2f,%.2f,%.2f,%u,%s,%d\n",
                       timestamp,
                       elapsedSec,
                       currentTemp,
                       rawTemp,
                       targetTemp,
                       pidOutput,
                       ssr2Pwm,
                       phaseStr,
                       programLoaded ? (currentProgram.currentStep + 1) : 0);

    dataLogFile.flush(); // Ensure data is written to SD card
    lastDataLogMs = millis();
}

void setupPins()
{
    // Hardware Serial (UART0) is used for Nano relay communication
    // No pin mode configuration needed - handled by Serial.begin()
    // Note: GPIO1 (TX) and GPIO3 (RX) are dedicated to UART0
}

// Display startup information screen for a few seconds
void displayStartupScreen()
{
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);

    // Title with version
    tft.setTextSize(1);
    tft.drawString("Four Verrier", 10, 10, 4);
    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    String versionText = "Version " + String(FIRMWARE_VERSION);
    tft.drawString(versionText, 10, 45, 2);

    // Separator line
    tft.drawFastHLine(10, 70, 300, TFT_DARKGREY);

    // Network information
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    int yPos = 85;

    // IP Address
    tft.drawString("Adresse IP:", 10, yPos, 2);
    String ipAddress;
    if (WiFi.getMode() == WIFI_STA && WiFi.status() == WL_CONNECTED)
    {
        ipAddress = WiFi.localIP().toString();
        tft.setTextColor(TFT_GREEN, TFT_BLACK);
    }
    else if (WiFi.getMode() == WIFI_AP)
    {
        ipAddress = WiFi.softAPIP().toString();
        tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    }
    else
    {
        ipAddress = "Non connecté";
        tft.setTextColor(TFT_RED, TFT_BLACK);
    }
    tft.drawString(ipAddress, 130, yPos, 2);
    yPos += 25;

    // WiFi Signal Strength (only in STA mode)
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString("Signal WiFi:", 10, yPos, 2);
    if (WiFi.getMode() == WIFI_STA && WiFi.status() == WL_CONNECTED)
    {
        int32_t rssi = WiFi.RSSI();
        String rssiText = String(rssi) + " dBm";

        // Color code signal strength
        if (rssi > -50)
        {
            tft.setTextColor(TFT_GREEN, TFT_BLACK); // Excellent
        }
        else if (rssi > -70)
        {
            tft.setTextColor(TFT_YELLOW, TFT_BLACK); // Good
        }
        else
        {
            tft.setTextColor(TFT_ORANGE, TFT_BLACK); // Fair
        }
        tft.drawString(rssiText, 130, yPos, 2);

        // Draw signal strength bars
        int barX = 240;
        int barY = yPos;
        int barCount = 5;
        int barSpacing = 8;
        int barMaxHeight = 20;

        for (int i = 0; i < barCount; i++)
        {
            int barHeight = (i + 1) * (barMaxHeight / barCount);
            int threshold = -90 + (i * 10); // -90, -80, -70, -60, -50
            uint16_t barColor = (rssi >= threshold) ? TFT_GREEN : TFT_DARKGREY;
            tft.fillRect(barX + (i * barSpacing), barY + (barMaxHeight - barHeight), 6, barHeight, barColor);
        }
    }
    else if (WiFi.getMode() == WIFI_AP)
    {
        tft.setTextColor(TFT_YELLOW, TFT_BLACK);
        tft.drawString("Mode AP", 130, yPos, 2);
    }
    else
    {
        tft.setTextColor(TFT_RED, TFT_BLACK);
        tft.drawString("Non dispo", 130, yPos, 2);
    }
    yPos += 25;

    // NTP Time
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString("Heure NTP:", 10, yPos, 2);
    time_t now = time(nullptr);
    if (now >= (time_t)1600000000)
    {
        struct tm timeinfo;
        localtime_r(&now, &timeinfo);
        char timeStr[32];
        strftime(timeStr, sizeof(timeStr), "%d/%m/%Y %H:%M:%S", &timeinfo);
        tft.setTextColor(TFT_GREEN, TFT_BLACK);
        tft.drawString(timeStr, 10, yPos + 20, 2);
    }
    else
    {
        tft.setTextColor(TFT_ORANGE, TFT_BLACK);
        tft.drawString("Non synchronise", 10, yPos + 20, 2);
    }

    // Footer message
    yPos += 60;
    tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
    tft.drawString("Demarrage en cours...", 10, yPos, 2);

    // Keep screen visible for 5 seconds
    delay(10000);
}

// Execute the current program step by step
void executeProgramStep()
{
    if (!programRunning || !programLoaded || currentProgram.numSteps == 0)
        return;

    unsigned long now = millis();
    uint8_t stepIndex = currentProgram.currentStep;

    // Check if program is complete
    if (stepIndex >= currentProgram.numSteps)
    {
        programRunning = false;
        programLoaded = false;
        currentProgramName = "Idle";
        targetTemp = 0;
        currentPhase = PHASE_IDLE;

        // Stop data logging when program completes
        stopDataLog();

        DEBUG_PRINTLN("Program completed");
        return;
    }

    ProgramStep &step = currentProgram.steps[stepIndex];

    // State machine for each step: RAMP → HOLD → next step
    switch (currentPhase)
    {
    case PHASE_IDLE:
        // Initialize first step or transition from previous step
        if (step.withRamp && step.rampRate > 0)
        {
            // Start ramp phase
            currentPhase = PHASE_RAMP;
            phaseStartMs = now;
            rampStartTemp = currentTemp;
            DEBUG_PRINTF("Step %d: Starting RAMP from %.1f°C to %.1f°C at %.1f°C/min\n",
                         stepIndex + 1, rampStartTemp, step.targetTemp, step.rampRate);
        }
        else
        {
            // No ramp, go directly to hold phase at target temp
            targetTemp = step.targetTemp;
            currentPhase = PHASE_HOLD;
            phaseStartMs = now;
            DEBUG_PRINTF("Step %d: Starting HOLD at %.1f°C for %d min\n",
                         stepIndex + 1, step.targetTemp, step.holdTime);
        }
        break;

    case PHASE_RAMP:
    {
        // Calculate elapsed time in minutes
        float elapsedMin = (now - phaseStartMs) / 60000.0f;
        float tempIncrease = step.rampRate * elapsedMin;
        float newTarget = rampStartTemp + tempIncrease;

        // Clamp target to final step temperature
        if ((step.targetTemp > rampStartTemp && newTarget >= step.targetTemp) ||
            (step.targetTemp < rampStartTemp && newTarget <= step.targetTemp))
        {
            // Ramp complete, transition to hold phase
            targetTemp = step.targetTemp;
            currentPhase = PHASE_HOLD;
            phaseStartMs = now;
            DEBUG_PRINTF("Step %d: RAMP complete, starting HOLD at %.1f°C for %d min\n",
                         stepIndex + 1, step.targetTemp, step.holdTime);
        }
        else
        {
            // Continue ramping
            targetTemp = newTarget;
        }
        break;
    }

    case PHASE_HOLD:
    {
        // Check if hold time has elapsed
        unsigned long holdElapsedMs = now - phaseStartMs;
        unsigned long holdDurationMs = step.holdTime * 60000UL;

        if (holdElapsedMs >= holdDurationMs)
        {
            // Hold complete, move to next step
            currentProgram.currentStep++;
            currentPhase = PHASE_IDLE;
            DEBUG_PRINTF("Step %d: HOLD complete, moving to next step\n", stepIndex + 1);
        }
        // else: continue holding at target temperature
        break;
    }
    }
}

// Draw static UI elements once (labels, borders) to avoid flicker
void drawStaticUI()
{
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextSize(1);

    // Static labels - left column
    tft.drawString("Temp Four:", 5, 5, 2);
    tft.drawString("C", 160, 5, 2);
    tft.drawString("Temp cible:", 5, 35, 2);
    tft.drawString("C", 160, 35, 2);

    // Static labels - right column
    tft.drawString("SSR1", 210, 10, 2);
    tft.drawString("SSR2", 265, 40, 2);

    // SSR2 PWM bar border (static)
    tft.drawRect(180, 38, 82, 14, TFT_WHITE);

    // Bottom row labels
    tft.drawString("Prog:", 5, 70, 2);
    tft.drawString("Etape:", 155, 70, 2);
    tft.drawString("Temps:", 220, 70, 2);
}

void updateDisplay(bool force)
{
    static unsigned long lastUpdate = 0;
    if (!force && millis() - lastUpdate < 1000)
        return; // Update once per second

    uint16_t tcol;
    if (currentTemp < targetTemp - 2.0)
    {
        tcol = TFT_CYAN; // Heating
    }
    else if (currentTemp > targetTemp + 2.0)
    {
        tcol = TFT_ORANGE; // Cooling
    }
    else
    {
        tcol = TFT_GREEN; // Stable
    }

    tft.setTextColor(tcol, TFT_BLACK);
    tft.setTextSize(1);

    // Update variable fields only (right-aligned with explicit background)
    // Temperature values (right-aligned in a fixed-width field)
    String tempStr = String(currentTemp, 1);
    tft.fillRect(90, 5, 65, 20, TFT_BLACK); // Clear old value
    tft.drawString(tempStr, 90, 5, 4);

    tft.setTextColor(TFT_DARKCYAN, TFT_BLACK);
    String targetStr = String(targetTemp, 1);
    tft.fillRect(90, 35, 65, 20, TFT_BLACK); // Clear old value
    tft.drawString(targetStr, 90, 35, 4);

    // SSR1 indicator (filled rect changes color)
    tft.fillRect(180, 5, 25, 25, programRunning ? TFT_RED : TFT_DARKGREY);

    // SSR2 PWM bar (redraw interior only)
    int barMaxW = 80;
    int barW = map(ssr2Pwm, 0, 1023, 0, barMaxW);
    tft.fillRect(181, 39, barMaxW, 12, TFT_BLACK); // Clear old bar
    tft.fillRect(181, 39, barW, 12, TFT_ORANGE);   // Draw new bar

    // Program name (clear and redraw, strip .json extension if present)
    String displayName = currentProgramName;
    if (displayName.endsWith(".json"))
    {
        displayName = displayName.substring(0, displayName.length() - 5);
    }
    tft.fillRect(50, 70, 100, 16, TFT_BLACK);
    tft.drawString(displayName, 50, 70, 2);

    // Step number (clear and redraw) - use currentProgram.currentStep + 1 for 1-indexed display
    String stepText = programLoaded ? String(currentProgram.currentStep + 1) : String(0);
    tft.fillRect(195, 70, 30, 16, TFT_BLACK);
    tft.drawString(stepText, 195, 70, 2);

    // Elapsed time formatted as h:m:s (clear wider area for formatted time)
    unsigned long elapsed = programRunning ? (millis() - programStartMs) / 1000 : 0;
    unsigned long hours = elapsed / 3600;
    unsigned long minutes = (elapsed % 3600) / 60;
    unsigned long seconds = elapsed % 60;
    String elapsedText = String(hours) + ":" +
                         (minutes < 10 ? "0" : "") + String(minutes) + ":" +
                         (seconds < 10 ? "0" : "") + String(seconds);
    tft.fillRect(265, 70, 55, 16, TFT_BLACK); // Wider clear area for h:m:s format
    tft.drawString(elapsedText, 265, 70, 2);

    // Draw temperature evolution graph with axes and scales
    int screenW = tft.width();
    const int leftMargin = 40;   // space for Y labels
    const int rightMargin = 5;   // small right margin
    const int bottomMargin = 10; // space for X labels below graph
    const int topGraphY = 100;   // Start below program info
    int gx = leftMargin;
    int gy = topGraphY;
    int gw = screenW - leftMargin - rightMargin; // graph drawable width
    int gh = 125;                                // reduced height to leave space for X labels

    // Clear graph background
    tft.fillRect(gx, gy, gw, gh, TFT_DARKGREY);
    // Compute min/max from buffer (with fallback)
    float minT = 9999.0, maxT = -9999.0;
    int count = tempSamplesFilled ? GRAPH_W : tempSampleIdx;
    if (count <= 1)
    {
        minT = currentTemp - 5;
        maxT = currentTemp + 5;
    }
    else
    {
        for (int i = 0; i < count; ++i)
        {
            int idx = tempSamplesFilled ? ((tempSampleIdx + i) % GRAPH_W) : i;
            float v = tempSamples[idx];
            if (v < minT)
                minT = v;
            if (v > maxT)
                maxT = v;
        }
        // add small padding
        float pad = (maxT - minT) * 0.1;
        if (pad < 0.5)
            pad = 0.5;
        minT -= pad;
        maxT += pad;
    }
    if (minT >= maxT)
    {
        maxT = minT + 1.0;
    }

    // Draw axes
    tft.drawFastVLine(gx, gy, gh, TFT_WHITE);      // Y axis
    tft.drawFastHLine(gx, gy + gh, gw, TFT_WHITE); // X axis

    // Draw horizontal grid lines and Y-axis labels (min..max)
    for (int g = 0; g <= 4; ++g)
    {
        int yy = gy + g * (gh / 4);
        tft.drawFastHLine(gx, yy, gw, TFT_BLACK);
        // Value at this grid line
        float val = maxT - (maxT - minT) * (float)g / 4.0f;
        String label = String(val, (maxT - minT) < 20 ? 1 : 0);
        // Draw label in left margin
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.drawString(label, 2, yy - 6, 1);
    }

    // Draw samples LEFT-TO-RIGHT: oldest at left edge, newest fills from left
    // Each sample gets one pixel, scrolling left when buffer is full
    int plotCount = (count > gw) ? gw : count;
    if (plotCount > 1)
    {
        int startIdx;
        if (tempSamplesFilled)
        {
            // Buffer is full, show the most recent gw samples
            // Newest sample is at (tempSampleIdx - 1), oldest is at (tempSampleIdx)
            startIdx = (count > gw) ? ((tempSampleIdx - gw + GRAPH_W) % GRAPH_W) : 0;
        }
        else
        {
            // Buffer not full yet, show from index 0
            startIdx = 0;
        }

        // Draw polyline from left to right
        int prevX = gx;
        int firstIdx = startIdx;
        float firstSample = tempSamples[firstIdx];
        int prevY = gy + gh - (int)((firstSample - minT) / (maxT - minT) * (gh - 1));

        for (int i = 1; i < plotCount; ++i)
        {
            int idx = tempSamplesFilled ? ((startIdx + i) % GRAPH_W) : i;
            int px = gx + i; // one pixel per sample, left to right
            float s = tempSamples[idx];
            int y = gy + gh - (int)((s - minT) / (maxT - minT) * (gh - 1));
            tft.drawLine(prevX, prevY, px, y, TFT_GREEN);
            prevX = px;
            prevY = y;
        }
    }

    // X-axis time labels BELOW the graph
    // Calculate total elapsed time (in seconds) represented by collected samples
    unsigned long totalSeconds = (unsigned long)count * (unsigned long)tempReadInterval / 1000UL;
    float secPerSample = (float)tempReadInterval / 1000.0f;

    // Visible time window based on graph width (gw pixels -> gw samples max)
    unsigned long windowSeconds = (unsigned long)((float)gw * secPerSample + 0.5f);
    unsigned long visibleSpanSeconds = (totalSeconds < windowSeconds) ? totalSeconds : windowSeconds;
    unsigned long startSec = (totalSeconds > visibleSpanSeconds) ? (totalSeconds - visibleSpanSeconds) : 0UL;

    // Determine tick interval based on currently visible span
    unsigned tickSec;
    if (visibleSpanSeconds <= 60)
        tickSec = 10; // 10s intervals for first minute
    else if (visibleSpanSeconds <= 600)
        tickSec = 60; // 1min intervals up to 10 minutes
    else if (visibleSpanSeconds <= 3600)
        tickSec = 300; // 5min intervals up to 1 hour
    else
        tickSec = 600; // 10min intervals beyond 1 hour

    // Clear label area below the graph to remove remnants
    int labelY = gy + gh + 2;
    tft.fillRect(gx, labelY, gw, 12, TFT_BLACK); // Clear old labels

    tft.setTextColor(TFT_WHITE, TFT_BLACK);

    // Align the first tick to a multiple of tickSec for stable movement
    unsigned long firstTick = (startSec / tickSec) * tickSec;
    if (firstTick < startSec)
        firstTick += tickSec;

    for (unsigned long t = firstTick; t <= totalSeconds; t += tickSec)
    {
        // Calculate pixel position for this time within the visible window
        int px = gx + (int)(((float)(t - startSec)) / secPerSample);
        if (px > gx + gw)
            break;

        // Draw small tick mark
        tft.drawFastVLine(px, gy + gh, 3, TFT_WHITE);

        // Format time label (absolute elapsed time)
        String lbl;
        if (visibleSpanSeconds >= 3600)
        {
            // Use hours:minutes format (HH:MM)
            unsigned h = t / 3600;
            unsigned m = (t % 3600) / 60;
            lbl = String(h) + "h" + (m < 10 ? "0" : "") + String(m);
        }
        else if (t >= 60)
        {
            // Use minutes:seconds format (MM:SS)
            unsigned m = t / 60;
            unsigned s = t % 60;
            lbl = String(m) + ":" + (s < 10 ? "0" : "") + String(s);
        }
        else
        {
            // Just seconds
            lbl = String(t) + "s";
        }

        // Draw label with smallest font (1)
        int tw = tft.textWidth(lbl, 1);
        tft.drawString(lbl, px - tw / 2, labelY, 1);
    }

    lastUpdate = millis();
}
void handleWebSocketMessage(void *arg, uint8_t *data, size_t len)
{
    AwsFrameInfo *info = (AwsFrameInfo *)arg;
    if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT)
    {
        StaticJsonDocument<256> doc;
        DeserializationError error = deserializeJson(doc, data);

        if (!error)
        {
            bool settingsChanged = false;

            if (doc.containsKey("target"))
            {
                targetTemp = doc["target"].as<double>();
            }
            if (doc.containsKey("updateInterval"))
            {
                unsigned long newInterval = doc["updateInterval"].as<unsigned long>();
                // Clamp to reasonable range: 100ms to 60s
                if (newInterval >= 100 && newInterval <= 60000)
                {
                    tempReadInterval = newInterval;
                    DEBUG_PRINTF("Update interval changed to %lu ms\n", tempReadInterval);
                    settingsChanged = true;
                }
            }
            if (doc.containsKey("pidKp"))
            {
                pidKp = doc["pidKp"].as<double>();
                settingsChanged = true;
            }
            if (doc.containsKey("pidKi"))
            {
                pidKi = doc["pidKi"].as<double>();
                settingsChanged = true;
            }
            if (doc.containsKey("pidKd"))
            {
                pidKd = doc["pidKd"].as<double>();
                settingsChanged = true;
            }
            if (doc.containsKey("tempOffset"))
            {
                tempOffset = doc["tempOffset"].as<double>();
                DEBUG_PRINTF("Temperature offset changed to %.2f°C\n", tempOffset);
                settingsChanged = true;
            }

            // If PID params changed, update the controller and save settings
            if (doc.containsKey("pidKp") || doc.containsKey("pidKi") || doc.containsKey("pidKd"))
            {
                tempPID.SetTunings(pidKp, pidKi, pidKd);
                DEBUG_PRINTF("PID tunings updated: Kp=%.2f, Ki=%.3f, Kd=%.2f\n", pidKp, pidKi, pidKd);
            }

            // Save settings to SD if anything changed
            if (settingsChanged)
            {
                saveSettings();
            }
        }
    }
}

void onWebSocketEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type,
                      void *arg, uint8_t *data, size_t len)
{
    switch (type)
    {
    case WS_EVT_CONNECT:
        DEBUG_PRINTF("WebSocket client #%u connected\n", client->id());
        break;
    case WS_EVT_DISCONNECT:
        DEBUG_PRINTF("WebSocket client #%u disconnected\n", client->id());
        break;
    case WS_EVT_DATA:
        handleWebSocketMessage(arg, data, len);
        break;
    case WS_EVT_PONG:
    case WS_EVT_ERROR:
        break;
    }
}

// Web endpoints and handlers
void setupWebServer()
{
    // Setup WebSocket
    ws.onEvent(onWebSocketEvent);
    server.addHandler(&ws);

    // Serve the web interface (from SD card)
    server.serveStatic("/", SD, "/www/").setDefaultFile("index.html");

    // WiFi configuration page (small form served dynamically)
    server.on("/wifi", HTTP_GET, [](AsyncWebServerRequest *request)
              {
        String sd_ssid, sd_password;
        String prefill = "";
        if (SD.exists("/config/wifi.json") && loadWiFiConfig(sd_ssid, sd_password)) {
            prefill = sd_ssid;
        }

        String html = "<html><head> <meta charset=\"UTF-8\"> <meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
                      "<title>WiFi Setup</title>"
                      "<style> body{font-family:Arial;margin:12px;background:#f5f5f5} "
                      ".header{background:white;padding:15px;border-radius:8px;margin-bottom:20px;display:flex;justify-content:space-between;align-items:center;box-shadow:0 2px 4px rgba(0,0,0,0.1)}"
                      ".header h2{margin:0}"
                      ".section{background:white;padding:20px;border-radius:8px;margin-bottom:20px;box-shadow:0 2px 4px rgba(0,0,0,0.1)}"                      
                      "input[type=text], input[type=password]{width:100%;padding:8px;margin:6px 0 12px 0;border:1px solid #ccc;border-radius:4px;box-sizing:border-box}"
                      "input[type=submit]{background-color:#4CAF50;color:white;padding:10px 15px;border:none;border-radius:4px;cursor:pointer}"
                      "input[type=submit]:hover{background-color:#45a049}"
                      ".back-link{color:#4CAF50;text-decoration:none;font-size:14px}"
                      ".back-link:hover{text-decoration:underline}"
                      "</style>"
                      "</head><body>"
                      "<div class=\"header\">"
                      "<h2>Paramètres WiFi</h2>"
                      "<a href=\"/\" class=\"back-link\">← Retour à l'accueil</a>"
                      "</div>"      
                      "<div class=\"section\">"
                      "<form method=\"POST\" action=\"/save_wifi\">"
                      "SSID:<br><input name=\"ssid\" placeholder=\"SSID\" value=\"" + prefill + "\"><br>"
                      "Password:<br><input name=\"password\" placeholder=\"Password\"><br><br>"
                      "<input type=\"submit\" value=\"Save & Reboot\">"
                      "</form>"
                      "</div>";
        if (prefill.length() > 0) html += "<div class=\"section\">Current SSID on SD: " + prefill + "</div>";
        html += "</body></html>";
        request->send(200, "text/html", html); });

    // File manager APIs
    server.on("/api/list", HTTP_GET, [](AsyncWebServerRequest *request)
              {
        // query param: dir (like /logs or /programs or /updates)
        String dir = "/";
        if (request->hasParam("dir")) dir = request->getParam("dir")->value();
        DynamicJsonDocument doc(2048);
        JsonArray arr = doc.to<JsonArray>();
        File root = SD.open(dir);
        if (root && root.isDirectory()) {
            File f;
            while (f = root.openNextFile()) {
                String name = f.name();
                // strip leading path
                int p = name.lastIndexOf('/');
                if (p >= 0) name = name.substring(p+1);
                arr.add(name);
                f.close();
            }
            root.close();
        }
        String out;
        serializeJson(arr, out);
        request->send(200, "application/json", out); });

    // Compatibility route: serve the file manager page at /files (some clients request /files)
    server.on("/files", HTTP_GET, [](AsyncWebServerRequest *request)
              {
        // Serve the static files.html from SD
        if (SD.exists("/www/files.html")) {
            request->send(SD, "/www/files.html", "text/html");
        } else {
            request->send(404, "text/plain", "files.html not found");
        } });

    // Compatibility: index.html requests /programs — proxy to the /api/programs listing
    server.on("/programs", HTTP_GET, [](AsyncWebServerRequest *request)
              {
        DynamicJsonDocument doc(2048);
        JsonArray arr = doc.to<JsonArray>();
        File root = SD.open("/programs");
        if (root && root.isDirectory()) {
            File f;
            while (f = root.openNextFile()) {
                String name = f.name();
                int p = name.lastIndexOf('/');
                if (p >= 0) name = name.substring(p+1);
                arr.add(name);
                f.close();
            }
            root.close();
        }
        String out; serializeJson(arr, out);
        request->send(200, "application/json", out); });

    server.on("/api/programs", HTTP_GET, [](AsyncWebServerRequest *request)
              {
        // return list of files under /programs
        DynamicJsonDocument doc(2048);
        JsonArray arr = doc.to<JsonArray>();
        File root = SD.open("/programs");
        if (root && root.isDirectory()) {
            File f;
            while (f = root.openNextFile()) {
                String name = f.name();
                int p = name.lastIndexOf('/');
                if (p >= 0) name = name.substring(p+1);
                arr.add(name);
                f.close();
            }
            root.close();
        }
        String out; serializeJson(arr, out);
        request->send(200, "application/json", out); });

    server.on("/api/logs", HTTP_GET, [](AsyncWebServerRequest *request)
              {
        DynamicJsonDocument doc(2048);
        JsonArray arr = doc.to<JsonArray>();
        File root = SD.open("/logs");
        if (root && root.isDirectory()) {
            File f;
            while (f = root.openNextFile()) {
                String name = f.name();
                int p = name.lastIndexOf('/');
                if (p >= 0) name = name.substring(p+1);
                arr.add(name);
                f.close();
            }
            root.close();
        }
        String out; serializeJson(arr, out);
        request->send(200, "application/json", out); });

    // Download handler: /download?path=/logs/xxx
    server.on("/download", HTTP_GET, [](AsyncWebServerRequest *request)
              {
        if (!request->hasParam("path")) { request->send(400, "text/plain", "missing path"); return; }
        String path = request->getParam("path")->value();
        
        // Extract filename from path (get everything after last /)
        String filename = path;
        int lastSlash = path.lastIndexOf('/');
        if (lastSlash >= 0) {
            filename = path.substring(lastSlash + 1);
        }
        
        // Create response with Content-Disposition header to suggest filename
        AsyncWebServerResponse *response = request->beginResponse(SD, path.c_str(), "application/octet-stream");
        if (response) {
            response->addHeader("Content-Disposition", "attachment; filename=\"" + filename + "\"");
            request->send(response);
        } else {
            request->send(404, "text/plain", "File not found");
        } });

    // Delete handler: POST { path: "/logs/xxx" }
    server.on("/api/delete", HTTP_POST, [](AsyncWebServerRequest *request)
              {
                  // Body is small JSON — actual response will be sent from the body handler below
              },
              NULL, [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total)
              {
        // parse body
        DynamicJsonDocument doc(512);
        DeserializationError err = deserializeJson(doc, (const char*)data, len);
        if (err) { request->send(400, "text/plain", "invalid json"); return; }
        const char* path = doc["path"] | "";
        if (strlen(path) == 0) { request->send(400, "text/plain", "missing path"); return; }
        bool ok = SD.remove(String(path));
        if (ok) request->send(200, "text/plain", "deleted"); else request->send(500, "text/plain", "delete failed"); });

    // Upload endpoint - handles file uploads and stores under /programs by default
    server.on("/upload", HTTP_POST, [](AsyncWebServerRequest *request)
              {
                  // Response sent after upload completes
                  request->send(200, "text/plain", "Upload complete"); }, [](AsyncWebServerRequest *request, const String &filename, size_t index, uint8_t *data, size_t len, bool final)
              {
        static File uploadFile;
        static String uploadDest;
        if (index == 0) {
            // new upload, open file for writing
            uploadDest = String("/programs/") + filename;
            
            // Delete existing file to ensure clean write (FILE_WRITE appends on ESP32)
            if (SD.exists(uploadDest)) {
                SD.remove(uploadDest);
                DEBUG_PRINTF("Removed existing file: %s\n", uploadDest.c_str());
            }
            
            uploadFile = SD.open(uploadDest, FILE_WRITE);
            if (!uploadFile) {
                DEBUG_PRINTF("Failed to open file for writing: %s\n", uploadDest.c_str());
            }
        }
        if (uploadFile) {
            size_t written = uploadFile.write(data, len);
            if (written != len) {
                DEBUG_PRINTF("Write error: wrote %u of %u bytes\n", written, len);
            }
            
            // Flush to SD every 4KB to avoid RAM overflow
            if ((index + len) % 4096 == 0 || final) {
                uploadFile.flush();
            }
            
            if (final) { 
                uploadFile.close();
                DEBUG_PRINTF("Upload complete: %s (%u bytes)\n", uploadDest.c_str(), index + len);
            }
        } });

    // Save program from manual editor - accepts JSON content directly
    // POST body: { "name": "program_name", "content": "{ json program data }" }
    server.on("/api/save_program", HTTP_POST, [](AsyncWebServerRequest *request) {}, NULL, [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total)
              {
        DynamicJsonDocument doc(4096);
        DeserializationError err = deserializeJson(doc, (const char*)data, len);
        if (err) { 
            request->send(400, "text/plain", "Invalid JSON"); 
            return; 
        }
        
        const char* name = doc["name"] | "";
        const char* content = doc["content"] | "";
        
        if (strlen(name) == 0 || strlen(content) == 0) { 
            request->send(400, "text/plain", "Missing name or content"); 
            return; 
        }
        
        // Ensure .json extension
        String filename = String(name);
        if (!filename.endsWith(".json")) {
            filename += ".json";
        }
        
        String path = "/programs/" + filename;
        File file = SD.open(path, FILE_WRITE);
        if (!file) {
            request->send(500, "text/plain", "Failed to create file");
            return;
        }
        
        file.print(content);
        file.close();
        
        DEBUG_PRINTF("Saved program: %s\n", path.c_str());
        request->send(200, "text/plain", "Program saved"); });

    // Flexible upload endpoint - accepts 'dest' query param for destination folder
    // Example: /upload_to?dest=/www or /upload_to?dest=/updates
    server.on("/upload_to", HTTP_POST, [](AsyncWebServerRequest *request)
              {
                  // Response sent after upload completes
                  request->send(200, "text/plain", "Upload complete"); }, [](AsyncWebServerRequest *request, const String &filename, size_t index, uint8_t *data, size_t len, bool final)
              {
        static File uploadFile;
        static String uploadPath;
        if (index == 0) {
            // Get destination folder from query parameter (default to /programs)
            String dest = "/programs";
            if (request->hasParam("dest")) {
                dest = request->getParam("dest")->value();
            }
            // Ensure destination ends with /
            if (!dest.endsWith("/")) dest += "/";
            
            // Create destination directory if it doesn't exist
            if (!SD.exists(dest)) {
                DEBUG_PRINTF("Creating directory: %s\n", dest.c_str());
                if (!SD.mkdir(dest)) {
                    DEBUG_PRINTF("Failed to create directory: %s\n", dest.c_str());
                }
            }
            
            uploadPath = dest + filename;
            DEBUG_PRINTF("Uploading to: %s\n", uploadPath.c_str());
            
            // Delete existing file to ensure clean write (FILE_WRITE appends on ESP32)
            if (SD.exists(uploadPath)) {
                SD.remove(uploadPath);
                DEBUG_PRINTF("Removed existing file: %s\n", uploadPath.c_str());
            }
            
            uploadFile = SD.open(uploadPath, FILE_WRITE);
            if (!uploadFile) {
                DEBUG_PRINTF("Failed to open file for writing: %s\n", uploadPath.c_str());
            } else {
                DEBUG_PRINTF("File opened successfully for writing\n");
            }
        }
        if (uploadFile) {
            size_t written = uploadFile.write(data, len);
            if (written != len) {
                DEBUG_PRINTF("Write error: wrote %u of %u bytes\n", written, len);
            }
            
            // Flush to SD every 4KB to avoid RAM overflow on large files
            if ((index + len) % 4096 == 0 || final) {
                uploadFile.flush();
            }
            
            if (final) { 
                uploadFile.close();
                DEBUG_PRINTF("Upload complete: %s (%u bytes)\n", uploadPath.c_str(), index + len);
            }
        } });

    // OTA apply: POST { path: "/updates/firmware.bin" }
    server.on("/apply_ota", HTTP_POST, [](AsyncWebServerRequest *request)
              {
                  // Read JSON body in body handler — response will be sent from the body handler
              },
              NULL, [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total)
              {
        DynamicJsonDocument doc(512);
        DeserializationError err = deserializeJson(doc, (const char*)data, len);
        if (err) { request->send(400, "text/plain", "invalid json"); return; }
        const char* path = doc["path"] | "";
        if (strlen(path) == 0) { request->send(400, "text/plain", "missing path"); return; }
        File f = SD.open(String(path));
        if (!f) { request->send(404, "text/plain", "file not found"); return; }
        size_t size = f.size();
        DEBUG_PRINTF("Starting OTA from SD: %s (size=%u)\n", path, (unsigned)size);
        if (!Update.begin(size)) { request->send(500, "text/plain", "Update.begin failed"); f.close(); return; }
        uint8_t buf[1024];
        size_t written = 0;
        while (f.available()) {
            size_t r = f.read(buf, sizeof(buf));
            if (r <= 0) break;
            size_t w = Update.write(buf, r);
            written += w;
        }
        f.close();
        if (!Update.end()) { request->send(500, "text/plain", "Update.end failed"); return; }
        if (!Update.isFinished()) { request->send(500, "text/plain", "Update not finished"); return; }
        request->send(200, "text/plain", "Update complete, rebooting");
        delay(500);
        ESP.restart(); });

    // Save WiFi credentials (form POST)
    server.on("/save_wifi", HTTP_POST, [](AsyncWebServerRequest *request)
              {
        // In AsyncWebServer the returned parameter is const AsyncWebParameter*
        const AsyncWebParameter* ssidParam = request->getParam("ssid", true);
        const AsyncWebParameter* passParam = request->getParam("password", true);
        String ssid = ssidParam ? ssidParam->value() : String("");
        String password = passParam ? passParam->value() : String("");

        bool ok = false;
        if (ssid.length() > 0) {
            ok = saveWiFiConfig(ssid, password);
        }

        if (ok) {
            request->send(200, "text/plain", "Saved. Rebooting...");
            delay(500);
            ESP.restart();
        } else {
            request->send(500, "text/plain", "Failed to save WiFi configuration");
        } });

    // Start program endpoint
    server.on("/start", HTTP_POST, [](AsyncWebServerRequest *request) {}, NULL, [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total)
              {
            StaticJsonDocument<128> doc;
            DeserializationError error = deserializeJson(doc, data, len);
            
            if (error) {
                request->send(400, "text/plain", "Invalid JSON");
                return;
            }
            
            String program = doc["program"] | "";
            if (program.length() > 0) {
                // Load the program from SD card
                if (loadProgramFromSD(program)) {
                    programLoaded = true;
                    programRunning = true;
                    currentProgramName = program;
                    programStartMs = millis();
                    currentProgram.currentStep = 0;
                    currentPhase = PHASE_IDLE;
                    
                    // Start data logging if enabled in program
                    if (currentProgram.enableDataLog) {
                        startDataLog(program);
                        logDataPoint(); // log initial point
                    }
                    
                    DEBUG_PRINTF("Program started: %s\n", program.c_str());
                    request->send(200, "text/plain", "Program started");
                } else {
                    request->send(500, "text/plain", "Failed to load program file");
                }
            } else {
                request->send(400, "text/plain", "No program specified");
            } });

    // Stop program endpoint
    server.on("/stop", HTTP_POST, [](AsyncWebServerRequest *request)
              {
        programRunning = false;
        programLoaded = false;
        currentProgramName = "Idle";
        targetTemp = 0;
        currentPhase = PHASE_IDLE;
        
        // Stop data logging if active
        stopDataLog();
        
        DEBUG_PRINTLN("Program stopped");
        request->send(200, "text/plain", "Program stopped"); });

    // Start server
    server.begin();

    // Status endpoint: return current telemetry so web UI can fetch initial values
    server.on("/status", HTTP_GET, [](AsyncWebServerRequest *request)
              {
        // Increase capacity to safely include all fields (prevents truncation)
        DynamicJsonDocument doc(1024);
        doc["temp"] = currentTemp;
        doc["target"] = targetTemp;
        // Telemetry: report commanded states to avoid stale reads
        doc["ssr1"] = programRunning;           // SSR1 follows program state
        doc["ssr2"] = ssr2Pwm;                  // SSR2 current PWM (0..1023)
        doc["program"] = programRunning ? 1 : 0;
        
        // Add elapsed time in seconds (resets when program starts)
        unsigned long elapsedMs = programRunning ? (millis() - programStartMs) : millis();
        doc["elapsedSeconds"] = elapsedMs / 1000;
        
        // Add firmware version
        doc["version"] = FIRMWARE_VERSION;
        
        // Add current update interval and PID parameters
        doc["updateInterval"] = tempReadInterval;
        doc["pidKp"] = pidKp;
        doc["pidKi"] = pidKi;
        doc["pidKd"] = pidKd;
        doc["tempOffset"] = tempOffset;
        
        // Add program execution status (always include for consistency)
        doc["programName"] = currentProgramName;
        doc["programStep"] = programLoaded ? (currentProgram.currentStep + 1) : 0; // 1-indexed
        doc["totalSteps"] = programLoaded ? currentProgram.numSteps : 0;
        const char* phaseStr = (currentPhase == PHASE_RAMP) ? "RAMP" : 
                               (currentPhase == PHASE_HOLD) ? "HOLD" : "IDLE";
        doc["phase"] = phaseStr;
        
        String out; serializeJson(doc, out);
        AsyncWebServerResponse *response = request->beginResponse(200, "application/json", out);
        response->addHeader("Cache-Control", "no-cache, no-store, must-revalidate");
        response->addHeader("Pragma", "no-cache");
        response->addHeader("Expires", "0");
        request->send(response); });

    // Calibrate offset endpoint: POST { "actualTemp": <float> }
    server.on("/calibrate_offset", HTTP_POST, [](AsyncWebServerRequest *request) {}, NULL, [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total)
              {
                  StaticJsonDocument<256> doc;
                  DeserializationError err = deserializeJson(doc, data, len);
                  if (err) {
                      request->send(400, "application/json", "{\"error\":\"invalid json\"}");
                      return;
                  }
                  double actual = doc["actualTemp"] | NAN;
                  if (isnan(actual)) {
                      request->send(400, "application/json", "{\"error\":\"missing actualTemp\"}");
                      return;
                  }
                  // Take a fresh raw reading (avoid stale)
                  rawTemp = thermocouple.readCelsius();
                  tempOffset = actual - rawTemp;
                  saveSettings();
                  StaticJsonDocument<256> outDoc;
                  outDoc["rawTemp"] = rawTemp;
                  outDoc["actualTemp"] = actual;
                  outDoc["tempOffset"] = tempOffset;
                  String outJson; serializeJson(outDoc, outJson);
                  request->send(200, "application/json", outJson); });
}

// Try to connect as a station first (useful if you set WIFI_STA_SSID in include/pins.h)
void setupWiFi()
{
    // Optimize WiFi for weak signal environments
    WiFi.setTxPower(WIFI_POWER_19_5dBm); // Max TX power for better range

    // Prefer SD-based WiFi credentials (config/wifi.json on SD card)
    String sd_ssid, sd_password;
    if (SD.exists("/config/wifi.json") && loadWiFiConfig(sd_ssid, sd_password))
    {
        DEBUG_PRINTF("Found WiFi config on SD: SSID='%s'\n", sd_ssid.c_str());
        WiFi.mode(WIFI_STA);
        WiFi.setAutoReconnect(true); // Auto-reconnect if connection drops
        WiFi.persistent(true);       // Save credentials to flash
        WiFi.begin(sd_ssid.c_str(), sd_password.c_str());

        unsigned long start = millis();
        while (millis() - start < 15000)
        {
            if (WiFi.status() == WL_CONNECTED)
                break;
            delay(500);
            DEBUG_PRINT('.');
        }
        DEBUG_PRINTLN();
        if (WiFi.status() == WL_CONNECTED)
        {
            DEBUG_PRINT("Connected (SD credentials), IP: ");
            DEBUG_PRINTLN(WiFi.localIP());
            DEBUG_PRINTF("Signal strength: %d dBm\n", WiFi.RSSI());
            return;
        }
        DEBUG_PRINTLN("Failed to connect with SD credentials, falling back");
    }

    // Fallback: try compile-time WIFI_STA_SSID macro if provided
    if (strlen(WIFI_STA_SSID) > 0)
    {
        DEBUG_PRINTF("Attempting to connect to WiFi SSID from macro: %s\n", WIFI_STA_SSID);
        WiFi.mode(WIFI_STA);
        WiFi.setAutoReconnect(true);
        WiFi.persistent(true);
        WiFi.begin(WIFI_STA_SSID, WIFI_STA_PASSWORD);

        unsigned long start = millis();
        while (millis() - start < 15000)
        {
            if (WiFi.status() == WL_CONNECTED)
                break;
            delay(500);
            DEBUG_PRINT('.');
        }
        DEBUG_PRINTLN();
        if (WiFi.status() == WL_CONNECTED)
        {
            DEBUG_PRINT("Connected (macro), IP: ");
            DEBUG_PRINTLN(WiFi.localIP());
            DEBUG_PRINTF("Signal strength: %d dBm\n", WiFi.RSSI());
            return;
        }
        DEBUG_PRINTLN("Failed to connect as station, falling back to AP mode");
    }

    // Start AP mode
    WiFi.mode(WIFI_AP);
    WiFi.softAP(WIFI_AP_SSID, WIFI_AP_PASSWORD);
    DEBUG_PRINT("AP started: ");
    DEBUG_PRINTLN(WIFI_AP_SSID);
    DEBUG_PRINT("AP IP: ");
    DEBUG_PRINTLN(WiFi.softAPIP());
}

// Sync system time using NTP servers. Leaves device time unchanged on failure.
void syncTimeWithNTP(unsigned long timeoutMs = 8000)
{
    DEBUG_PRINTLN("Starting NTP sync...");
    // Default to UTC; user can change TZ if needed via setenv("TZ",
    configTime(3600, 3600, "pool.ntp.org", "time.nist.gov");
    unsigned long start = millis();
    time_t now = time(nullptr);
    while (now < (time_t)1600000000 && (millis() - start) < timeoutMs)
    {
        delay(200);
        now = time(nullptr);
    }
    if (now >= (time_t)1600000000)
    {
        struct tm timeinfo;
        gmtime_r(&now, &timeinfo);
        DEBUG_PRINTF("NTP time set: %s", asctime(&timeinfo));
    }
    else
    {
        DEBUG_PRINTLN("NTP sync failed or timed out");
    }
}

void setup()
{
    Serial.begin(115200);
#ifdef RELAY_DEBUG
    DEBUG_PRINTLN("RELAY_DEBUG: enabled");
#endif

    setupPins();

    delay(10);

    tft.init();
    tft.setRotation(3);      // Landscape orientation with USB on left
    tft.invertDisplay(true); // Fix color inversion
    tft.fillScreen(TFT_BLACK);

    // Ensure TFT CS is deselected before talking to SD
    pinMode(TFT_CS, OUTPUT);
    digitalWrite(TFT_CS, HIGH);

    // Initialize SD card on default SPI (VSPI with standard pins)
    // CYD uses: CLK=18, MISO=19, MOSI=23, CS=5
    DEBUG_PRINTLN("Initializing SD card on VSPI (standard pins)...");
    DEBUG_PRINTF("SD CS pin: %d\n", (int)SD_CS_PIN);

    pinMode(SD_CS_PIN, OUTPUT);
    digitalWrite(SD_CS_PIN, HIGH);
    // Use default SPI.begin() which will initialize VSPI with standard pins
    SPI.begin();

    // Small delay to ensure SD card power is stable on some boards
    delay(100);

    bool sdOk = false;
    // Try multiple frequencies from fast to conservative
    const uint32_t sdFreqs[] = {25000000, 16000000, 8000000, 4000000, 1000000};

    for (uint32_t f : sdFreqs)
    {
        DEBUG_PRINTF("SD.begin @ %u Hz... ", (unsigned)f);
        sdOk = SD.begin(SD_CS_PIN, SPI, f); // Use global SPI object
        DEBUG_PRINTLN(sdOk ? "OK" : "FAIL");
        if (sdOk)
            break;
        delay(50);
    }

    if (!sdOk)
    {
        DEBUG_PRINTLN("SD Card initialization failed!");
        tft.fillScreen(TFT_BLACK);
        tft.setTextColor(TFT_RED, TFT_BLACK);
        tft.setTextSize(1);
        tft.drawString("SD CARD FAILED", 10, 10, 2);
        while (1)
            delay(1000);
    }

    // Report SD and WiFi config status
    {
        String sd_ssid, sd_password;
        if (SD.exists("/config/wifi.json") && loadWiFiConfig(sd_ssid, sd_password))
        {
            DEBUG_PRINTF("SD: found /config/wifi.json with SSID='%s'\n", sd_ssid.c_str());
            // show brief message on TFT (no password)
            tft.fillScreen(TFT_BLACK);
            tft.setTextColor(TFT_WHITE, TFT_BLACK);
            tft.drawString("WiFi config on SD:", 10, 10, FONT_SIZE_SMALL);
            tft.drawString(sd_ssid.c_str(), 10, 30, FONT_SIZE_SMALL);
        }
        else if (SD.exists("/config/wifi.json"))
        {
            DEBUG_PRINTLN("SD: /config/wifi.json exists but failed to parse");
        }
        else
        {
            DEBUG_PRINTLN("SD: no /config/wifi.json found");
        }
    }

    // Ensure required directories exist on SD card
    DEBUG_PRINTLN("Checking SD directory structure...");
    const char *requiredDirs[] = {"/config", "/programs", "/logs", "/www", "/updates"};
    for (const char *dir : requiredDirs)
    {
        if (!SD.exists(dir))
        {
            DEBUG_PRINTF("Creating directory: %s\n", dir);
            if (SD.mkdir(dir))
            {
                DEBUG_PRINTF("  Created: %s\n", dir);
            }
            else
            {
                DEBUG_PRINTF("  Failed to create: %s\n", dir);
            }
        }
        else
        {
            DEBUG_PRINTF("  Exists: %s\n", dir);
        }
    }

    // Clean up /updates folder after successful boot (indicates OTA was successful)
    DEBUG_PRINTLN("Cleaning up /updates folder...");
    File updatesDir = SD.open("/updates");
    if (updatesDir && updatesDir.isDirectory())
    {
        File file = updatesDir.openNextFile();
        int cleanedCount = 0;
        while (file)
        {
            String fileName = String(file.name());
            file.close();

            // Delete firmware binaries (.bin files) from updates folder
            if (fileName.endsWith(".bin"))
            {
                String fullPath = String("/updates/") + fileName;
                if (SD.remove(fullPath))
                {
                    DEBUG_PRINTF("  Deleted: %s\n", fullPath.c_str());
                    cleanedCount++;
                }
                else
                {
                    DEBUG_PRINTF("  Failed to delete: %s\n", fullPath.c_str());
                }
            }

            file = updatesDir.openNextFile();
        }
        updatesDir.close();

        if (cleanedCount > 0)
        {
            DEBUG_PRINTF("Cleaned %d firmware file(s) from /updates\n", cleanedCount);
        }
        else
        {
            DEBUG_PRINTLN("No firmware files to clean in /updates");
        }
    }

    // Initialize PID
    tempPID.SetMode(AUTOMATIC);
    tempPID.SetOutputLimits(0, 1023); // Full range for higher-resolution PWM output

    // Load settings from SD (update interval, PID tunings)
    loadSettings();

    // Setup WiFi (SD credentials preferred, STA macro fallback, then AP)
    setupWiFi();

    // Setup web server (network already configured by setupWiFi)
    setupWebServer();

    // Try to sync time from NTP (non-blocking-ish; short timeout)
    syncTimeWithNTP(8000);

    // Initialize relay serial bridge if enabled
    if (USE_RELAY_SERIAL)
    {
        RelaySerial::begin(RELAY_SERIAL_BAUD);
        // ask for initial states
        RelaySerial::requestState(1);
        RelaySerial::requestState(2);
    }

    // Display startup information screen (IP, WiFi signal, NTP time) for 5 seconds
    displayStartupScreen();

    // Draw static UI elements once to avoid flicker
    drawStaticUI();

    DEBUG_PRINTLN("System ready!");
}

void loop()
{
    // Execute program step logic (handles ramp/hold phases)
    executeProgramStep();

    // Read temperature at regular intervals (configurable via web)
    if (millis() - lastTempRead >= tempReadInterval)
    {
        // Read temperature from MAX6675 (no pin switching needed with dedicated Serial)
        rawTemp = thermocouple.readCelsius();
        currentTemp = rawTemp + tempOffset;
        lastTempRead = millis();
        // Store sample for graph (append newest at tempSampleIdx)
        tempSamples[tempSampleIdx] = (float)currentTemp;
        tempSampleIdx++;
        if (tempSampleIdx >= GRAPH_W)
        {
            tempSampleIdx = 0;
            tempSamplesFilled = true;
        }

        // Basic error checking
        if (currentTemp < MIN_TEMP || currentTemp > MAX_TEMP)
        {
            DEBUG_PRINTLN("Temperature reading error!");
            RelaySerial::setRelay(1, false); // Safety shutoff
            RelaySerial::setRelay(2, false);
            return;
        }
    }

    // Compute PID at regular intervals
    if (millis() - lastPIDCompute >= PID_INTERVAL)
    {
        if (tempPID.Compute())
        {
            // IMPORTANT: SSR1 and SSR2 are in SERIES with the heating element
            // Both must be active for current to flow through the resistor
            //
            // Control strategy when program is running:
            // - SSR1: Always ON (provides power path)
            // - SSR2: PWM control (0-1023) for temperature regulation
            //
            // When program is stopped: both OFF

            uint16_t pwm = (uint16_t)constrain((int)round(pidOutput), 0, 1023);
            bool ssr1On = programRunning; // SSR1 on when program active

            // SSR2 PWM: only active when program running AND PID demands power
            // If program stopped, force PWM to 0
            uint16_t ssr2Pwm_value = programRunning ? pwm : 0;
            if (ssr1On != lastSsr1State)
            {
                lastSsr1State = ssr1On;
                RelaySerial::setRelay(1, ssr1On);
            }
            if (ssr2Pwm_value != lastSsr2Pwm)
            {
                lastSsr2Pwm = ssr2Pwm_value;
                RelaySerial::setRelayPWM(2, ssr2Pwm_value);
            }
            // Send commands to the Nano for both relays
            // RelaySerial::setRelay(1, ssr1On);           // SSR1: on/off based on program state
            // RelaySerial::setRelayPWM(2, ssr2Pwm_value); // SSR2: PWM (0..1023) when program running
            ssr2Pwm = ssr2Pwm_value; // Track for display/telemetry

#ifdef RELAY_DEBUG
            DEBUG_PRINTF("[RELAY_DEBUG] PID -> SSR1=%d SSR2_PWM=%u pidOutput=%.1f\n", ssr1On ? 1 : 0, ssr2Pwm_value, pidOutput);
#endif
        }
        lastPIDCompute = millis();
    }

    // Log data point if logging is active and interval has passed
    if (dataLogActive && programRunning && (millis() - lastDataLogMs >= DATA_LOG_INTERVAL))
    {
        logDataPoint();
    }

    // Update display
    updateDisplay(false);

    // Handle WebSocket clients - cleanup more frequently to avoid queue buildup
    static unsigned long lastCleanup = 0;
    if (millis() - lastCleanup >= 1000)
    {
        ws.cleanupClients();
        lastCleanup = millis();
    }

    // Send status update to connected clients
    static unsigned long lastWebUpdate = 0;
    if (millis() - lastWebUpdate >= 500)
    { // Update twice per second
        // Only send if we have connected clients
        if (ws.count() > 0)
        {
            StaticJsonDocument<768> doc; // Increased capacity for consistent field set
            doc["temp"] = currentTemp;
            doc["target"] = targetTemp;
            // Use commanded states (avoid polling latency when using Nano relay board)
            doc["ssr1"] = programRunning; // SSR1 on when program active
            doc["ssr2"] = ssr2Pwm;        // Current PWM value (0..1023)

            // Elapsed seconds (program-relative when running)
            unsigned long elapsedMs = programRunning ? (millis() - programStartMs) : millis();
            doc["elapsedSeconds"] = elapsedMs / 1000;

            // Firmware version
            doc["version"] = FIRMWARE_VERSION;

            // Current intervals and PID
            doc["updateInterval"] = tempReadInterval;
            doc["pidKp"] = pidKp;
            doc["pidKi"] = pidKi;
            doc["pidKd"] = pidKd;
            doc["tempOffset"] = tempOffset;

            // Program execution status (always include even if inactive for UI consistency)
            doc["program"] = programRunning ? 1 : 0;
            doc["programName"] = currentProgramName;
            doc["programStep"] = programLoaded ? (currentProgram.currentStep + 1) : 0;
            doc["totalSteps"] = programLoaded ? currentProgram.numSteps : 0;
            const char *phaseStr = (currentPhase == PHASE_RAMP) ? "RAMP" : (currentPhase == PHASE_HOLD) ? "HOLD"
                                                                                                        : "IDLE";
            doc["phase"] = phaseStr;

            String status;
            serializeJson(doc, status);

            // Send only to clients that can handle messages (not overloaded)
            safeBroadcast(status);
        }

        lastWebUpdate = millis();
    }

    // Process relay serial port I/O
    if (USE_RELAY_SERIAL)
        RelaySerial::loop();

#ifdef RELAY_DEBUG
    static unsigned long lastRelayDebug = 0;
    static uint8_t lastState1 = 0xFF, lastState2 = 0xFF;
    if (millis() - lastRelayDebug >= 2000)
    {
        // Poll states and print diffs
        RelaySerial::requestState(1);
        RelaySerial::requestState(2);
        uint8_t s1 = RelaySerial::getRelayState(1);
        uint8_t s2 = RelaySerial::getRelayState(2);
        if (s1 != lastState1 || s2 != lastState2)
        {
            DEBUG_PRINTF("[RELAY_DEBUG] state1=%u state2=%u currentTemp=%.1f\n", s1, s2, currentTemp);
            lastState1 = s1;
            lastState2 = s2;
        }
        lastRelayDebug = millis();
    }
#endif

    // Small delay to prevent CPU hogging
    delay(10);
}