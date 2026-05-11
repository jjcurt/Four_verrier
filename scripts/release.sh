#!/bin/bash
#
# Script de Release Automatique pour Four_Verrier
# Gère la mise à jour de version, compilation, tests, et publication sur GitHub
#
# Usage: ./scripts/release.sh [major|minor|patch|VERSION]
#   major  : Incrémente MAJOR (1.2.3 → 2.0.0)
#   minor  : Incrémente MINOR (1.2.3 → 1.3.0)
#   patch  : Incrémente PATCH (1.2.3 → 1.2.4) [défaut]
#   VERSION: Version spécifique format MAJOR.MINOR.PATCH (ex: 1.6.1)
#

set -e  # Arrêt immédiat en cas d'erreur

# Couleurs pour l'affichage
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Fonction d'affichage
print_step() {
    echo -e "${BLUE}==>${NC} ${1}"
}

print_success() {
    echo -e "${GREEN}✓${NC} ${1}"
}

print_warning() {
    echo -e "${YELLOW}⚠${NC} ${1}"
}

print_error() {
    echo -e "${RED}✗${NC} ${1}"
}

# Vérification des prérequis
print_step "Vérification des prérequis..."

if ! command -v git &> /dev/null; then
    print_error "Git n'est pas installé"
    exit 1
fi

PIO_BIN="$HOME/.platformio/penv/bin/platformio"
if ! "$PIO_BIN" --version &> /dev/null; then
    print_error "PlatformIO n'est pas installé ($PIO_BIN)"
    exit 1
fi

PIO_CMD="$PIO_BIN"

# Vérifier qu'on est dans le bon répertoire
if [ ! -f "platformio.ini" ]; then
    print_error "Fichier platformio.ini introuvable. Êtes-vous dans le répertoire du projet ?"
    exit 1
fi

# Vérifier qu'on est sur la branche main
CURRENT_BRANCH=$(git branch --show-current)
if [ "$CURRENT_BRANCH" != "main" ]; then
    print_warning "Vous n'êtes pas sur la branche main (actuellement sur: $CURRENT_BRANCH)"
    read -p "Continuer quand même ? (y/N) " -n 1 -r
    echo
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        exit 1
    fi
fi

# Vérifier l'état Git
print_step "Vérification de l'état Git..."
if [ -n "$(git status --porcelain)" ]; then
    print_warning "Des fichiers non commités existent:"
    git status --short
    read -p "Continuer quand même ? (y/N) " -n 1 -r
    echo
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        exit 1
    fi
fi

# Récupérer la version actuelle depuis include/config.h (source canonique)
print_step "Récupération de la version actuelle..."
CURRENT_VERSION=$(grep -oP '#define FIRMWARE_VERSION "\K[^"]+' include/config.h | head -1)
if [ -z "$CURRENT_VERSION" ]; then
    print_error "Impossible de trouver la version actuelle dans include/config.h"
    exit 1
fi

# Nettoyer la version (enlever suffixes -maj, -dev, etc.)
CLEAN_VERSION=$(echo "$CURRENT_VERSION" | sed 's/-[a-z]*$//')
print_success "Version actuelle: $CURRENT_VERSION (base: $CLEAN_VERSION)"

# Déterminer la nouvelle version
if [ -z "$1" ]; then
    # Par défaut: incrémenter PATCH
    INCREMENT_TYPE="patch"
elif [[ "$1" =~ ^(major|minor|patch)$ ]]; then
    INCREMENT_TYPE="$1"
elif [[ "$1" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]]; then
    # Version spécifique fournie
    NEW_VERSION="$1"
    INCREMENT_TYPE="manual"
else
    print_error "Argument invalide: $1"
    echo "Usage: $0 [major|minor|patch|X.Y.Z]"
    exit 1
fi

# Calculer la nouvelle version si nécessaire
if [ "$INCREMENT_TYPE" != "manual" ]; then
    IFS='.' read -r -a version_parts <<< "$CLEAN_VERSION"
    MAJOR="${version_parts[0]}"
    MINOR="${version_parts[1]}"
    PATCH="${version_parts[2]}"
    
    case "$INCREMENT_TYPE" in
        major)
            NEW_VERSION="$((MAJOR + 1)).0.0"
            ;;
        minor)
            NEW_VERSION="${MAJOR}.$((MINOR + 1)).0"
            ;;
        patch)
            NEW_VERSION="${MAJOR}.${MINOR}.$((PATCH + 1))"
            ;;
    esac
    print_warning "Incrémentation $INCREMENT_TYPE: $CURRENT_VERSION → $NEW_VERSION"
fi

print_success "Nouvelle version: $NEW_VERSION"

# Confirmation
echo ""
print_warning "Résumé de la release:"
echo "  Version actuelle: $CURRENT_VERSION"
echo "  Nouvelle version: $NEW_VERSION"
echo "  Branche: $CURRENT_BRANCH"
echo ""
read -p "Confirmer la release ? (y/N) " -n 1 -r
echo
if [[ ! $REPLY =~ ^[Yy]$ ]]; then
    print_warning "Release annulée"
    exit 0
fi

# Date actuelle pour build_flags
BUILD_DATE=$(date +%Y-%m-%d)

# Mise à jour des fichiers
print_step "Mise à jour de include/config.h et src/main.cpp..."
sed -i "s/#define FIRMWARE_VERSION \".*\"/#define FIRMWARE_VERSION \"$NEW_VERSION\"/" include/config.h
sed -i "s/#define FIRMWARE_VERSION \".*\"/#define FIRMWARE_VERSION \"$NEW_VERSION\"/" src/main.cpp
print_success "include/config.h et src/main.cpp mis à jour"

print_step "Mise à jour de platformio.ini..."
# Mise à jour de FIRMWARE_VERSION dans build_flags
sed -i "s/-DFIRMWARE_VERSION=\\\\\".*\\\\\"/-DFIRMWARE_VERSION=\\\\\"$NEW_VERSION\\\\\"/" platformio.ini
# Mise à jour de BUILD_DATE dans build_flags
if grep -q "DBUILD_DATE" platformio.ini; then
    sed -i "s/-DBUILD_DATE=\\\\\".*\\\\\"/-DBUILD_DATE=\\\\\"$BUILD_DATE\\\\\"/" platformio.ini
else
    # Ajouter BUILD_DATE si absent
    sed -i "/build_flags/a\    -DBUILD_DATE=\\\\\"$BUILD_DATE\\\\\"" platformio.ini
fi
print_success "platformio.ini mis à jour"

print_step "Mise à jour de README.md..."
sed -i "s/version-[0-9.]*-blue/version-$NEW_VERSION-blue/" README.md
print_success "README.md mis à jour"

# Demander les notes de release
echo ""
print_step "Notes de release pour CHANGELOG.md"
echo "Entrez les changements (une ligne par changement, ligne vide pour terminer):"
echo "Format suggéré: - Description du changement"
CHANGELOG_ENTRIES=""
while IFS= read -r line; do
    [ -z "$line" ] && break
    CHANGELOG_ENTRIES="${CHANGELOG_ENTRIES}${line}\n"
done

if [ -z "$CHANGELOG_ENTRIES" ]; then
    print_warning "Aucune note de release fournie"
    CHANGELOG_ENTRIES="- Mise à jour de version\n"
fi

# Mettre à jour CHANGELOG.md
print_step "Mise à jour de CHANGELOG.md..."
CHANGELOG_HEADER="## [$NEW_VERSION] - $BUILD_DATE\n\n$CHANGELOG_ENTRIES"
# Insérer après la première ligne (titre)
sed -i "1 a\\$CHANGELOG_HEADER" CHANGELOG.md
print_success "CHANGELOG.md mis à jour"

# Nettoyage et compilation
print_step "Nettoyage des builds précédents..."
eval "$PIO_CMD run -e esp32_four -t clean" > /dev/null 2>&1
print_success "Nettoyage effectué"

print_step "Compilation du firmware..."
if eval "$PIO_CMD run -e esp32_four"; then
    print_success "Compilation réussie"
else
    print_error "Échec de la compilation"
    print_warning "Les fichiers ont été modifiés mais pas commités"
    exit 1
fi

# Vérification de la taille du firmware
FIRMWARE_PATH=".pio/build/esp32_four/firmware.bin"
if [ -f "$FIRMWARE_PATH" ]; then
    FIRMWARE_SIZE=$(du -h "$FIRMWARE_PATH" | cut -f1)
    print_success "Firmware généré: $FIRMWARE_SIZE"
else
    print_error "Firmware introuvable: $FIRMWARE_PATH"
    exit 1
fi

# Archivage du binaire versionné (binaires/firmware.bin déjà copié par le post-build)
print_step "Archivage du binaire versionné..."
mkdir -p binaires
BINARY_NAME="Four_Verrier_v${NEW_VERSION}.bin"
cp "$FIRMWARE_PATH" "binaires/$BINARY_NAME"
print_success "Binaire archivé: binaires/$BINARY_NAME (gitignored — à joindre à la GitHub Release)"
print_success "binaires/firmware.bin mis à jour par le post-build (accessible depuis le navigateur)"

# Mise à jour des footers HTML (version + date de release)
print_step "Mise à jour des footers HTML..."
bash "$(dirname "$0")/update_html_version.sh" "$NEW_VERSION"

# Commit Git
print_step "Commit des changements..."
# Staged : fichiers versionnés modifiés + tous les nouveaux fichiers src/ et include/
git add -u
git add src/ include/ platformio.ini README.md CHANGELOG.md
for html_file in "${HTML_FILES[@]}"; do
    [ -f "$html_file" ] && git add "$html_file"
done
# Note : binaires/ est dans .gitignore — les binaires sont distribués via GitHub Releases
git commit -m "Release v$NEW_VERSION

$(echo -e "$CHANGELOG_ENTRIES")
See CHANGELOG.md for full details"
print_success "Changements commités"

# Création du tag
print_step "Création du tag v$NEW_VERSION..."
git tag -a "v$NEW_VERSION" -m "Version $NEW_VERSION

$(echo -e "$CHANGELOG_ENTRIES")"
print_success "Tag créé"

# Push vers GitHub
print_step "Push vers GitHub..."
read -p "Pusher vers origin/main maintenant ? (y/N) " -n 1 -r
echo
if [[ $REPLY =~ ^[Yy]$ ]]; then
    git push origin main
    git push origin "v$NEW_VERSION"
    print_success "Pushé vers GitHub"
    
    echo ""
    print_success "✨ Release v$NEW_VERSION terminée avec succès !"
    echo ""
    echo "Prochaines étapes:"
    echo "  1. Créer la release GitHub: https://github.com/jjcurt/Four_verrier/releases/new"
    echo "  2. Sélectionner le tag: v$NEW_VERSION"
    echo "  3. Attacher le binaire: binaires/$BINARY_NAME"
    echo "  4. Tester sur hardware"
else
    print_warning "Push annulé"
    echo ""
    echo "Pour pusher manuellement plus tard:"
    echo "  git push origin main"
    echo "  git push origin v$NEW_VERSION"
fi

echo ""
print_success "🎉 Release locale terminée !"
echo ""
echo "Résumé:"
echo "  Version: $NEW_VERSION"
echo "  Tag: v$NEW_VERSION"
echo "  Archive binaire: binaires/$BINARY_NAME"
echo "  Binaire pour upload sur ESP32: binaires/firmware.bin"
echo "  Firmware size: $FIRMWARE_SIZE"
