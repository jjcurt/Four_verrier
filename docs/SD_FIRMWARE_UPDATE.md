# Mise à Jour Firmware via Carte SD

## 📌 Vue d'ensemble

Ce document décrit le mécanisme de mise à jour du firmware via la carte SD, utile lorsque la mise à jour OTA WiFi échoue ou n'est pas disponible.

---

## 🔄 Principe de Fonctionnement

Le système utilise un **fichier marqueur** `/updates/.pending` pour signaler qu'une mise à jour doit être appliquée au démarrage.

### Séquence Normale (pas de mise à jour en attente)

```
Démarrage ESP32
   ↓
Vérifier si /updates/.pending existe
   ↓ NON
Nettoyer tous les fichiers .bin dans /updates/
   ↓
Démarrage normal du firmware
```

### Séquence de Mise à Jour SD

```
Upload firmware.bin dans /updates/
   ↓
Création automatique de /updates/.pending
   ↓
Redémarrage ESP32
   ↓
Détection de /updates/.pending au démarrage
   ↓
Application de /updates/firmware.bin
   ↓
Si succès:
   - Suppression de .pending
   - Suppression de firmware.bin
   - Redémarrage avec nouveau firmware
   ↓
Si échec:
   - .pending reste (retry au prochain boot)
   - Ancien firmware continue de fonctionner
```

---

## 📝 Méthodes de Mise à Jour

### Méthode 1 : Via Interface Web (Recommandée)

1. **Télécharger le nouveau firmware** sur votre PC
   - Fichier : `firmware.bin` (depuis GitHub releases ou build local)

2. **Accéder à l'interface web du four**
   - Naviguer vers `http://<IP_ESP32>/files.html`
   - Ou depuis l'accueil : cliquer sur "Gestion Fichiers"

3. **Uploader le firmware**
   - Sélectionner l'onglet "Updates"
   - Cliquer sur "Choisir un fichier"
   - Sélectionner `firmware.bin`
   - Cliquer sur "Upload"
   - ✅ Le fichier `.pending` est créé automatiquement

4. **Appliquer la mise à jour**
   - Dans l'onglet "Updates", cliquer sur le bouton "Apply SD Update"
   - **OU** redémarrer manuellement l'ESP32
   - L'ESP32 redémarre et applique automatiquement le firmware

5. **Vérification**
   - Après redémarrage, vérifier la nouvelle version sur l'écran de démarrage
   - Ou via l'interface web dans l'onglet Status

---

### Méthode 2 : Manuelle (Carte SD déconnectée)

1. **Retirer la carte SD** de l'ESP32

2. **Copier `firmware.bin`** dans le dossier `/updates/` de la carte SD
   - Via PC/Mac avec lecteur de carte SD

3. **Créer le fichier marqueur** `/updates/.pending`
   - Créer un fichier texte vide nommé `.pending`
   - Contenu (optionnel) : `SD firmware update pending`

4. **Réinsérer la carte SD** dans l'ESP32

5. **Redémarrer** l'ESP32
   - La mise à jour s'applique automatiquement au démarrage

---

## 🖥️ Affichage durant la Mise à Jour

Durant la mise à jour SD, l'écran TFT affiche :

```
┌─────────────────────────────────┐
│  MISE A JOUR FIRMWARE           │
│  Depuis carte SD...             │
│  NE PAS ETEINDRE !              │
│                                 │
│  75% (307200/409600 bytes)      │
│  ▓▓▓▓▓▓▓▓▓▓▓▓▓▓░░░░░            │
│                                 │
│  [Status: SUCCESS/ERROR]        │
└─────────────────────────────────┘
```

**Messages possibles** :
- ✅ `MISE A JOUR REUSSIE !` → Redémarrage automatique
- ❌ `ERREUR: begin failed` → Échec de l'initialisation
- ❌ `ERREUR: incomplete` → Échec de l'écriture
- ❌ `ERREUR: end failed` → Échec de la finalisation

---

## 🔧 API Endpoints

### Upload Firmware (avec création automatique de .pending)

**Endpoint** : `POST /upload_to?dest=/updates`

**Usage** : 
```bash
curl -X POST -F "file=@firmware.bin" "http://192.168.1.100/upload_to?dest=/updates"
```

**Comportement** :
- Upload `firmware.bin` dans `/updates/`
- Création automatique de `/updates/.pending`
- Prêt pour mise à jour au prochain redémarrage

---

### Déclencher Mise à Jour SD et Redémarrage

**Endpoint** : `POST /apply_sd_update`

**Prérequis** : `/updates/firmware.bin` doit exister

**Usage** :
```bash
curl -X POST http://192.168.1.100/apply_sd_update
```

**Comportement** :
1. Vérifier que `firmware.bin` existe
2. Créer `/updates/.pending`
3. Redémarrer ESP32
4. Mise à jour appliquée au boot

**Réponse** :
```
200 OK: "SD update scheduled - rebooting now"
404 Not Found: "firmware.bin not found in /updates/"
500 Error: "Failed to create .pending marker"
```

---

### Mise à Jour OTA Immédiate (sans redémarrage préalable)

**Endpoint** : `POST /apply_ota`

**Body** : `{ "path": "/updates/firmware.bin" }`

**Usage** :
```bash
curl -X POST http://192.168.1.100/apply_ota \
  -H "Content-Type: application/json" \
  -d '{"path":"/updates/firmware.bin"}'
```

**Comportement** :
- Applique immédiatement le firmware (pas de marqueur)
- Redémarre après succès
- **Attention** : Si échec réseau pendant update → ESP32 peut devenir inaccessible

---

## 🛡️ Sécurité et Récupération

### Protection contre les Boucles de Boot

Si une mise à jour échoue de manière répétée :

1. **Le marqueur `.pending` est supprimé** après échec
2. Empêche les tentatives infinies au boot
3. L'ancien firmware continue de fonctionner

### Récupération Manuelle

Si l'ESP32 ne démarre plus après une mise à jour :

1. **Retirer la carte SD**
2. **Supprimer** `/updates/.pending` et `/updates/firmware.bin`
3. **Réinsérer la carte SD**
4. **Redémarrer** → Le firmware précédent reprend

### Logs Série (Debug)

Activer `DEBUG_SERIAL` dans `main.cpp` pour voir les logs via USB :

```cpp
#define DEBUG_SERIAL  // Décommenter cette ligne
```

**Attention** : Désactiver en production (conflit avec relais Nano)

---

## 📊 Comparaison des Méthodes

| Méthode | Avantages | Inconvénients | Cas d'usage |
|---------|-----------|---------------|-------------|
| **OTA WiFi Direct** (`/apply_ota`) | - Immédiat<br>- Pas de redémarrage préalable | - Risque si WiFi faible<br>- ESP32 peut devenir inaccessible si échec | Connexion WiFi stable |
| **SD avec Redémarrage** (`/apply_sd_update`) | - Sûr (retry automatique)<br>- Pas de risque réseau | - Nécessite redémarrage | WiFi instable, sécurité maximale |
| **SD Manuel** (copie directe) | - Fonctionne sans réseau<br>- Contrôle total | - Nécessite démonter SD<br>- Plus long | Pas d'accès réseau |

---

## ✅ Checklist de Mise à Jour

### Avant la Mise à Jour
- [ ] Vérifier la version actuelle du firmware (écran de démarrage ou `/status`)
- [ ] Télécharger le bon fichier `firmware.bin` (depuis releases GitHub)
- [ ] Vérifier l'espace disponible sur la carte SD (`/updates/` doit avoir ~500KB libre)
- [ ] Noter l'adresse IP de l'ESP32 (si mise à jour via web)

### Durant la Mise à Jour
- [ ] **NE PAS COUPER L'ALIMENTATION** durant l'écriture du firmware
- [ ] Observer la barre de progression sur l'écran TFT
- [ ] Attendre le message "MISE A JOUR REUSSIE !" avant manipulation

### Après la Mise à Jour
- [ ] Vérifier la nouvelle version affichée sur l'écran de démarrage
- [ ] Tester la connexion WiFi
- [ ] Tester l'interface web (`/control.html`)
- [ ] Vérifier que les programmes chargent correctement

---

## 🐛 Dépannage

### Problème : "firmware.bin not found"

**Cause** : Le fichier n'a pas été uploadé ou a un nom différent

**Solution** :
```bash
# Vérifier les fichiers présents
curl http://192.168.1.100/api/list?dir=/updates

# Renommer si nécessaire (via interface web ou manuellement sur SD)
```

### Problème : Mise à jour en boucle (redémarre sans cesse)

**Cause** : Le fichier `.pending` existe mais `firmware.bin` est corrompu

**Solution** :
1. Retirer la carte SD
2. Supprimer `/updates/.pending`
3. Réinsérer et redémarrer

### Problème : "Update.begin failed"

**Cause** : Pas assez d'espace flash ou partition incorrecte

**Solution** :
- Vérifier `platformio.ini` → `board_build.partitions = huge_app.csv`
- La taille du firmware ne doit pas dépasser ~1.9MB

### Problème : Écran figé sur "MISE A JOUR FIRMWARE"

**Cause** : Crash durant l'écriture ou fichier corrompu

**Solution** :
1. Attendre 2 minutes (timeout)
2. Si toujours figé → couper/rallumer l'alimentation
3. Supprimer `.pending` manuellement via carte SD

---

## 📚 Voir Aussi

- [VERSION_MANAGEMENT.md](VERSION_MANAGEMENT.md) - Gestion des versions et releases
- [HARDWARE_CONNECTIONS.md](HARDWARE_CONNECTIONS.md) - Branchements matériels
- Interface Web : `/files.html` - Gestionnaire de fichiers intégré

---

**Dernière mise à jour** : 2025-11-16  
**Projet** : Four_Verrier (jjcurt/Four_verrier)  
**Version introduite** : 1.1.0 (à venir)
