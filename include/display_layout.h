#pragma once

// =============================================================================
// display_layout.h — Coordonnées et dimensions de l'affichage TFT (320×240)
//
// Centralise toutes les constantes de mise en page qui étaient éparpillées
// dans drawStaticUI() et updateDisplay() sous forme de nombres magiques.
//
// Organisation de l'écran :
//
//   ┌──────────────────────────────────────────┐  y=0
//   │  TEMP FOUR (grande)  │ SSR1 │ SSR2 bar  │
//   │  XXX.X °C            │ [■]  │ [========]│  y=0..55
//   │  Cible: XXX.X        │      │            │
//   ├──────────────────────────────────────────┤  y=56
//   │ Prog: xxxxxx │ Etape: x/x │ Temps: xx:xx│  y=56..89
//   ├──────────────────────────────────────────┤  y=90
//   │                                          │
//   │   ZONE GRAPHE (idle) ou TIMELINE (prog)  │  y=90..239
//   │                                          │
//   └──────────────────────────────────────────┘  y=239
// =============================================================================

// --- Bandeau supérieur : température ---
#define DISP_TEMP_X          5
#define DISP_TEMP_Y          5
#define DISP_TEMP_W        150
#define DISP_TEMP_H         32
#define DISP_TEMP_UNIT_X   120
#define DISP_TEMP_UNIT_Y    10

// Température cible
#define DISP_TARGET_X        5
#define DISP_TARGET_Y       40

// --- SSR1 ---
#define DISP_SSR1_LABEL_X  170
#define DISP_SSR1_LABEL_Y   10
#define DISP_SSR1_BOX_X    210
#define DISP_SSR1_BOX_Y     10
#define DISP_SSR1_BOX_W     20
#define DISP_SSR1_BOX_H     20

// --- SSR2 PWM bar ---
#define DISP_SSR2_LABEL_X  170
#define DISP_SSR2_LABEL_Y   40
#define DISP_SSR2_BAR_X    210
#define DISP_SSR2_BAR_Y     38
#define DISP_SSR2_BAR_W     82
#define DISP_SSR2_BAR_H     14
// Barre intérieure (padding 1px)
#define DISP_SSR2_FILL_X   211
#define DISP_SSR2_FILL_Y    41
#define DISP_SSR2_FILL_MAX  78  // pixels pour 100% PWM

// --- Ligne d'état programme ---
#define DISP_PROG_ROW_Y     70

#define DISP_PROG_NAME_X     5
#define DISP_PROG_NAME_W   150

#define DISP_STEP_X        160
#define DISP_STEP_W         60

#define DISP_TIME_X        230
#define DISP_TIME_W         90

#define DISP_STATE_X       330
#define DISP_STATE_W        80

// --- Zone graphe / timeline ---
#define DISP_GRAPH_TOP_Y   100   // Y de début de la zone inférieure
#define DISP_GRAPH_LEFT_MARGIN  40
#define DISP_GRAPH_RIGHT_MARGIN  5
#define DISP_GRAPH_HEIGHT  125   // hauteur de la zone graphe en mode idle

// Timeline (mode programme)
#define DISP_TIMELINE_X      0
#define DISP_TIMELINE_Y    100
#define DISP_TIMELINE_H    140
#define DISP_TIMELINE_PAD_X 10  // marge intérieure gauche/droite
#define DISP_TIMELINE_PAD_Y 10  // marge intérieure haut/bas
#define DISP_TIMELINE_STEP_GAP 6  // espace entre les blocs d'étape
