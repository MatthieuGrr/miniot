#!/bin/bash
# Script pour extraire la version depuis git

# Essayer de récupérer le dernier tag git
VERSION=$(git describe --tags --always --dirty 2>/dev/null)

# Si pas de tag, utiliser une version par défaut
if [ -z "$VERSION" ] || [ "$VERSION" = "" ]; then
    VERSION="v0.0.0-dev"
fi

# Afficher la version (sera capturée par CMake)
echo "$VERSION"
