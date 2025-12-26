# MiniOT - Gestionnaire IoT pour ESP32-S3

[![ESP-IDF](https://img.shields.io/badge/ESP--IDF-v5.3-blue)](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/)
[![Platform](https://img.shields.io/badge/platform-ESP32--S3-orange)](https://www.espressif.com/en/products/socs/esp32-s3)
[![License](https://img.shields.io/badge/license-À_définir-lightgrey)]()

Plateforme IoT embarquée pour ESP32-S3 avec configuration WiFi via portail captif, mises à jour OTA depuis GitHub, et interface web de gestion.

---

## 🌟 Fonctionnalités

### Configuration WiFi
- **Portail captif automatique** au premier démarrage
- Mode Access Point (AP) pour la configuration initiale
- SSID par défaut : `MiniOT-Setup-XXXX` (XXXX = 4 derniers caractères MAC)
- Interface web responsive à http://192.168.4.1
- Sauvegarde persistante des credentials en NVS Flash

### Mises à Jour OTA (Over-The-Air)
- ✅ **Mises à jour depuis GitHub Releases** (automatique)
- ✅ **Mises à jour manuelles** via URL personnalisée
- ✅ **Barre de progression en temps réel** dans l'interface web
- ✅ **Dual-bank OTA** (partitions ota_0 / ota_1) pour rollback automatique
- ✅ **Validation de certificats HTTPS** pour téléchargements sécurisés
- ✅ **Vérification d'intégrité** du firmware avant installation

### Découverte Réseau
- **mDNS/Bonjour** : Accès via `http://miniot.local`
- Annonce du service HTTP sur le réseau local
- Compatible avec la découverte macOS/iOS/Android

### Interface Web
- Dashboard de configuration responsive
- Scan des réseaux WiFi disponibles avec force du signal
- Informations système (version, partition, IP, MAC)
- Gestion OTA avec progression visuelle
- Factory Reset
- Redémarrage à distance

---

## 🛠️ Matériel Requis

| Composant | Spécification |
|-----------|---------------|
| **Microcontrôleur** | ESP32-S3 |
| **Flash** | 4MB minimum (dual-bank OTA) |
| **RAM** | 512KB SRAM (inclus dans ESP32-S3) |
| **WiFi** | 802.11 b/g/n 2.4GHz |
| **Alimentation** | 3.3V via USB ou régulateur |

**Testé sur:** ESP32-S3-DevKitC-1

---

## 🚀 Démarrage Rapide

### Prérequis

```bash
# ESP-IDF v5.3 ou supérieur
git clone --recursive https://github.com/espressif/esp-idf.git
cd esp-idf && git checkout v5.3
./install.sh esp32s3
. ./export.sh
```

### Installation

```bash
# Cloner le projet
git clone https://github.com/MatthieuGrr/miniot.git
cd miniot

# Configurer la cible
idf.py set-target esp32s3

# Compiler et flasher
idf.py build flash monitor
```

### Configuration Initiale

1. **Premier démarrage** : L'ESP32 démarre en mode Access Point
   ```
   I (XX) MAIN: === MiniOT Ready (AP Mode - First Boot) ===
   I (XX) MAIN: Connect to WiFi network and navigate to http://192.168.4.1
   ```

2. **Connectez-vous au réseau WiFi** : `MiniOT-Setup-XXXX`
   - Mot de passe : aucun (réseau ouvert)

3. **Ouvrez un navigateur** : http://192.168.4.1
   - Scannez les réseaux WiFi disponibles
   - Entrez vos credentials WiFi
   - Cliquez sur "Save Configuration"

4. **Redémarrage automatique** en mode Station (STA)
   - L'appareil se connecte à votre réseau WiFi
   - Accessible via `http://miniot.local` ou l'IP affichée dans les logs

---

## 📡 API REST

### Endpoints Disponibles

#### Configuration WiFi

**`POST /api/configure`** - Sauvegarder la configuration WiFi
```json
{
  "ssid": "MonReseauWiFi",
  "password": "MonMotDePasse",
  "ap_timeout": 60
}
```

**`GET /api/scan`** - Scanner les réseaux WiFi
```json
{
  "networks": [
    {"ssid": "WiFi-1", "rssi": -45, "auth": "WPA2"},
    {"ssid": "WiFi-2", "rssi": -67, "auth": "WPA3"}
  ]
}
```

#### Informations Système

**`GET /api/status`** - Statut du device
```json
{
  "wifi_state": 3,
  "ip": "192.168.1.100",
  "mac": "AA:BB:CC:DD:EE:FF"
}
```

**`GET /api/ota_version`** - Version du firmware
```json
{
  "version": "v1.0.5",
  "partition": "ota_0"
}
```

#### Mises à Jour OTA

**`GET /api/check_github_update`** - Vérifier les mises à jour
```json
{
  "update_available": true,
  "current_version": "v1.0.0",
  "new_version": "v1.0.5",
  "download_url": "https://github.com/MatthieuGrr/miniot/releases/download/v1.0.5/miniot-v1.0.5.bin"
}
```

**`POST /api/install_github_update`** - Installer depuis GitHub
```json
{
  "success": true,
  "message": "GitHub OTA update started"
}
```

**`POST /api/ota_update`** - Mise à jour manuelle
```json
{
  "url": "http://192.168.1.50:8000/firmware.bin"
}
```

**`GET /api/ota_progress`** - Progression du téléchargement
```json
{
  "in_progress": true,
  "total_size": 819200,
  "downloaded": 409600,
  "percent": 50,
  "status": "Downloading..."
}
```

#### Actions Système

**`POST /api/reboot`** - Redémarrer l'appareil

**`POST /api/factory_reset`** - Réinitialisation usine

---

## 🔧 Configuration Avancée

### Modifier le Hostname mDNS

**Fichier:** `main/components/mdns_service/mdns_service.c:28`
```c
#define MDNS_HOSTNAME "miniot"  // Changer en "mon-device"
// Accessible via: http://mon-device.local
```

### Personnaliser le SSID du Point d'Accès

**Fichier:** `main/components/wifi_manager/wifi_manager.c:31-32`
```c
#define AP_SSID_PREFIX "MiniOT-Setup"  // Changer en "MonProduit-Setup"
```

### Modifier la Version du Firmware

**Développement local:**
```bash
echo "v1.0.6" > VERSION
idf.py build flash
```

**Release GitHub:**
```bash
git tag v1.0.6
git push origin v1.0.6
# GitHub Actions compile et publie automatiquement
```

---

## 📦 Structure du Projet

```
miniot/
├── main/
│   ├── main.c                      # Point d'entrée de l'application
│   ├── version.h.in                # Template de version (auto-généré)
│   └── components/
│       ├── nvs_storage/            # Stockage persistant (WiFi config)
│       ├── wifi_manager/           # Gestion WiFi (AP/STA)
│       ├── dns_server/             # Serveur DNS captif
│       ├── web_server/             # Serveur HTTP + API REST
│       ├── mdns_service/           # Découverte réseau mDNS
│       └── ota_manager/            # Mises à jour OTA + GitHub
├── partitions.csv                  # Table de partitions (dual-bank OTA)
├── sdkconfig.defaults              # Configuration ESP-IDF minimale
├── CMakeLists.txt                  # Build system
├── get_version.sh                  # Script d'extraction de version
├── VERSION                         # Fichier de version (dev local)
└── .github/workflows/
    └── build-and-release.yml       # CI/CD GitHub Actions
```

---

## 🔐 Sécurité

### ⚠️ Avertissements Importants

**Le système actuel présente des limitations de sécurité :**

1. **Mots de passe en clair** : Les credentials WiFi sont stockés non chiffrés dans la NVS Flash
2. **HTTP non sécurisé** : L'interface web utilise HTTP (pas HTTPS)
3. **Pas d'authentification** : Les API REST ne sont pas protégées

**Recommandé pour :**
- ✅ Prototypage
- ✅ Environnements de développement
- ✅ Réseaux privés de confiance

**Non recommandé pour :**
- ❌ Environnements de production sans modifications
- ❌ Réseaux publics
- ❌ Données sensibles

### Améliorations Futures

Voir [SECURITY_AUDIT.md](SECURITY_AUDIT.md) pour l'analyse complète et les recommandations.

**Prévues :**
- Chiffrement NVS pour les credentials
- HTTPS pour le serveur web
- Authentification Basic Auth ou API token
- Signature des firmwares OTA

---

## 🔄 Workflow de Mise à Jour

### Option 1 : Via GitHub Releases (Automatique)

```bash
# 1. Développement local
echo "v1.0.6" > VERSION
idf.py build flash  # Test local

# 2. Commit et tag
git add .
git commit -m "Add new feature"
git tag v1.0.6
git push origin main
git push origin v1.0.6

# 3. GitHub Actions
# - Compile automatiquement
# - Crée la release v1.0.6
# - Attache miniot-v1.0.6.bin

# 4. Sur l'ESP32
# - Interface web : "Check GitHub for Updates"
# - Détecte v1.0.6 disponible
# - Clic sur "Install Update"
# - Téléchargement + installation automatique
# - Redémarrage sur nouvelle version
```

### Option 2 : Mise à Jour Manuelle

```bash
# 1. Héberger le firmware
python3 -m http.server 8000

# 2. Interface web ESP32
# - Section "Manual Update"
# - URL: http://192.168.1.50:8000/build/miniot.bin
# - Clic "Update Firmware"
```

---

## 🧪 Développement

### Activer les Logs de Debug

**menuconfig:**
```bash
idf.py menuconfig
# Component config → Log output → Default log verbosity → Debug
```

**Ou dans sdkconfig.defaults:**
```
CONFIG_LOG_DEFAULT_LEVEL_DEBUG=y
```

### Monitorer les Logs

```bash
idf.py monitor

# Filtrer par composant
idf.py monitor | grep "OTA_MANAGER"
```

### Tests Locaux

```bash
# Servir l'interface web localement
cd main/components/web_server
python3 -m http.server 8000
# Ouvrir http://localhost:8000
```

---

## 🐛 Dépannage

### Problème : "Failed to connect to WiFi"

**Causes possibles :**
- SSID ou mot de passe incorrect
- Réseau WiFi hors de portée
- Canal WiFi incompatible

**Solution :**
```bash
# Logs détaillés
idf.py monitor
# Chercher : "wifi:state: xxx -> REASON_CODE"

# Réinitialiser la config
# Interface web → Factory Reset
```

### Problème : "OTA update failed: Out of buffer"

**Solution :**
- Le buffer HTTP est déjà configuré à 8192 bytes
- Vérifier la taille du firmware (< 1.5MB)
- Augmenter `buffer_size` dans `ota_manager.c:110`

### Problème : "miniot.local" non accessible

**Causes :**
- mDNS non supporté sur le routeur
- Firewall bloque les requêtes mDNS

**Solution :**
```bash
# Utiliser l'IP directe (voir les logs)
I (XX) MAIN: Device IP: 192.168.1.100
# Accéder via http://192.168.1.100

# Ou forcer la résolution
avahi-browse -a  # Linux
dns-sd -B _http._tcp  # macOS
```

---

## 📊 Statistiques du Projet

- **Lignes de code C** : ~2,500
- **Composants** : 6 modules
- **Taille firmware** : ~800 KB (compilé)
- **RAM utilisée** : ~120 KB (runtime)
- **Flash utilisée** : ~1.2 MB (firmware + partitions système)

---

## 🤝 Contribution

Les contributions sont les bienvenues ! Veuillez :

1. Fork le projet
2. Créer une branche feature (`git checkout -b feature/AmazingFeature`)
3. Commit vos changements (`git commit -m 'Add AmazingFeature'`)
4. Push vers la branche (`git push origin feature/AmazingFeature`)
5. Ouvrir une Pull Request

**Guidelines :**
- Code en anglais (commentaires, variables, fonctions)
- Suivre le style ESP-IDF
- Ajouter des tests si possible
- Mettre à jour la documentation

---

## 📝 Roadmap

### v1.1 (En cours)
- [x] Barre de progression OTA temps réel
- [x] Vérification automatique GitHub au boot
- [ ] Reconnexion WiFi automatique
- [ ] Watchdog pour détection de hang

### v2.0 (Planifié)
- [ ] Authentification web (Basic Auth)
- [ ] HTTPS pour serveur web
- [ ] Chiffrement NVS
- [ ] Signature firmware OTA
- [ ] Support MQTT
- [ ] API REST v2 (JSON standardisé)

### v3.0 (Futur)
- [ ] Multi-langue (FR/EN)
- [ ] Thème dark/light
- [ ] Historique des mises à jour
- [ ] Sauvegarde/restauration config
- [ ] Support ESP32-C3/C6

---

## 📄 Licence

**À définir** - Voir [LICENSE](LICENSE) pour plus de détails.

Suggestions :
- MIT License (permissive)
- Apache 2.0 (avec protection brevets)
- GPL v3 (copyleft)

---

## 👨‍💻 Auteur

**Matthieu Grr** - [@MatthieuGrr](https://github.com/MatthieuGrr)

---

## 🙏 Remerciements

- **Espressif Systems** - ESP-IDF framework
- **cJSON** - JSON parsing library
- **esp-idf-ci-action** - GitHub Actions workflow

---

## 📚 Ressources

- [ESP-IDF Programming Guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/)
- [ESP32-S3 Datasheet](https://www.espressif.com/sites/default/files/documentation/esp32-s3_datasheet_en.pdf)
- [OTA Updates Documentation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/system/ota.html)
- [HTTP Server API](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/protocols/esp_http_server.html)

---

**⭐ Si ce projet vous est utile, n'hésitez pas à lui donner une étoile sur GitHub !**
