/**
 * @file artnet_receiver.c
 * @brief Art-Net v4 Protocol Receiver Implementation
 * 
 * This component receives Art-Net packets over UDP and processes them.
 * It handles ArtDmx (DMX data), ArtPoll (discovery), and sends ArtPollReply.
 * 
 * Thread Safety:
 * - All public APIs are thread-safe using mutexes
 * - Receiver runs on dedicated task (Core 0)
 * - Callbacks executed from receiver task context
 * 
 * Memory Usage:
 * - ~2KB for context and buffers
 * - Task stack: 4KB
 * - Total: ~6KB
 */

#include "artnet_receiver.h"
#include "config_manager.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_netif.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "lwip/sockets.h"
#include "lwip/netdb.h"
#include <string.h>
#include <arpa/inet.h>

static const char *TAG = "artnet_receiver";

// Task configuration
#define ARTNET_TASK_STACK_SIZE 4096
#define ARTNET_TASK_PRIORITY   5
#define ARTNET_TASK_CORE       0

// Buffer sizes
#define ARTNET_MAX_PACKET_SIZE 1024

// Timeout
#define ARTNET_RECEIVE_TIMEOUT_MS 1000

/**
 * @brief Module state
 */
static struct {
    bool initialized;
    bool running;
    bool poll_reply_enabled;
    
    int socket_fd;
    TaskHandle_t task_handle;
    
    artnet_dmx_callback_t dmx_callback;
    void *dmx_callback_user_data;
    
    artnet_stats_t stats;
    uint8_t last_sequence[ARTNET_MAX_UNIVERSES];  // Track sequence per universe
    
    SemaphoreHandle_t mutex;
} artnet_state = {
    .initialized = false,
    .running = false,
    .poll_reply_enabled = true,
    .socket_fd = -1,
};

// Forward declarations
static void artnet_receive_task(void *arg);
static esp_err_t process_artnet_packet(const uint8_t *buffer, size_t length, 
                                       const struct sockaddr_in *src_addr);
static esp_err_t process_artdmx(const artnet_dmx_packet_t *packet);
static esp_err_t process_artpoll(const artnet_poll_packet_t *packet, 
                                 const struct sockaddr_in *src_addr);
static esp_err_t send_artpoll_reply(const struct sockaddr_in *dest_addr);

/**
 * @brief Validate Art-Net header
 */
static bool validate_artnet_header(const uint8_t *buffer, size_t length)
{
    if (length < 12) {
        return false;
    }
    
    // Check "Art-Net\0" signature
    if (memcmp(buffer, ARTNET_HEADER, 8) != 0) {
        return false;
    }
    
    return true;
}

/**
 * @brief Convert network byte order to host byte order for uint16_t
 */
static inline uint16_t artnet_ntohs(uint16_t netshort)
{
    return ntohs(netshort);
}

// ============================================================================
// Public API Implementation
// ============================================================================

esp_err_t artnet_receiver_init(void)
{
    if (artnet_state.initialized) {
        ESP_LOGW(TAG, "Already initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    ESP_LOGI(TAG, "Initializing Art-Net receiver...");
    
    // Create mutex
    artnet_state.mutex = xSemaphoreCreateMutex();
    if (!artnet_state.mutex) {
        ESP_LOGE(TAG, "Failed to create mutex");
        return ESP_ERR_NO_MEM;
    }
    
    // Initialize statistics
    memset(&artnet_state.stats, 0, sizeof(artnet_stats_t));
    memset(artnet_state.last_sequence, 0, sizeof(artnet_state.last_sequence));
    
    artnet_state.initialized = true;
    ESP_LOGI(TAG, "Art-Net receiver initialized successfully");
    
    return ESP_OK;
}

esp_err_t artnet_receiver_deinit(void)
{
    if (!artnet_state.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    ESP_LOGI(TAG, "Deinitializing Art-Net receiver...");
    
    // Stop if running
    if (artnet_state.running) {
        artnet_receiver_stop();
    }
    
    // Delete mutex
    if (artnet_state.mutex) {
        vSemaphoreDelete(artnet_state.mutex);
        artnet_state.mutex = NULL;
    }
    
    artnet_state.initialized = false;
    ESP_LOGI(TAG, "Art-Net receiver deinitialized");
    
    return ESP_OK;
}

esp_err_t artnet_receiver_start(void)
{
    if (!artnet_state.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (artnet_state.running) {
        ESP_LOGW(TAG, "Already running");
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "Starting Art-Net receiver on port %d...", ARTNET_PORT);
    
    xSemaphoreTake(artnet_state.mutex, portMAX_DELAY);
    
    // Create UDP socket
    artnet_state.socket_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (artnet_state.socket_fd < 0) {
        ESP_LOGE(TAG, "Failed to create socket: %d", errno);
        xSemaphoreGive(artnet_state.mutex);
        return ESP_FAIL;
    }
    
    // Set socket options
    int opt = 1;
    setsockopt(artnet_state.socket_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    // Set receive timeout
    struct timeval timeout = {
        .tv_sec = ARTNET_RECEIVE_TIMEOUT_MS / 1000,
        .tv_usec = (ARTNET_RECEIVE_TIMEOUT_MS % 1000) * 1000
    };
    setsockopt(artnet_state.socket_fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    
    // Bind to Art-Net port
    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port = htons(ARTNET_PORT),
        .sin_addr.s_addr = INADDR_ANY
    };
    
    if (bind(artnet_state.socket_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        ESP_LOGE(TAG, "Failed to bind socket: %d", errno);
        close(artnet_state.socket_fd);
        artnet_state.socket_fd = -1;
        xSemaphoreGive(artnet_state.mutex);
        return ESP_FAIL;
    }
    
    // Create receiver task
    BaseType_t ret = xTaskCreatePinnedToCore(
        artnet_receive_task,
        "artnet_rx",
        ARTNET_TASK_STACK_SIZE,
        NULL,
        ARTNET_TASK_PRIORITY,
        &artnet_state.task_handle,
        ARTNET_TASK_CORE
    );
    
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create receiver task");
        close(artnet_state.socket_fd);
        artnet_state.socket_fd = -1;
        xSemaphoreGive(artnet_state.mutex);
        return ESP_FAIL;
    }
    
    artnet_state.running = true;
    
    xSemaphoreGive(artnet_state.mutex);
    
    ESP_LOGI(TAG, "Art-Net receiver started successfully");
    
    return ESP_OK;
}

esp_err_t artnet_receiver_stop(void)
{
    if (!artnet_state.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (!artnet_state.running) {
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "Stopping Art-Net receiver...");
    
    xSemaphoreTake(artnet_state.mutex, portMAX_DELAY);
    
    artnet_state.running = false;
    
    // Close socket (this will unblock the receive task)
    if (artnet_state.socket_fd >= 0) {
        close(artnet_state.socket_fd);
        artnet_state.socket_fd = -1;
    }
    
    xSemaphoreGive(artnet_state.mutex);
    
    // Wait for task to terminate
    if (artnet_state.task_handle) {
        // Task will delete itself
        vTaskDelay(pdMS_TO_TICKS(100));
        artnet_state.task_handle = NULL;
    }
    
    ESP_LOGI(TAG, "Art-Net receiver stopped");
    
    return ESP_OK;
}

esp_err_t artnet_receiver_set_callback(artnet_dmx_callback_t callback, void *user_data)
{
    if (!artnet_state.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (!callback) {
        return ESP_ERR_INVALID_ARG;
    }
    
    xSemaphoreTake(artnet_state.mutex, portMAX_DELAY);
    artnet_state.dmx_callback = callback;
    artnet_state.dmx_callback_user_data = user_data;
    xSemaphoreGive(artnet_state.mutex);
    
    ESP_LOGI(TAG, "DMX callback registered");
    
    return ESP_OK;
}

esp_err_t artnet_receiver_get_stats(artnet_stats_t *stats)
{
    if (!artnet_state.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (!stats) {
        return ESP_ERR_INVALID_ARG;
    }
    
    xSemaphoreTake(artnet_state.mutex, portMAX_DELAY);
    memcpy(stats, &artnet_state.stats, sizeof(artnet_stats_t));
    xSemaphoreGive(artnet_state.mutex);
    
    return ESP_OK;
}

esp_err_t artnet_receiver_enable_poll_reply(bool enable)
{
    if (!artnet_state.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    xSemaphoreTake(artnet_state.mutex, portMAX_DELAY);
    artnet_state.poll_reply_enabled = enable;
    xSemaphoreGive(artnet_state.mutex);
    
    ESP_LOGI(TAG, "ArtPollReply %s", enable ? "enabled" : "disabled");
    
    return ESP_OK;
}

bool artnet_receiver_is_running(void)
{
    return artnet_state.running;
}

// ============================================================================
// Internal Functions
// ============================================================================

/**
 * @brief Art-Net receiver task
 */
static void artnet_receive_task(void *arg)
{
    uint8_t buffer[ARTNET_MAX_PACKET_SIZE];
    struct sockaddr_in src_addr;
    socklen_t src_addr_len = sizeof(src_addr);
    
    ESP_LOGI(TAG, "Art-Net receiver task started");
    
    while (artnet_state.running) {
        // Receive packet
        int len = recvfrom(artnet_state.socket_fd, buffer, sizeof(buffer), 0,
                          (struct sockaddr *)&src_addr, &src_addr_len);
        
        if (len < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // Timeout, continue
                continue;
            }
            
            if (!artnet_state.running) {
                // Socket closed, exit
                break;
            }
            
            ESP_LOGW(TAG, "Receive error: %d", errno);
            continue;
        }
        
        if (len == 0) {
            continue;
        }
        
        // Update statistics
        artnet_state.stats.packets_received++;
        
        // Process packet
        esp_err_t ret = process_artnet_packet(buffer, len, &src_addr);
        if (ret != ESP_OK) {
            artnet_state.stats.invalid_packets++;
        }
    }
    
    ESP_LOGI(TAG, "Art-Net receiver task stopped");
    vTaskDelete(NULL);
}

/**
 * @brief Process received Art-Net packet
 */
static esp_err_t process_artnet_packet(const uint8_t *buffer, size_t length,
                                       const struct sockaddr_in *src_addr)
{
    // Validate header
    if (!validate_artnet_header(buffer, length)) {
        return ESP_FAIL;
    }
    
    // Get opcode (bytes 8-9, little endian)
    uint16_t opcode = buffer[8] | (buffer[9] << 8);
    
    switch (opcode) {
        case ARTNET_OP_DMX:
            if (length >= sizeof(artnet_dmx_packet_t)) {
                artnet_state.stats.dmx_packets++;
                return process_artdmx((const artnet_dmx_packet_t *)buffer);
            }
            break;
            
        case ARTNET_OP_POLL:
            if (length >= sizeof(artnet_poll_packet_t)) {
                artnet_state.stats.poll_packets++;
                return process_artpoll((const artnet_poll_packet_t *)buffer, src_addr);
            }
            break;
            
        default:
            // Unknown or unsupported opcode
            break;
    }
    
    return ESP_OK;
}

/**
 * @brief Process ArtDmx packet
 */
static esp_err_t process_artdmx(const artnet_dmx_packet_t *packet)
{
    // Extract universe (little endian)
    uint16_t universe = packet->universe;
    
    // Extract length (big endian / network byte order)
    uint16_t length = artnet_ntohs(packet->length);
    
    // Validate length
    if (length < 2 || length > 512) {
        return ESP_FAIL;
    }
    
    // Check sequence number (optional - for statistics)
    uint8_t sequence = packet->sequence;
    if (universe < ARTNET_MAX_UNIVERSES) {
        uint8_t expected = artnet_state.last_sequence[universe] + 1;
        if (sequence != expected && artnet_state.last_sequence[universe] != 0) {
            artnet_state.stats.sequence_errors++;
        }
        artnet_state.last_sequence[universe] = sequence;
    }
    
    // Call callback if registered
    if (artnet_state.dmx_callback) {
        artnet_state.dmx_callback(universe, packet->data, length, sequence,
                                  artnet_state.dmx_callback_user_data);
    }
    
    return ESP_OK;
}

/**
 * @brief Process ArtPoll packet
 */
static esp_err_t process_artpoll(const artnet_poll_packet_t *packet,
                                 const struct sockaddr_in *src_addr)
{
    // Send ArtPollReply if enabled
    if (artnet_state.poll_reply_enabled) {
        return send_artpoll_reply(src_addr);
    }
    
    return ESP_OK;
}

/**
 * @brief Send ArtPollReply packet
 */
static esp_err_t send_artpoll_reply(const struct sockaddr_in *dest_addr)
{
    artnet_poll_reply_packet_t reply;
    memset(&reply, 0, sizeof(reply));
    
    // Fill header
    memcpy(reply.id, ARTNET_HEADER, 8);
    reply.opcode = ARTNET_OP_POLL_REPLY;
    
    // Get IP address
    esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (!netif) {
        netif = esp_netif_get_handle_from_ifkey("ETH_DEF");
    }
    
    if (netif) {
        esp_netif_ip_info_t ip_info;
        if (esp_netif_get_ip_info(netif, &ip_info) == ESP_OK) {
            reply.ip[0] = (ip_info.ip.addr >> 0) & 0xFF;
            reply.ip[1] = (ip_info.ip.addr >> 8) & 0xFF;
            reply.ip[2] = (ip_info.ip.addr >> 16) & 0xFF;
            reply.ip[3] = (ip_info.ip.addr >> 24) & 0xFF;
        }
    }
    
    // Port
    reply.port = htons(ARTNET_PORT);
    
    // Version info
    reply.version_info = htons(0x0001);  // Version 0.1
    
    // Get node names from config
    config_t *config = config_get();
    if (config) {
        strncpy(reply.short_name, config->node_info.short_name, sizeof(reply.short_name) - 1);
        strncpy(reply.long_name, config->node_info.long_name, sizeof(reply.long_name) - 1);
    }
    
    // Node report
    snprintf(reply.node_report, sizeof(reply.node_report), "#0001 [%04ld] OK",
             artnet_state.stats.dmx_packets);
    
    // Number of ports
    reply.num_ports = htons(2);  // 2 DMX ports
    
    // Port types (DMX output)
    reply.port_types[0] = 0x80;  // DMX512 output
    reply.port_types[1] = 0x80;  // DMX512 output
    
    // Output universe mapping
    if (config) {
        reply.swout[0] = config->port1.universe_primary & 0x0F;
        reply.swout[1] = config->port2.universe_primary & 0x0F;
    }
    
    // Status
    reply.status1 = 0xE0;  // Indicators normal, network configured
    reply.status2 = 0x08;  // Supports ArtNet 4
    
    // Style
    reply.style = 0x00;  // ST_NODE (DMX to/from Art-Net device)
    
    // MAC address
    uint8_t mac[6];
    if (netif && esp_netif_get_mac(netif, mac) == ESP_OK) {
        memcpy(reply.mac, mac, 6);
    }
    
    // Send reply
    int sent = sendto(artnet_state.socket_fd, &reply, sizeof(reply), 0,
                     (struct sockaddr *)dest_addr, sizeof(*dest_addr));
    
    if (sent < 0) {
        ESP_LOGW(TAG, "Failed to send ArtPollReply: %d", errno);
        return ESP_FAIL;
    }
    
    artnet_state.stats.poll_replies_sent++;
    ESP_LOGD(TAG, "Sent ArtPollReply to %s:%d",
             inet_ntoa(dest_addr->sin_addr), ntohs(dest_addr->sin_port));
    
    return ESP_OK;
}
