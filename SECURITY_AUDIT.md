# Audit de Sécurité et Qualité - Projet MiniOT

**Date:** 26 Décembre 2025
**Version analysée:** v1.0.0
**Cible:** ESP32-S3, ESP-IDF v5.3
**Auditeur:** Analyse automatisée complète

---

## Résumé Exécutif

### Score Global: 6.5/10

**Points forts:**
- ✅ Architecture modulaire bien structurée
- ✅ HTTPS avec validation de certificats
- ✅ Dual-bank OTA pour mises à jour sûres
- ✅ Gestion d'erreurs correcte dans la plupart des modules

**Points critiques à corriger:**
- 🔴 **CRITIQUE:** Mots de passe WiFi transmis et stockés en clair
- 🔴 **CRITIQUE:** Pas d'authentification sur l'interface web
- 🔴 **CRITIQUE:** API REST non sécurisée
- 🟡 **IMPORTANT:** Fuites potentielles de mémoire
- 🟡 **IMPORTANT:** README générique non mis à jour

---

## 1. VULNÉRABILITÉS DE SÉCURITÉ

### 🔴 CRITIQUE - Sécurité Niveau 1

#### 1.1 Stockage des Mots de Passe en Clair (CVE Potentiel)

**Fichier:** `main/components/nvs_storage/nvs_storage.c:52-58`

```c
// Sauvegarder le mot de passe
ret = nvs_set_str(nvs_handle, "password", config->password);
```

**Problème:**
- Les mots de passe WiFi sont stockés en texte clair dans la NVS Flash
- Accessible avec un accès physique au chip (flash dump)
- Aucun chiffrement au repos

**Impact:**
- Exposition des credentials WiFi
- Compromission du réseau WiFi en cas de vol du device

**Recommandation:**
```c
// Option 1: Utiliser NVS encryption
esp_err_t nvs_flash_init_partition_encrypted("nvs");

// Option 2: Chiffrer avant stockage avec AES
// Utiliser les clés hardware du ESP32-S3 (eFuse)
```

**Priorité:** CRITIQUE - À corriger immédiatement

---

#### 1.2 Transmission HTTP Non Sécurisée des Credentials

**Fichier:** `main/components/web_server/web_server.c:327-384`

```c
static esp_err_t configure_handler(httpd_req_t *req)
{
    // Récupère SSID et password via HTTP POST
    cJSON *ssid_json = cJSON_GetObjectItem(json, "ssid");
    cJSON *password_json = cJSON_GetObjectItem(json, "password");
```

**Problème:**
- L'interface web utilise HTTP (pas HTTPS)
- Les credentials WiFi transitent en clair sur le réseau
- Vulnérable aux attaques Man-in-the-Middle (MitM)
- Capture facile avec Wireshark/tcpdump

**Impact:**
- Interception des mots de passe WiFi pendant la configuration
- Compromission du réseau lors du setup initial

**Recommandation:**
```c
// Implémenter HTTPS pour le serveur web
httpd_ssl_config_t ssl_config = HTTPD_SSL_CONFIG_DEFAULT();
ssl_config.cacert_pem = server_cert_pem_start;
ssl_config.prvtkey_pem = server_key_pem_start;
httpd_ssl_start(&server, &ssl_config);
```

**Priorité:** CRITIQUE - Production non recommandée sans HTTPS

---

#### 1.3 Pas d'Authentification sur les API Critiques

**Fichier:** `main/components/web_server/web_server.c`

**Endpoints non protégés:**
- `POST /api/configure` - Change la config WiFi
- `POST /api/factory_reset` - Efface toutes les données
- `POST /api/reboot` - Redémarre l'appareil
- `POST /api/ota_update` - Flash un firmware arbitraire
- `POST /api/install_github_update` - Installe une mise à jour

**Problème:**
- Aucune authentification (username/password, API key, token)
- Aucune protection CSRF
- N'importe qui sur le réseau peut:
  - Effacer la configuration
  - Redémarrer l'appareil
  - Flasher un firmware malveillant
  - Changer les credentials WiFi

**Impact:**
- Prise de contrôle complète de l'appareil
- Déni de service (DoS)
- Installation de firmware malveillant

**Recommandation:**
```c
// Option 1: Basic Auth
if (!check_basic_auth(req, "admin", "password_hash")) {
    httpd_resp_set_status(req, "401 Unauthorized");
    httpd_resp_set_hdr(req, "WWW-Authenticate", "Basic realm=\"MiniOT\"");
    return ESP_FAIL;
}

// Option 2: API Token
const char *token = httpd_req_get_hdr_value_str(req, "X-API-Token");
if (!verify_token(token)) {
    httpd_resp_set_status(req, "403 Forbidden");
    return ESP_FAIL;
}

// Option 3: CSRF Token
// Générer un token unique par session
// Valider dans chaque requête POST
```

**Priorité:** CRITIQUE - Risque de prise de contrôle

---

### 🟡 IMPORTANT - Sécurité Niveau 2

#### 1.4 Validation d'Entrée Insuffisante

**Fichier:** `main/components/web_server/web_server.c:327-384`

```c
// Pas de validation de longueur avant strcpy
strncpy(wifi_config.ssid, ssid_json->valuestring, MAX_SSID_LEN - 1);
strncpy(wifi_config.password, password_json->valuestring, MAX_PASSWORD_LEN - 1);
```

**Problème:**
- Pas de vérification de caractères spéciaux
- Pas de sanitization des entrées
- Potentiel injection dans les logs

**Impact:**
- Log injection
- Buffer overflow potentiel si MAX_*_LEN mal défini

**Recommandation:**
```c
// Valider les entrées
bool is_valid_ssid(const char *ssid) {
    if (!ssid || strlen(ssid) == 0 || strlen(ssid) > 32) return false;
    // Vérifier caractères valides
    for (int i = 0; ssid[i]; i++) {
        if (!isprint(ssid[i])) return false;
    }
    return true;
}
```

**Priorité:** IMPORTANT - Ajouter validation

---

#### 1.5 Firmware Update Sans Vérification de Signature

**Fichier:** `main/components/ota_manager/ota_manager.c:86-232`

**Problème:**
- Le firmware téléchargé n'est pas signé/vérifié
- N'importe quel fichier .bin peut être flashé
- Pas de vérification de l'origine du firmware

**Impact:**
- Installation de firmware malveillant via:
  - URL arbitraire dans l'API `/api/ota_update`
  - Release GitHub compromise

**Recommandation:**
```c
// ESP-IDF supporte la signature de firmware
// 1. Générer une clé de signature
// 2. Signer le firmware dans le build
// 3. Activer la vérification:
esp_https_ota_config_t ota_config = {
    .http_config = &config,
    .partial_http_download = true,
};
// ESP-IDF vérifiera automatiquement la signature si configuré
```

**Configuration requise dans sdkconfig:**
```
CONFIG_SECURE_BOOT=y
CONFIG_SECURE_BOOT_V2_ENABLED=y
CONFIG_SECURE_SIGNED_APPS_SCHEME=RSA
```

**Priorité:** IMPORTANT - Pour environnement production

---

#### 1.6 Exposition du Hostname GitHub Hardcodé

**Fichier:** `main/main.c:64`, `web_server.c:521,552`

```c
ota_manager_check_github_update("MatthieuGrr", "miniot", &update_info);
```

**Problème:**
- Username/repo hardcodés
- Pas de configuration possible
- Si le repo est compromis, tous les devices sont vulnérables

**Recommandation:**
```c
// Stocker dans NVS avec configuration par défaut
#define DEFAULT_GITHUB_OWNER "MatthieuGrr"
#define DEFAULT_GITHUB_REPO "miniot"

// Permettre la configuration via web UI
nvs_get_str(handle, "github_owner", owner_buf, &len);
```

**Priorité:** MOYEN - Amélioration de flexibilité

---

### 🟢 BONNES PRATIQUES DE SÉCURITÉ

#### ✅ Points Positifs

1. **HTTPS avec Validation de Certificats**
   - `ota_manager.c:115` - Utilise `esp_crt_bundle_attach`
   - Validation correcte des certificats GitHub
   - Pas de `skip_cert_verification`

2. **Dual-Bank OTA**
   - Partitions `ota_0` et `ota_1`
   - Rollback automatique si boot fail
   - Validation du firmware avant commit

3. **Gestion d'Erreurs Robuste**
   - Vérification systématique des retours `esp_err_t`
   - Logs détaillés avec ESP_LOG*
   - Cleanup des ressources en cas d'erreur

---

## 2. FUITES DE MÉMOIRE ET GESTION DES RESSOURCES

### 🟡 Fuites Potentielles

#### 2.1 JSON Non Libéré dans web_server.c

**Lignes multiples:**

```c
// web_server.c:347 - Pas de cJSON_Delete si erreur avant
cJSON *root = cJSON_Parse(buf);
if (!root) {
    httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
    return ESP_FAIL;  // ❌ Fuite: root déjà alloué mais parse échoué
}
```

**Recommandation:**
```c
cJSON *root = cJSON_Parse(buf);
if (!root) {
    httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
    // cJSON_Parse retourne NULL en cas d'erreur, pas de fuite ici
    return ESP_FAIL;
}
// Mais ajouter cJSON_Delete dans TOUS les chemins de sortie
```

#### 2.2 Tâches OTA Non Nettoyées

**Fichier:** `web_server.c:433-448, 548-559`

```c
static void ota_task_function(void *param) {
    char *url = (char *)param;
    esp_err_t ret = ota_manager_start_update(url);
    free(url);
    vTaskDelete(NULL);  // ✅ Correct
}
```

**Status:** ✅ Correct - Tâche se supprime elle-même

---

#### 2.3 Handles HTTP Client Non Fermés en Cas d'Erreur

**Fichier:** `ota_manager.c:300-314`

```c
esp_http_client_handle_t client = esp_http_client_init(&config);
if (client == NULL) {
    return ESP_FAIL;
}
esp_err_t err = esp_http_client_perform(client);
if (err != ESP_OK) {
    esp_http_client_cleanup(client);  // ✅ Correct
    return err;
}
```

**Status:** ✅ Correct - Cleanup en cas d'erreur

---

## 3. PROBLÈMES DE MAINTENABILITÉ

### 🟡 Code Duplication

#### 3.1 Duplication du Code de Démarrage AP

**Fichier:** `main/main.c:89-101` et `105-120`

```c
// Bloc 1 (ligne 89-101)
wifi_manager_start_ap();
dns_server_start();
web_server_start();

// Bloc 2 (ligne 105-120) - IDENTIQUE
wifi_manager_start_ap();
dns_server_start();
web_server_start();
```

**Recommandation:**
```c
static void start_ap_mode(void) {
    ESP_LOGI(TAG, "Starting Access Point mode...");
    ESP_ERROR_CHECK(wifi_manager_start_ap());

    ESP_LOGI(TAG, "Starting DNS captive portal...");
    ESP_ERROR_CHECK(dns_server_start());

    ESP_LOGI(TAG, "Starting web server...");
    ESP_ERROR_CHECK(web_server_start());

    ESP_LOGI(TAG, "=== MiniOT Ready (AP Mode) ===");
    ESP_LOGI(TAG, "Connect to WiFi network and navigate to http://192.168.4.1");
}
```

---

#### 3.2 Magic Numbers Non Définis

**Exemples:**

```c
// web_server.c:86
"<div style='background:#ddd;border-radius:10px;overflow:hidden;height:30px'>"

// main.c:59
vTaskDelay(2000 / portTICK_PERIOD_MS);  // Pourquoi 2000ms?

// web_server.c:246
if(elapsed<2){return;}  // Pourquoi 2 secondes?
```

**Recommandation:**
```c
#define DNS_INIT_DELAY_MS 2000
#define OTA_START_TIMEOUT_SEC 2
#define PROGRESS_BAR_HEIGHT_PX 30
```

---

### 🟡 Commentaires et Documentation

#### 3.3 Code en Français Mélangé avec Anglais

**Incohérence linguistique:**

```c
// Français
ESP_LOGI(TAG, "=== MiniOT Ready (AP Mode) ===");
// Sauvegarder le mot de passe

// Anglais
ESP_LOGI(TAG, "Successfully connected to WiFi!");
static const char *TAG = "OTA_MANAGER";
```

**Recommandation:**
- Choisir une langue (anglais recommandé pour open-source)
- Uniformiser tous les messages et commentaires

---

#### 3.4 TODO Non Implémentés

**Fichier:** `main/main.c:131-132`

```c
// TODO: Implémenter la logique de reconnexion automatique
// Si en mode STA et déconnecté pendant plus de X secondes -> passer en mode AP
```

**Impact:**
- Fonctionnalité critique manquante
- Device peut rester déconnecté indéfiniment

**Recommandation:**
```c
static time_t disconnect_timestamp = 0;
#define RECONNECT_TO_AP_TIMEOUT_SEC 300  // 5 minutes

if (state == WIFI_STATE_DISCONNECTED) {
    if (disconnect_timestamp == 0) {
        disconnect_timestamp = time(NULL);
    } else if (time(NULL) - disconnect_timestamp > RECONNECT_TO_AP_TIMEOUT_SEC) {
        ESP_LOGW(TAG, "Disconnected for too long, switching to AP mode");
        start_ap_mode();
    }
}
```

---

### 🟢 Points Positifs de Maintenabilité

1. **Architecture Modulaire**
   - Composants bien séparés
   - Interfaces publiques claires (.h files)
   - Responsabilités distinctes

2. **Gestion d'Erreurs Cohérente**
   - Retours `esp_err_t` systématiques
   - Logs informatifs

3. **Nommage Clair**
   - Fonctions bien nommées
   - Préfixes par composant (`wifi_manager_*`, `ota_manager_*`)

---

## 4. PROBLÈMES DE DOCUMENTATION

### 🔴 CRITIQUE - README Générique

**Fichier:** `README.md`

**Problème:**
- **Le README est celui du template ESP-IDF par défaut**
- Aucune information sur MiniOT
- Pas d'instructions d'installation
- Pas de description des fonctionnalités

**Contenu actuel:**
```markdown
# _Sample project_

This is the simplest buildable example. The example is used by command `idf.py create-project`
```

**Ce qu'il devrait contenir:**

```markdown
# MiniOT - IoT Device Manager

ESP32-S3 based IoT device with:
- WiFi configuration via captive portal
- OTA firmware updates from GitHub releases
- Web-based configuration interface
- mDNS device discovery

## Features
- Automatic WiFi setup (AP mode on first boot)
- Secure HTTPS OTA updates
- Real-time progress monitoring
- Factory reset capability

## Hardware
- ESP32-S3
- 4MB Flash minimum
- WiFi required

## Quick Start
1. Flash firmware: `idf.py flash monitor`
2. Connect to "MiniOT-Setup-XXXX" WiFi
3. Navigate to http://192.168.4.1
4. Configure WiFi credentials

## API Documentation
[Liste des endpoints REST]

## OTA Updates
[Instructions GitHub releases]

## Development
- ESP-IDF: v5.3
- Target: esp32s3
- Partition: Custom dual-bank OTA

## License
[À définir]
```

**Priorité:** CRITIQUE - Documentation essentielle

---

### 🟡 Documentation Manquante

**Fichiers manquants:**

1. **ARCHITECTURE.md** - Diagramme des composants
2. **API.md** - Documentation REST API complète
3. **SECURITY.md** - Politiques de sécurité
4. **CONTRIBUTING.md** - Guide pour contributeurs
5. **CHANGELOG.md** - Historique des versions
6. **LICENSE** - Licence du projet

---

## 5. CONFIGURATION ET BUILD

### 🟡 Problèmes de Configuration

#### 5.1 Fichier VERSION Tracké dans Git

**Problème:**
```bash
# VERSION devrait être dans .gitignore pour développement
# Mais gardé pour releases
```

**Solution actuelle:** ✅ Correcte
- VERSION ignoré sur GitHub Actions
- Utilisé en développement local

---

#### 5.2 sdkconfig Non Ignoré

**Fichier:** `.gitignore:3`

```
sdkconfig
```

**Problème:**
- `sdkconfig` est ignoré (correct)
- Mais `sdkconfig.defaults` est tracké (correct)

**Status:** ✅ Configuration correcte

---

### 🟢 Points Positifs

1. **sdkconfig.defaults Complet**
   - Configuration minimale mais fonctionnelle
   - HTTPS activé
   - Tailles appropriées

2. **Partitions OTA Correctes**
   - 2x 1.5MB pour firmware
   - NVS, OTA data, PHY init

3. **CI/CD Fonctionnel**
   - GitHub Actions bien configuré
   - Build automatique sur tags
   - Releases automatiques

---

## 6. TESTS ET QUALITÉ

### 🔴 CRITIQUE - Aucun Test

**Problèmes:**
- Aucun test unitaire
- Aucun test d'intégration
- Pas de tests de sécurité
- Pas de CI pour tests

**Recommandation:**

Créer `main/components/*/test/`:
```c
// test_nvs_storage.c
TEST_CASE("nvs_storage saves and loads config", "[nvs]") {
    miniot_wifi_config_t config = {
        .ssid = "TestSSID",
        .password = "TestPass123",
        .ap_timeout = 60
    };

    TEST_ASSERT_EQUAL(ESP_OK, nvs_storage_save_wifi_config(&config));

    miniot_wifi_config_t loaded;
    TEST_ASSERT_EQUAL(ESP_OK, nvs_storage_load_wifi_config(&loaded));
    TEST_ASSERT_EQUAL_STRING(config.ssid, loaded.ssid);
}
```

**Priorité:** IMPORTANT - Qualité production

---

## 7. RECOMMANDATIONS PRIORITAIRES

### 🔴 URGENT (< 1 semaine)

1. **Sécurité Web:**
   - Ajouter authentification Basic Auth sur API
   - Implémenter CSRF tokens
   - Validation stricte des entrées

2. **Documentation:**
   - Réécrire README.md complet
   - Documenter les API REST
   - Ajouter LICENSE

3. **Credentials:**
   - Activer NVS encryption
   - Avertir utilisateurs: HTTP non sécurisé

### 🟡 IMPORTANT (< 1 mois)

4. **HTTPS pour Web Server:**
   - Générer certificats auto-signés
   - Configurer HTTPS

5. **Signature Firmware:**
   - Activer Secure Boot v2
   - Signer tous les firmwares

6. **Tests:**
   - Tests unitaires pour composants critiques
   - Tests d'intégration WiFi

### 🟢 AMÉLIORATIONS (< 3 mois)

7. **Reconnexion Automatique:**
   - Implémenter le TODO ligne 131-132

8. **Refactoring:**
   - Éliminer duplication de code
   - Uniformiser la langue (français/anglais)
   - Définir magic numbers

9. **Monitoring:**
   - Watchdog pour detect hang
   - Logs structurés (JSON)

---

## 8. SCORE DÉTAILLÉ

| Catégorie | Score | Commentaire |
|-----------|-------|-------------|
| **Sécurité** | 4/10 | Vulnérabilités critiques présentes |
| **Architecture** | 8/10 | Modulaire et bien structuré |
| **Gestion Mémoire** | 7/10 | Quelques fuites potentielles |
| **Maintenabilité** | 6/10 | Duplication, TODOs, langue mixte |
| **Documentation** | 2/10 | README générique, docs manquantes |
| **Tests** | 0/10 | Aucun test implémenté |
| **Configuration** | 8/10 | Build bien configuré |

**SCORE GLOBAL: 6.5/10**

---

## 9. CONCLUSION

Le projet **MiniOT** présente une **architecture solide et modulaire**, avec une bonne gestion des mises à jour OTA et une structure de code claire. Cependant, il souffre de **vulnérabilités de sécurité critiques** qui le rendent **inadapté pour un environnement de production** sans corrections majeures.

### Actions Bloquantes pour Production:

1. ✅ Implémenter authentification web
2. ✅ Chiffrer les credentials stockés
3. ✅ Ajouter HTTPS au serveur web
4. ✅ Documenter le projet (README)

### Recommandation:

**Version actuelle: Prototype/Développement uniquement**

Après corrections de sécurité → **Prêt pour déploiement limité**

---

**Rapport généré le:** 2025-12-26
**Prochaine révision recommandée:** Après implémentation des corrections urgentes
