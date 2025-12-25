#include "wifi_manager.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include <string.h>

static const char *TAG = "WIFI_MANAGER";

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT BIT1

static EventGroupHandle_t s_wifi_event_group;
static wifi_manager_state_t s_wifi_state = WIFI_STATE_IDLE;
static wifi_event_cb_t s_event_callback = NULL;
static int s_retry_num = 0;
static uint32_t s_sta_timeout_sec = 0;
static esp_netif_t *s_sta_netif = NULL;
static esp_netif_t *s_ap_netif = NULL;

static void set_state(wifi_manager_state_t new_state)
{
    if (s_wifi_state != new_state) {
        s_wifi_state = new_state;
        ESP_LOGI(TAG, "State changed to: %d", new_state);

        if (s_event_callback) {
            s_event_callback(new_state);
        }
    }
}

static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT) {
        switch (event_id) {
            case WIFI_EVENT_AP_START:
                ESP_LOGI(TAG, "Access Point started");
                set_state(WIFI_STATE_AP_STARTED);
                break;

            case WIFI_EVENT_AP_STACONNECTED: {
                wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*) event_data;
                ESP_LOGI(TAG, "Station %02x:%02x:%02x:%02x:%02x:%02x joined, AID=%d",
                         event->mac[0], event->mac[1], event->mac[2],
                         event->mac[3], event->mac[4], event->mac[5], event->aid);
                break;
            }

            case WIFI_EVENT_AP_STADISCONNECTED: {
                wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*) event_data;
                ESP_LOGI(TAG, "Station %02x:%02x:%02x:%02x:%02x:%02x left, AID=%d",
                         event->mac[0], event->mac[1], event->mac[2],
                         event->mac[3], event->mac[4], event->mac[5], event->aid);
                break;
            }

            case WIFI_EVENT_STA_START:
                ESP_LOGI(TAG, "Station mode started, connecting...");
                esp_wifi_connect();
                set_state(WIFI_STATE_STA_CONNECTING);
                break;

            case WIFI_EVENT_STA_DISCONNECTED:
                if (s_retry_num < WIFI_STA_MAXIMUM_RETRY) {
                    esp_wifi_connect();
                    s_retry_num++;
                    ESP_LOGI(TAG, "Retry to connect to the AP (attempt %d/%d)",
                             s_retry_num, WIFI_STA_MAXIMUM_RETRY);
                    set_state(WIFI_STATE_STA_CONNECTING);
                } else {
                    xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
                    ESP_LOGE(TAG, "Failed to connect to AP after %d attempts", WIFI_STA_MAXIMUM_RETRY);
                    set_state(WIFI_STATE_STA_DISCONNECTED);
                }
                break;

            default:
                break;
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
        set_state(WIFI_STATE_STA_CONNECTED);
    }
}

esp_err_t wifi_manager_init(void)
{
    ESP_LOGI(TAG, "Initializing WiFi Manager");

    // Créer le groupe d'événements
    s_wifi_event_group = xEventGroupCreate();
    if (!s_wifi_event_group) {
        ESP_LOGE(TAG, "Failed to create event group");
        return ESP_FAIL;
    }

    // Initialiser TCP/IP
    ESP_ERROR_CHECK(esp_netif_init());

    // Créer la boucle d'événements par défaut
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // Créer les interfaces réseau
    s_sta_netif = esp_netif_create_default_wifi_sta();
    s_ap_netif = esp_netif_create_default_wifi_ap();

    // Configuration WiFi par défaut
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // Enregistrer les gestionnaires d'événements
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        NULL));

    ESP_LOGI(TAG, "WiFi Manager initialized successfully");
    return ESP_OK;
}

esp_err_t wifi_manager_start_ap(void)
{
    ESP_LOGI(TAG, "Starting Access Point mode");

    // Récupérer l'adresse MAC pour créer un SSID unique
    uint8_t mac[6];
    esp_wifi_get_mac(WIFI_IF_AP, mac);

    char ssid[32];
    snprintf(ssid, sizeof(ssid), "%s%02X%02X", WIFI_AP_SSID_PREFIX, mac[4], mac[5]);

    wifi_config_t wifi_config = {
        .ap = {
            .ssid_len = strlen(ssid),
            .channel = WIFI_AP_CHANNEL,
            .max_connection = WIFI_AP_MAX_CONNECTIONS,
            .authmode = WIFI_AUTH_OPEN,  // Pas de mot de passe
            .pmf_cfg = {
                .required = false,
            },
        },
    };

    strcpy((char *)wifi_config.ap.ssid, ssid);

    // Utiliser APSTA pour permettre le scan WiFi en mode AP
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "AP started with SSID: %s (APSTA mode for scanning)", ssid);
    return ESP_OK;
}

esp_err_t wifi_manager_start_sta(const char *ssid, const char *password, uint32_t timeout_sec)
{
    if (!ssid || strlen(ssid) == 0) {
        ESP_LOGE(TAG, "SSID cannot be empty");
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "Starting Station mode, connecting to: %s", ssid);

    s_sta_timeout_sec = timeout_sec;
    s_retry_num = 0;

    // Arrêter le WiFi s'il est déjà actif
    esp_wifi_stop();

    wifi_config_t wifi_config = {
        .sta = {
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
            .pmf_cfg = {
                .capable = true,
                .required = false
            },
        },
    };

    strncpy((char *)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid) - 1);
    if (password) {
        strncpy((char *)wifi_config.sta.password, password, sizeof(wifi_config.sta.password) - 1);
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    // Attendre la connexion ou l'échec
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
            pdTRUE,
            pdFALSE,
            (timeout_sec * 1000) / portTICK_PERIOD_MS);

    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "Connected to AP successfully");
        return ESP_OK;
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGE(TAG, "Failed to connect to AP");
        return ESP_FAIL;
    } else {
        ESP_LOGE(TAG, "Connection timeout");
        return ESP_ERR_TIMEOUT;
    }
}

esp_err_t wifi_manager_stop(void)
{
    ESP_LOGI(TAG, "Stopping WiFi");

    esp_err_t ret = esp_wifi_stop();
    if (ret == ESP_OK) {
        set_state(WIFI_STATE_IDLE);
    }

    return ret;
}

esp_err_t wifi_manager_scan(wifi_ap_record_t *ap_records, uint16_t max_records, uint16_t *count)
{
    if (!ap_records || !count) {
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "Starting WiFi scan...");

    // S'assurer que le WiFi est démarré
    wifi_mode_t mode;
    esp_err_t ret = esp_wifi_get_mode(&mode);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get WiFi mode: %s", esp_err_to_name(ret));
        return ret;
    }

    // Le scan nécessite que l'interface STA soit active
    if (mode != WIFI_MODE_STA && mode != WIFI_MODE_APSTA) {
        ESP_LOGW(TAG, "WiFi not in STA/APSTA mode, current mode: %d", mode);
        // En mode AP pur, on ne devrait pas arriver ici grâce à APSTA
        return ESP_ERR_WIFI_MODE;
    }

    wifi_scan_config_t scan_config = {
        .ssid = NULL,
        .bssid = NULL,
        .channel = 0,
        .show_hidden = true,
        .scan_type = WIFI_SCAN_TYPE_ACTIVE,
        .scan_time = {
            .active = {
                .min = 100,
                .max = 300
            }
        }
    };

    ret = esp_wifi_scan_start(&scan_config, true);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "WiFi scan start failed: %s", esp_err_to_name(ret));
        return ret;
    }

    // Récupérer d'abord le nombre de réseaux trouvés
    uint16_t ap_num = 0;
    ret = esp_wifi_scan_get_ap_num(&ap_num);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get AP count: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "Scan found %d networks", ap_num);

    // Limiter au nombre demandé
    *count = (ap_num < max_records) ? ap_num : max_records;

    if (*count > 0) {
        ret = esp_wifi_scan_get_ap_records(count, ap_records);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to get scan records: %s", esp_err_to_name(ret));
            return ret;
        }
    }

    ESP_LOGI(TAG, "Scan completed, returning %d networks", *count);
    return ESP_OK;
}

wifi_manager_state_t wifi_manager_get_state(void)
{
    return s_wifi_state;
}

void wifi_manager_set_event_callback(wifi_event_cb_t callback)
{
    s_event_callback = callback;
}

esp_err_t wifi_manager_get_ip(char *ip_str)
{
    if (!ip_str) {
        return ESP_ERR_INVALID_ARG;
    }

    if (s_wifi_state != WIFI_STATE_STA_CONNECTED || !s_sta_netif) {
        return ESP_FAIL;
    }

    esp_netif_ip_info_t ip_info;
    esp_err_t ret = esp_netif_get_ip_info(s_sta_netif, &ip_info);
    if (ret == ESP_OK) {
        sprintf(ip_str, IPSTR, IP2STR(&ip_info.ip));
    }

    return ret;
}

esp_err_t wifi_manager_get_mac(char *mac_str)
{
    if (!mac_str) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t mac[6];
    esp_err_t ret = esp_wifi_get_mac(WIFI_IF_STA, mac);
    if (ret == ESP_OK) {
        sprintf(mac_str, "%02X:%02X:%02X:%02X:%02X:%02X",
                mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    }

    return ret;
}
