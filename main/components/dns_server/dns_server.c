#include "dns_server.h"
#include "lwip/sockets.h"
#include "lwip/netdb.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

static const char *TAG = "DNS_SERVER";

#define DNS_SERVER_PORT 53
#define DNS_MAX_PACKET_SIZE 512
#define DNS_QUERY_FLAG 0x0100
#define DNS_RESPONSE_FLAG 0x8180
#define DNS_QR_QUERY 0
#define DNS_QR_RESPONSE 1

static int s_dns_socket = -1;
static TaskHandle_t s_dns_task_handle = NULL;
static bool s_running = false;

// Structure DNS Header (12 octets)
typedef struct __attribute__((packed)) {
    uint16_t id;           // Identification
    uint16_t flags;        // Flags
    uint16_t qdcount;      // Number of questions
    uint16_t ancount;      // Number of answers
    uint16_t nscount;      // Number of authority records
    uint16_t arcount;      // Number of additional records
} dns_header_t;

// IP de l'AP (192.168.4.1)
#define AP_IP_ADDR 0x0104A8C0  // 192.168.4.1 en little-endian

static void dns_server_task(void *pvParameters)
{
    char rx_buffer[DNS_MAX_PACKET_SIZE];
    char tx_buffer[DNS_MAX_PACKET_SIZE];
    struct sockaddr_in source_addr;
    socklen_t socklen = sizeof(source_addr);

    ESP_LOGI(TAG, "DNS server task started");

    while (s_running) {
        // Recevoir une requête DNS
        int len = recvfrom(s_dns_socket, rx_buffer, sizeof(rx_buffer) - 1, 0,
                          (struct sockaddr *)&source_addr, &socklen);

        if (len < 0) {
            if (s_running) {
                ESP_LOGE(TAG, "recvfrom failed: errno %d", errno);
            }
            break;
        }

        if (len < sizeof(dns_header_t)) {
            ESP_LOGW(TAG, "Received packet too small");
            continue;
        }

        dns_header_t *header = (dns_header_t *)rx_buffer;

        // Vérifier que c'est une requête (pas une réponse)
        if ((ntohs(header->flags) & 0x8000) != 0) {
            continue;  // C'est une réponse, on ignore
        }

        ESP_LOGD(TAG, "DNS query received, ID: 0x%04X", ntohs(header->id));

        // Préparer la réponse
        memcpy(tx_buffer, rx_buffer, len);
        dns_header_t *response_header = (dns_header_t *)tx_buffer;

        // Modifier les flags pour une réponse
        response_header->flags = htons(DNS_RESPONSE_FLAG);
        response_header->ancount = htons(1);  // 1 réponse
        response_header->nscount = 0;
        response_header->arcount = 0;

        // Ajouter la réponse DNS (pointeur vers la question + type A + classe IN + TTL + IP)
        int response_len = len;

        // Pointeur vers le nom de domaine dans la question (compression DNS)
        tx_buffer[response_len++] = 0xC0;  // Pointeur
        tx_buffer[response_len++] = 0x0C;  // Offset vers la question (12 octets après le début)

        // Type A (IPv4)
        tx_buffer[response_len++] = 0x00;
        tx_buffer[response_len++] = 0x01;

        // Classe IN (Internet)
        tx_buffer[response_len++] = 0x00;
        tx_buffer[response_len++] = 0x01;

        // TTL (60 secondes)
        tx_buffer[response_len++] = 0x00;
        tx_buffer[response_len++] = 0x00;
        tx_buffer[response_len++] = 0x00;
        tx_buffer[response_len++] = 0x3C;

        // Longueur des données (4 octets pour IPv4)
        tx_buffer[response_len++] = 0x00;
        tx_buffer[response_len++] = 0x04;

        // Adresse IP (192.168.4.1)
        tx_buffer[response_len++] = 192;
        tx_buffer[response_len++] = 168;
        tx_buffer[response_len++] = 4;
        tx_buffer[response_len++] = 1;

        // Envoyer la réponse
        int err = sendto(s_dns_socket, tx_buffer, response_len, 0,
                        (struct sockaddr *)&source_addr, socklen);

        if (err < 0) {
            ESP_LOGE(TAG, "sendto failed: errno %d", errno);
        } else {
            ESP_LOGD(TAG, "DNS response sent: 192.168.4.1");
        }
    }

    ESP_LOGI(TAG, "DNS server task stopped");
    vTaskDelete(NULL);
}

esp_err_t dns_server_start(void)
{
    if (s_running) {
        ESP_LOGW(TAG, "DNS server already running");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Starting DNS server on port %d", DNS_SERVER_PORT);

    // Créer le socket UDP
    s_dns_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (s_dns_socket < 0) {
        ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
        return ESP_FAIL;
    }

    // Configurer l'adresse
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(DNS_SERVER_PORT);

    // Lier le socket
    int err = bind(s_dns_socket, (struct sockaddr *)&server_addr, sizeof(server_addr));
    if (err < 0) {
        ESP_LOGE(TAG, "Socket unable to bind: errno %d", errno);
        close(s_dns_socket);
        s_dns_socket = -1;
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Socket bound to port %d", DNS_SERVER_PORT);

    // Créer la tâche DNS
    s_running = true;
    BaseType_t ret = xTaskCreate(dns_server_task, "dns_server", 4096, NULL, 5, &s_dns_task_handle);
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create DNS server task");
        close(s_dns_socket);
        s_dns_socket = -1;
        s_running = false;
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "DNS server started successfully");
    return ESP_OK;
}

esp_err_t dns_server_stop(void)
{
    if (!s_running) {
        ESP_LOGW(TAG, "DNS server not running");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Stopping DNS server");

    s_running = false;

    // Fermer le socket pour débloquer recvfrom()
    if (s_dns_socket >= 0) {
        close(s_dns_socket);
        s_dns_socket = -1;
    }

    // Attendre que la tâche se termine
    if (s_dns_task_handle) {
        // La tâche va se terminer toute seule
        vTaskDelay(pdMS_TO_TICKS(100));
        s_dns_task_handle = NULL;
    }

    ESP_LOGI(TAG, "DNS server stopped");
    return ESP_OK;
}
