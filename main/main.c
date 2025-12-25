#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_system.h"
#include "nvs_storage.h"
#include "wifi_manager.h"
#include "dns_server.h"
#include "web_server.h"
#include "mdns_service.h"
#include "ota_manager.h"

static const char *TAG = "MAIN";

void app_main(void)
{
    ESP_LOGI(TAG, "=== MiniOT Starting ===");
    ESP_LOGI(TAG, "ESP-IDF Version: %s", esp_get_idf_version());

    // Etape 0 : Initialiser le gestionnaire OTA
    ESP_LOGI(TAG, "Initializing OTA manager...");
    ESP_ERROR_CHECK(ota_manager_init());

    
    // Étape 1 : Initialiser le stockage NVS
    ESP_LOGI(TAG, "Initializing NVS storage...");
    ESP_ERROR_CHECK(nvs_storage_init());

    // Étape 2 : Initialiser le gestionnaire WiFi
    ESP_LOGI(TAG, "Initializing WiFi manager...");
    ESP_ERROR_CHECK(wifi_manager_init());

    // Étape 3 : Vérifier si une configuration WiFi existe
    miniot_wifi_config_t wifi_config;
    esp_err_t ret = nvs_storage_load_wifi_config(&wifi_config);

    if (ret == ESP_OK && wifi_config.is_configured) {
        // Configuration trouvée, essayer de se connecter en mode Station
        ESP_LOGI(TAG, "WiFi configuration found, attempting to connect to: %s", wifi_config.ssid);

        ret = wifi_manager_start_sta(wifi_config.ssid, wifi_config.password, wifi_config.ap_timeout);

        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "Successfully connected to WiFi!");

            char ip[16];
            if (wifi_manager_get_ip(ip) == ESP_OK) {
                ESP_LOGI(TAG, "Device IP: %s", ip);
            }

            // Initialiser mDNS
            ESP_LOGI(TAG, "Starting mDNS service...");
            if (mdns_service_init() == ESP_OK) {
                mdns_service_announce_http(80);
            }

            // Vérifier les mises à jour GitHub
            ESP_LOGI(TAG, "Checking for firmware updates on GitHub...");
            ota_update_info_t update_info;
            esp_err_t ota_check = ota_manager_check_github_update("matthieu", "miniot", &update_info);
            if (ota_check == ESP_OK) {
                if (update_info.update_available) {
                    ESP_LOGW(TAG, "New firmware version available: %s", update_info.version);
                    ESP_LOGW(TAG, "Update available at: %s", update_info.download_url);
                    ESP_LOGW(TAG, "Use the web interface to install the update");
                } else {
                    ESP_LOGI(TAG, "Firmware is up to date (version %s)", ota_manager_get_version());
                }
            } else {
                ESP_LOGW(TAG, "Could not check for updates (this is normal if no release exists yet)");
            }

            // En mode connecté, démarrer le serveur web pour permettre la reconfiguration
            ESP_LOGI(TAG, "Starting web server for configuration...");
            web_server_start();

            ESP_LOGI(TAG, "=== MiniOT Ready (STA Mode) ===");
            ESP_LOGI(TAG, "Access device at: http://miniot.local or http://%s", ip);
        } else {
            // Échec de connexion, passer en mode AP
            ESP_LOGW(TAG, "Failed to connect to WiFi, switching to AP mode");
            wifi_manager_stop();

            // Démarrer le mode Access Point
            ESP_LOGI(TAG, "Starting Access Point mode...");
            ESP_ERROR_CHECK(wifi_manager_start_ap());

            // Démarrer le serveur DNS captif
            ESP_LOGI(TAG, "Starting DNS captive portal...");
            ESP_ERROR_CHECK(dns_server_start());

            // Démarrer le serveur web
            ESP_LOGI(TAG, "Starting web server...");
            ESP_ERROR_CHECK(web_server_start());

            ESP_LOGI(TAG, "=== MiniOT Ready (AP Mode) ===");
            ESP_LOGI(TAG, "Connect to WiFi network and navigate to http://192.168.4.1");
        }
    } else {
        // Pas de configuration trouvée, premier boot ou après factory reset
        ESP_LOGI(TAG, "No WiFi configuration found, starting in AP mode for initial setup");

        // Démarrer le mode Access Point
        ESP_LOGI(TAG, "Starting Access Point mode...");
        ESP_ERROR_CHECK(wifi_manager_start_ap());

        // Démarrer le serveur DNS captif
        ESP_LOGI(TAG, "Starting DNS captive portal...");
        ESP_ERROR_CHECK(dns_server_start());

        // Démarrer le serveur web
        ESP_LOGI(TAG, "Starting web server...");
        ESP_ERROR_CHECK(web_server_start());

        ESP_LOGI(TAG, "=== MiniOT Ready (AP Mode - First Boot) ===");
        ESP_LOGI(TAG, "Connect to WiFi network and navigate to http://192.168.4.1");
    }

    // Boucle principale
    while (1) {
        vTaskDelay(10000 / portTICK_PERIOD_MS);

        // Surveillance de l'état du WiFi
        wifi_manager_state_t state = wifi_manager_get_state();
        ESP_LOGD(TAG, "Current WiFi state: %d", state);

        // TODO: Implémenter la logique de reconnexion automatique
        // Si en mode STA et déconnecté pendant plus de X secondes -> passer en mode AP
    }
}
