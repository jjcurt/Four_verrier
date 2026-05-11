#!/usr/bin/env bash
# update_html_version.sh — Met à jour le build-footer de tous les fichiers HTML
#
# Lit la version depuis include/config.h et utilise la date/heure courantes.
# À lancer :
#   - avant d'uploader les HTML sur la carte SD (après une modif manuelle)
#   - automatiquement depuis release.sh (version + date de release)
#
# Usage : ./scripts/update_html_version.sh [VERSION]
#   Sans argument : lit la version dans include/config.h
#   Avec argument : utilise la version fournie (ex: 1.6.2)

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
cd "$PROJECT_DIR"

# --------------------------------------------------------------------------
# Version
# --------------------------------------------------------------------------
if [ -n "${1:-}" ]; then
    VERSION="$1"
else
    VERSION=$(grep -oP '#define FIRMWARE_VERSION "\K[^"]+' include/config.h | head -1)
    if [ -z "$VERSION" ]; then
        echo "ERREUR: impossible de lire la version dans include/config.h" >&2
        exit 1
    fi
fi

# Date et heure courantes (format identique aux footers existants)
BUILD_DATE=$(date +"%Y-%m-%d %H:%M")

# --------------------------------------------------------------------------
# Fichiers cibles
# --------------------------------------------------------------------------
HTML_FILES=(
    "data/www/index.html"
    "data/www/control.html"
    "data/www/programs.html"
    "data/www/files.html"
    "data/www/config.html"
)

UPDATED=0
for f in "${HTML_FILES[@]}"; do
    if [ ! -f "$f" ]; then
        echo "  WARN: $f introuvable, ignoré"
        continue
    fi

    # Remplace le contenu de la div build-footer (pattern précis, ne touche pas au CSS)
    sed -i 's|<div class="build-footer">Build:.*</div>|<div class="build-footer">Build: v'"$VERSION"' - '"$BUILD_DATE"'</div>|g' "$f"
    echo "  ✓ $(basename "$f") → v$VERSION - $BUILD_DATE"
    UPDATED=$((UPDATED + 1))
done

echo ""
echo "→ $UPDATED fichier(s) HTML mis à jour (v$VERSION - $BUILD_DATE)"
