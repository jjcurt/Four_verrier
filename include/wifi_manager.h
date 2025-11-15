#pragma once

#include <Arduino.h>
#include <SD.h>
#include <ArduinoJson.h>

// Path for WiFi config on SD
static const char* WIFI_CONFIG_PATH = "/config/wifi.json";

// Load WiFi credentials from SD into given Strings. Returns true if successful.
bool loadWiFiConfig(String &ssid, String &password) {
    if (!SD.exists(WIFI_CONFIG_PATH)) return false;
    File f = SD.open(WIFI_CONFIG_PATH, FILE_READ);
    if (!f) return false;

    // Parse JSON
    StaticJsonDocument<256> doc;
    DeserializationError err = deserializeJson(doc, f);
    f.close();
    if (err) return false;

    const char* s = doc["ssid"] | "";
    const char* p = doc["password"] | "";
    ssid = String(s);
    password = String(p);
    return ssid.length() > 0;
}

// Save WiFi credentials to SD. Returns true on success.
bool saveWiFiConfig(const String &ssid, const String &password) {
    // Ensure config directory exists
    if (!SD.exists("/config")) {
        SD.mkdir("/config");
    }

    File f = SD.open(WIFI_CONFIG_PATH, FILE_WRITE);
    if (!f) return false;

    StaticJsonDocument<256> doc;
    doc["ssid"] = ssid.c_str();
    doc["password"] = password.c_str();

    if (serializeJson(doc, f) == 0) {
        f.close();
        return false;
    }
    f.close();
    return true;
}
