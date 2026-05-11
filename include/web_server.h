#pragma once

// =============================================================================
// web_server.h — Serveur HTTP et WebSocket
//
// Ce module regroupe toute la logique réseau qui était dans main.cpp :
//   - setupWebServer() avec tous les server.on()
//   - handleWebSocketMessage() / onWebSocketEvent()
//   - safeBroadcast()
//   - buildStatusJson() — serialisation centralisée (remplace la duplication
//     entre /status et le broadcast WebSocket dans loop())
//
// Les objets globaux `server` et `ws` restent déclarés dans main.cpp
// (nécessaire pour que les modules display et program_executor puissent
// appeler safeBroadcast si besoin). Ils sont déclarés extern ici.
// =============================================================================

#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <ArduinoJson.h>

// Déclarations extern des objets réseau définis dans main.cpp
extern AsyncWebServer server;
extern AsyncWebSocket ws;

// Initialise le serveur web et enregistre tous les endpoints
void setupWebServer();

// Envoie un message texte à tous les clients WebSocket dont la file n'est pas saturée
void safeBroadcast(const String &message);

// Construit le JSON de statut complet (temp, PID, programme, phase...)
// Utilisé à la fois par /status (HTTP GET) et le broadcast WebSocket (loop)
String buildStatusJson();
