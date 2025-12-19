# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [1.6.1] - 2025-12-19

### Added
- **Complete Maintenance Log Implementation** (21 columns):
  - Temperature tracking: `CurrentTemp`, `FilteredTemp` (low-pass filter), `RawTemp`, `TargetTemp`, `TempOffset`
  - PID internals: `Error`, `Pterm`, `Iterm`, `Dterm`, `PidKp`, `PidKi`, `PidKd`, `PidOutput`
  - Time tracking: `EffectiveHoldSec` (actual hold time), `StabilizingTimeSec` (time within tolerance)
  - State tracking: `InitialBoost` (heating boost active), `LogReason` (INIT/STEP_CHANGE/TEMP_DELTA/PWM_DELTA/PERIODIC)
  - Program info: `RampRate`, `HoldTime`, `Phase`, `Step`
  - SSR control: `SSR2_PWM` (SSR1 removed - always ON when program running)
- **Advanced Logging System** (v1.6.0+):
  - 3 log types: User (basic), Test (PID tuning), Maintenance (full debug)
  - Adaptive logging reduces data volume by ~95% while capturing critical events
  - Configurable log intervals per program
  - Manual mode for open-loop testing
  - **Correct log nomenclature**: `{type}_{programName}_{timestamp}.csv` (e.g., `maint_Bouteilles_1_20251211_114343.csv`)
- **Temperature Filtering**:
  - Low-pass filter with configurable alpha (0.3 default)
  - FilteredTemp exposed in maintenance logs for noise analysis
- **Stabilization Tracking**:
  - Real-time tracking of temperature stability (±2°C tolerance)
  - EffectiveHoldSec: actual time spent in hold phase
  - StabilizingTimeSec: cumulative time within tolerance zone
- **Configuration Page** (`config.html`):
  - WiFi configuration (SSID/password)
  - PID default parameters
  - Stabilization parameters (timeouts, tolerances, durations)
  - Timeline estimation (heating/cooling rates, stabilization time)
  - HOLD phase parameters (cooking zone margin)
  - Graph update interval control
- **PID Reset Control** (v1.6.1):
  - `disablePidReset` parameter (global and per-program)
  - Option to maintain PID integrator state between steps
  - Prevents overshoot on rapid step changes
- **Enhanced Program Structure**:
  - Support for `disablePidReset`, `manualMode`, `manualPWM`
  - Adaptive logging parameters (`adaptiveTempDelta`, `adaptivePwmDelta`)
  - Multiple log type flags (`enableDataLog`, `enableTestLog`, `enableMaintenanceLog`)

### Changed
- Updated all HTML interfaces to v1.6.1 with build footers
- Enhanced `control.html` with dual graph view and block visualization
- Improved settings persistence with expanded JSON schema (512 bytes)
- **Log filenames now use correct prefix format**: `{type}_{program}_{timestamp}.csv` (not suffix)
- **Removed SSR1 from all logs** (redundant - always ON when program running)
- Test logs now have 8 columns (removed SSR1)
- Maintenance logs now have 21 columns (was 16)

### Fixed
- PID integrator windup on step transitions (optional reset)
- Logging interval now respects program-specific settings
- Log filename nomenclature matches v1.6.1 production format
- Adaptive log reason tracking (INIT, STEP_CHANGE, TEMP_DELTA, PWM_DELTA, PERIODIC)
- Hold phase timing now accurately tracked with effectiveHoldStartMs


## [1.0.0] - 2025-11-15

### Note de version
Cette version 1.0.0 représente la première version stable et production-ready du contrôleur de four verrier. Le numéro de version a été réinitialisé depuis la 1.1.3a (développement interne) pour marquer la première release publique officielle.

### Features Principales

#### Interface Utilisateur
- **Écran TFT 2.8" tactile** avec affichage temps réel de :
  - Température actuelle du four avec code couleur (cyan=chauffage, vert=stable, orange=refroidissement)
  - Température cible
  - État des relais SSR1 (ON/OFF) et SSR2 (barre PWM 0-100%)
  - Nom du programme en cours
  - Numéro d'étape actuelle
  - Temps écoulé (format h:mm:ss)
  - Graphique d'évolution de température avec échelles dynamiques et axes gradués
- **Écran de démarrage** (10 secondes) affichant :
  - Version du firmware
  - Adresse IP (STA ou AP)
  - Force du signal WiFi avec barres visuelles
  - Heure NTP synchronisée

#### Interface Web
- **Page d'accueil** (`/`) : vue d'ensemble et navigation
- **Page de contrôle** (`/control.html`) :
  - Surveillance temps réel via WebSocket (température, cible, SSR1/2, programme)
  - Démarrage/arrêt de programmes
  - Réglage des paramètres PID (Kp, Ki, Kd)
  - Ajustement de l'intervalle de mise à jour (100ms - 60s)
  - Calibration de l'offset de température
  - Protection contre l'écrasement des champs lors de la saisie utilisateur
- **Gestionnaire de fichiers** (`/files.html`) :
  - Liste des programmes (`/programs`), logs CSV (`/logs`), mises à jour OTA (`/updates`)
  - Téléchargement de fichiers avec nom suggéré automatiquement
  - Upload de programmes et firmwares
  - Suppression de fichiers
- **Configuration WiFi** (`/wifi`) : formulaire de mise à jour des identifiants

#### Gestion des Programmes
- **Format JSON** avec structure complète :
  - Nom du programme
  - Nombre d'étapes (1-10)
  - Par étape : température cible, rampe (°C/min), maintien (minutes), mode avec/sans rampe
  - Option `enableDataLog` pour journalisation CSV automatique
- **Exécution automatique** :
  - Machine à états IDLE → RAMP → HOLD → étape suivante
  - Calcul temps réel de la rampe avec interpolation linéaire
  - Contrôle PID continu pendant rampes et maintiens
  - Arrêt automatique en fin de programme

#### Régulation Thermique
- **Capteur MAX6675** thermocouple K (CS=27, SO=35, SCK=22)
- **Contrôle PID** avec paramètres configurables :
  - Kp, Ki, Kd ajustables via interface web
  - Plage de sortie : 0-1023 (10 bits) pour modulation PWM fine
  - Persistance des paramètres sur carte SD
- **Calibration température** :
  - Offset utilisateur ajustable (°C)
  - Endpoint `/calibrate_offset` pour calibration automatique
  - Affichage température brute et corrigée dans les logs

#### Commande des Relais SSR
- **Communication série UART0** avec Arduino Nano (115200 baud)
- **Protocole RelaySerial** :
  - SSR1 : commande ON/OFF tout ou rien (sécurité principale)
  - SSR2 : commande PWM 0-1023 (10 bits) pour modulation fine
  - Format : `SSR1:<0-1>,SSR2:<0-1023>\n`
- **Architecture de sécurité** : 2 SSRs en série (redondance matérielle)

#### Connectivité WiFi
- **Mode Station (STA)** :
  - Identifiants depuis `/config/wifi.json` sur SD (prioritaire)
  - Fallback sur macros compilées (`WIFI_STA_SSID`)
  - Optimisations pour signal faible : TX power max (19.5 dBm), auto-reconnect, persistent
  - Affichage force du signal (dBm + barres graphiques)
- **Mode Access Point (AP)** : fallback automatique si STA échoue
- **Synchronisation NTP** : horloge UTC+1 (Europe/Paris) au démarrage

#### Mise à Jour OTA (Over-The-Air)
- **Upload via interface web** : `/upload_to?dest=/updates`
- **Application OTA** : endpoint `/apply_ota` avec reboot automatique
- **Sécurité** :
  - Suppression avant écriture (évite corruption par append)
  - Flush tous les 4KB (prévention overflow RAM sur fichiers >1MB)
  - Auto-création répertoire `/updates` si absent
  - Nettoyage automatique des `.bin` au boot (si OTA réussie)

#### Journalisation CSV
- **Activation** : paramètre `enableDataLog: true` dans le programme JSON
- **Fichiers horodatés** : `/logs/<nom_programme>_YYYYMMDD_HHMMSS.csv`
- **Intervalle** : 5 secondes
- **Colonnes enregistrées** :
  - Timestamp (YYYY-MM-DD HH:MM:SS)
  - Temps écoulé (secondes)
  - Température actuelle et brute (rawTemp)
  - Température cible
  - Sortie PID (pidOutput)
  - PWM SSR2
  - Phase (IDLE/RAMP/HOLD)
  - Numéro d'étape
- **Téléchargement** : via gestionnaire de fichiers web

#### Stockage SD Card
- **Structure automatique** : création des répertoires au boot
  - `/config` : settings.json, wifi.json
  - `/programs` : programmes de cuisson .json
  - `/logs` : fichiers CSV de journalisation
  - `/www` : fichiers interface web (HTML/CSS/JS)
  - `/updates` : firmwares OTA temporaires
- **Compatibilité SPI** : VSPI avec pins standards (CLK=18, MISO=19, MOSI=23, CS=5)
- **Auto-négociation fréquence** : 25MHz → 1MHz (compatibilité cartes lentes)

### Améliorations Techniques

#### Performance
- **Gestion mémoire** :
  - Flash : 32.7% utilisé (1,027,669 / 3,145,728 octets)
  - RAM : 15.0% utilisé (49,224 / 327,680 octets)
  - Buffer graphique température : 300 échantillons circulaires
- **WebSocket optimisé** :
  - Vérification queue client (évite saturation)
  - Broadcast sélectif (skip clients surchargés)
  - Nettoyage périodique (toutes les secondes)
  - Mise à jour 500ms (équilibre réactivité/bande passante)

#### Fiabilité
- **Protection édition** : listeners focus/input empêchent écrasement saisie WebSocket
- **Cache-Control HTTP** : headers no-cache sur `/status` (évite données obsolètes)
- **Validation température** : plages min/max, détection erreurs capteur
- **Debug conditionnel** : macro `DEBUG_SERIAL` pour développement (évite conflit UART0 production)

#### Compatibilité
- **Polices TFT** : ASCII pur (caractères français sans accents pour compatibilité bitmap)
- **Encodage** : UTF-8 dans fichiers JSON/HTML, ASCII dans affichage TFT
- **Navigateurs** : testé Chrome, Firefox, Safari (WebSocket + download natif)

### Matériel Supporté
- **ESP32-2432S028R** (CYD - Cheap Yellow Display)
  - Écran TFT 2.8" ILI9341 (320x240)
  - Touchscreen XPT2046
  - Carte SD intégrée
- **MAX6675** interface thermocouple K
- **Arduino Nano** pont série relais (115200 baud, UART0)
- **2x SSR** relais statiques (commande série, montage série pour sécurité)

### Dépendances Bibliothèques
- `TFT_eSPI` : pilote écran TFT (configuration User_Setup.h)
- `ESPAsyncWebServer` : serveur web asynchrone
- `AsyncTCP` : couche TCP pour serveur web
- `ArduinoJson` : parsing/sérialisation JSON
- `PID_v1` : régulateur PID
- `max6675` (Adafruit) : lecture thermocouple
- `RelaySerial` (lib privée) : protocole série ESP32 ↔ Nano

### Documentation
- README.md principal avec instructions build/upload
- READMEs sous-dossiers (include/, lib/, test/, data/)
- Commentaires inline dans main.cpp
- Instructions `.github/copilot-instructions.md` pour contexte projet

### Notes de Migration (1.1.3a → 1.0.0)
Cette version consolide les développements suivants :
- **1.1.0** : Version initiale déployée (calibration offset, OTA basique)
- **1.1.1** : Corrections OTA (SD.remove avant write, mkdir auto)
- **1.1.2** : Optimisations WiFi (max TX power, startup screen, tempOffset UI)
- **1.1.3a** : Fixes caractères accentués TFT (ASCII pur)

La numérotation 1.0.0 marque la stabilité production et le déploiement initial public.

---

## Historique de Développement (pré-1.0.0)

### [1.1.3a] - 2025-11-12
#### Fixed
- Remplacement caractères accentués par ASCII dans textes TFT
  - "Non synchronisé" → "Non synchronise"
  - "Démarrage en cours..." → "Demarrage en cours..."
- Compatibilité totale polices bitmap TFT_eSPI

### [1.1.2] - 2025-11-10
#### Added
- Écran de démarrage avec informations réseau (IP, WiFi, NTP) - 10 secondes
- Champ `tempOffset` dans interface web control.html
- Listeners focus/input pour protection édition champs (évite écrasement WebSocket)

#### Fixed
- Override des champs de saisie lors de mises à jour WebSocket
- Affichage force signal WiFi avec barres graphiques

### [1.1.1] - 2025-11-08
#### Added
- Auto-création répertoires SD (/config, /programs, /logs, /www, /updates)
- Nettoyage automatique /updates au boot (suppression .bin après OTA réussie)
- Optimisations WiFi pour signal faible (TX power max, auto-reconnect, persistent)
- Flush upload tous les 4KB (prévention overflow RAM)

#### Fixed
- Corruption uploads OTA (SD.open FILE_WRITE append → SD.remove avant write)
- Échecs upload >1MB (ajout flush périodique)
- Headers cache navigateur (ajout Cache-Control: no-cache sur /status)

### [1.1.0] - 2025-11-05
#### Added
- Calibration offset température (/calibrate_offset endpoint)
- Journalisation CSV avec horodatage (paramètre enableDataLog)
- Version firmware dynamique (FIRMWARE_VERSION constant)
- Content-Disposition headers pour download fichiers
- Paramètre `tempOffset` dans settings.json

#### Changed
- Suppression paramètre obsolète `useSSR2` de structure programmes
- Intervalle journalisation CSV : 5 secondes (constante DATA_LOG_INTERVAL)

### [1.0.x] - 2025-10-28 (versions initiales)
#### Added
- Architecture de base ESP32 + MAX6675 + Arduino Nano
- Interface TFT tactile avec graphique température
- Serveur web asynchrone avec WebSocket
- Contrôle PID température
- Exécution programmes JSON multi-étapes
- Modes WiFi STA/AP avec fallback
- Upload/download fichiers SD via web
- Protocole RelaySerial pour commande SSRs

---

## [Unreleased]
### Prévu pour prochaines versions
- Tests PID avec données CSV collectées
- Documentation images/schémas architecture matérielle
- Tests OTA avec répéteur WiFi (amélioration signal)
- Optimisations graphique température (lissage, zoom)

---

**Légende des types de changements :**
- `Added` : Nouvelles fonctionnalités
- `Changed` : Modifications de fonctionnalités existantes
- `Deprecated` : Fonctionnalités obsolètes (à supprimer prochainement)
- `Removed` : Fonctionnalités supprimées
- `Fixed` : Corrections de bugs
- `Security` : Corrections de sécurité
