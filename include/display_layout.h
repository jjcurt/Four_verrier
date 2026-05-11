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

// --- Ligne Prog : "Prog:" statique, nom dynamique ---
#define DISP_PROG_ROW_Y      58
#define DISP_PROG_LABEL_X     5   // "Prog:"
#define DISP_PROG_NAME_X     52   // nom du programme
#define DISP_PROG_NAME_W    268   // zone à effacer (52..320)

// --- Ligne statut : Etape | [temps étape] | Temps total ---
#define DISP_STATUS_ROW_Y    74
#define DISP_STEP_LABEL_X     5   // "Etape:"
#define DISP_STEP_X          52   // numéro étape "x/x"
#define DISP_STEP_W          36   // zone à effacer ("9/9" + marge)
#define DISP_STEP_TIME_X     92   // "[H:MM:SS]" temps étape (sans label)
#define DISP_STEP_TIME_W     78   // zone à effacer
#define DISP_TIME_LABEL_X   174   // "Temps:"
#define DISP_TIME_X         220   // valeur H:MM:SS total programme
#define DISP_TIME_W          90   // zone à effacer ("9:59:59")

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

// --- Palette corrigée pour panel CYD (ESP32-2432S028R) ---
// Ce panel a un swap R↔B physique avec TFT_RGB_ORDER=1 (seul mode sans inversion).
// Chaque alias envoie la couleur R↔B-inversée pour que l'affichage soit correct.
// Couleurs symétriques (TFT_BLACK, WHITE, GREEN, DARKGREY) : inchangées.
#define DISP_YELLOW  TFT_CYAN     // envoyer 0x07FF → affiche jaune (255,255,0)
#define DISP_CYAN    TFT_YELLOW   // envoyer 0xFFE0 → affiche cyan  (0,255,255)
#define DISP_RED     TFT_BLUE     // envoyer 0x001F → affiche rouge (255,0,0)
#define DISP_BLUE    TFT_RED      // envoyer 0xF800 → affiche bleu  (0,0,255)
#define DISP_ORANGE  0x05BF       // envoyer (R=0,G=45,B=31) → affiche orange (255,180,0)
#define DISP_VIOLET  0xE152       // envoyer (R=28,G=10,B=18) → affiche violet (180,46,226)
