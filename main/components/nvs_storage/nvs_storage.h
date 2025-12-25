#ifndef NVS_STORAGE_H
#define NVS_STORAGE_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#define MAX_SSID_LEN 32
#define MAX_PASSWORD_LEN 64
#define DEFAULT_AP_TIMEOUT 60  // 60 secondes par défaut

typedef struct {
    char ssid[MAX_SSID_LEN];
    char password[MAX_PASSWORD_LEN];
    uint32_t ap_timeout;
    bool is_configured;
} miniot_wifi_config_t;

/**
 * @brief Initialise le système NVS
 * @return ESP_OK si succès
 */
esp_err_t nvs_storage_init(void);

/**
 * @brief Sauvegarde la configuration WiFi dans NVS
 * @param config Structure contenant SSID, mot de passe et timeout
 * @return ESP_OK si succès
 */
esp_err_t nvs_storage_save_wifi_config(const miniot_wifi_config_t *config);

/**
 * @brief Charge la configuration WiFi depuis NVS
 * @param config Structure où charger la configuration
 * @return ESP_OK si succès et configuration trouvée
 */
esp_err_t nvs_storage_load_wifi_config(miniot_wifi_config_t *config);

/**
 * @brief Réinitialisation usine - efface toute la configuration
 * @return ESP_OK si succès
 */
esp_err_t nvs_storage_factory_reset(void);

/**
 * @brief Vérifie si l'ESP a déjà été configuré
 * @return true si configuré, false sinon
 */
bool nvs_storage_is_configured(void);

#endif // NVS_STORAGE_H
