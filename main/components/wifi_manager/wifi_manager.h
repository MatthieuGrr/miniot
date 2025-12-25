#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include "esp_err.h"
#include "esp_wifi_types.h"
#include <stdbool.h>

#define WIFI_AP_SSID_PREFIX "MiniOT-Setup-"
#define WIFI_AP_PASSWORD ""  // AP ouvert par défaut
#define WIFI_AP_CHANNEL 1
#define WIFI_AP_MAX_CONNECTIONS 4

#define WIFI_STA_MAXIMUM_RETRY 5

typedef enum {
    WIFI_STATE_IDLE,
    WIFI_STATE_AP_STARTED,
    WIFI_STATE_STA_CONNECTING,
    WIFI_STATE_STA_CONNECTED,
    WIFI_STATE_STA_DISCONNECTED
} wifi_manager_state_t;

typedef void (*wifi_event_cb_t)(wifi_manager_state_t state);

/**
 * @brief Initialise le gestionnaire WiFi
 * @return ESP_OK si succès
 */
esp_err_t wifi_manager_init(void);

/**
 * @brief Démarre le mode Access Point
 * @return ESP_OK si succès
 */
esp_err_t wifi_manager_start_ap(void);

/**
 * @brief Démarre le mode Station (client WiFi)
 * @param ssid SSID du réseau WiFi
 * @param password Mot de passe du réseau
 * @param timeout_sec Timeout en secondes avant de passer en mode AP si échec
 * @return ESP_OK si succès
 */
esp_err_t wifi_manager_start_sta(const char *ssid, const char *password, uint32_t timeout_sec);

/**
 * @brief Arrête le WiFi
 * @return ESP_OK si succès
 */
esp_err_t wifi_manager_stop(void);

/**
 * @brief Lance un scan des réseaux WiFi disponibles
 * @param ap_records Pointeur vers un tableau pour stocker les résultats
 * @param max_records Nombre maximum de réseaux à récupérer
 * @param count Pointeur vers le nombre de réseaux trouvés
 * @return ESP_OK si succès
 */
esp_err_t wifi_manager_scan(wifi_ap_record_t *ap_records, uint16_t max_records, uint16_t *count);

/**
 * @brief Récupère l'état actuel du WiFi
 * @return État actuel
 */
wifi_manager_state_t wifi_manager_get_state(void);

/**
 * @brief Enregistre un callback pour les événements WiFi
 * @param callback Fonction à appeler lors des changements d'état
 */
void wifi_manager_set_event_callback(wifi_event_cb_t callback);

/**
 * @brief Récupère l'adresse IP actuelle en mode STA
 * @param ip_str Buffer pour stocker l'IP (minimum 16 caractères)
 * @return ESP_OK si connecté et IP récupérée
 */
esp_err_t wifi_manager_get_ip(char *ip_str);

/**
 * @brief Récupère l'adresse MAC
 * @param mac_str Buffer pour stocker la MAC (minimum 18 caractères)
 * @return ESP_OK si succès
 */
esp_err_t wifi_manager_get_mac(char *mac_str);

#endif // WIFI_MANAGER_H
