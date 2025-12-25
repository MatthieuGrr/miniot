#ifndef OTA_MANAGER_H
#define OTA_MANAGER_H

#include "esp_err.h"
#include <stdbool.h>

/**
 * @brief Structure contenant les informations d'une mise à jour disponible
 */
typedef struct {
    char version[32];           // Version disponible (ex: "v1.0.1")
    char download_url[256];     // URL de téléchargement du .bin
    bool update_available;      // True si une nouvelle version existe
} ota_update_info_t;

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

/**
 * @brief Vérifier si une nouvelle version est disponible sur GitHub
 *
 * @param owner Propriétaire du repository (ex: "matthieu")
 * @param repo Nom du repository (ex: "miniot")
 * @param info Structure qui sera remplie avec les infos de mise à jour
 * @return ESP_OK si la vérification a réussi (même si pas de MAJ disponible)
 */
esp_err_t ota_manager_check_github_update(const char *owner, const char *repo, ota_update_info_t *info);

/**
 * @brief Lancer une mise à jour depuis GitHub (raccourci)
 *
 * Vérifie s'il y a une mise à jour et la télécharge si disponible
 * @param owner Propriétaire du repository
 * @param repo Nom du repository
 * @return ESP_OK si succès
 */
esp_err_t ota_manager_update_from_github(const char *owner, const char *repo);

#endif // OTA_MANAGER_H
