/**
 * @file sacn_receiver.c
 * @brief sACN (E1.31) Protocol Receiver Implementation
 * 
 * This component receives sACN packets over UDP multicast and processes them.
 * It handles E1.31 data packets with priority, sequence validation, and preview detection.
 * 
 * Thread Safety:
 * - All public APIs are thread-safe using mutexes
 * - Receiver runs on dedicated task (Core 0)
 * - Callbacks executed from receiver task context
 * 
 * Memory Usage:
 * - ~3KB for context and buffers
 * - Task stack: 4KB
 * - Total: ~7KB
 */

#include "sacn_receiver.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_netif.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "lwip/sockets.h"
#include "lwip/netdb.h"
#include "lwip/igmp.h"
#include <string.h>
#include <arpa/inet.h>

static const char *TAG = "sacn_receiver";

// Task configuration
#define SACN_TASK_STACK_SIZE 4096
#define SACN_TASK_PRIORITY   5
#define SACN_TASK_CORE       0

// Buffer sizes
#define SACN_MAX_PACKET_SIZE 638  // Full sACN packet size

// Timeout
#define SACN_RECEIVE_TIMEOUT_MS 1000

/**
 * @brief Universe subscription entry
 */
typedef struct {
    uint16_t universe;
    bool subscribed;
    uint8_t last_sequence;
    struct ip_mreq mreq;
} universe_subscription_t;

/**
 * @brief Module state
 */
static struct {
    bool initialized;
    bool running;
    
    int socket_fd;
    TaskHandle_t task_handle;
    
    sacn_dmx_callback_t dmx_callback;
    void *dmx_callback_user_data;
    
    universe_subscription_t subscriptions[SACN_MAX_UNIVERSES];
    uint8_t subscription_count;
    
    sacn_stats_t stats;
    
    SemaphoreHandle_t mutex;
} sacn_state = {
    .initialized = false,
    .running = false,
    .socket_fd = -1,
    .subscription_count = 0,
};

// Forward declarations
static void sacn_receive_task(void *arg);
static esp_err_t process_sacn_packet(const uint8_t *buffer, size_t length,
                                     const struct sockaddr_in *src_addr);
static esp_err_t process_sacn_data(const sacn_packet_t *packet,
                                   const struct sockaddr_in *src_addr);
static void calculate_multicast_addr(uint16_t universe, struct in_addr *addr);

/**
 * @brief Calculate multicast address for universe
 * 
 * Universe 1 = 239.255.0.1
 * Universe 256 = 239.255.1.0
 * Formula: 239.255.(universe / 256).(universe % 256)
 */
static void calculate_multicast_addr(uint16_t universe, struct in_addr *addr)
{
    uint8_t octet3 = (universe >> 8) & 0xFF;
    uint8_t octet4 = universe & 0xFF;
    
    // 239.255.x.x format
    addr->s_addr = htonl(0xEFFF0000 | (octet3 << 8) | octet4);
}

/**
 * @brief Validate sACN packet header
 */
static bool validate_sacn_header(const uint8_t *buffer, size_t length)
{
    if (length < sizeof(sacn_packet_t)) {
        return false;
    }
    
    const sacn_packet_t *packet = (const sacn_packet_t *)buffer;
    
    // Check preamble
    if (ntohs(packet->root.preamble_size) != 0x0010) {
        return false;
    }
    
    // Check ACN packet identifier
    if (memcmp(packet->root.acn_pid, SACN_PACKET_IDENTIFIER, 12) != 0) {
        return false;
    }
    
    // Check root vector
    if (ntohl(packet->root.vector) != SACN_ROOT_VECTOR) {
        return false;
    }
    
    // Check framing vector
    if (ntohl(packet->framing.vector) != SACN_FRAME_VECTOR) {
        return false;
    }
    
    // Check DMP vector
    if (packet->dmp.vector != SACN_DMP_VECTOR) {
        return false;
    }
    
    // Check start code (DMX)
    if (packet->dmp.start_code != 0x00) {
        return false;
    }
    
    return true;
}

// ============================================================================
// Public API Implementation
// ============================================================================

esp_err_t sacn_receiver_init(void)
{
    if (sacn_state.initialized) {
        ESP_LOGW(TAG, "Already initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    ESP_LOGI(TAG, "Initializing sACN receiver...");
    
    // Create mutex
    sacn_state.mutex = xSemaphoreCreateMutex();
    if (!sacn_state.mutex) {
        ESP_LOGE(TAG, "Failed to create mutex");
        return ESP_ERR_NO_MEM;
    }
    
    // Initialize statistics
    memset(&sacn_state.stats, 0, sizeof(sacn_stats_t));
    memset(sacn_state.subscriptions, 0, sizeof(sacn_state.subscriptions));
    sacn_state.subscription_count = 0;
    
    sacn_state.initialized = true;
    ESP_LOGI(TAG, "sACN receiver initialized successfully");
    
    return ESP_OK;
}

esp_err_t sacn_receiver_deinit(void)
{
    if (!sacn_state.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    ESP_LOGI(TAG, "Deinitializing sACN receiver...");
    
    // Stop if running
    if (sacn_state.running) {
        sacn_receiver_stop();
    }
    
    // Delete mutex
    if (sacn_state.mutex) {
        vSemaphoreDelete(sacn_state.mutex);
        sacn_state.mutex = NULL;
    }
    
    sacn_state.initialized = false;
    ESP_LOGI(TAG, "sACN receiver deinitialized");
    
    return ESP_OK;
}

esp_err_t sacn_receiver_start(void)
{
    if (!sacn_state.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (sacn_state.running) {
        ESP_LOGW(TAG, "Already running");
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "Starting sACN receiver on port %d...", SACN_PORT);
    
    xSemaphoreTake(sacn_state.mutex, portMAX_DELAY);
    
    // Create UDP socket
    sacn_state.socket_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sacn_state.socket_fd < 0) {
        ESP_LOGE(TAG, "Failed to create socket: %d", errno);
        xSemaphoreGive(sacn_state.mutex);
        return ESP_FAIL;
    }
    
    // Set socket options
    int opt = 1;
    setsockopt(sacn_state.socket_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    // Set receive timeout
    struct timeval timeout = {
        .tv_sec = SACN_RECEIVE_TIMEOUT_MS / 1000,
        .tv_usec = (SACN_RECEIVE_TIMEOUT_MS % 1000) * 1000
    };
    setsockopt(sacn_state.socket_fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    
    // Bind to sACN port
    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port = htons(SACN_PORT),
        .sin_addr.s_addr = INADDR_ANY
    };
    
    if (bind(sacn_state.socket_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        ESP_LOGE(TAG, "Failed to bind socket: %d", errno);
        close(sacn_state.socket_fd);
        sacn_state.socket_fd = -1;
        xSemaphoreGive(sacn_state.mutex);
        return ESP_FAIL;
    }
    
    // Create receiver task
    BaseType_t ret = xTaskCreatePinnedToCore(
        sacn_receive_task,
        "sacn_rx",
        SACN_TASK_STACK_SIZE,
        NULL,
        SACN_TASK_PRIORITY,
        &sacn_state.task_handle,
        SACN_TASK_CORE
    );
    
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create receiver task");
        close(sacn_state.socket_fd);
        sacn_state.socket_fd = -1;
        xSemaphoreGive(sacn_state.mutex);
        return ESP_FAIL;
    }
    
    sacn_state.running = true;
    
    xSemaphoreGive(sacn_state.mutex);
    
    ESP_LOGI(TAG, "sACN receiver started successfully");
    
    return ESP_OK;
}

esp_err_t sacn_receiver_stop(void)
{
    if (!sacn_state.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (!sacn_state.running) {
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "Stopping sACN receiver...");
    
    xSemaphoreTake(sacn_state.mutex, portMAX_DELAY);
    
    // Unsubscribe from all multicast groups
    for (int i = 0; i < sacn_state.subscription_count; i++) {
        if (sacn_state.subscriptions[i].subscribed && sacn_state.socket_fd >= 0) {
            setsockopt(sacn_state.socket_fd, IPPROTO_IP, IP_DROP_MEMBERSHIP,
                      &sacn_state.subscriptions[i].mreq, sizeof(struct ip_mreq));
        }
    }
    
    sacn_state.running = false;
    
    // Close socket (this will unblock the receive task)
    if (sacn_state.socket_fd >= 0) {
        close(sacn_state.socket_fd);
        sacn_state.socket_fd = -1;
    }
    
    xSemaphoreGive(sacn_state.mutex);
    
    // Wait for task to terminate
    if (sacn_state.task_handle) {
        vTaskDelay(pdMS_TO_TICKS(100));
        sacn_state.task_handle = NULL;
    }
    
    ESP_LOGI(TAG, "sACN receiver stopped");
    
    return ESP_OK;
}

esp_err_t sacn_receiver_subscribe_universe(uint16_t universe)
{
    if (!sacn_state.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (!sacn_state.running) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (universe < 1 || universe > 63999) {
        return ESP_ERR_INVALID_ARG;
    }
    
    xSemaphoreTake(sacn_state.mutex, portMAX_DELAY);
    
    // Check if already subscribed
    for (int i = 0; i < sacn_state.subscription_count; i++) {
        if (sacn_state.subscriptions[i].universe == universe &&
            sacn_state.subscriptions[i].subscribed) {
            ESP_LOGW(TAG, "Already subscribed to universe %d", universe);
            xSemaphoreGive(sacn_state.mutex);
            return ESP_OK;
        }
    }
    
    // Check if we have space for more subscriptions
    if (sacn_state.subscription_count >= SACN_MAX_UNIVERSES) {
        ESP_LOGE(TAG, "Maximum universes reached");
        xSemaphoreGive(sacn_state.mutex);
        return ESP_ERR_NO_MEM;
    }
    
    // Calculate multicast address
    struct in_addr multicast_addr;
    calculate_multicast_addr(universe, &multicast_addr);
    
    // Prepare multicast request
    struct ip_mreq mreq = {
        .imr_multiaddr = multicast_addr,
        .imr_interface.s_addr = INADDR_ANY
    };
    
    // Join multicast group
    if (setsockopt(sacn_state.socket_fd, IPPROTO_IP, IP_ADD_MEMBERSHIP,
                  &mreq, sizeof(mreq)) < 0) {
        ESP_LOGE(TAG, "Failed to join multicast group for universe %d: %d",
                 universe, errno);
        xSemaphoreGive(sacn_state.mutex);
        return ESP_FAIL;
    }
    
    // Add to subscriptions
    universe_subscription_t *sub = &sacn_state.subscriptions[sacn_state.subscription_count];
    sub->universe = universe;
    sub->subscribed = true;
    sub->last_sequence = 0;
    sub->mreq = mreq;
    
    sacn_state.subscription_count++;
    
    ESP_LOGI(TAG, "Subscribed to universe %d (multicast %s)",
             universe, inet_ntoa(multicast_addr));
    
    xSemaphoreGive(sacn_state.mutex);
    
    return ESP_OK;
}

esp_err_t sacn_receiver_unsubscribe_universe(uint16_t universe)
{
    if (!sacn_state.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (!sacn_state.running) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (universe < 1 || universe > 63999) {
        return ESP_ERR_INVALID_ARG;
    }
    
    xSemaphoreTake(sacn_state.mutex, portMAX_DELAY);
    
    // Find subscription
    int found_index = -1;
    for (int i = 0; i < sacn_state.subscription_count; i++) {
        if (sacn_state.subscriptions[i].universe == universe &&
            sacn_state.subscriptions[i].subscribed) {
            found_index = i;
            break;
        }
    }
    
    if (found_index < 0) {
        ESP_LOGW(TAG, "Not subscribed to universe %d", universe);
        xSemaphoreGive(sacn_state.mutex);
        return ESP_ERR_NOT_FOUND;
    }
    
    // Leave multicast group
    universe_subscription_t *sub = &sacn_state.subscriptions[found_index];
    if (setsockopt(sacn_state.socket_fd, IPPROTO_IP, IP_DROP_MEMBERSHIP,
                  &sub->mreq, sizeof(struct ip_mreq)) < 0) {
        ESP_LOGW(TAG, "Failed to leave multicast group: %d", errno);
    }
    
    // Remove from array (shift remaining items)
    for (int i = found_index; i < sacn_state.subscription_count - 1; i++) {
        sacn_state.subscriptions[i] = sacn_state.subscriptions[i + 1];
    }
    
    sacn_state.subscription_count--;
    
    ESP_LOGI(TAG, "Unsubscribed from universe %d", universe);
    
    xSemaphoreGive(sacn_state.mutex);
    
    return ESP_OK;
}

esp_err_t sacn_receiver_set_callback(sacn_dmx_callback_t callback, void *user_data)
{
    if (!sacn_state.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (!callback) {
        return ESP_ERR_INVALID_ARG;
    }
    
    xSemaphoreTake(sacn_state.mutex, portMAX_DELAY);
    sacn_state.dmx_callback = callback;
    sacn_state.dmx_callback_user_data = user_data;
    xSemaphoreGive(sacn_state.mutex);
    
    ESP_LOGI(TAG, "DMX callback registered");
    
    return ESP_OK;
}

esp_err_t sacn_receiver_get_stats(sacn_stats_t *stats)
{
    if (!sacn_state.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (!stats) {
        return ESP_ERR_INVALID_ARG;
    }
    
    xSemaphoreTake(sacn_state.mutex, portMAX_DELAY);
    memcpy(stats, &sacn_state.stats, sizeof(sacn_stats_t));
    xSemaphoreGive(sacn_state.mutex);
    
    return ESP_OK;
}

bool sacn_receiver_is_running(void)
{
    return sacn_state.running;
}

uint8_t sacn_receiver_get_subscription_count(void)
{
    return sacn_state.subscription_count;
}

// ============================================================================
// Internal Functions
// ============================================================================

/**
 * @brief sACN receiver task
 */
static void sacn_receive_task(void *arg)
{
    uint8_t buffer[SACN_MAX_PACKET_SIZE];
    struct sockaddr_in src_addr;
    socklen_t src_addr_len = sizeof(src_addr);
    
    ESP_LOGI(TAG, "sACN receiver task started");
    
    while (sacn_state.running) {
        // Receive packet
        int len = recvfrom(sacn_state.socket_fd, buffer, sizeof(buffer), 0,
                          (struct sockaddr *)&src_addr, &src_addr_len);
        
        if (len < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // Timeout, continue
                continue;
            }
            
            if (!sacn_state.running) {
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
        sacn_state.stats.packets_received++;
        
        // Process packet
        esp_err_t ret = process_sacn_packet(buffer, len, &src_addr);
        if (ret != ESP_OK) {
            sacn_state.stats.invalid_packets++;
        }
    }
    
    ESP_LOGI(TAG, "sACN receiver task stopped");
    vTaskDelete(NULL);
}

/**
 * @brief Process received sACN packet
 */
static esp_err_t process_sacn_packet(const uint8_t *buffer, size_t length,
                                     const struct sockaddr_in *src_addr)
{
    // Validate header
    if (!validate_sacn_header(buffer, length)) {
        return ESP_FAIL;
    }
    
    const sacn_packet_t *packet = (const sacn_packet_t *)buffer;
    
    // Process data packet
    sacn_state.stats.data_packets++;
    return process_sacn_data(packet, src_addr);
}

/**
 * @brief Process sACN data packet
 */
static esp_err_t process_sacn_data(const sacn_packet_t *packet,
                                   const struct sockaddr_in *src_addr)
{
    // Extract universe (big endian / network byte order)
    uint16_t universe = ntohs(packet->framing.universe);
    
    // Check if preview data
    bool is_preview = (packet->framing.options & SACN_OPT_PREVIEW) != 0;
    if (is_preview) {
        sacn_state.stats.preview_packets++;
    }
    
    // Extract priority
    uint8_t priority = packet->framing.priority;
    
    // Extract sequence number
    uint8_t sequence = packet->framing.sequence_number;
    
    // Check sequence number for this universe (optional - for statistics)
    for (int i = 0; i < sacn_state.subscription_count; i++) {
        if (sacn_state.subscriptions[i].universe == universe) {
            uint8_t expected = sacn_state.subscriptions[i].last_sequence + 1;
            if (sequence != expected && sacn_state.subscriptions[i].last_sequence != 0) {
                sacn_state.stats.sequence_errors++;
            }
            sacn_state.subscriptions[i].last_sequence = sequence;
            break;
        }
    }
    
    // Extract source name (null terminated)
    char source_name[65];
    strncpy(source_name, packet->framing.source_name, 64);
    source_name[64] = '\0';
    
    // Extract source IP address
    uint32_t source_ip = src_addr->sin_addr.s_addr;
    
    // Call callback if registered
    if (sacn_state.dmx_callback) {
        sacn_state.dmx_callback(universe, packet->dmp.data, priority, sequence,
                               is_preview, source_name, source_ip,
                               sacn_state.dmx_callback_user_data);
    }
    
    return ESP_OK;
}
