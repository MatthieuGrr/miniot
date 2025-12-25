#ifndef OTA_MANAGER_H
#define OTA_MANAGER_H

#include "esp_err.h"

/**
 * @brief Initialiser le gestionnaire OTA
 * 
 * Vérifie l'état des partitions et valide le firmware actuel
 */
esp_err_t ota_manager_init(void);

/**
 * @brief Lancer une mise à jour OTA depuis une URL
 * 
 * @param url URL du fichier .bin à télécharger (ex: "http://192.168.1.100:8000/firmware.bin")
 * @return ESP_OK si succès
 */
esp_err_t ota_manager_start_update(const char *url);

/**
 * @brief Obtenir la version du firmware actuel
 * 
 * @return String de la version (ex: "v1.0.0")
 */
const char* ota_manager_get_version(void);

#endif // OTA_MANAGER_H
