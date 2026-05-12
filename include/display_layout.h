#pragma once

// =============================================================================
// display_layout.h — Coordonnées et dimensions de l'affichage TFT (320×240)
//
//   ┌──────────────────────────────────────────┐  y=0
//   │ Four: [XXX.X] C         RAMP            │  y=5..31   ← PHASE font 4, colorée
//   │ Cible:[XXX.X] C    SSR2 [========]      │  y=32..56
//   ├──────────────────────────────────────────┤  y=57
//   │ Prog: xxxxxxxxxxxxxxxxxxxxxxxxx          │  y=58..73
//   │ Etape:x/x  [H:MM:SS]  Temps:H:MM:SS     │  y=74..90  ← temps étape entre crochets
//   ├──────────────────────────────────────────┤  y=91
//   │                                          │
//   │   ZONE GRAPHE (idle) ou TIMELINE (prog)  │  y=92..239
//   │                         ← graduations X  │
//   └──────────────────────────────────────────┘  y=239
// =============================================================================

#define SCREEN_WIDTH  320

// --- Température four (label statique dans drawStaticUI, valeur dynamique) ---
#define DISP_TEMP_LABEL_X    5    // "Four:"
#define DISP_TEMP_LABEL_Y    8    // centré verticalement avec font 4
#define DISP_TEMP_X         52    // valeur dynamique (font 4)
#define DISP_TEMP_Y          5
#define DISP_TEMP_W        105    // zone à effacer avant réécriture
#define DISP_TEMP_H         26    // hauteur glyphe font 4
#define DISP_TEMP_UNIT_X   160    // "C" statique
#define DISP_TEMP_UNIT_Y    10

// --- Température cible ---
#define DISP_TARGET_LABEL_X   5   // "Cible:"
#define DISP_TARGET_LABEL_Y  36
#define DISP_TARGET_X        52   // valeur dynamique (font 4)
#define DISP_TARGET_Y        32
#define DISP_TARGET_W       105
#define DISP_TARGET_H        26
#define DISP_TARGET_UNIT_X  160   // "C" statique
#define DISP_TARGET_UNIT_Y   37

// --- Phase (haut droite, remplace SSR1 — valeur dynamique font 4, colorée) ---
#define DISP_PHASE_TOP_X    195   // IDLE/RAMP/HOLD/STAB (font 4)
#define DISP_PHASE_TOP_Y      5   // aligné avec DISP_TEMP_Y
#define DISP_PHASE_TOP_W    123   // zone à effacer (195..318)
#define DISP_PHASE_TOP_H     26   // hauteur font 4

// --- SSR2 (label statique, barre dynamique) ---
#define DISP_SSR2_LABEL_X   195   // "SSR2"
#define DISP_SSR2_LABEL_Y    37
#define DISP_SSR2_BAR_X     240   // cadre statique dessiné dans drawStaticUI
#define DISP_SSR2_BAR_Y      35
#define DISP_SSR2_BAR_W      78
#define DISP_SSR2_BAR_H      16
#define DISP_SSR2_FILL_X    241   // remplissage dynamique (1px de marge)
#define DISP_SSR2_FILL_Y     36
#define DISP_SSR2_FILL_MAX   74   // pixels pour 100% PWM (78-4)

// --- Ligne Prog : "Prog:" statique, nom dynamique, temps total à droite ---
#define DISP_PROG_ROW_Y      58
#define DISP_PROG_LABEL_X     5   // "Prog:"
#define DISP_PROG_NAME_X     52   // nom du programme
#define DISP_PROG_NAME_W    165   // zone à effacer (52..217) — limité pour laisser place au temps total
#define DISP_PROG_TOTAL_X   222   // temps total écoulé programme (blanc, calé à droite)
#define DISP_PROG_TOTAL_W    98   // zone à effacer (222..320)

// --- Ligne statut : Etape | rampe écoulée (orange) | hold restant (vert) ---
#define DISP_STATUS_ROW_Y    74
#define DISP_STEP_LABEL_X     5   // "Etape:"
#define DISP_STEP_X          52   // numéro étape "x/x"
#define DISP_STEP_W          36   // zone à effacer ("9/9" + marge)
#define DISP_STEP_TIME_X     92   // temps rampe écoulé (orange, "MM:SS")
#define DISP_STEP_TIME_W     78   // zone à effacer
#define DISP_HOLD_TIME_X    185   // décompteur HOLD restant (vert, "MM:SS")
#define DISP_HOLD_TIME_W     95   // zone à effacer (185..280)

// --- Zone graphe / timeline ---
#define DISP_GRAPH_TOP_Y     92
#define DISP_GRAPH_LEFT_MARGIN   42
#define DISP_GRAPH_RIGHT_MARGIN   5
#define DISP_GRAPH_HEIGHT       148   // 92+148 = 240
#define DISP_GRAPH_XLABEL_H      12   // réservé en bas pour graduations X

// Timeline (mode programme)
#define DISP_TIMELINE_X       0
#define DISP_TIMELINE_Y      92
#define DISP_TIMELINE_H     148
#define DISP_TIMELINE_PAD_X  10
#define DISP_TIMELINE_PAD_Y  10
#define DISP_TIMELINE_STEP_GAP  6

// --- Palette corrigée pour panel CYD ESP32-2432S028R ---
// Transform hardware : invert(bits) composé avec swap(R,B)
// Pour afficher couleur C, envoyer T(C) = invert(swap_RB(C))  [T est son propre inverse]
// Toutes les couleurs sont affectées — y compris BLACK, WHITE, GREEN.
#define DISP_BLACK    0xFFFF  // T(0x0000) → affiche noir
#define DISP_WHITE    0x0000  // T(0xFFFF) → affiche blanc
#define DISP_DARKGREY 0x8410  // T(0x7BEF) → affiche gris sombre
#define DISP_GREEN    0xF81F  // T(0x07E0) → affiche vert
#define DISP_YELLOW   0xF800  // T(0xFFE0) → affiche jaune
#define DISP_CYAN     0x001F  // T(0x07FF) → affiche cyan
#define DISP_RED      0xFFE0  // T(0xF800) → affiche rouge
#define DISP_BLUE     0x07FF  // T(0x001F) → affiche bleu
#define DISP_ORANGE   0xFA40  // T(0xFDA0) → affiche orange
#define DISP_VIOLET   0x1EAD  // T(0x915C) → affiche violet
#define DISP_MAGENTA  0x9E6C  // T(0x998C) → affiche rgb(153,51,102) — descente contrôlée
