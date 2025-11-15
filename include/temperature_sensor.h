#pragma once
#include <Arduino.h>
#include <max6675.h>
#include "config.h"

class TemperatureSensor {
public:
    TemperatureSensor() : thermocouple(TEMP_SCK_PIN, TEMP_CS_PIN, TEMP_SO_PIN) {
        lastReadTime = 0;
        lastTemp = 0;
    }

    void begin() {
        // MAX6675 needs a bit of time to settle
        delay(500);
    }

    float readTemperature() {
        unsigned long currentTime = millis();
        
        // Only read every TEMP_READ_INTERVAL ms to avoid excessive reads
        if (currentTime - lastReadTime >= TEMP_READ_INTERVAL) {
            lastTemp = thermocouple.readCelsius();
            lastReadTime = currentTime;
            
            // Basic error checking
            if (lastTemp < MIN_TEMP || lastTemp > MAX_TEMP) {
                return -1; // Error condition
            }
        }
        
        return lastTemp;
    }

    bool isConnected() {
        float temp = readTemperature();
        return temp >= MIN_TEMP && temp <= MAX_TEMP;
    }

private:
    MAX6675 thermocouple;
    unsigned long lastReadTime;
    float lastTemp;
};