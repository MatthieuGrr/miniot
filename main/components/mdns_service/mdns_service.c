#include "mdns_service.h"
#include "mdns.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "MDNS_SERVICE";

esp_err_t mdns_service_init(void)
{
    ESP_LOGI(TAG, "Initializing mDNS service...");

    // Initialiser mDNS
    esp_err_t ret = mdns_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize mDNS: %s", esp_err_to_name(ret));
        return ret;
    }

    // Définir le hostname
    ret = mdns_hostname_set(MDNS_HOSTNAME);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set hostname: %s", esp_err_to_name(ret));
        return ret;
    }

    // Définir l'instance par défaut
    ret = mdns_instance_name_set(MDNS_INSTANCE);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set instance name: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "mDNS initialized successfully");
    ESP_LOGI(TAG, "Device accessible at: http://%s.local", MDNS_HOSTNAME);

    return ESP_OK;
}

esp_err_t mdns_service_announce_http(uint16_t port)
{
    ESP_LOGI(TAG, "Announcing HTTP service on port %d", port);

    // Annoncer le service HTTP
    esp_err_t ret = mdns_service_add(NULL, "_http", "_tcp", port, NULL, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add HTTP service: %s", esp_err_to_name(ret));
        return ret;
    }

    // Ajouter des métadonnées TXT pour le service
    mdns_txt_item_t txt_data[] = {
        {"board", "ESP32-S3"},
        {"app", "MiniOT"},
        {"version", "1.0.0"}
    };

    ret = mdns_service_txt_set("_http", "_tcp", txt_data, 3);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to set TXT records: %s", esp_err_to_name(ret));
        // Non bloquant, on continue
    }

    ESP_LOGI(TAG, "HTTP service announced via mDNS");
    return ESP_OK;
}

esp_err_t mdns_service_stop(void)
{
    ESP_LOGI(TAG, "Stopping mDNS service");
    mdns_free();
    return ESP_OK;
}
