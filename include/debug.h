#pragma once

// =============================================================================
// debug.h — Macros de debug conditionnelles
//
// Définir DEBUG_SERIAL avant d'inclure ce fichier (ou dans platformio.ini
// via build_flags = -DDEBUG_SERIAL) pour activer la sortie série.
//
// À inclure dans tous les modules qui utilisent DEBUG_PRINT / DEBUG_PRINTLN /
// DEBUG_PRINTF. main.cpp définissait ces macros localement — elles sont
// désormais centralisées ici.
// =============================================================================

#ifdef DEBUG_SERIAL
  #define DEBUG_PRINT(x)          Serial.print(x)
  #define DEBUG_PRINTLN(x)        Serial.println(x)
  #define DEBUG_PRINTF(fmt, ...)  Serial.printf(fmt, ##__VA_ARGS__)
#else
  #define DEBUG_PRINT(x)
  #define DEBUG_PRINTLN(x)
  #define DEBUG_PRINTF(fmt, ...)
#endif
