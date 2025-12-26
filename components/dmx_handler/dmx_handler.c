/**
 * @file dmx_handler.c
 * @brief DMX/RDM Handler Implementation
 * 
 * This component manages 2 independent DMX512/RDM ports using the esp-dmx library.
 * It provides a unified interface for DMX output, input, and RDM operations.
 * 
 * Thread Safety:
 * - All public APIs are thread-safe using mutexes
 * - DMX output runs on dedicated tasks (Core 1)
 * - Callbacks are executed from DMX task context
 * 
 * Memory Usage:
 * - ~2KB per port (context + buffers)
 * - ~1KB for RDM device list
 * - Task stacks: 4KB per port × 2 = 8KB
 * - Total: ~13KB
 */

#include "dmx_handler.h"
#include "esp_dmx.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include <string.h>

static const char *TAG = "dmx_handler";

// GPIO pin definitions
#define DMX_PORT1_TX_PIN    GPIO_NUM_17
#define DMX_PORT1_RX_PIN    GPIO_NUM_16
#define DMX_PORT1_DIR_PIN   GPIO_NUM_21

#define DMX_PORT2_TX_PIN    GPIO_NUM_19
#define DMX_PORT2_RX_PIN    GPIO_NUM_18
#define DMX_PORT2_DIR_PIN   GPIO_NUM_20

// UART assignments
#define DMX_PORT1_UART      DMX_NUM_1
#define DMX_PORT2_UART      DMX_NUM_2

// Task configuration
#define DMX_TASK_STACK_SIZE 4096
#define DMX_TASK_PRIORITY   10
#define DMX_TASK_CORE       1

// Timing constants
#define DMX_OUTPUT_RATE_MS  23  // ~44Hz (1000/44 ≈ 23ms)
#define DMX_RX_TIMEOUT_MS   1000
#define RDM_RESPONSE_TIMEOUT_MS 200

// DMX constants (fallback if not defined by esp-dmx)
#ifndef DMX_SC
#define DMX_SC 0x00  // DMX512 start code
#endif

#ifndef DMX_TIMEOUT_TICK
#define DMX_TIMEOUT_TICK portMAX_DELAY
#endif

#ifndef DMX_INTR_FLAGS_DEFAULT
#define DMX_INTR_FLAGS_DEFAULT 0
#endif

// Blackout value
#define DMX_BLACKOUT_VALUE 0

/**
 * @brief Port context structure
 */
typedef struct {
    uint8_t port_num;                   // Port number (1 or 2)
    dmx_port_t dmx_num;                 // esp-dmx port number
    gpio_num_t tx_pin;                  // TX GPIO
    gpio_num_t rx_pin;                  // RX GPIO
    gpio_num_t dir_pin;                 // Direction control GPIO
    
    // Configuration
    dmx_mode_t mode;                    // Current mode
    uint16_t universe;                  // Primary universe
    bool is_configured;                 // Configuration status
    bool is_active;                     // Active status
    
    // DMX data
    uint8_t dmx_buffer[DMX_CHANNEL_COUNT]; // DMX channel data
    SemaphoreHandle_t buffer_mutex;     // Buffer protection
    
    // Statistics
    dmx_port_stats_t stats;
    
    // Tasks
    TaskHandle_t output_task;           // Output task handle
    TaskHandle_t input_task;            // Input task handle
    
    // RDM
    rdm_device_t rdm_devices[DMX_MAX_DEVICES];
    uint8_t rdm_device_count;
    SemaphoreHandle_t rdm_mutex;        // RDM data protection
    bool rdm_discovery_running;
    
    // Callbacks
    dmx_rx_callback_t rx_callback;
    void *rx_callback_user_data;
    rdm_discovery_callback_t discovery_callback;
    void *discovery_callback_user_data;
    
} dmx_port_context_t;

/**
 * @brief Module state
 */
static struct {
    bool initialized;
    dmx_port_context_t ports[DMX_PORT_MAX];
    SemaphoreHandle_t state_mutex;
} dmx_state = {
    .initialized = false,
};

// Forward declarations
static void dmx_output_task(void *arg);
static void dmx_input_task(void *arg);
static esp_err_t port_install_driver(dmx_port_context_t *port_ctx);
static esp_err_t port_uninstall_driver(dmx_port_context_t *port_ctx);

/**
 * @brief Initialize port context
 */
static void init_port_context(dmx_port_context_t *port_ctx, uint8_t port_num)
{
    memset(port_ctx, 0, sizeof(dmx_port_context_t));
    
    port_ctx->port_num = port_num;
    
    if (port_num == 1) {
        port_ctx->dmx_num = DMX_PORT1_UART;
        port_ctx->tx_pin = DMX_PORT1_TX_PIN;
        port_ctx->rx_pin = DMX_PORT1_RX_PIN;
        port_ctx->dir_pin = DMX_PORT1_DIR_PIN;
    } else {
        port_ctx->dmx_num = DMX_PORT2_UART;
        port_ctx->tx_pin = DMX_PORT2_TX_PIN;
        port_ctx->rx_pin = DMX_PORT2_RX_PIN;
        port_ctx->dir_pin = DMX_PORT2_DIR_PIN;
    }
    
    port_ctx->buffer_mutex = xSemaphoreCreateMutex();
    port_ctx->rdm_mutex = xSemaphoreCreateMutex();
    port_ctx->mode = DMX_MODE_DISABLED;
}

/**
 * @brief Validate port number
 */
static bool is_valid_port(uint8_t port)
{
    return (port >= 1 && port <= DMX_PORT_MAX);
}

/**
 * @brief Get port context
 */
static dmx_port_context_t* get_port_context(uint8_t port)
{
    if (!is_valid_port(port)) {
        return NULL;
    }
    return &dmx_state.ports[port - 1];
}

// ============================================================================
// Public API Implementation
// ============================================================================

esp_err_t dmx_handler_init(void)
{
    if (dmx_state.initialized) {
        ESP_LOGW(TAG, "Already initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    ESP_LOGI(TAG, "Initializing DMX handler...");
    
    // Create state mutex
    dmx_state.state_mutex = xSemaphoreCreateMutex();
    if (!dmx_state.state_mutex) {
        ESP_LOGE(TAG, "Failed to create state mutex");
        return ESP_ERR_NO_MEM;
    }
    
    // Initialize port contexts
    for (int i = 0; i < DMX_PORT_MAX; i++) {
        init_port_context(&dmx_state.ports[i], i + 1);
    }
    
    dmx_state.initialized = true;
    ESP_LOGI(TAG, "DMX handler initialized successfully");
    
    return ESP_OK;
}

esp_err_t dmx_handler_deinit(void)
{
    if (!dmx_state.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    ESP_LOGI(TAG, "Deinitializing DMX handler...");
    
    // Stop all ports
    for (int i = 0; i < DMX_PORT_MAX; i++) {
        dmx_handler_stop_port(i + 1);
        
        // Delete mutexes
        if (dmx_state.ports[i].buffer_mutex) {
            vSemaphoreDelete(dmx_state.ports[i].buffer_mutex);
        }
        if (dmx_state.ports[i].rdm_mutex) {
            vSemaphoreDelete(dmx_state.ports[i].rdm_mutex);
        }
    }
    
    // Delete state mutex
    if (dmx_state.state_mutex) {
        vSemaphoreDelete(dmx_state.state_mutex);
    }
    
    dmx_state.initialized = false;
    ESP_LOGI(TAG, "DMX handler deinitialized");
    
    return ESP_OK;
}

esp_err_t dmx_handler_configure_port(uint8_t port, const port_config_t *config)
{
    if (!dmx_state.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    dmx_port_context_t *port_ctx = get_port_context(port);
    if (!port_ctx || !config) {
        return ESP_ERR_INVALID_ARG;
    }
    
    ESP_LOGI(TAG, "Configuring port %d: mode=%d, universe=%d", 
             port, config->mode, config->universe_primary);
    
    xSemaphoreTake(dmx_state.state_mutex, portMAX_DELAY);
    
    // Stop port if running
    if (port_ctx->is_active) {
        dmx_handler_stop_port(port);
    }
    
    // Update configuration
    port_ctx->mode = config->mode;
    port_ctx->universe = config->universe_primary;
    port_ctx->is_configured = true;
    
    // Clear DMX buffer
    xSemaphoreTake(port_ctx->buffer_mutex, portMAX_DELAY);
    memset(port_ctx->dmx_buffer, DMX_BLACKOUT_VALUE, DMX_CHANNEL_COUNT);
    xSemaphoreGive(port_ctx->buffer_mutex);
    
    xSemaphoreGive(dmx_state.state_mutex);
    
    ESP_LOGI(TAG, "Port %d configured successfully", port);
    
    return ESP_OK;
}

esp_err_t dmx_handler_start_port(uint8_t port)
{
    if (!dmx_state.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    dmx_port_context_t *port_ctx = get_port_context(port);
    if (!port_ctx) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (!port_ctx->is_configured) {
        ESP_LOGE(TAG, "Port %d not configured", port);
        return ESP_ERR_INVALID_STATE;
    }
    
    if (port_ctx->is_active) {
        ESP_LOGW(TAG, "Port %d already active", port);
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "Starting port %d in mode %d", port, port_ctx->mode);
    
    xSemaphoreTake(dmx_state.state_mutex, portMAX_DELAY);
    
    // Install driver based on mode
    esp_err_t ret = port_install_driver(port_ctx);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to install driver for port %d", port);
        xSemaphoreGive(dmx_state.state_mutex);
        return ret;
    }
    
    // Create tasks based on mode
    char task_name[16];
    
    if (port_ctx->mode == DMX_MODE_OUTPUT || port_ctx->mode == DMX_MODE_RDM_MASTER) {
        snprintf(task_name, sizeof(task_name), "dmx_out_%d", port);
        BaseType_t task_ret = xTaskCreatePinnedToCore(
            dmx_output_task,
            task_name,
            DMX_TASK_STACK_SIZE,
            port_ctx,
            DMX_TASK_PRIORITY,
            &port_ctx->output_task,
            DMX_TASK_CORE
        );
        
        if (task_ret != pdPASS) {
            ESP_LOGE(TAG, "Failed to create output task for port %d", port);
            port_uninstall_driver(port_ctx);
            xSemaphoreGive(dmx_state.state_mutex);
            return ESP_FAIL;
        }
    }
    
    if (port_ctx->mode == DMX_MODE_INPUT || port_ctx->mode == DMX_MODE_RDM_RESPONDER) {
        snprintf(task_name, sizeof(task_name), "dmx_in_%d", port);
        BaseType_t task_ret = xTaskCreatePinnedToCore(
            dmx_input_task,
            task_name,
            DMX_TASK_STACK_SIZE,
            port_ctx,
            DMX_TASK_PRIORITY,
            &port_ctx->input_task,
            DMX_TASK_CORE
        );
        
        if (task_ret != pdPASS) {
            ESP_LOGE(TAG, "Failed to create input task for port %d", port);
            if (port_ctx->output_task) {
                vTaskDelete(port_ctx->output_task);
                port_ctx->output_task = NULL;
            }
            port_uninstall_driver(port_ctx);
            xSemaphoreGive(dmx_state.state_mutex);
            return ESP_FAIL;
        }
    }
    
    port_ctx->is_active = true;
    
    xSemaphoreGive(dmx_state.state_mutex);
    
    ESP_LOGI(TAG, "Port %d started successfully", port);
    
    return ESP_OK;
}

esp_err_t dmx_handler_stop_port(uint8_t port)
{
    if (!dmx_state.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    dmx_port_context_t *port_ctx = get_port_context(port);
    if (!port_ctx) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (!port_ctx->is_active) {
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "Stopping port %d", port);
    
    xSemaphoreTake(dmx_state.state_mutex, portMAX_DELAY);
    
    port_ctx->is_active = false;
    
    // Delete tasks
    if (port_ctx->output_task) {
        vTaskDelete(port_ctx->output_task);
        port_ctx->output_task = NULL;
    }
    
    if (port_ctx->input_task) {
        vTaskDelete(port_ctx->input_task);
        port_ctx->input_task = NULL;
    }
    
    // Uninstall driver
    port_uninstall_driver(port_ctx);
    
    xSemaphoreGive(dmx_state.state_mutex);
    
    ESP_LOGI(TAG, "Port %d stopped", port);
    
    return ESP_OK;
}

esp_err_t dmx_handler_send_dmx(uint8_t port, const uint8_t *data)
{
    if (!dmx_state.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    dmx_port_context_t *port_ctx = get_port_context(port);
    if (!port_ctx || !data) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (port_ctx->mode != DMX_MODE_OUTPUT && port_ctx->mode != DMX_MODE_RDM_MASTER) {
        return ESP_ERR_INVALID_STATE;
    }
    
    // Copy data to buffer
    xSemaphoreTake(port_ctx->buffer_mutex, portMAX_DELAY);
    memcpy(port_ctx->dmx_buffer, data, DMX_CHANNEL_COUNT);
    xSemaphoreGive(port_ctx->buffer_mutex);
    
    return ESP_OK;
}

esp_err_t dmx_handler_read_dmx(uint8_t port, uint8_t *data, uint32_t timeout_ms)
{
    if (!dmx_state.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    dmx_port_context_t *port_ctx = get_port_context(port);
    if (!port_ctx || !data) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (port_ctx->mode != DMX_MODE_INPUT) {
        return ESP_ERR_INVALID_STATE;
    }
    
    // Check if we have recent data
    uint32_t current_time = esp_timer_get_time() / 1000;
    if (timeout_ms > 0 && (current_time - port_ctx->stats.last_frame_time_ms) > timeout_ms) {
        return ESP_ERR_TIMEOUT;
    }
    
    // Copy data from buffer
    xSemaphoreTake(port_ctx->buffer_mutex, portMAX_DELAY);
    memcpy(data, port_ctx->dmx_buffer, DMX_CHANNEL_COUNT);
    xSemaphoreGive(port_ctx->buffer_mutex);
    
    return ESP_OK;
}

esp_err_t dmx_handler_set_channel(uint8_t port, uint16_t channel, uint8_t value)
{
    if (!dmx_state.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (channel < 1 || channel > DMX_CHANNEL_COUNT) {
        return ESP_ERR_INVALID_ARG;
    }
    
    dmx_port_context_t *port_ctx = get_port_context(port);
    if (!port_ctx) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (port_ctx->mode != DMX_MODE_OUTPUT && port_ctx->mode != DMX_MODE_RDM_MASTER) {
        return ESP_ERR_INVALID_STATE;
    }
    
    xSemaphoreTake(port_ctx->buffer_mutex, portMAX_DELAY);
    port_ctx->dmx_buffer[channel - 1] = value;
    xSemaphoreGive(port_ctx->buffer_mutex);
    
    return ESP_OK;
}

esp_err_t dmx_handler_set_channels(uint8_t port, uint16_t start_channel, 
                                   const uint8_t *data, uint16_t length)
{
    if (!dmx_state.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (!data || length == 0 || start_channel < 1 || start_channel > DMX_CHANNEL_COUNT ||
        (start_channel + length - 1) > DMX_CHANNEL_COUNT) {
        return ESP_ERR_INVALID_ARG;
    }
    
    dmx_port_context_t *port_ctx = get_port_context(port);
    if (!port_ctx) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (port_ctx->mode != DMX_MODE_OUTPUT && port_ctx->mode != DMX_MODE_RDM_MASTER) {
        return ESP_ERR_INVALID_STATE;
    }
    
    xSemaphoreTake(port_ctx->buffer_mutex, portMAX_DELAY);
    memcpy(&port_ctx->dmx_buffer[start_channel - 1], data, length);
    xSemaphoreGive(port_ctx->buffer_mutex);
    
    return ESP_OK;
}

esp_err_t dmx_handler_blackout(uint8_t port)
{
    if (!dmx_state.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    dmx_port_context_t *port_ctx = get_port_context(port);
    if (!port_ctx) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (port_ctx->mode != DMX_MODE_OUTPUT && port_ctx->mode != DMX_MODE_RDM_MASTER) {
        return ESP_ERR_INVALID_STATE;
    }
    
    xSemaphoreTake(port_ctx->buffer_mutex, portMAX_DELAY);
    memset(port_ctx->dmx_buffer, DMX_BLACKOUT_VALUE, DMX_CHANNEL_COUNT);
    xSemaphoreGive(port_ctx->buffer_mutex);
    
    ESP_LOGI(TAG, "Port %d blackout", port);
    
    return ESP_OK;
}

esp_err_t dmx_handler_get_port_status(uint8_t port, dmx_port_status_t *status)
{
    if (!dmx_state.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    dmx_port_context_t *port_ctx = get_port_context(port);
    if (!port_ctx || !status) {
        return ESP_ERR_INVALID_ARG;
    }
    
    xSemaphoreTake(dmx_state.state_mutex, portMAX_DELAY);
    
    status->mode = port_ctx->mode;
    status->is_active = port_ctx->is_active;
    status->universe = port_ctx->universe;
    memcpy(&status->stats, &port_ctx->stats, sizeof(dmx_port_stats_t));
    status->rdm_device_count = port_ctx->rdm_device_count;
    
    xSemaphoreGive(dmx_state.state_mutex);
    
    return ESP_OK;
}

esp_err_t dmx_handler_register_rx_callback(uint8_t port, dmx_rx_callback_t callback, 
                                           void *user_data)
{
    if (!dmx_state.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    dmx_port_context_t *port_ctx = get_port_context(port);
    if (!port_ctx) {
        return ESP_ERR_INVALID_ARG;
    }
    
    xSemaphoreTake(dmx_state.state_mutex, portMAX_DELAY);
    port_ctx->rx_callback = callback;
    port_ctx->rx_callback_user_data = user_data;
    xSemaphoreGive(dmx_state.state_mutex);
    
    return ESP_OK;
}

esp_err_t dmx_handler_register_discovery_callback(uint8_t port, 
                                                  rdm_discovery_callback_t callback,
                                                  void *user_data)
{
    if (!dmx_state.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    dmx_port_context_t *port_ctx = get_port_context(port);
    if (!port_ctx) {
        return ESP_ERR_INVALID_ARG;
    }
    
    xSemaphoreTake(dmx_state.state_mutex, portMAX_DELAY);
    port_ctx->discovery_callback = callback;
    port_ctx->discovery_callback_user_data = user_data;
    xSemaphoreGive(dmx_state.state_mutex);
    
    return ESP_OK;
}

// RDM functions (stubs for now - full implementation requires esp-dmx library)
esp_err_t dmx_handler_rdm_discover(uint8_t port)
{
    if (!dmx_state.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    dmx_port_context_t *port_ctx = get_port_context(port);
    if (!port_ctx) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (port_ctx->mode != DMX_MODE_RDM_MASTER) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (port_ctx->rdm_discovery_running) {
        ESP_LOGW(TAG, "RDM discovery already running on port %d", port);
        return ESP_ERR_INVALID_STATE;
    }
    
    ESP_LOGI(TAG, "Starting RDM discovery on port %d", port);
    
    // TODO: Implement RDM discovery using esp-dmx API
    // This is a placeholder that will be completed when esp-dmx is integrated
    
    port_ctx->rdm_discovery_running = true;
    
    return ESP_OK;
}

esp_err_t dmx_handler_get_rdm_devices(uint8_t port, rdm_device_t *devices, size_t *count)
{
    if (!dmx_state.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    dmx_port_context_t *port_ctx = get_port_context(port);
    if (!port_ctx || !devices || !count) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (port_ctx->mode != DMX_MODE_RDM_MASTER) {
        return ESP_ERR_INVALID_STATE;
    }
    
    xSemaphoreTake(port_ctx->rdm_mutex, portMAX_DELAY);
    
    size_t copy_count = (port_ctx->rdm_device_count < *count) ? 
                        port_ctx->rdm_device_count : *count;
    
    if (copy_count > 0) {
        memcpy(devices, port_ctx->rdm_devices, copy_count * sizeof(rdm_device_t));
    }
    
    *count = port_ctx->rdm_device_count;
    
    xSemaphoreGive(port_ctx->rdm_mutex);
    
    return ESP_OK;
}

esp_err_t dmx_handler_rdm_get(uint8_t port, const rdm_uid_t uid, uint16_t pid, 
                              uint8_t *response_data, size_t *response_size)
{
    if (!dmx_state.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    dmx_port_context_t *port_ctx = get_port_context(port);
    if (!port_ctx || !uid || !response_data || !response_size) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (port_ctx->mode != DMX_MODE_RDM_MASTER) {
        return ESP_ERR_INVALID_STATE;
    }
    
    // TODO: Implement RDM GET using esp-dmx API
    ESP_LOGW(TAG, "RDM GET not yet implemented");
    
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t dmx_handler_rdm_set(uint8_t port, const rdm_uid_t uid, uint16_t pid, 
                              const uint8_t *data, size_t size)
{
    if (!dmx_state.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    dmx_port_context_t *port_ctx = get_port_context(port);
    if (!port_ctx || !uid || !data) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (port_ctx->mode != DMX_MODE_RDM_MASTER) {
        return ESP_ERR_INVALID_STATE;
    }
    
    // TODO: Implement RDM SET using esp-dmx API
    ESP_LOGW(TAG, "RDM SET not yet implemented");
    
    return ESP_ERR_NOT_SUPPORTED;
}

// ============================================================================
// Internal Functions
// ============================================================================

/**
 * @brief Install DMX driver for port
 * 
 * Note: This function uses esp-dmx library API. Constants like DMX_CONFIG_DEFAULT
 * and DMX_INTR_FLAGS_DEFAULT should be provided by the esp-dmx library headers.
 * Fallback definitions are provided above if not available.
 */
static esp_err_t port_install_driver(dmx_port_context_t *port_ctx)
{
    // Configure DMX driver
    dmx_config_t dmx_config = DMX_CONFIG_DEFAULT;
    
    // Install driver
    esp_err_t ret = dmx_driver_install(port_ctx->dmx_num, &dmx_config, DMX_INTR_FLAGS_DEFAULT);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to install DMX driver: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Set pins
    ret = dmx_set_pin(port_ctx->dmx_num, port_ctx->tx_pin, port_ctx->rx_pin, port_ctx->dir_pin);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set DMX pins: %s", esp_err_to_name(ret));
        dmx_driver_delete(port_ctx->dmx_num);
        return ret;
    }
    
    ESP_LOGI(TAG, "DMX driver installed for port %d", port_ctx->port_num);
    
    return ESP_OK;
}

/**
 * @brief Uninstall DMX driver for port
 */
static esp_err_t port_uninstall_driver(dmx_port_context_t *port_ctx)
{
    esp_err_t ret = dmx_driver_delete(port_ctx->dmx_num);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to delete DMX driver: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGI(TAG, "DMX driver uninstalled for port %d", port_ctx->port_num);
    
    return ESP_OK;
}

/**
 * @brief DMX output task
 */
static void dmx_output_task(void *arg)
{
    dmx_port_context_t *port_ctx = (dmx_port_context_t *)arg;
    uint8_t dmx_data[DMX_CHANNEL_COUNT];
    
    ESP_LOGI(TAG, "DMX output task started for port %d", port_ctx->port_num);
    
    while (port_ctx->is_active) {
        // Copy DMX data from buffer
        xSemaphoreTake(port_ctx->buffer_mutex, portMAX_DELAY);
        memcpy(dmx_data, port_ctx->dmx_buffer, DMX_CHANNEL_COUNT);
        xSemaphoreGive(port_ctx->buffer_mutex);
        
        // Send DMX frame
        dmx_wait_sent(port_ctx->dmx_num, DMX_TIMEOUT_TICK);
        dmx_write(port_ctx->dmx_num, dmx_data, DMX_CHANNEL_COUNT);
        dmx_send(port_ctx->dmx_num, DMX_CHANNEL_COUNT);
        
        // Update statistics
        port_ctx->stats.frames_sent++;
        port_ctx->stats.last_frame_time_ms = esp_timer_get_time() / 1000;
        
        // Wait for next frame (~44Hz)
        vTaskDelay(pdMS_TO_TICKS(DMX_OUTPUT_RATE_MS));
    }
    
    ESP_LOGI(TAG, "DMX output task stopped for port %d", port_ctx->port_num);
    vTaskDelete(NULL);
}

/**
 * @brief DMX input task
 */
static void dmx_input_task(void *arg)
{
    dmx_port_context_t *port_ctx = (dmx_port_context_t *)arg;
    dmx_packet_t packet;
    
    ESP_LOGI(TAG, "DMX input task started for port %d", port_ctx->port_num);
    
    while (port_ctx->is_active) {
        // Wait for DMX packet
        esp_err_t ret = dmx_receive(port_ctx->dmx_num, &packet, pdMS_TO_TICKS(DMX_RX_TIMEOUT_MS));
        
        if (ret == ESP_OK) {
            // Valid DMX packet received
            if (packet.sc == DMX_SC) {
                // Copy data to buffer
                xSemaphoreTake(port_ctx->buffer_mutex, portMAX_DELAY);
                memcpy(port_ctx->dmx_buffer, packet.data, DMX_CHANNEL_COUNT);
                xSemaphoreGive(port_ctx->buffer_mutex);
                
                // Update statistics
                port_ctx->stats.frames_received++;
                port_ctx->stats.last_frame_time_ms = esp_timer_get_time() / 1000;
                
                // Call callback if registered
                if (port_ctx->rx_callback) {
                    port_ctx->rx_callback(port_ctx->port_num, packet.data, 
                                        DMX_CHANNEL_COUNT, port_ctx->rx_callback_user_data);
                }
            }
        } else if (ret != ESP_ERR_TIMEOUT) {
            // Error occurred
            port_ctx->stats.error_count++;
        }
    }
    
    ESP_LOGI(TAG, "DMX input task stopped for port %d", port_ctx->port_num);
    vTaskDelete(NULL);
}
