# Guide de Gestion des Versions et Releases

## 📌 Vue d'ensemble

Ce document décrit le processus complet de gestion des versions pour le projet **Four_Verrier** (Glass Furnace Controller ESP32).

> **Note sur les commandes PlatformIO** : Ce document utilise `platformio` pour invoquer PlatformIO CLI.  
> Alternatives équivalentes :
> - `platformio` (commande directe - recommandée)
> - `python3 -m platformio` (via module Python - la plus robuste, utilisée dans les scripts)
> - ~~`pio`~~ (raccourci non fonctionnel sur ce système - conflit avec un autre package)

---

## 🔢 Schéma de Versionnement

Le projet suit le **Semantic Versioning 2.0.0** ([semver.org](https://semver.org/)).

### Format : `MAJOR.MINOR.PATCH`

- **MAJOR** : Changements incompatibles avec les versions précédentes
  - Exemple : modification du format JSON des programmes, refonte de l'API
- **MINOR** : Ajout de fonctionnalités rétrocompatibles
  - Exemple : nouvelle page web, nouveau capteur supporté
- **PATCH** : Corrections de bugs rétrocompatibles
  - Exemple : correction d'affichage, fix de calibration

### Exemples
```
1.0.0 → Première version stable/production
1.0.1 → Correction bug affichage température
1.1.0 → Ajout support capteur PT100
2.0.0 → Nouveau format de fichier incompatible
```

---

## 📂 Fichiers à Maintenir

### 1. **`platformio.ini`** (Métadonnées firmware)
Aucune variable de version native PlatformIO, mais on peut définir :
```ini
[env:esp32_four]
build_flags = 
    -DFIRMWARE_VERSION=\"1.0.0\"
    -DBUILD_DATE=\"2025-11-15\"
```

### 2. **`README.md`** (Badge version)
```markdown
![Version](https://img.shields.io/badge/version-1.0.0-blue)
```

### 3. **`CHANGELOG.md`** (Historique détaillé)
```markdown
## [1.0.1] - 2025-11-20
### Fixed
- Correction calibration touchscreen
```

### 4. **`src/main.cpp`** (Version affichée à l'écran)
```cpp
#ifndef FIRMWARE_VERSION
#define FIRMWARE_VERSION "1.0.0"
#endif
```

### 5. **`lib/RelaySerial/library.json`** (Version de la lib privée)
```json
{
  "name": "RelaySerial",
  "version": "1.0.0",
  "description": "Serial relay control for Nano/ESP32"
}
```

---

## 🔄 Workflow de Release

### **Phase 1 : Préparation**

#### 1.1 Vérifier l'état du code
```bash
# S'assurer que tout est commité
git status

# Vérifier qu'on est sur la branche main
git branch

# Mettre à jour depuis origin
git pull origin main
```

#### 1.2 Décider du numéro de version
- Lire le `CHANGELOG.md` pour voir la dernière version
- Déterminer le type de changements (MAJOR/MINOR/PATCH)
- Exemple : `1.0.0` → `1.0.1` (fix) ou `1.1.0` (feature)

---

### **Phase 2 : Mise à Jour des Fichiers**

#### 2.1 Modifier `platformio.ini`
```ini
build_flags = 
    -DFIRMWARE_VERSION=\"1.1.0\"
    -DBUILD_DATE=\"2025-11-20\"
    # ... autres flags
```

#### 2.2 Modifier `README.md`
```markdown
![Version](https://img.shields.io/badge/version-1.1.0-blue)
```

#### 2.3 Mettre à jour `CHANGELOG.md`
Ajouter en haut du fichier (après l'en-tête) :
```markdown
## [1.1.0] - 2025-11-20

### Added
- Support capteur PT100 via MAX31865
- Nouvelle page de diagnostic `/diag.html`

### Fixed
- Correction bug arrêt d'urgence
- Fix affichage graphique avec températures négatives

### Changed
- Amélioration performance WebSocket (30% plus rapide)

### Deprecated
- Ancien format de programme v0.x (sera retiré en v2.0)
```

#### 2.4 (Optionnel) Mettre à jour les libs privées
Si `lib/RelaySerial` a changé :
```bash
nano lib/RelaySerial/library.json
# Modifier "version": "1.1.0"
```

---

### **Phase 3 : Build et Test**

#### 3.1 Nettoyer et rebuild
```bash
# Nettoyer les anciens builds
platformio run -e esp32_four -t clean

# Build complet
platformio run -e esp32_four

# Vérifier les logs de build (pas d'erreurs/warnings)
```

#### 3.2 Tester sur hardware
```bash
# Upload sur l'ESP32
platformio run -e esp32_four -t upload --upload-port /dev/ttyUSB1

# Monitorer le démarrage
platformio device monitor -e esp32_four

# Vérifier :
# - Version affichée sur écran de démarrage
# - Connexion WiFi
# - Interface web accessible
# - Chargement de programme
# - Régulation température
```

#### 3.3 Tests fonctionnels
- [ ] Écran tactile responsive
- [ ] WebSocket temps réel
- [ ] Upload/download fichiers
- [ ] Exécution programme complet
- [ ] Arrêt d'urgence
- [ ] Logs CSV corrects

---

### **Phase 4 : Commit Git**

#### 4.1 Stager les fichiers modifiés
```bash
git add platformio.ini
git add README.md
git add CHANGELOG.md
git add src/main.cpp  # si modifié
git add lib/RelaySerial/library.json  # si modifié
```

#### 4.2 Commit avec message structuré
```bash
git commit -m "Release v1.1.0

- Add PT100 sensor support
- Fix emergency stop bug
- Improve WebSocket performance
- Update documentation

See CHANGELOG.md for full details"
```

#### 4.3 Push vers GitHub
```bash
git push origin main
```

---

### **Phase 5 : Créer le Tag et la Release GitHub**

#### 5.1 Créer un tag annoté
```bash
# Tag local
git tag -a v1.1.0 -m "Version 1.1.0 - PT100 support and performance improvements"

# Push du tag
git push origin v1.1.0
```

#### 5.2 Créer la Release sur GitHub
1. Aller sur `https://github.com/jjcurt/Four_verrier/releases/new`
2. Sélectionner le tag `v1.1.0`
3. Titre : `v1.1.0 - PT100 Support & Performance`
4. Description (copier depuis `CHANGELOG.md`) :
```markdown
## 🎉 Version 1.1.0

### ✨ Nouveautés
- Support capteur PT100 via MAX31865
- Page de diagnostic `/diag.html`

### 🐛 Corrections
- Bug arrêt d'urgence résolu
- Affichage graphique avec températures négatives

### ⚡ Améliorations
- WebSocket 30% plus rapide

### 📦 Installation
Télécharger `firmware.bin` et uploader via interface web `/files.html`
```

#### 5.3 Attacher le binaire compilé
```bash
# Copier le firmware compilé
cp .pio/build/esp32_four/firmware.bin binaires/Four_Verrier_v1.1.0.bin

# Ajouter aux assets de la release GitHub
# (via interface web ou GitHub CLI)
gh release upload v1.1.0 binaires/Four_Verrier_v1.1.0.bin
```

---

## 🗂️ Archivage des Binaires

### Structure recommandée
```
binaires/
├── Four_Verrier_v1.0.0.bin
├── Four_Verrier_v1.0.1.bin
├── Four_Verrier_v1.1.0.bin
└── changelog_archive.txt
```

### Commandes
```bash
# Copier le binaire avec versioning
cp .pio/build/esp32_four/firmware.bin binaires/Four_Verrier_v1.1.0.bin

# Ajouter au git
git add binaires/Four_Verrier_v1.1.0.bin
git commit -m "Archive binaire v1.1.0"
git push origin main
```

---

## 📋 Checklist Complète

### Avant la Release
- [ ] Tous les tests passent
- [ ] Documentation à jour
- [ ] Pas de `TODO` critiques dans le code
- [ ] `git status` clean

### Mise à Jour Fichiers
- [ ] `platformio.ini` → `FIRMWARE_VERSION`
- [ ] `README.md` → badge version
- [ ] `CHANGELOG.md` → nouvelle section
- [ ] `lib/RelaySerial/library.json` (si applicable)
- [ ] `src/main.cpp` (si version hardcodée)

### Build & Test
- [ ] `platformio run -e esp32_four -t clean`
- [ ] `platformio run -e esp32_four` (0 errors)
- [ ] Upload et test hardware
- [ ] Interface web fonctionnelle
- [ ] Programme test exécuté avec succès

### Git & GitHub
- [ ] `git add` fichiers modifiés
- [ ] `git commit -m "Release vX.Y.Z"`
- [ ] `git push origin main`
- [ ] `git tag -a vX.Y.Z -m "Message"`
- [ ] `git push origin vX.Y.Z`
- [ ] Release GitHub créée avec binaire

### Post-Release
- [ ] Binaire archivé dans `binaires/`
- [ ] Annonce sur discussions/wiki si nécessaire
- [ ] Mise à jour de la roadmap

---

## 🚨 Hotfix (correction urgente)

Si bug critique en production (v1.1.0) :

```bash
# 1. Partir de la version taguée
git checkout v1.1.0
git checkout -b hotfix/1.1.1

# 2. Corriger le bug
nano src/main.cpp
# ... fix ...

# 3. Mettre à jour version → 1.1.1
nano platformio.ini  # FIRMWARE_VERSION "1.1.1"
nano README.md       # badge version
nano CHANGELOG.md    # section [1.1.1]

# 4. Tester
platformio run -e esp32_four
platformio run -e esp32_four -t upload

# 5. Merger dans main
git add .
git commit -m "Hotfix v1.1.1 - Critical temperature calculation fix"
git checkout main
git merge hotfix/1.1.1
git push origin main

# 6. Tagger
git tag -a v1.1.1 -m "Hotfix: temperature calculation"
git push origin v1.1.1

# 7. Release GitHub
# ... procédure habituelle ...
```

---

## 🔗 Ressources

- [Semantic Versioning](https://semver.org/)
- [Keep a Changelog](https://keepachangelog.com/)
- [GitHub Releases](https://docs.github.com/en/repositories/releasing-projects-on-github)
- [PlatformIO Build Flags](https://docs.platformio.org/en/latest/projectconf/section_env_build.html)

---

## 📝 Notes Importantes

1. **Ne jamais éditer un tag existant** - créer une nouvelle version
2. **Toujours tester sur hardware avant de pusher un tag**
3. **Garder CHANGELOG.md synchronisé avec les commits**
4. **Archiver les binaires avant d'écraser le dossier `.pio/build/`**
5. **Versionner aussi les libs privées** (`lib/RelaySerial`)

---

**Dernière mise à jour :** 2025-11-15  
**Projet :** Four_Verrier (jjcurt/Four_verrier)  
**Mainteneur :** @jjcurt
