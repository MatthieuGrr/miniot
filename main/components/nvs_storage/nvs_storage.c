#include "nvs_storage.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "NVS_STORAGE";
static const char *NVS_NAMESPACE = "wifi_config";

esp_err_t nvs_storage_init(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS partition was truncated and needs to be erased");
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }

    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "NVS initialized successfully");
    } else {
        ESP_LOGE(TAG, "Failed to initialize NVS: %s", esp_err_to_name(ret));
    }

    return ret;
}

esp_err_t nvs_storage_save_wifi_config(const miniot_wifi_config_t *config)
{
    if (!config) {
        ESP_LOGE(TAG, "Config pointer is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    nvs_handle_t nvs_handle;
    esp_err_t ret;

    ret = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error opening NVS handle: %s", esp_err_to_name(ret));
        return ret;
    }

    // Sauvegarder le SSID
    ret = nvs_set_str(nvs_handle, "ssid", config->ssid);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to save SSID: %s", esp_err_to_name(ret));
        nvs_close(nvs_handle);
        return ret;
    }

    // Sauvegarder le mot de passe
    ret = nvs_set_str(nvs_handle, "password", config->password);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to save password: %s", esp_err_to_name(ret));
        nvs_close(nvs_handle);
        return ret;
    }

    // Sauvegarder le timeout AP
    ret = nvs_set_u32(nvs_handle, "ap_timeout", config->ap_timeout);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to save AP timeout: %s", esp_err_to_name(ret));
        nvs_close(nvs_handle);
        return ret;
    }

    // Marquer comme configuré
    ret = nvs_set_u8(nvs_handle, "configured", 1);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to save configured flag: %s", esp_err_to_name(ret));
        nvs_close(nvs_handle);
        return ret;
    }

    // Commit les changements
    ret = nvs_commit(nvs_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to commit NVS: %s", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "WiFi configuration saved successfully");
    }

    nvs_close(nvs_handle);
    return ret;
}

esp_err_t nvs_storage_load_wifi_config(miniot_wifi_config_t *config)
{
    if (!config) {
        ESP_LOGE(TAG, "Config pointer is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    nvs_handle_t nvs_handle;
    esp_err_t ret;

    ret = nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error opening NVS handle: %s", esp_err_to_name(ret));
        return ret;
    }

    // Vérifier si configuré
    uint8_t configured = 0;
    ret = nvs_get_u8(nvs_handle, "configured", &configured);
    if (ret != ESP_OK || configured == 0) {
        ESP_LOGW(TAG, "No WiFi configuration found");
        nvs_close(nvs_handle);
        config->is_configured = false;
        return ESP_ERR_NVS_NOT_FOUND;
    }

    // Charger le SSID
    size_t ssid_len = MAX_SSID_LEN;
    ret = nvs_get_str(nvs_handle, "ssid", config->ssid, &ssid_len);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to load SSID: %s", esp_err_to_name(ret));
        nvs_close(nvs_handle);
        return ret;
    }

    // Charger le mot de passe
    size_t password_len = MAX_PASSWORD_LEN;
    ret = nvs_get_str(nvs_handle, "password", config->password, &password_len);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to load password: %s", esp_err_to_name(ret));
        nvs_close(nvs_handle);
        return ret;
    }

    // Charger le timeout AP
    ret = nvs_get_u32(nvs_handle, "ap_timeout", &config->ap_timeout);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "AP timeout not found, using default: %d", DEFAULT_AP_TIMEOUT);
        config->ap_timeout = DEFAULT_AP_TIMEOUT;
    }

    config->is_configured = true;
    ESP_LOGI(TAG, "WiFi configuration loaded: SSID=%s, AP_Timeout=%lu", config->ssid, config->ap_timeout);

    nvs_close(nvs_handle);
    return ESP_OK;
}

esp_err_t nvs_storage_factory_reset(void)
{
    ESP_LOGW(TAG, "Performing factory reset...");

    nvs_handle_t nvs_handle;
    esp_err_t ret;

    ret = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error opening NVS handle: %s", esp_err_to_name(ret));
        return ret;
    }

    // Effacer tout le namespace
    ret = nvs_erase_all(nvs_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to erase NVS: %s", esp_err_to_name(ret));
        nvs_close(nvs_handle);
        return ret;
    }

    ret = nvs_commit(nvs_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to commit NVS: %s", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "Factory reset completed successfully");
    }

    nvs_close(nvs_handle);
    return ret;
}

bool nvs_storage_is_configured(void)
{
    nvs_handle_t nvs_handle;
    esp_err_t ret;
    uint8_t configured = 0;

    ret = nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (ret != ESP_OK) {
        return false;
    }

    ret = nvs_get_u8(nvs_handle, "configured", &configured);
    nvs_close(nvs_handle);

    return (ret == ESP_OK && configured == 1);
}
