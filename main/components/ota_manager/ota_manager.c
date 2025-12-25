#include "ota_manager.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_http_client.h"
#include "esp_https_ota.h"
#include "esp_app_format.h"
#include "esp_crt_bundle.h"
#include "cJSON.h"
#include "version.h"
#include <string.h>

static const char *TAG = "OTA_MANAGER";

// Buffer pour stocker la réponse HTTP de l'API GitHub
// Les releases GitHub peuvent contenir beaucoup de métadonnées (changelog, assets, etc.)
#define HTTP_RESPONSE_BUFFER_SIZE 8192
static char http_response_buffer[HTTP_RESPONSE_BUFFER_SIZE];
static int http_response_len = 0;

/**
 * ÉTAPE A : Initialisation - Valider le firmware actuel
 */
esp_err_t ota_manager_init(void)
{
    ESP_LOGI(TAG, "Initializing OTA manager...");
    ESP_LOGI(TAG, "Current firmware version: %s", FIRMWARE_VERSION);

    // Obtenir la partition sur laquelle on boot actuellement
    const esp_partition_t *running = esp_ota_get_running_partition();
    ESP_LOGI(TAG, "Running partition: %s at offset 0x%lx", 
             running->label, running->address);

    // Obtenir l'état de l'OTA
    esp_ota_img_states_t ota_state;
    if (esp_ota_get_state_partition(running, &ota_state) == ESP_OK) {
        if (ota_state == ESP_OTA_IMG_PENDING_VERIFY) {
            // Le firmware vient d'être mis à jour mais pas encore validé
            ESP_LOGW(TAG, "New firmware detected, validating...");
            
            // IMPORTANT : Valider le nouveau firmware
            // Sans ça, au prochain reboot, l'ESP32 reviendra à l'ancien firmware
            esp_ota_mark_app_valid_cancel_rollback();
            ESP_LOGI(TAG, "New firmware validated successfully!");
        }
    }

    return ESP_OK;
}

/**
 * ÉTAPE B : Callback de progression du téléchargement
 */
static esp_err_t ota_http_event_handler(esp_http_client_event_t *evt)
{
    switch (evt->event_id) {
    case HTTP_EVENT_ERROR:
        ESP_LOGE(TAG, "HTTP Error");
        break;
    case HTTP_EVENT_ON_CONNECTED:
        ESP_LOGI(TAG, "Connected to server");
        break;
    case HTTP_EVENT_HEADER_SENT:
        ESP_LOGI(TAG, "Sending HTTP request...");
        break;
    case HTTP_EVENT_ON_HEADER:
        ESP_LOGD(TAG, "Header: %s: %s", evt->header_key, evt->header_value);
        break;
    case HTTP_EVENT_ON_DATA:
        // Afficher la progression tous les 10KB
        if (evt->data_len > 0) {
            static int total_downloaded = 0;
            total_downloaded += evt->data_len;
            if (total_downloaded % 10240 == 0) {
                ESP_LOGI(TAG, "Downloaded: %d bytes", total_downloaded);
            }
        }
        break;
    case HTTP_EVENT_ON_FINISH:
        ESP_LOGI(TAG, "HTTP download finished");
        break;
    case HTTP_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "Disconnected from server");
        break;
    default:
        break;
    }
    return ESP_OK;
}

/**
 * ÉTAPE C : Lancer la mise à jour OTA
 */
esp_err_t ota_manager_start_update(const char *url)
{
    if (url == NULL) {
        ESP_LOGE(TAG, "URL cannot be NULL");
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "Starting OTA update from: %s", url);

    // Configuration du client HTTP
    esp_http_client_config_t config = {
        .url = url,
        .event_handler = ota_http_event_handler,
        .timeout_ms = 30000,         // Timeout de 30 secondes
        .keep_alive_enable = true,
        .skip_cert_common_name_check = true,  // Pour HTTP simple
    };

    // IMPORTANT: Pour HTTP (pas HTTPS), on doit définir au moins une option
    // de sécurité même si on ne l'utilise pas. C'est une exigence d'esp_https_ota.
    // On met cert_pem à une string vide pour satisfaire la validation.
    static const char dummy_cert[] = "";
    if (strncmp(url, "http://", 7) == 0) {
        // HTTP simple : définir cert_pem pour passer la validation
        config.cert_pem = dummy_cert;
    }

    // Configuration OTA
    esp_https_ota_config_t ota_config = {
        .http_config = &config,
        .partial_http_download = false,       // Télécharger tout d'un coup
        .max_http_request_size = 0,           // Pas de limite
    };

    ESP_LOGI(TAG, "Attempting to download firmware...");
    
    // FONCTION MAGIQUE : Fait tout le travail !
    // - Télécharge le .bin
    // - Vérifie qu'il y a assez d'espace
    // - Écrit sur la partition inactive (ota_0 ou ota_1)
    // - Vérifie le checksum
    // - Marque la nouvelle partition comme prête à booter
    esp_err_t ret = esp_https_ota(&ota_config);
    
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "OTA update completed successfully!");
        ESP_LOGI(TAG, "Rebooting in 3 seconds...");
        vTaskDelay(3000 / portTICK_PERIOD_MS);
        esp_restart();  // Redémarrer pour booter sur le nouveau firmware
    } else {
        ESP_LOGE(TAG, "OTA update failed: %s", esp_err_to_name(ret));
        return ret;
    }

    return ESP_OK;
}

/**
 * ÉTAPE D : Obtenir la version
 */
const char* ota_manager_get_version(void)
{
    return FIRMWARE_VERSION;
}

/**
 * ÉTAPE E : Gestionnaire d'événements HTTP pour récupérer la réponse JSON
 */
static esp_err_t github_http_event_handler(esp_http_client_event_t *evt)
{
    switch (evt->event_id) {
    case HTTP_EVENT_ON_DATA:
        // Accumuler les données reçues dans le buffer
        if (http_response_len + evt->data_len < HTTP_RESPONSE_BUFFER_SIZE) {
            memcpy(http_response_buffer + http_response_len, evt->data, evt->data_len);
            http_response_len += evt->data_len;
            http_response_buffer[http_response_len] = '\0'; // Null terminator
        } else {
            ESP_LOGW(TAG, "Response buffer full, truncating data");
        }
        break;
    default:
        break;
    }
    return ESP_OK;
}

/**
 * ÉTAPE F : Comparer deux versions sémantiques (ex: "v1.0.0" vs "v1.0.1")
 * Retourne : 1 si new_ver > current_ver, 0 si égales, -1 si new_ver < current_ver
 */
static int compare_versions(const char *current_ver, const char *new_ver)
{
    int c_major = 0, c_minor = 0, c_patch = 0;
    int n_major = 0, n_minor = 0, n_patch = 0;

    // Parser la version actuelle (skip le 'v' si présent)
    const char *c_start = (current_ver[0] == 'v') ? current_ver + 1 : current_ver;
    sscanf(c_start, "%d.%d.%d", &c_major, &c_minor, &c_patch);

    // Parser la nouvelle version (skip le 'v' si présent)
    const char *n_start = (new_ver[0] == 'v') ? new_ver + 1 : new_ver;
    sscanf(n_start, "%d.%d.%d", &n_major, &n_minor, &n_patch);

    ESP_LOGD(TAG, "Comparing versions: current=%d.%d.%d vs new=%d.%d.%d",
             c_major, c_minor, c_patch, n_major, n_minor, n_patch);

    // Comparer major
    if (n_major > c_major) return 1;
    if (n_major < c_major) return -1;

    // Comparer minor
    if (n_minor > c_minor) return 1;
    if (n_minor < c_minor) return -1;

    // Comparer patch
    if (n_patch > c_patch) return 1;
    if (n_patch < c_patch) return -1;

    return 0; // Versions identiques
}

/**
 * ÉTAPE G : Vérifier si une mise à jour est disponible sur GitHub
 */
esp_err_t ota_manager_check_github_update(const char *owner, const char *repo, ota_update_info_t *info)
{
    if (owner == NULL || repo == NULL || info == NULL) {
        ESP_LOGE(TAG, "Invalid parameters");
        return ESP_ERR_INVALID_ARG;
    }

    // Initialiser la structure
    memset(info, 0, sizeof(ota_update_info_t));
    info->update_available = false;

    // Construire l'URL de l'API GitHub pour récupérer la dernière release
    char api_url[256];
    snprintf(api_url, sizeof(api_url),
             "https://api.github.com/repos/%s/%s/releases/latest", owner, repo);

    ESP_LOGI(TAG, "Checking for updates at: %s", api_url);

    // Reset le buffer de réponse
    http_response_len = 0;
    memset(http_response_buffer, 0, HTTP_RESPONSE_BUFFER_SIZE);

    // Configuration du client HTTP avec vérification SSL sécurisée
    esp_http_client_config_t config = {
        .url = api_url,
        .event_handler = github_http_event_handler,
        .timeout_ms = 10000,
        .user_agent = "ESP32-OTA-Updater/1.0",
        .crt_bundle_attach = esp_crt_bundle_attach,  // Utiliser le bundle de certificats racines
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == NULL) {
        ESP_LOGE(TAG, "Failed to initialize HTTP client");
        return ESP_FAIL;
    }

    // Effectuer la requête GET
    esp_err_t err = esp_http_client_perform(client);
    int status_code = esp_http_client_get_status_code(client);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "HTTP GET request failed: %s", esp_err_to_name(err));
        esp_http_client_cleanup(client);
        return err;
    }

    if (status_code != 200) {
        ESP_LOGE(TAG, "HTTP GET failed with status code: %d", status_code);
        esp_http_client_cleanup(client);
        return ESP_FAIL;
    }

    esp_http_client_cleanup(client);

    ESP_LOGD(TAG, "GitHub API Response: %s", http_response_buffer);

    // Parser le JSON
    cJSON *json = cJSON_Parse(http_response_buffer);
    if (json == NULL) {
        ESP_LOGE(TAG, "Failed to parse JSON response");
        return ESP_FAIL;
    }

    // Extraire le tag_name (version)
    cJSON *tag_name = cJSON_GetObjectItem(json, "tag_name");
    if (tag_name == NULL || !cJSON_IsString(tag_name)) {
        ESP_LOGE(TAG, "tag_name not found in JSON");
        cJSON_Delete(json);
        return ESP_FAIL;
    }

    // Copier la version
    strncpy(info->version, tag_name->valuestring, sizeof(info->version) - 1);
    ESP_LOGI(TAG, "Latest GitHub release: %s (current: %s)", info->version, FIRMWARE_VERSION);

    // Comparer les versions
    int version_cmp = compare_versions(FIRMWARE_VERSION, info->version);
    if (version_cmp < 0) {
        info->update_available = true;
        ESP_LOGI(TAG, "New version available!");

        // Chercher l'asset .bin dans les assets
        cJSON *assets = cJSON_GetObjectItem(json, "assets");
        if (assets != NULL && cJSON_IsArray(assets)) {
            int asset_count = cJSON_GetArraySize(assets);
            for (int i = 0; i < asset_count; i++) {
                cJSON *asset = cJSON_GetArrayItem(assets, i);
                cJSON *name = cJSON_GetObjectItem(asset, "name");
                cJSON *download_url = cJSON_GetObjectItem(asset, "browser_download_url");

                if (name != NULL && download_url != NULL &&
                    cJSON_IsString(name) && cJSON_IsString(download_url)) {

                    // Chercher un fichier .bin
                    if (strstr(name->valuestring, ".bin") != NULL) {
                        strncpy(info->download_url, download_url->valuestring,
                                sizeof(info->download_url) - 1);
                        ESP_LOGI(TAG, "Firmware binary found: %s", info->download_url);
                        break;
                    }
                }
            }

            if (strlen(info->download_url) == 0) {
                ESP_LOGW(TAG, "No .bin file found in release assets");
                info->update_available = false;
            }
        }
    } else if (version_cmp == 0) {
        ESP_LOGI(TAG, "Already running the latest version");
    } else {
        ESP_LOGI(TAG, "Current version is newer than GitHub release");
    }

    cJSON_Delete(json);
    return ESP_OK;
}

/**
 * ÉTAPE H : Mise à jour depuis GitHub (raccourci)
 */
esp_err_t ota_manager_update_from_github(const char *owner, const char *repo)
{
    ota_update_info_t info;

    esp_err_t err = ota_manager_check_github_update(owner, repo, &info);
    if (err != ESP_OK) {
        return err;
    }

    if (!info.update_available) {
        ESP_LOGI(TAG, "No update available");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Starting update to version %s", info.version);
    return ota_manager_start_update(info.download_url);
}
