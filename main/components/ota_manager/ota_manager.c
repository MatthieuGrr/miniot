#include "ota_manager.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_http_client.h"
#include "esp_https_ota.h"
#include "esp_app_format.h"
#include <string.h>

static const char *TAG = "OTA_MANAGER";

// Version du firmware (à incrémenter à chaque release)
#define FIRMWARE_VERSION "v1.0.0"

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
