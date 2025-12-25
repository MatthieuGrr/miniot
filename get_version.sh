#!/bin/bash
# Script pour extraire la version
# Priorité : 1) Fichier VERSION, 2) Tag git, 3) Version par défaut

# Vérifier si un fichier VERSION existe (pour le développement local)
if [ -f "VERSION" ]; then
    VERSION=$(cat VERSION | tr -d '[:space:]')
    echo "$VERSION"
    exit 0
fi

# Sinon, essayer de récupérer le dernier tag git (pour GitHub Actions)
VERSION=$(git describe --tags --exact-match 2>/dev/null)

# Si pas de tag exact, essayer avec --always
if [ -z "$VERSION" ]; then
    VERSION=$(git describe --tags --always 2>/dev/null)
fi

# Si toujours rien, utiliser une version par défaut
if [ -z "$VERSION" ] || [ "$VERSION" = "" ]; then
    VERSION="v0.0.0-dev"
fi

# Afficher la version (sera capturée par CMake)
echo "$VERSION"
