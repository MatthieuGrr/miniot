# Configuration ESP-IDF pour MiniOT

## Problème : "Header fields are too long"

Si vous rencontrez l'erreur "414 URI Too Long" ou "Header fields are too long" sur Android, vous devez augmenter les limites HTTP dans la configuration ESP-IDF.

## Solution

### Méthode 1 : Via menuconfig (Recommandé)

```bash
idf.py menuconfig
```

Naviguez vers :
```
Component config →
  ESP HTTP Server →
    Max HTTP Request Header Length → Changez de 512 à 2048
    Max HTTP URI Length → Changez de 512 à 1024
```

### Méthode 2 : Via sdkconfig directement

Ajoutez ces lignes dans votre fichier `sdkconfig` :

```
CONFIG_HTTPD_MAX_REQ_HDR_LEN=2048
CONFIG_HTTPD_MAX_URI_LEN=1024
```

### Méthode 3 : Créer un fichier sdkconfig.defaults

Créez le fichier `sdkconfig.defaults` à la racine du projet avec :

```
# HTTP Server Configuration
CONFIG_HTTPD_MAX_REQ_HDR_LEN=2048
CONFIG_HTTPD_MAX_URI_LEN=1024

# WiFi Configuration
CONFIG_ESP_WIFI_DYNAMIC_RX_BUFFER_NUM=32
CONFIG_ESP_WIFI_STATIC_RX_BUFFER_NUM=16
CONFIG_ESP_WIFI_DYNAMIC_TX_BUFFER_NUM=32

# FreeRTOS Configuration
CONFIG_FREERTOS_HZ=100

# Partition Table
CONFIG_PARTITION_TABLE_CUSTOM=y
CONFIG_PARTITION_TABLE_CUSTOM_FILENAME="partitions.csv"
```

## Après configuration

Recompilez complètement :

```bash
idf.py fullclean
idf.py build
idf.py flash
```

## Vérification

Après le flash, connectez-vous au WiFi MiniOT-Setup-XXXX depuis Android.
La page de configuration devrait s'ouvrir automatiquement sans erreur.
