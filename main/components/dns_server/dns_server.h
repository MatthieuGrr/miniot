#ifndef DNS_SERVER_H
#define DNS_SERVER_H

#include "esp_err.h"

/**
 * @brief Démarre le serveur DNS captif
 * Redirige toutes les requêtes DNS vers l'IP de l'ESP32 (192.168.4.1)
 * @return ESP_OK si succès
 */
esp_err_t dns_server_start(void);

/**
 * @brief Arrête le serveur DNS captif
 * @return ESP_OK si succès
 */
esp_err_t dns_server_stop(void);

#endif // DNS_SERVER_H
