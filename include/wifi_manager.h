#pragma once

// =============================================================================
// wifi_manager.h — Connexion WiFi et synchronisation NTP
//
// Extrait de main.cpp :
//   - setupWiFi()            → STA (SD credentials → macro fallback → AP)
//   - syncTimeWithNTP(ms)    → NTP avec timeout
//   - loadWiFiConfig(s, p)   → lit /config/wifi.json sur SD
//   - saveWiFiConfig(s, p)   → écrit /config/wifi.json sur SD
//
// Implémentations inline (ce fichier est inclus uniquement par main.cpp).
// Les macros DEBUG_* doivent être définies avant cet include.
// =============================================================================

#include <Arduino.h>
#include <WiFi.h>
#include <SD.h>
#include <time.h>
#include <ArduinoJson.h>
#include "secrets.h"

static const char *WIFI_CONFIG_PATH = "/config/wifi.json";

inline bool loadWiFiConfig(String &ssid, String &password)
{
    if (!SD.exists(WIFI_CONFIG_PATH)) return false;
    File f = SD.open(WIFI_CONFIG_PATH, FILE_READ);
    if (!f) return false;

    StaticJsonDocument<256> doc;
    DeserializationError err = deserializeJson(doc, f);
    f.close();
    if (err) return false;

    ssid     = String(doc["ssid"]     | "");
    password = String(doc["password"] | "");
    return ssid.length() > 0;
}

inline bool saveWiFiConfig(const String &ssid, const String &password)
{
    if (!SD.exists("/config")) SD.mkdir("/config");
    File f = SD.open(WIFI_CONFIG_PATH, FILE_WRITE);
    if (!f) return false;

    StaticJsonDocument<256> doc;
    doc["ssid"]     = ssid.c_str();
    doc["password"] = password.c_str();
    bool ok = (serializeJson(doc, f) > 0);
    f.close();
    return ok;
}

inline void setupWiFi()
{
    WiFi.setTxPower(WIFI_POWER_19_5dBm);

    // 1. Credentials sur SD (/config/wifi.json)
    String sd_ssid, sd_password;
    if (loadWiFiConfig(sd_ssid, sd_password))
    {
        DEBUG_PRINTF("WiFi SD config: SSID='%s'\n", sd_ssid.c_str());
        WiFi.mode(WIFI_STA);
        WiFi.setAutoReconnect(true);
        WiFi.persistent(true);
        WiFi.begin(sd_ssid.c_str(), sd_password.c_str());

        unsigned long start = millis();
        while (millis() - start < 15000 && WiFi.status() != WL_CONNECTED)
            delay(500);

        if (WiFi.status() == WL_CONNECTED)
        {
            DEBUG_PRINTF("Connected (SD), IP: %s, RSSI: %d dBm\n",
                         WiFi.localIP().toString().c_str(), WiFi.RSSI());
            return;
        }
        DEBUG_PRINTLN("SD credentials failed, trying fallback");
    }

    // 2. Credentials compile-time (secrets.h)
    if (strlen(WIFI_STA_SSID) > 0)
    {
        WiFi.mode(WIFI_STA);
        WiFi.setAutoReconnect(true);
        WiFi.persistent(true);
        WiFi.begin(WIFI_STA_SSID, WIFI_STA_PASSWORD);

        unsigned long start = millis();
        while (millis() - start < 15000 && WiFi.status() != WL_CONNECTED)
            delay(500);

        if (WiFi.status() == WL_CONNECTED)
        {
            DEBUG_PRINTF("Connected (macro), IP: %s\n",
                         WiFi.localIP().toString().c_str());
            return;
        }
        DEBUG_PRINTLN("Macro credentials failed, starting AP");
    }

    // 3. Mode AP fallback
    WiFi.mode(WIFI_AP);
    WiFi.softAP(WIFI_AP_SSID, WIFI_AP_PASSWORD);
    DEBUG_PRINTF("AP mode: %s, IP: %s\n",
                 WIFI_AP_SSID, WiFi.softAPIP().toString().c_str());
}

inline void syncTimeWithNTP(unsigned long timeoutMs = 8000)
{
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
        DEBUG_PRINTF("NTP OK: %s", asctime(&timeinfo));
    }
    else
    {
        DEBUG_PRINTLN("NTP sync timeout");
    }
}
