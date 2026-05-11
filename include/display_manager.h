#pragma once

// =============================================================================
// display_manager.h — Gestion de l'affichage TFT
//
// Extrait de main.cpp :
//   - drawStaticUI()          → éléments fixes (cadres, labels)
//   - updateDisplay(force)    → éléments dynamiques (temp, SSR, graphe/timeline)
//   - drawProgramTimeline()   → zone graphe en bas de l'écran pendant l'exécution
//   - drawIdleGraph()         → graphe de température circulaire en mode veille
//   - displayStartupScreen()  → écran d'info au démarrage (IP, WiFi, NTP)
//
// Toutes les coordonnées TFT sont centralisées dans display_layout.h
// (évite les nombres magiques dans le code).
// =============================================================================

// Dessine les éléments statiques (une seule fois au démarrage ou après reset)
void drawStaticUI();

// Met à jour les éléments dynamiques.
// force=true : ignorer le rate-limit de 1 seconde (ex: changement d'état)
void updateDisplay(bool force);

// Affiche l'écran de démarrage (IP, signal WiFi, heure NTP) pendant 5 secondes
void displayStartupScreen();
