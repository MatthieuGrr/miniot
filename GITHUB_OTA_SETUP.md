# Configuration OTA via GitHub Releases

Ce projet est maintenant configuré pour des mises à jour OTA automatiques depuis GitHub Releases.

## 🚀 Comment ça fonctionne

1. **Vérification automatique au démarrage** : L'ESP32 vérifie automatiquement s'il y a une nouvelle version sur GitHub à chaque démarrage (en mode STA uniquement)
2. **Notification** : Si une mise à jour est disponible, un message s'affiche dans les logs
3. **Installation manuelle** : L'utilisateur décide d'installer ou non la mise à jour via l'interface web

## 📝 Workflow de release

### 1. Préparer une nouvelle version

Modifiez la version dans le fichier `main/components/ota_manager/ota_manager.c` :

```c
#define FIRMWARE_VERSION "v1.0.1"  // Incrémenter la version
```

### 2. Commiter et pusher les changements

```bash
git add .
git commit -m "Release v1.0.1: Description des changements"
git push origin main
```

### 3. Créer un tag et le pousser

```bash
git tag v1.0.1
git push origin v1.0.1
```

### 4. GitHub Actions compile automatiquement

Le workflow `.github/workflows/build-and-release.yml` va :
- Compiler le firmware avec ESP-IDF v5.3
- Créer une release GitHub avec le tag
- Uploader le fichier `.bin` comme asset de la release

### 5. L'ESP32 détecte la mise à jour

Au prochain démarrage, l'ESP32 :
- Vérifie l'API GitHub (`https://api.github.com/repos/matthieu/miniot/releases/latest`)
- Compare la version actuelle avec la version disponible
- Affiche un message si une mise à jour est disponible

## 🌐 Interface Web

### Vérifier les mises à jour manuellement

1. Accédez à l'interface web : `http://miniot.local` ou `http://<IP_ESP32>`
2. Dans la section "Firmware Update (OTA)", cliquez sur **"🔍 Check GitHub for Updates"**
3. Si une mise à jour est disponible, un bouton **"⬆️ Install Update"** apparaît
4. Cliquez pour installer la mise à jour
5. L'ESP32 télécharge et installe automatiquement, puis redémarre

## 🔧 API Endpoints

### Vérifier les mises à jour
```bash
curl http://miniot.local/api/check_github_update
```

Réponse :
```json
{
  "success": true,
  "update_available": true,
  "current_version": "v1.0.0",
  "new_version": "v1.0.1",
  "download_url": "https://github.com/matthieu/miniot/releases/download/v1.0.1/miniot-v1.0.1.bin"
}
```

### Installer une mise à jour GitHub
```bash
curl -X POST http://miniot.local/api/install_github_update
```

## 📊 Versioning sémantique

Le système utilise le versioning sémantique (SemVer) : `vMAJOR.MINOR.PATCH`

- **MAJOR** : Changements incompatibles
- **MINOR** : Nouvelles fonctionnalités rétrocompatibles
- **PATCH** : Corrections de bugs

Exemples :
- `v1.0.0` → `v1.0.1` : Correction de bug
- `v1.0.1` → `v1.1.0` : Nouvelle fonctionnalité
- `v1.1.0` → `v2.0.0` : Changement majeur

## 🔒 Sécurité

- L'API GitHub est accessible en HTTPS
- Les fichiers `.bin` sont téléchargés via les URLs officielles de GitHub
- Le firmware est vérifié par l'ESP32 avant installation (checksum, signature)
- En cas d'échec, l'ESP32 revient automatiquement à l'ancienne version (rollback)

## ⚙️ Configuration

Pour modifier le repository GitHub, éditez ces fichiers :

1. **main/main.c** (ligne ~60) :
```c
ota_manager_check_github_update("matthieu", "miniot", &update_info);
```

2. **main/components/web_server/web_server.c** (lignes ~482, ~513) :
```c
ota_manager_check_github_update("matthieu", "miniot", &info);
ota_manager_update_from_github("matthieu", "miniot");
```

## 🧪 Test local (sans GitHub)

Vous pouvez toujours tester les mises à jour OTA en mode manuel :

```bash
# Compiler le firmware
idf.py build

# Servir le firmware localement
cd build
python3 -m http.server 8000

# Dans l'interface web, entrer l'URL
http://<IP_ORDINATEUR>:8000/miniot.bin
```

## 📜 Logs

Les logs de mise à jour sont visibles via :

```bash
idf.py monitor
```

Exemples de messages :
```
I (12345) OTA_MANAGER: Checking for updates at: https://api.github.com/repos/matthieu/miniot/releases/latest
I (12456) OTA_MANAGER: Latest GitHub release: v1.0.1 (current: v1.0.0)
I (12467) OTA_MANAGER: New version available!
I (12478) OTA_MANAGER: Firmware binary found: https://github.com/...
```

## 🎯 Prochaines étapes

- [ ] Ajouter une notification LED quand une mise à jour est disponible
- [ ] Programmer des vérifications périodiques (toutes les 24h)
- [ ] Ajouter un historique des mises à jour
- [ ] Permettre le rollback manuel vers une version précédente
