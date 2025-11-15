# lib/

Ce répertoire contient les bibliothèques privées spécifiques au projet.

PlatformIO compile ces bibliothèques en bibliothèques statiques et les lie à l'exécutable final.

## Structure

Le code source de chaque bibliothèque doit être placé dans un répertoire séparé :
```
lib/
├── YourLibraryName/
│   ├── src/
│   │   ├── YourLibrary.cpp
│   │   └── YourLibrary.h
│   ├── include/         (optionnel, headers publics)
│   ├── examples/        (optionnel)
│   ├── docs/           (optionnel)
│   └── library.json    (optionnel, options de build personnalisées)
```

## Bibliothèques du Projet

### RelaySerial
Bibliothèque de communication série entre ESP32 et Arduino Nano pour contrôle des relais SSR.

**Emplacement** : `lib/RelaySerial/`

**Fichiers** :
- `src/relay_serial.cpp` : Implémentation protocole série
- `src/relay_serial.h` : Header principal
- `include/relay_pins.h` : Définitions broches
- `include/relay_serial.h` : Interface publique
- `library.json` : Configuration LDF (Library Dependency Finder)

**Documentation** : Voir `lib/RelaySerial/README.md`

## Configuration library.json

Le fichier `library.json` permet de personnaliser les options de build :
```json
{
  "name": "RelaySerial",
  "version": "1.0.0",
  "build": {
    "flags": ["-DRELAY_DEBUG"],
    "srcFilter": ["+<*>", "-<examples/>"]
  }
}
```

Documentation complète : https://docs.platformio.org/page/librarymanager/config.html

## Library Dependency Finder (LDF)

PlatformIO analyse automatiquement les dépendances en scannant les fichiers source du projet.

Les bibliothèques privées dans `lib/` sont toujours compilées et liées, même si elles ne sont pas explicitement incluses dans le code source (utile pour les bibliothèques utilisées conditionnellement).

**Modes LDF** (configurables dans `platformio.ini`) :
- `chain` : analyse récursive des dépendances (par défaut)
- `deep` : analyse profonde de tous les fichiers
- `off` : désactive l'analyse automatique

Plus d'informations :
- https://docs.platformio.org/page/librarymanager/ldf.html

## Utilisation dans le Code

```cpp
// Dans src/main.cpp
#include <relay_serial.h>  // Bibliothèque privée (lib/)

void setup() {
    RelaySerial::begin(115200);
    RelaySerial::setSSR(1, true);   // SSR1 ON
    RelaySerial::setSSR(2, 512);    // SSR2 50% PWM
}
```

## Ajout d'une Nouvelle Bibliothèque

1. Créer le répertoire : `lib/MaNouvelleBiblio/`
2. Ajouter le code source dans `src/`
3. (Optionnel) Créer `library.json` pour options personnalisées
4. Inclure dans votre code : `#include <MaNouvelleBiblio.h>`
5. PlatformIO détecte et compile automatiquement

---

**Note** : Pour les bibliothèques publiques (Arduino Library Manager, PlatformIO Registry), utilisez plutôt la section `lib_deps` dans `platformio.ini`.
