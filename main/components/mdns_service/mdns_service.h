#ifndef MDNS_SERVICE_H
#define MDNS_SERVICE_H

#include "esp_err.h"

#define MDNS_HOSTNAME "miniot"
#define MDNS_INSTANCE "MiniOT Home Automation"

/**
 * @brief Initialise le service mDNS
 * Permet d'accéder à l'ESP32 via miniot.local
 * @return ESP_OK si succès
 */
esp_err_t mdns_service_init(void);

/**
 * @brief Annonce le serveur web via mDNS
 * @param port Port du serveur HTTP (généralement 80)
 * @return ESP_OK si succès
 */
esp_err_t mdns_service_announce_http(uint16_t port);

/**
 * @brief Arrête le service mDNS
 * @return ESP_OK si succès
 */
esp_err_t mdns_service_stop(void);

#endif // MDNS_SERVICE_H
