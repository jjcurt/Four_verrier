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
// IP statique optionnelle via wifi.json : champs staticIp, gateway, subnet, dns.
// Si absents → DHCP. saveWiFiConfig() préserve ces champs lors d'une mise à jour
// des credentials depuis l'interface web.
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

struct WifiStaticConfig
{
    bool      enabled = false;
    IPAddress ip;
    IPAddress gateway;
    IPAddress subnet;
    IPAddress dns;
};

inline bool loadWiFiConfig(String &ssid, String &password, WifiStaticConfig &staticCfg)
{
    if (!SD.exists(WIFI_CONFIG_PATH)) return false;
    File f = SD.open(WIFI_CONFIG_PATH, FILE_READ);
    if (!f) return false;

    StaticJsonDocument<384> doc;
    DeserializationError err = deserializeJson(doc, f);
    f.close();
    if (err) return false;

    ssid     = String(doc["ssid"]     | "");
    password = String(doc["password"] | "");

    const char *staticIpStr = doc["staticIp"] | "";
    if (strlen(staticIpStr) > 0)
    {
        staticCfg.enabled = true;
        staticCfg.ip.fromString(staticIpStr);
        staticCfg.gateway.fromString(doc["gateway"] | "192.168.0.1");
        staticCfg.subnet.fromString(doc["subnet"]   | "255.255.255.0");
        staticCfg.dns.fromString(doc["dns"]         | "192.168.0.1");
    }

    return ssid.length() > 0;
}

inline bool saveWiFiConfig(const String &ssid,     const String &password,
                            const String &staticIp = "", const String &gateway = "",
                            const String &subnet   = "", const String &dns     = "")
{
    if (!SD.exists("/config")) SD.mkdir("/config");

    // Lire d'abord le fichier existant pour merger
    StaticJsonDocument<384> doc;
    if (SD.exists(WIFI_CONFIG_PATH))
    {
        File fRead = SD.open(WIFI_CONFIG_PATH, FILE_READ);
        if (fRead) { deserializeJson(doc, fRead); fRead.close(); }
    }

    doc["ssid"] = ssid.c_str();
    // Ne pas écraser le mot de passe si le champ est vide (formulaire web)
    if (password.length() > 0)
        doc["password"] = password.c_str();

    // IP statique : si staticIp non vide → sauvegarder les 4 champs
    //               si vide → supprimer (retour au DHCP)
    if (staticIp.length() > 0)
    {
        doc["staticIp"] = staticIp.c_str();
        doc["gateway"]  = gateway.c_str();
        doc["subnet"]   = subnet.c_str();
        doc["dns"]      = dns.c_str();
    }
    else
    {
        doc.remove("staticIp");
        doc.remove("gateway");
        doc.remove("subnet");
        doc.remove("dns");
    }

    // FILE_WRITE appends sur ESP32 — supprimer avant d'écrire
    if (SD.exists(WIFI_CONFIG_PATH))
        SD.remove(WIFI_CONFIG_PATH);

    File f = SD.open(WIFI_CONFIG_PATH, FILE_WRITE);
    if (!f) return false;
    bool ok = (serializeJsonPretty(doc, f) > 0);
    f.close();
    return ok;
}

inline void setupWiFi()
{
    WiFi.setTxPower(WIFI_POWER_19_5dBm);

    // 1. Credentials depuis SD (/config/wifi.json)
    String sd_ssid, sd_password;
    WifiStaticConfig staticCfg;
    if (loadWiFiConfig(sd_ssid, sd_password, staticCfg))
    {
        DEBUG_PRINTF("WiFi SD config: SSID='%s' IP=%s\n", sd_ssid.c_str(),
                     staticCfg.enabled ? staticCfg.ip.toString().c_str() : "DHCP");
        WiFi.mode(WIFI_STA);
        WiFi.setAutoReconnect(true);
        WiFi.persistent(true);
        if (staticCfg.enabled)
            WiFi.config(staticCfg.ip, staticCfg.gateway, staticCfg.subnet, staticCfg.dns);
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

    // 2. Credentials compile-time (secrets.h) — DHCP uniquement
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
