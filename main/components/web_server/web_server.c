#include "web_server.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_system.h"
#include "cJSON.h"
#include "wifi_manager.h"
#include "nvs_storage.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include "ota_manager.h"
#include "esp_ota_ops.h"
#include "esp_partition.h"

static const char *TAG = "WEB_SERVER";
static httpd_handle_t s_server = NULL;

// Déclarations des pages HTML (définis plus bas)
extern const char index_html_start[] asm("_binary_index_html_start");
extern const char index_html_end[] asm("_binary_index_html_end");

// Page HTML principale (sera intégrée plus tard)
static const char *html_page =
"<!DOCTYPE html>"
"<html>"
"<head>"
"<meta charset='UTF-8'>"
"<meta name='viewport' content='width=device-width, initial-scale=1.0'>"
"<title>MiniOT Configuration</title>"
"<style>"
"body{font-family:Arial,sans-serif;margin:0;padding:20px;background:#f0f0f0}"
".container{max-width:600px;margin:0 auto;background:white;padding:20px;border-radius:8px;box-shadow:0 2px 4px rgba(0,0,0,0.1)}"
"h1{color:#333;margin-top:0}"
".section{margin:20px 0;padding:15px;background:#f9f9f9;border-radius:4px}"
"label{display:block;margin:10px 0 5px;font-weight:bold}"
"input,select{width:100%;padding:8px;border:1px solid #ddd;border-radius:4px;box-sizing:border-box}"
"button{background:#007bff;color:white;padding:10px 20px;border:none;border-radius:4px;cursor:pointer;margin:5px 5px 5px 0}"
"button:hover{background:#0056b3}"
"button.danger{background:#dc3545}"
"button.danger:hover{background:#c82333}"
".info{background:#e7f3ff;padding:10px;border-radius:4px;margin:10px 0}"
".status{padding:10px;margin:10px 0;border-radius:4px}"
".success{background:#d4edda;color:#155724}"
".error{background:#f8d7da;color:#721c24}"
"#networks{max-height:200px;overflow-y:auto}"
".network-item{padding:8px;margin:5px 0;background:white;border:1px solid #ddd;border-radius:4px;cursor:pointer}"
".network-item:hover{background:#f0f0f0}"
".loading{display:none;text-align:center;padding:20px}"
"</style>"
"</head>"
"<body>"
"<div class='container'>"
"<h1>🏠 MiniOT Configuration</h1>"
"<div id='statusMsg'></div>"
"<div class='info' id='deviceInfo'>"
"<strong>Device Info:</strong><br>"
"IP: <span id= 'ipAddr'>Loading...</span><br>"
"MAC: <span id='macAddr'>Loading...</span><br>"
"State: <span id='wifiState'>Loading...</span>"
"</div>"
"<div class='section'>"
"<h2>WiFi Configuration</h2>"
"<button onclick='scanNetworks()'>🔍 Scan WiFi Networks</button>"
"<div id='networks'></div>"
"<label>SSID:</label>"
"<input type='text' id='ssid' placeholder='Enter WiFi SSID'>"
"<label>Password:</label>"
"<input type='password' id='password' placeholder='Enter WiFi Password'>"
"<label>AP Timeout (seconds):</label>"
"<input type='number' id='apTimeout' value='60' min='10' max='300'>"
"<button onclick='saveWifiConfig()'>💾 Save WiFi Config</button>"
"</div>"
"<div class='section'>"
"<h2>System Actions</h2>"
"<button onclick='rebootDevice()'>🔄 Reboot Device</button>"
"<button class='danger' onclick='factoryReset()'>⚠️ Factory Reset</button>"
"</div>"
"<div class='section'>"
"<h2>Firmware Update (OTA)</h2>"
"<div class='info'>"
"Version: <span id='firmwareVersion'>Loading...</span><br>"
"Partition: <span id='runningPartition'>Loading...</span>"
"</div>"
"<label>Firmware URL:</label>"
"<input type='text' id='firmwareUrl' placeholder='http://192.168.1.100:8000/firmware.bin'>"
"<button onclick='startOtaUpdate()'>⬆️ Update Firmware</button>"
"</div>"
"</div>"
"<script>"
"function showStatus(msg,isError){"
"const div=document.getElementById('statusMsg');"
"div.className=isError?'status error':'status success';"
"div.textContent=msg;"
"setTimeout(()=>div.textContent='',5000);"
"}"
"async function loadDeviceInfo(){"
"try{"
"const res=await fetch('/api/status');"
"const data=await res.json();"
"document.getElementById('ipAddr').textContent=data.ip||'N/A';"
"document.getElementById('macAddr').textContent=data.mac||'N/A';"
"document.getElementById('wifiState').textContent=data.state||'N/A';"
"}catch(e){console.error('Failed to load device info',e);}"
"}"
"async function scanNetworks(){"
"const div=document.getElementById('networks');"
"div.innerHTML='<div class=\"loading\" style=\"display:block\">Scanning...</div>';"
"try{"
"const res=await fetch('/api/scan');"
"const data=await res.json();"
"if(data.networks&&data.networks.length>0){"
"div.innerHTML=data.networks.map(n=>"
"`<div class='network-item' onclick='selectNetwork(\"${n.ssid}\",${n.rssi})'>"
"${n.ssid} (${n.rssi} dBm) ${n.auth?'🔒':''}</div>`"
").join('');"
"}else{div.innerHTML='<p>No networks found</p>';}"
"}catch(e){"
"div.innerHTML='<p class=\"error\">Scan failed</p>';"
"console.error('Scan failed',e);"
"}"
"}"
"function selectNetwork(ssid,rssi){"
"document.getElementById('ssid').value=ssid;"
"document.getElementById('password').focus();"
"}"
"async function saveWifiConfig(){"
"const ssid=document.getElementById('ssid').value;"
"const password=document.getElementById('password').value;"
"const apTimeout=document.getElementById('apTimeout').value;"
"if(!ssid){showStatus('Please enter SSID',true);return;}"
"try{"
"const res=await fetch('/api/configure',{"
"method:'POST',"
"headers:{'Content-Type':'application/json'},"
"body:JSON.stringify({ssid,password,ap_timeout:parseInt(apTimeout)})"
"});"
"const data=await res.json();"
"if(data.success){"
"showStatus('Configuration saved! Rebooting...',false);"
"setTimeout(()=>location.reload(),3000);"
"}else{showStatus('Failed to save configuration',true);}"
"}catch(e){"
"showStatus('Error saving configuration',true);"
"console.error('Save failed',e);"
"}"
"}"
"async function rebootDevice(){"
"if(confirm('Reboot the device?')){"
"try{"
"await fetch('/api/reboot',{method:'POST'});"
"showStatus('Rebooting...',false);"
"}catch(e){console.error('Reboot failed',e);}"
"}"
"}"
"async function factoryReset(){"
"if(confirm('Factory reset will erase all settings. Continue?')){"
"try{"
"await fetch('/api/factory_reset',{method:'POST'});"
"showStatus('Factory reset completed. Rebooting...',false);"
"setTimeout(()=>location.reload(),3000);"
"}catch(e){console.error('Factory reset failed',e);}"
"}"
"}"
"async function loadFirmwareInfo(){"
"const res=await fetch('/api/ota_version');"
"const data=await res.json();"
"document.getElementById('firmwareVersion').textContent=data.version||'N/A';"
"document.getElementById('runningPartition').textContent=data.partition||'N/A';"
"}"
"async function startOtaUpdate(){"
"const url=document.getElementById('firmwareUrl').value;"
"if(!url){showStatus('Please enter firmware URL',true);return;}"
"if(confirm('Start OTA update? Device will reboot after update.')){"
"try{"
"const res=await fetch('/api/ota_update',{"
"method:'POST',"
"headers:{'Content-Type':'application/json'},"
"body:JSON.stringify({url})"
"});"
"const data=await res.json();"
"if(data.success){"
"showStatus('OTA update in progress... Device will reboot automatically.',false);"
"}else{showStatus('Failed to start OTA update',true);}"
"}catch(e){showStatus('Error starting OTA update',true);console.error('OTA failed',e);}"
"}"
"}"
"loadFirmwareInfo();"
"loadDeviceInfo();"
"</script>"
"</body>"
"</html>";

/* Handler pour la page principale */
static esp_err_t index_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, html_page, strlen(html_page));
    return ESP_OK;
}

/* Handler pour les requêtes de détection de portail captif */
static esp_err_t captive_portal_handler(httpd_req_t *req)
{
    // Rediriger vers la page principale pour forcer l'ouverture du portail captif
    httpd_resp_set_status(req, "302 Found");
    httpd_resp_set_hdr(req, "Location", "http://192.168.4.1/");
    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
}

/* Handler pour GET /api/status */
static esp_err_t status_handler(httpd_req_t *req)
{
    cJSON *root = cJSON_CreateObject();

    char mac[18];
    if (wifi_manager_get_mac(mac) == ESP_OK) {
        cJSON_AddStringToObject(root, "mac", mac);
    }

    wifi_manager_state_t state = wifi_manager_get_state();
    const char *state_str;
    switch (state) {
        case WIFI_STATE_IDLE: state_str = "Idle"; break;
        case WIFI_STATE_AP_STARTED: state_str = "AP Mode"; break;
        case WIFI_STATE_STA_CONNECTING: state_str = "Connecting"; break;
        case WIFI_STATE_STA_CONNECTED: state_str = "Connected"; break;
        case WIFI_STATE_STA_DISCONNECTED: state_str = "Disconnected"; break;
        default: state_str = "Unknown"; break;
    }
    cJSON_AddStringToObject(root, "state", state_str);

    char ip[16];
    if (wifi_manager_get_ip(ip) == ESP_OK) {
        cJSON_AddStringToObject(root, "ip", ip);
    }

    const char *json_str = cJSON_Print(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, json_str);

    free((void *)json_str);
    cJSON_Delete(root);
    return ESP_OK;
}

/* Handler pour GET /api/scan */
static esp_err_t scan_handler(httpd_req_t *req)
{
    wifi_ap_record_t ap_records[20];
    uint16_t ap_count = 0;

    esp_err_t ret = wifi_manager_scan(ap_records, 20, &ap_count);

    cJSON *root = cJSON_CreateObject();
    cJSON *networks = cJSON_CreateArray();

    if (ret == ESP_OK) {
        for (int i = 0; i < ap_count; i++) {
            cJSON *network = cJSON_CreateObject();
            cJSON_AddStringToObject(network, "ssid", (char *)ap_records[i].ssid);
            cJSON_AddNumberToObject(network, "rssi", ap_records[i].rssi);
            cJSON_AddBoolToObject(network, "auth", ap_records[i].authmode != WIFI_AUTH_OPEN);
            cJSON_AddItemToArray(networks, network);
        }
    }

    cJSON_AddItemToObject(root, "networks", networks);
    cJSON_AddNumberToObject(root, "count", ap_count);

    const char *json_str = cJSON_Print(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, json_str);

    free((void *)json_str);
    cJSON_Delete(root);
    return ESP_OK;
}

/* Handler pour POST /api/configure */
static esp_err_t configure_handler(httpd_req_t *req)
{
    char content[512];
    int ret = httpd_req_recv(req, content, sizeof(content) - 1);
    if (ret <= 0) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    content[ret] = '\0';

    cJSON *root = cJSON_Parse(content);
    if (!root) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    cJSON *ssid_json = cJSON_GetObjectItem(root, "ssid");
    cJSON *password_json = cJSON_GetObjectItem(root, "password");
    cJSON *timeout_json = cJSON_GetObjectItem(root, "ap_timeout");

    bool success = false;
    if (ssid_json && cJSON_IsString(ssid_json)) {
        miniot_wifi_config_t config = {0};
        strncpy(config.ssid, ssid_json->valuestring, MAX_SSID_LEN - 1);

        if (password_json && cJSON_IsString(password_json)) {
            strncpy(config.password, password_json->valuestring, MAX_PASSWORD_LEN - 1);
        }

        if (timeout_json && cJSON_IsNumber(timeout_json)) {
            config.ap_timeout = timeout_json->valueint;
        } else {
            config.ap_timeout = DEFAULT_AP_TIMEOUT;
        }

        config.is_configured = true;

        if (nvs_storage_save_wifi_config(&config) == ESP_OK) {
            success = true;
            ESP_LOGI(TAG, "WiFi configuration saved, scheduling reboot...");
        }
    }

    cJSON_Delete(root);

    cJSON *response = cJSON_CreateObject();
    cJSON_AddBoolToObject(response, "success", success);

    const char *json_str = cJSON_Print(response);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, json_str);

    free((void *)json_str);
    cJSON_Delete(response);

    if (success) {
        // Reboot après 2 secondes
        vTaskDelay(2000 / portTICK_PERIOD_MS);
        esp_restart();
    }

    return ESP_OK;
}

/* Handler pour POST /api/factory_reset */
static esp_err_t factory_reset_handler(httpd_req_t *req)
{
    ESP_LOGW(TAG, "Factory reset requested");

    esp_err_t ret = nvs_storage_factory_reset();

    cJSON *response = cJSON_CreateObject();
    cJSON_AddBoolToObject(response, "success", ret == ESP_OK);

    const char *json_str = cJSON_Print(response);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, json_str);

    free((void *)json_str);
    cJSON_Delete(response);

    if (ret == ESP_OK) {
        // Reboot après 2 secondes
        vTaskDelay(2000 / portTICK_PERIOD_MS);
        esp_restart();
    }

    return ESP_OK;
}

/* Handler pour POST /api/reboot */
static esp_err_t reboot_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "Reboot requested");

    cJSON *response = cJSON_CreateObject();
    cJSON_AddBoolToObject(response, "success", true);

    const char *json_str = cJSON_Print(response);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, json_str);

    free((void *)json_str);
    cJSON_Delete(response);

    // Reboot après 1 seconde
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    esp_restart();

    return ESP_OK;
}

/* Fonction de tâche pour l'OTA (ne peut pas être une lambda en C) */
static void ota_task_function(void *param)
{
    char *url = (char *)param;
    ESP_LOGI(TAG, "OTA task started for URL: %s", url);
    
    esp_err_t ret = ota_manager_start_update(url);
    
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "OTA update failed: %s", esp_err_to_name(ret));
    }
    
    free(url);
    vTaskDelete(NULL);
}

/* Handler pour POST /api/ota_update */
static esp_err_t ota_update_handler(httpd_req_t *req)
{
    char content[256];
    int ret = httpd_req_recv(req, content, sizeof(content) - 1);
    if (ret <= 0) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    content[ret] = '\0';

    cJSON *root = cJSON_Parse(content);
    if (!root) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    cJSON *url_json = cJSON_GetObjectItem(root, "url");

    if (url_json && cJSON_IsString(url_json)) {
        ESP_LOGI(TAG, "OTA update requested from: %s", url_json->valuestring);

        // IMPORTANT: Copier l'URL AVANT de supprimer le JSON
        char *url_copy = strdup(url_json->valuestring);

        // Maintenant on peut supprimer le JSON reçu
        cJSON_Delete(root);

        // Répondre immédiatement avant de commencer l'OTA
        cJSON *response = cJSON_CreateObject();
        cJSON_AddBoolToObject(response, "success", true);
        cJSON_AddStringToObject(response, "message", "OTA update started");

        const char *json_str = cJSON_Print(response);
        httpd_resp_set_type(req, "application/json");
        httpd_resp_sendstr(req, json_str);

        free((void *)json_str);
        cJSON_Delete(response);

        // Lancer l'OTA dans une tâche séparée
        xTaskCreate(ota_task_function, "ota_task", 8192, url_copy, 5, NULL);

        return ESP_OK;
    }

    cJSON_Delete(root);
    httpd_resp_send_500(req);
    return ESP_FAIL;
}

/* Handler pour GET /api/ota_version */
static esp_err_t ota_version_handler(httpd_req_t *req)
{
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "version", ota_manager_get_version());
    
    const esp_partition_t *running = esp_ota_get_running_partition();
    cJSON_AddStringToObject(root, "partition", running->label);
    
    const char *json_str = cJSON_Print(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, json_str);
    
    free((void *)json_str);
    cJSON_Delete(root);
    return ESP_OK;
}

/* Définition des URIs */
static const httpd_uri_t uri_index = {
    .uri       = "/",
    .method    = HTTP_GET,
    .handler   = index_handler,
    .user_ctx  = NULL
};

static const httpd_uri_t uri_status = {
    .uri       = "/api/status",
    .method    = HTTP_GET,
    .handler   = status_handler,
    .user_ctx  = NULL
};

static const httpd_uri_t uri_scan = {
    .uri       = "/api/scan",
    .method    = HTTP_GET,
    .handler   = scan_handler,
    .user_ctx  = NULL
};

static const httpd_uri_t uri_configure = {
    .uri       = "/api/configure",
    .method    = HTTP_POST,
    .handler   = configure_handler,
    .user_ctx  = NULL
};

static const httpd_uri_t uri_factory_reset = {
    .uri       = "/api/factory_reset",
    .method    = HTTP_POST,
    .handler   = factory_reset_handler,
    .user_ctx  = NULL
};

static const httpd_uri_t uri_reboot = {
    .uri       = "/api/reboot",
    .method    = HTTP_POST,
    .handler   = reboot_handler,
    .user_ctx  = NULL
};

/* URIs pour la détection de portail captif (Android, iOS, etc.) */
static const httpd_uri_t uri_generate_204 = {
    .uri       = "/generate_204",
    .method    = HTTP_GET,
    .handler   = captive_portal_handler,
    .user_ctx  = NULL
};

static const httpd_uri_t uri_gen_204 = {
    .uri       = "/gen_204",
    .method    = HTTP_GET,
    .handler   = captive_portal_handler,
    .user_ctx  = NULL
};

static const httpd_uri_t uri_hotspot_detect = {
    .uri       = "/hotspot-detect.html",
    .method    = HTTP_GET,
    .handler   = captive_portal_handler,
    .user_ctx  = NULL
};

static const httpd_uri_t uri_success_txt = {
    .uri       = "/success.txt",
    .method    = HTTP_GET,
    .handler   = captive_portal_handler,
    .user_ctx  = NULL
};

static const httpd_uri_t uri_ota_update = {
    .uri       = "/api/ota_update",
    .method    = HTTP_POST,
    .handler   = ota_update_handler,
    .user_ctx  = NULL
};

static const httpd_uri_t uri_ota_version = {
    .uri       = "/api/ota_version",
    .method    = HTTP_GET,
    .handler   = ota_version_handler,
    .user_ctx  = NULL
};

esp_err_t web_server_start(void)
{
    if (s_server) {
        ESP_LOGW(TAG, "Web server already running");
        return ESP_OK;
    }

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.lru_purge_enable = true;
    config.stack_size = 8192;
    config.max_uri_handlers = 16;  // Augmenter le nombre de handlers
    config.max_resp_headers = 16;  // Augmenter les headers de réponse
    config.recv_wait_timeout = 10; // Timeout de réception

    ESP_LOGI(TAG, "Starting web server on port %d", config.server_port);

    if (httpd_start(&s_server, &config) == ESP_OK) {
        ESP_LOGI(TAG, "Registering URI handlers");
        httpd_register_uri_handler(s_server, &uri_index);
        httpd_register_uri_handler(s_server, &uri_status);
        httpd_register_uri_handler(s_server, &uri_scan);
        httpd_register_uri_handler(s_server, &uri_configure);
        httpd_register_uri_handler(s_server, &uri_factory_reset);
        httpd_register_uri_handler(s_server, &uri_reboot);
        httpd_register_uri_handler(s_server, &uri_ota_update);
        httpd_register_uri_handler(s_server, &uri_ota_version);

        // Enregistrer les URIs pour la détection de portail captif
        httpd_register_uri_handler(s_server, &uri_generate_204);
        httpd_register_uri_handler(s_server, &uri_gen_204);
        httpd_register_uri_handler(s_server, &uri_hotspot_detect);
        httpd_register_uri_handler(s_server, &uri_success_txt);

        ESP_LOGI(TAG, "Web server started successfully with captive portal support");
        return ESP_OK;
    }

    ESP_LOGE(TAG, "Failed to start web server");
    return ESP_FAIL;
}

esp_err_t web_server_stop(void)
{
    if (s_server) {
        ESP_LOGI(TAG, "Stopping web server");
        httpd_stop(s_server);
        s_server = NULL;
        return ESP_OK;
    }
    return ESP_OK;
}

httpd_handle_t web_server_get_handle(void)
{
    return s_server;
}
