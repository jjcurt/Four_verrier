# Aperçu Visuel - Interface de Mise à Jour Firmware

## Vue d'ensemble de la page `/files.html`

```
┌─────────────────────────────────────────────────────────────────────┐
│  📁 Gestionnaire de fichiers              ← Retour à l'accueil      │
└─────────────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────────┐
│  📄 Fichiers HTML (Interface Web)                                   │
│  ─────────────────────────────────────────────────────────────────  │
│  ┌─────────────────────────────────────────────────────┐            │
│  │ index.html                    ⬇️ Télécharger 🗑️ Supprimer │
│  │ control.html                  ⬇️ Télécharger 🗑️ Supprimer │
│  │ files.html                    ⬇️ Télécharger 🗑️ Supprimer │
│  └─────────────────────────────────────────────────────┘            │
│  [Choisir fichier .html]  [📤 Téléverser HTML]                      │
└─────────────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────────┐
│  📋 Programmes de cuisson                                           │
│  ─────────────────────────────────────────────────────────────────  │
│  ┌─────────────────────────────────────────────────────┐            │
│  │ essai1.json                   ⬇️ Télécharger 🗑️ Supprimer │
│  │ test.json                     ⬇️ Télécharger 🗑️ Supprimer │
│  └─────────────────────────────────────────────────────┘            │
│  [Choisir fichier .json]  [📤 Téléverser Programme]                 │
└─────────────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────────┐
│  📊 Logs                                                             │
│  ─────────────────────────────────────────────────────────────────  │
│  ┌─────────────────────────────────────────────────────┐            │
│  │ essai1_20251115_143022.csv    ⬇️ Télécharger 🗑️ Supprimer │
│  └─────────────────────────────────────────────────────┘            │
└─────────────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────────┐
│  🔧 Mise à jour Firmware (OTA)                                      │
│  ─────────────────────────────────────────────────────────────────  │
│  ┌─────────────────────────────────────────────────────────────┐   │
│  │ ⚠️ Mise à jour SD en attente                                 │   │
│  │ Un firmware sera appliqué au prochain redémarrage            │   │
│  │ Fichier: firmware.bin                                        │   │
│  └─────────────────────────────────────────────────────────────┘   │
│                                                                      │
│  [Choisir fichier .bin]  [📤 Téléverser Firmware (.bin)]            │
│  Téléverse le fichier .bin vers /updates/                           │
│  (crée automatiquement le marqueur .pending)                        │
│                                                                      │
│  ─────────────────────────────────────────────────────────────────  │
│                                                                      │
│  Fichiers firmware disponibles sur carte SD:                        │
│  ┌────────────────────────┐                                         │
│  │ firmware.bin        ▼  │                                         │
│  └────────────────────────┘                                         │
│                                                                      │
│  ┌───────────────────────────┐  ┌──────────────────────────────┐   │
│  │ ⚡ Appliquer OTA Immédiat │  │ 💾 Appliquer au Redémarrage  │   │
│  └───────────────────────────┘  └──────────────────────────────┘   │
│         (orange)                         (vert - recommandé)        │
│                                                                      │
│  ⚡ OTA Immédiat: Applique la mise à jour via WiFi maintenant       │
│     (risque si signal faible)                                       │
│  💾 Via SD: Applique au prochain boot depuis carte SD               │
│     (plus sûr, recommandé)                                          │
│  ⚠️ L'appareil redémarrera dans les deux cas                        │
└─────────────────────────────────────────────────────────────────────┘
```

---

## Workflow Visuel - Mise à Jour Réussie

### Étape 1 : Upload du Firmware

```
┌─────────────────────────────────────────┐
│  Interface Web - /files.html            │
├─────────────────────────────────────────┤
│                                         │
│  [firmware.bin sélectionné]             │
│                                         │
│  [📤 Téléverser Firmware (.bin)]       │
│          ↓ clic                         │
│                                         │
│  "Téléversement en cours..."            │
│  ████████████████ 100%                  │
│                                         │
│  ✅ "Firmware téléversé avec succès!"   │
│                                         │
└─────────────────────────────────────────┘
```

### Étape 2 : Statut Mis à Jour Automatiquement

```
┌─────────────────────────────────────────────────┐
│  🔧 Mise à jour Firmware (OTA)                  │
├─────────────────────────────────────────────────┤
│  ┌───────────────────────────────────────────┐ │
│  │ ⚠️ Mise à jour SD en attente              │ │
│  │ Un firmware sera appliqué au prochain     │ │
│  │ redémarrage. Fichier: firmware.bin        │ │
│  └───────────────────────────────────────────┘ │
│                      ↑                          │
│           Détecté automatiquement               │
│           (.pending existe)                     │
└─────────────────────────────────────────────────┘
```

### Étape 3 : Confirmation Utilisateur

```
┌─────────────────────────────────────────────────┐
│  Bouton cliqué:                                 │
│  [💾 Appliquer au Redémarrage (SD)]             │
│          ↓                                      │
│                                                 │
│  ┌───────────────────────────────────────────┐ │
│  │  💾 MISE À JOUR VIA CARTE SD              │ │
│  │                                           │ │
│  │  Fichier: firmware.bin                    │ │
│  │                                           │ │
│  │  ✅ AVANTAGES:                            │ │
│  │  - Appliquée au prochain boot             │ │
│  │    (pas de risque WiFi)                   │ │
│  │  - Retry automatique si échec             │ │
│  │  - Plus sûr que OTA WiFi                  │ │
│  │                                           │ │
│  │  L'ESP32 va redémarrer maintenant et      │ │
│  │  appliquer la mise à jour.                │ │
│  │                                           │ │
│  │  Continuer?                               │ │
│  │                                           │ │
│  │      [Annuler]      [OK]                  │ │
│  └───────────────────────────────────────────┘ │
└─────────────────────────────────────────────────┘
```

### Étape 4 : Affichage sur TFT Durant le Boot

```
┌─────────────────────────────────────┐
│  ESP32 - Écran TFT 2.8"             │
├─────────────────────────────────────┤
│                                     │
│  MISE A JOUR FIRMWARE               │
│  Depuis carte SD...                 │
│  NE PAS ETEINDRE !                  │
│                                     │
│  75% (307200/409600 bytes)          │
│  ▓▓▓▓▓▓▓▓▓▓▓▓▓▓░░░░░                │
│                                     │
│  MISE A JOUR REUSSIE !              │
│  Redemarrage...                     │
│                                     │
└─────────────────────────────────────┘
         ↓ 3 secondes
         ↓
    REDÉMARRAGE
         ↓
    NOUVEAU FIRMWARE
```

### Étape 5 : Vérification Post-Update

```
┌─────────────────────────────────────┐
│  ESP32 - Écran de Démarrage         │
├─────────────────────────────────────┤
│                                     │
│  Four Verrier                       │
│  Version 1.1.0  ← Nouvelle version  │
│                                     │
│  ──────────────────────────────     │
│                                     │
│  Adresse IP:                        │
│  192.168.1.100                      │
│                                     │
│  Signal WiFi:                       │
│  -45 dBm  ▓▓▓▓▓                     │
│                                     │
│  Heure NTP:                         │
│  16/11/2025 14:30:45                │
│                                     │
│  Demarrage en cours...              │
│                                     │
└─────────────────────────────────────┘
```

### Étape 6 : Interface Web Après Update

```
┌─────────────────────────────────────────────────┐
│  🔧 Mise à jour Firmware (OTA)                  │
├─────────────────────────────────────────────────┤
│  ┌───────────────────────────────────────────┐ │
│  │ ℹ️ Aucune mise à jour SD en attente       │ │
│  │ Vous pouvez téléverser un nouveau         │ │
│  │ firmware.                                 │ │
│  └───────────────────────────────────────────┘ │
│                      ↑                          │
│          .pending et firmware.bin               │
│          supprimés automatiquement              │
└─────────────────────────────────────────────────┘
```

---

## Comparaison Visuelle des Deux Méthodes

```
╔═══════════════════════════════════════════════════════════════╗
║              MÉTHODE 1: OTA IMMÉDIAT (WiFi)                   ║
╠═══════════════════════════════════════════════════════════════╣
║                                                               ║
║  ESP32 ←------ WiFi ------→ Interface Web                    ║
║    ↓                                                          ║
║  Télécharge firmware.bin via WiFi                            ║
║    ↓                                                          ║
║  Applique immédiatement                                      ║
║    ↓                                                          ║
║  [SI ÉCHEC RÉSEAU → ESP32 INACCESSIBLE ⚠️]                   ║
║    ↓                                                          ║
║  Redémarre avec nouveau firmware                             ║
║                                                               ║
║  ⚡ RAPIDE mais RISQUÉ                                        ║
║                                                               ║
╚═══════════════════════════════════════════════════════════════╝

╔═══════════════════════════════════════════════════════════════╗
║          MÉTHODE 2: SD + REDÉMARRAGE (Recommandé)             ║
╠═══════════════════════════════════════════════════════════════╣
║                                                               ║
║  Upload via WiFi → Copie sur carte SD                        ║
║    ↓                                                          ║
║  Création .pending                                           ║
║    ↓                                                          ║
║  Redémarrage ESP32                                           ║
║    ↓                                                          ║
║  Boot → Détection .pending                                   ║
║    ↓                                                          ║
║  Lecture firmware.bin depuis SD (pas de WiFi)                ║
║    ↓                                                          ║
║  Application de la mise à jour                               ║
║    ↓                                                          ║
║  [SI ÉCHEC → Suppression .pending, retry possible]           ║
║    ↓                                                          ║
║  Redémarre avec nouveau firmware                             ║
║    ↓                                                          ║
║  Nettoyage auto (.pending + firmware.bin supprimés)          ║
║                                                               ║
║  💾 Plus lent mais FIABLE et SÛR ✅                           ║
║                                                               ║
╚═══════════════════════════════════════════════════════════════╝
```

---

## Codes Couleur de l'Interface

```
┌──────────────────────────────────────────────┐
│  Élément              Couleur    Signification│
├──────────────────────────────────────────────┤
│  Bouton Normal        #2196F3   Bleu (neutre) │
│  Bouton Danger        #f44336   Rouge (danger)│
│  OTA Immédiat         #FF9800   Orange (warn) │
│  Via SD               #4CAF50   Vert (safe)   │
│  Status Info          #E3F2FD   Bleu clair    │
│  Status Warning       #FFF3E0   Orange clair  │
│  Status Success       #E8F5E9   Vert clair    │
└──────────────────────────────────────────────┘
```

---

**Ce visuel aide à comprendre rapidement le nouveau workflow de mise à jour !**
