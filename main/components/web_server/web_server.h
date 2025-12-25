#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include "esp_err.h"
#include "esp_http_server.h"

/**
 * @brief Démarre le serveur web de configuration
 * @return ESP_OK si succès
 */
esp_err_t web_server_start(void);

/**
 * @brief Arrête le serveur web
 * @return ESP_OK si succès
 */
esp_err_t web_server_stop(void);

/**
 * @brief Récupère le handle du serveur HTTP
 * @return Handle du serveur ou NULL
 */
httpd_handle_t web_server_get_handle(void);

#endif // WEB_SERVER_H
