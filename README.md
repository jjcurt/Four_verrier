# Glass Furnace Controller (ESP32)# Glass Furnace Controller (ESP32)



![Version](https://img.shields.io/badge/version-1.0.0-blue) ![Platform](https://img.shields.io/badge/platform-ESP32-green) ![License](https://img.shields.io/badge/license-Proprietary-red)ESP32-based glass furnace controller with dual SSR control, touchscreen interface, and network connectivity.



Contrôleur de four verrier professionnel basé sur ESP32 avec régulation PID, interface tactile TFT, interface web et journalisation de données.## Features



---- Temperature monitoring via MAX6675 thermocouple sensor

- Dual SSR relay control for heating elements

## 📋 Table des Matières- 2.8" TFT touchscreen display (ESP32-2432S028R / CYD)

- SD card support for program storage and logging

- [Caractéristiques](#-caractéristiques)- Web interface for remote monitoring and control

- [Matériel Requis](#-matériel-requis)- WiFi connectivity (AP or STA mode)

- [Quick Start](#-quick-start)- PID temperature control

- [Structure du Projet](#-structure-du-projet)- Program-based firing schedules

- [Build & Upload](#-build--upload)

- [Configuration](#%EF%B8%8F-configuration)## Hardware

- [Utilisation](#-utilisation)

- [API Documentation](#-api-documentation)- ESP32 development board (ESP32-2432S028R)

- [Développement](#-développement)- MAX6675 thermocouple interface

- [Dépannage](#-dépannage)- Arduino Nano for relay control

- [Changelog](#-changelog)- SD card for storage

- [Licence](#-licence)- 2.8" TFT display with XPT2046 touchscreen



---## Project Structure



## ✨ Caractéristiques- `src/` - Main ESP32 firmware source

- `include/` - Header files and configuration

### Interface Utilisateur- `lib/` - Private libraries including RelaySerial protocol

- **Écran TFT 2.8" tactile** (320x240) avec affichage temps réel :- `test/` - Unit tests (PlatformIO Test Runner)

  - Température four + graphique d'évolution (300 échantillons)- `tools/` - Support tools and Nano relay controller

  - État SSR1 (ON/OFF) et SSR2 (barre PWM)  - `nano/` - Arduino Nano relay controller sketch

  - Programme en cours, étape, temps écoulé  - `nano_pio/` - PlatformIO project for building Nano sketch

  - Écran de démarrage : IP, signal WiFi, heure NTP

- **Interface web responsive** accessible depuis n'importe quel appareil :## Building and Flashing

  - Monitoring temps réel via WebSocket

  - Démarrage/arrêt programmes### ESP32 Firmware

  - Réglages PID et calibration température

  - Gestionnaire de fichiers (upload/download/delete)```bash

  - Configuration WiFi# Build main firmware

platformio run -e esp32_four

### Régulation Thermique

- **Capteur MAX6675** thermocouple type K (0-1024°C)# Upload to ESP32

- **Contrôleur PID** avec paramètres ajustables (Kp, Ki, Kd)platformio run -e esp32_four -t upload

- **Calibration offset** température persistante

- **Double contrôle SSR** :# Monitor serial output

  - SSR1 : sécurité ON/OFF (coupe alimentation)platformio device monitor -e esp32_four

  - SSR2 : modulation PWM 10 bits (contrôle fin puissance)```



### Programmes de Cuisson### Nano Relay Controller

- **Format JSON** flexible (1-10 étapes par programme)

- **Rampes progressives** avec vitesse configurable (°C/min)```bash

- **Maintiens temporisés** avec précision minute# Build Nano sketch

- **Journalisation CSV** optionnelle (timestamp, températures, PID, phase)platformio run -d tools/nano_pio -e nano



### Connectivité# Upload to Nano (example port)

- **WiFi dual-mode** : Station (STA) prioritaire, fallback Access Point (AP)platformio run -d tools/nano_pio -e nano -t upload --upload-port /dev/ttyUSB0

- **Credentials SD card** : configuration WiFi persistante```

- **Optimisations signal faible** : TX power max, auto-reconnect

- **Serveur web asynchrone** : ESPAsyncWebServerSee `tools/nano/README.md` for detailed Nano setup and wiring instructions.

- **WebSocket temps réel** : mise à jour 500ms

- **Synchronisation NTP** : horloge UTC+1## Configuration



### Mise à Jour OTAKey configuration files:

- **Upload firmware** via interface web- `platformio.ini` - Build configuration and library dependencies

- **Application automatique** avec reboot- `include/pins.h` - Pin definitions and hardware configuration

- **Gestion erreurs** robuste (flush 4KB, delete avant write)- `include/User_Setup.h` - TFT display configuration

- `data/config/wifi.json` - WiFi credentials (on SD card)

---

## Development

## 🔧 Matériel Requis

This is a PlatformIO project. See `.vscode/` for editor configuration and `platformio.ini` for build settings.

### Composants Principaux

### Adding Dependencies

| Composant | Modèle | Rôle |

|-----------|--------|------|New libraries should be added to `platformio.ini` under `lib_deps`. Example:

| **Microcontrôleur** | ESP32-2432S028R (CYD) | Contrôleur principal |

| **Écran** | TFT 2.8" ILI9341 (320x240) | Interface locale |```ini

| **Touchscreen** | XPT2046 | Saisie tactile |lib_deps =

| **Thermocouple** | MAX6675 + Thermocouple K | Mesure température |    adafruit/MAX6675 library @ ^1.1.2

| **Pont relais** | Arduino Nano | Communication série SSRs |    ...

| **Relais statiques** | 2x SSR (montés en série) | Commande résistances |```

| **Stockage** | Carte SD (2-8 GB recommandé) | Programmes, logs, web |

### Project Structure Conventions

### Câblage Simplifié

- Private libraries go in `lib/<name>/src/`

```- Global headers go in `include/`

ESP32-2432S028R- Tests belong in `test/`

├─ MAX6675 (SPI)- Web files are served from `data/www/`

│  ├─ CS  → GPIO 27

│  ├─ SO  → GPIO 35## Testing

│  └─ SCK → GPIO 22

│PlatformIO Test Runner is configured but no tests are currently implemented. Add tests under `test/` using the standard PlatformIO test framework:

├─ Arduino Nano (UART0)

│  ├─ TX → GPIO 1 (ESP32) → RX (Nano)```bash

│  └─ RX → GPIO 3 (ESP32) ← TX (Nano)platformio test -e esp32_four

│```
└─ SD Card (VSPI)
   ├─ CS   → GPIO 5
   ├─ MOSI → GPIO 23
   ├─ MISO → GPIO 19
   └─ SCK  → GPIO 18

Arduino Nano
├─ SSR1 (Digital OUT) → Relais principal
└─ SSR2 (PWM OUT)     → Relais modulation
```

**⚠️ Sécurité** : SSR1 et SSR2 doivent être montés **en série** électriquement (double coupure).

---

## 🚀 Quick Start

### 1. Prérequis

```bash
# Installer PlatformIO CLI
pip install platformio

# OU utiliser PlatformIO IDE dans VS Code
# Extension: PlatformIO IDE (platformio.platformio-ide)
```

### 2. Cloner et Build

```bash
git clone <votre-repo>
cd Four_Verrier_VS_20251028

# Build firmware ESP32
platformio run -e esp32_four

# Upload vers ESP32
platformio run -e esp32_four -t upload --upload-port /dev/ttyUSB0

# Moniteur série (115200 baud)
platformio device monitor -e esp32_four
```

### 3. Préparer la Carte SD

```bash
# Formater en FAT32
# Copier fichiers web et config
cp -r data/www /media/SD_CARD/
cp data/config/wifi.json.example /media/SD_CARD/config/wifi.json

# Éditer wifi.json avec vos credentials WiFi
nano /media/SD_CARD/config/wifi.json
```

### 4. Premier Démarrage

1. Insérer carte SD dans ESP32
2. Alimenter l'ESP32
3. Écran de démarrage affiche **IP locale** (10 secondes)
4. Connectez-vous à l'interface web : `http://<IP>/`

### 5. Tester un Programme

1. Interface web → **control.html**
2. Sélectionner programme (ex: `test.json`)
3. Cliquer **"Start Program"**
4. Observer écran TFT et graphique web

---

## 📁 Structure du Projet

```
.
├── src/
│   └── main.cpp                   # Firmware principal (1869 lignes)
├── include/
│   ├── pins.h                     # Définitions broches matérielles
│   ├── config.h                   # Constantes système
│   ├── furnace_types.h            # Structures de données
│   ├── User_Setup.h               # Config TFT_eSPI (CYD)
│   └── *.h                        # Autres headers
├── lib/
│   └── RelaySerial/               # Bibliothèque protocole série
│       ├── src/                   # Implémentation
│       ├── include/               # Headers publics
│       └── README.md              # Doc protocole
├── data/
│   ├── config/
│   │   └── wifi.json.example      # Template credentials WiFi
│   ├── programs/
│   │   └── test.json              # Programme exemple
│   └── www/
│       ├── index.html             # Page accueil web
│       ├── control.html           # Interface contrôle
│       └── files.html             # Gestionnaire fichiers
├── test/                          # Tests unitaires (à implémenter)
├── tools/
│   └── nano_pio/                  # Projet Arduino Nano
│       └── src/relay_nano.ino     # Sketch pont relais
├── scripts/
│   ├── build.sh                   # Script build rapide
│   ├── upload.sh                  # Script upload USB
│   └── monitor.sh                 # Script moniteur série
├── platformio.ini                 # Configuration PlatformIO
├── CHANGELOG.md                   # Historique versions
├── LICENSE                        # Licence propriétaire
└── README.md                      # Ce fichier
```

**Documentation détaillée** :
- [`include/README.md`](include/README.md) : Headers et configuration
- [`lib/README.md`](lib/README.md) : Bibliothèques privées
- [`data/README.md`](data/README.md) : Structure SD card
- [`test/README.md`](test/README.md) : Tests unitaires

---

## 🔨 Build & Upload

### Méthode 1 : PlatformIO CLI

```bash
# Build firmware
platformio run -e esp32_four

# Upload vers ESP32
platformio run -e esp32_four -t upload --upload-port /dev/ttyUSB0

# Moniteur série
platformio device monitor -e esp32_four
```

### Méthode 2 : Scripts Rapides

```bash
./scripts/build.sh      # Build
./scripts/upload.sh     # Upload (auto-détecte port)
./scripts/monitor.sh    # Moniteur série
```

### Méthode 3 : VS Code (PlatformIO IDE)

1. Ouvrir projet dans VS Code
2. Barre latérale PlatformIO : **Build** (✓)
3. **Upload** (→)
4. **Serial Monitor** (⚡)

### Upload Arduino Nano (Pont Relais)

```bash
# Build sketch Nano
platformio run -d tools/nano_pio -e nano

# Upload vers Nano
platformio run -d tools/nano_pio -e nano -t upload --upload-port /dev/ttyUSB1
```

**Note** : Débrancher ESP32 avant upload Nano (conflit UART0).

---

## ⚙️ Configuration

### Fichiers de Configuration Principaux

| Fichier | Localisation | Usage |
|---------|-------------|-------|
| `platformio.ini` | Racine | Build config, libs, upload |
| `include/pins.h` | include/ | Définitions broches ESP32 |
| `include/config.h` | include/ | Constantes système |
| `include/User_Setup.h` | include/ | Config écran TFT (CYD) |
| `data/config/wifi.json` | SD Card | Credentials WiFi |
| `data/config/settings.json` | SD Card | PID, offset (auto-généré) |

### Modifier Paramètres PID

**Méthode 1 : Interface Web**
1. `http://<IP>/control.html`
2. Section "PID Settings"
3. Ajuster Kp, Ki, Kd → sauvegarde automatique

**Méthode 2 : SD Card**
Éditer `/config/settings.json` :
```json
{
  "updateInterval": 1000,
  "pidKp": 40.0,
  "pidKi": 0.1,
  "pidKd": 4.0,
  "tempOffset": 0.0
}
```

### Ajouter Programme Cuisson

**Méthode 1 : Upload Web**
1. `http://<IP>/files.html`
2. Onglet "Programs"
3. Upload fichier `.json`

**Méthode 2 : SD Card Directe**
Copier fichier JSON dans `/programs/` sur SD card.

**Format** : Voir [`data/README.md`](data/README.md) pour structure complète.

---

## 📱 Utilisation

### Démarrage Programme

**Via Interface Web** (`/control.html`) :
1. Sélectionner programme dans liste déroulante
2. Cliquer "Start Program"
3. Observer monitoring temps réel

**Arrêt Manuel** : Cliquer "Stop Program" → SSRs coupés immédiatement

### Calibration Température

Si le capteur affiche un offset constant :

1. Chauffer le four à température connue (thermocouple de référence)
2. Interface web → section "Temperature Calibration"
3. Entrer température réelle mesurée
4. Cliquer "Calibrate" → offset calculé et sauvegardé

### Télécharger Logs CSV

1. `http://<IP>/files.html`
2. Onglet "Logs"
3. Cliquer "Download" sur fichier souhaité
4. Analyser avec Excel/Python

### Mise à Jour Firmware OTA

1. Build nouveau firmware : `platformio run -e esp32_four`
2. Récupérer `.bin` : `.pio/build/esp32_four/firmware.bin`
3. Interface web → `/files.html` → onglet "Updates"
4. Upload `.bin`
5. Cliquer "Apply OTA" → reboot automatique (~30 secondes)

**⚠️ Prérequis** : Signal WiFi stable (>-70 dBm recommandé).

---

## 🔌 API Documentation

### WebSocket (`ws://<IP>/ws`)

**Envoi commandes** (JSON) :
```json
{
  "target": 800,
  "updateInterval": 1000,
  "pidKp": 40.0,
  "pidKi": 0.1,
  "pidKd": 4.0,
  "tempOffset": -2.5
}
```

**Réception telemetry** (500ms) :
```json
{
  "temp": 785.3,
  "target": 800.0,
  "ssr1": 1,
  "ssr2": 723,
  "program": 1,
  "programName": "bisque.json",
  "programStep": 2,
  "phase": "RAMP",
  "elapsedSeconds": 3245
}
```

### REST Endpoints

| Method | Endpoint | Description |
|--------|----------|-------------|
| GET | `/status` | État système complet |
| GET | `/programs` | Liste programmes |
| GET | `/api/logs` | Liste logs CSV |
| GET | `/download?path=<path>` | Télécharger fichier |
| POST | `/start` | Démarrer programme |
| POST | `/stop` | Arrêter programme |
| POST | `/calibrate_offset` | Calibrer offset |
| POST | `/upload` | Upload programme |
| POST | `/apply_ota` | Appliquer firmware |

**Exemple cURL** :
```bash
# Obtenir statut
curl http://192.168.1.100/status

# Démarrer programme
curl -X POST http://192.168.1.100/start \
  -H "Content-Type: application/json" \
  -d '{"program":"bisque.json"}'
```

---

## 💻 Développement

### Environnement

- **PlatformIO** : gestionnaire build
- **VS Code** + extension PlatformIO IDE (optionnel)
- **Python 3.7+** : requis par PlatformIO

### Ajouter Dépendances

Modifier `platformio.ini` :
```ini
lib_deps =
    adafruit/MAX6675 library@^1.1.2
    ma-nouvelle-lib@^2.0.0
```

### Tests Unitaires

```bash
platformio test -e esp32_four                    # Tous les tests
platformio test -e esp32_four --filter test_temp # Test spécifique
```

**⚠️ Aucun test implémenté.** Voir [`test/README.md`](test/README.md).

---

## 🔧 Dépannage

### ESP32 ne boot pas

1. Vérifier alimentation (5V 2A minimum)
2. Bouton "Boot" + "Reset" pour mode download
3. Re-flasher firmware via USB

### Température incorrecte

1. Vérifier câblage MAX6675 (CS=27, SO=35, SCK=22)
2. Tester thermocouple avec multimètre
3. Calibrer offset via `/calibrate_offset`

### WiFi ne se connecte pas

1. Vérifier `/config/wifi.json` sur SD card
2. JSON syntaxe correcte
3. Signal >-80 dBm
4. Fallback AP : SSID `FurnaceController` → http://192.168.4.1/

### Upload OTA échoue

1. Signal WiFi faible → rapprocher routeur
2. Re-compiler firmware propre
3. Vérifier espace SD >10 MB

---

## 📝 Changelog

Voir [CHANGELOG.md](CHANGELOG.md) pour historique complet.

**Version actuelle** : 1.0.0 (15 novembre 2025)

---

## 📄 Licence

**Propriétaire** - Tous droits réservés

Voir [LICENSE](LICENSE) pour détails.

---

**Version Documentation** : 1.0.0  
**Dernière mise à jour** : 15 novembre 2025
