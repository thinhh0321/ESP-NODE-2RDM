/**
 * @file merge_engine.c
 * @brief Merge Engine Implementation
 * 
 * This component merges DMX data from multiple sources using various algorithms.
 * It supports HTP, LTP, LAST, BACKUP, and DISABLE modes with timeout management.
 * 
 * Thread Safety:
 * - All public APIs are thread-safe using mutexes
 * - Merge operations are atomic per port
 * 
 * Memory Usage:
 * - ~4KB per port (context + sources)
 * - Total: ~8KB for 2 ports
 */

#include "merge_engine.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <string.h>
#include <inttypes.h>
#include <inttypes.h>

static const char *TAG = "merge_engine";

/**
 * @brief Merge context for one port
 */
typedef struct {
    uint8_t port_num;              /**< Port number (1 or 2) */
    uint16_t universe;             /**< Target universe */
    merge_mode_t mode;             /**< Merge mode */
    uint32_t timeout_us;           /**< Timeout in microseconds */
    
    // Source tracking
    dmx_source_data_t sources[MERGE_MAX_SOURCES];
    uint8_t source_count;
    
    // Output buffer
    uint8_t merged_data[512];
    uint64_t last_merge_time_us;
    bool output_active;
    
    // Statistics
    merge_stats_t stats;
    
    // Primary source index (for BACKUP mode)
    int8_t primary_source_index;
} merge_context_t;

/**
 * @brief Module state
 */
static struct {
    bool initialized;
    merge_context_t ports[2];      /**< Contexts for port 1 and 2 */
    SemaphoreHandle_t mutex;
} merge_state = {
    .initialized = false,
};

// Forward declarations
static void perform_merge(merge_context_t *ctx);
static void merge_htp(merge_context_t *ctx);
static void merge_ltp(merge_context_t *ctx);
static void merge_last(merge_context_t *ctx);
static void merge_backup(merge_context_t *ctx);
static void merge_disable(merge_context_t *ctx);
static bool is_source_timeout(const dmx_source_data_t *source, uint64_t timeout_us);
static int find_or_create_source(merge_context_t *ctx, uint32_t source_ip, 
                                 source_protocol_t protocol);
static void cleanup_timeout_sources(merge_context_t *ctx);

/**
 * @brief Get current time in microseconds
 */
static inline uint64_t get_time_us(void)
{
    return esp_timer_get_time();
}

/**
 * @brief Check if source has timed out
 */
static bool is_source_timeout(const dmx_source_data_t *source, uint64_t timeout_us)
{
    if (!source->is_valid) {
        return true;
    }
    
    uint64_t now = get_time_us();
    uint64_t elapsed = now - source->timestamp_us;
    
    return elapsed > timeout_us;
}

/**
 * @brief Count active (non-timeout) sources
 */
static uint8_t count_active_sources(merge_context_t *ctx)
{
    uint8_t count = 0;
    for (int i = 0; i < ctx->source_count; i++) {
        if (!is_source_timeout(&ctx->sources[i], ctx->timeout_us)) {
            count++;
        }
    }
    return count;
}

/**
 * @brief Find or create source entry
 */
static int find_or_create_source(merge_context_t *ctx, uint32_t source_ip,
                                source_protocol_t protocol)
{
    // Try to find existing source by IP and protocol
    for (int i = 0; i < ctx->source_count; i++) {
        if (ctx->sources[i].source_ip == source_ip &&
            ctx->sources[i].protocol == protocol) {
            return i;
        }
    }
    
    // Try to reuse timeout source slot
    for (int i = 0; i < ctx->source_count; i++) {
        if (is_source_timeout(&ctx->sources[i], ctx->timeout_us)) {
            // Reuse this slot
            memset(&ctx->sources[i], 0, sizeof(dmx_source_data_t));
            return i;
        }
    }
    
    // Create new source if space available
    if (ctx->source_count < MERGE_MAX_SOURCES) {
        int index = ctx->source_count;
        ctx->source_count++;
        memset(&ctx->sources[index], 0, sizeof(dmx_source_data_t));
        return index;
    }
    
    return -1;  // No space available
}

/**
 * @brief Cleanup timeout sources
 */
static void cleanup_timeout_sources(merge_context_t *ctx)
{
    for (int i = 0; i < ctx->source_count; i++) {
        if (is_source_timeout(&ctx->sources[i], ctx->timeout_us)) {
            if (ctx->sources[i].is_valid) {
                ctx->stats.source_timeouts++;
                ctx->sources[i].is_valid = false;
            }
        }
    }
}

/**
 * @brief Get port context
 */
static merge_context_t* get_port_context(uint8_t port)
{
    if (port < 1 || port > 2) {
        return NULL;
    }
    return &merge_state.ports[port - 1];
}

// ============================================================================
// Public API Implementation
// ============================================================================

esp_err_t merge_engine_init(void)
{
    if (merge_state.initialized) {
        ESP_LOGW(TAG, "Already initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    ESP_LOGI(TAG, "Initializing merge engine...");
    
    // Create mutex
    merge_state.mutex = xSemaphoreCreateMutex();
    if (!merge_state.mutex) {
        ESP_LOGE(TAG, "Failed to create mutex");
        return ESP_ERR_NO_MEM;
    }
    
    // Initialize port contexts
    for (int i = 0; i < 2; i++) {
        merge_context_t *ctx = &merge_state.ports[i];
        memset(ctx, 0, sizeof(merge_context_t));
        ctx->port_num = i + 1;
        ctx->mode = MERGE_MODE_HTP;  // Default mode
        ctx->timeout_us = MERGE_DEFAULT_TIMEOUT_US;
        ctx->primary_source_index = -1;
        ctx->output_active = false;
    }
    
    merge_state.initialized = true;
    ESP_LOGI(TAG, "Merge engine initialized successfully");
    
    return ESP_OK;
}

esp_err_t merge_engine_deinit(void)
{
    if (!merge_state.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    ESP_LOGI(TAG, "Deinitializing merge engine...");
    
    // Delete mutex
    if (merge_state.mutex) {
        vSemaphoreDelete(merge_state.mutex);
        merge_state.mutex = NULL;
    }
    
    merge_state.initialized = false;
    ESP_LOGI(TAG, "Merge engine deinitialized");
    
    return ESP_OK;
}

esp_err_t merge_engine_config(uint8_t port, merge_mode_t mode, uint32_t timeout_ms)
{
    if (!merge_state.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    merge_context_t *ctx = get_port_context(port);
    if (!ctx) {
        return ESP_ERR_INVALID_ARG;
    }
    
    xSemaphoreTake(merge_state.mutex, portMAX_DELAY);
    
    ctx->mode = mode;
    
    if (timeout_ms > 0) {
        ctx->timeout_us = timeout_ms * 1000;
    } else {
        ctx->timeout_us = MERGE_DEFAULT_TIMEOUT_US;
    }
    
    ESP_LOGI(TAG, "Port %d configured: mode=%d, timeout=%lu ms", 
             port, mode, ctx->timeout_us / 1000);
    
    xSemaphoreGive(merge_state.mutex);
    
    return ESP_OK;
}

esp_err_t merge_engine_push_artnet(uint8_t port, uint16_t universe,
                                   const uint8_t *data, uint8_t sequence,
                                   uint32_t source_ip)
{
    if (!merge_state.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    merge_context_t *ctx = get_port_context(port);
    if (!ctx || !data) {
        return ESP_ERR_INVALID_ARG;
    }
    
    xSemaphoreTake(merge_state.mutex, portMAX_DELAY);
    
    // Find or create source
    int source_idx = find_or_create_source(ctx, source_ip, SOURCE_PROTOCOL_ARTNET);
    if (source_idx < 0) {
        ESP_LOGW(TAG, "Port %d: Maximum sources reached", port);
        xSemaphoreGive(merge_state.mutex);
        return ESP_ERR_NO_MEM;
    }
    
    // Update source data
    dmx_source_data_t *source = &ctx->sources[source_idx];
    memcpy(source->data, data, 512);
    source->timestamp_us = get_time_us();
    source->sequence = sequence;
    source->priority = 100;  // Default priority for Art-Net
    snprintf(source->source_name, sizeof(source->source_name), "ArtNet_%08" PRIX32, source_ip);
    source->source_ip = source_ip;
    source->protocol = SOURCE_PROTOCOL_ARTNET;
    source->is_valid = true;
    
    xSemaphoreGive(merge_state.mutex);
    
    return ESP_OK;
}

esp_err_t merge_engine_push_sacn(uint8_t port, uint16_t universe,
                                 const uint8_t *data, uint8_t sequence,
                                 uint8_t priority, const char *source_name,
                                 uint32_t source_ip)
{
    if (!merge_state.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    merge_context_t *ctx = get_port_context(port);
    if (!ctx || !data) {
        return ESP_ERR_INVALID_ARG;
    }
    
    xSemaphoreTake(merge_state.mutex, portMAX_DELAY);
    
    // Find or create source
    int source_idx = find_or_create_source(ctx, source_ip, SOURCE_PROTOCOL_SACN);
    if (source_idx < 0) {
        ESP_LOGW(TAG, "Port %d: Maximum sources reached", port);
        xSemaphoreGive(merge_state.mutex);
        return ESP_ERR_NO_MEM;
    }
    
    // Update source data
    dmx_source_data_t *source = &ctx->sources[source_idx];
    memcpy(source->data, data, 512);
    source->timestamp_us = get_time_us();
    source->sequence = sequence;
    source->priority = priority;
    if (source_name) {
        strncpy(source->source_name, source_name, sizeof(source->source_name) - 1);
        source->source_name[sizeof(source->source_name) - 1] = '\0';
    } else {
        snprintf(source->source_name, sizeof(source->source_name), "sACN_%08" PRIX32, source_ip);
    }
    source->source_ip = source_ip;
    source->protocol = SOURCE_PROTOCOL_SACN;
    source->is_valid = true;
    
    xSemaphoreGive(merge_state.mutex);
    
    return ESP_OK;
}

esp_err_t merge_engine_push_dmx_in(uint8_t port, const uint8_t *data)
{
    if (!merge_state.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    merge_context_t *ctx = get_port_context(port);
    if (!ctx || !data) {
        return ESP_ERR_INVALID_ARG;
    }
    
    xSemaphoreTake(merge_state.mutex, portMAX_DELAY);
    
    // DMX input always uses source IP 0 and index 0
    int source_idx = find_or_create_source(ctx, 0, SOURCE_PROTOCOL_DMX_IN);
    if (source_idx < 0) {
        ESP_LOGW(TAG, "Port %d: Maximum sources reached", port);
        xSemaphoreGive(merge_state.mutex);
        return ESP_ERR_NO_MEM;
    }
    
    // Update source data
    dmx_source_data_t *source = &ctx->sources[source_idx];
    memcpy(source->data, data, 512);
    source->timestamp_us = get_time_us();
    source->sequence = 0;
    source->priority = 100;
    snprintf(source->source_name, sizeof(source->source_name), "DMX_IN_%d", port);
    source->source_ip = 0;
    source->protocol = SOURCE_PROTOCOL_DMX_IN;
    source->is_valid = true;
    
    xSemaphoreGive(merge_state.mutex);
    
    return ESP_OK;
}

esp_err_t merge_engine_get_output(uint8_t port, uint8_t *data)
{
    if (!merge_state.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    merge_context_t *ctx = get_port_context(port);
    if (!ctx || !data) {
        return ESP_ERR_INVALID_ARG;
    }
    
    xSemaphoreTake(merge_state.mutex, portMAX_DELAY);
    
    // Cleanup timeout sources
    cleanup_timeout_sources(ctx);
    
    // Perform merge
    perform_merge(ctx);
    
    // Copy output data
    if (ctx->output_active) {
        memcpy(data, ctx->merged_data, 512);
        xSemaphoreGive(merge_state.mutex);
        return ESP_OK;
    } else {
        // No active sources - return blackout
        memset(data, 0, 512);
        xSemaphoreGive(merge_state.mutex);
        return ESP_ERR_TIMEOUT;
    }
}

bool merge_engine_is_output_active(uint8_t port)
{
    if (!merge_state.initialized) {
        return false;
    }
    
    merge_context_t *ctx = get_port_context(port);
    if (!ctx) {
        return false;
    }
    
    xSemaphoreTake(merge_state.mutex, portMAX_DELAY);
    cleanup_timeout_sources(ctx);
    uint8_t active = count_active_sources(ctx);
    xSemaphoreGive(merge_state.mutex);
    
    return active > 0;
}

esp_err_t merge_engine_blackout(uint8_t port)
{
    if (!merge_state.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    merge_context_t *ctx = get_port_context(port);
    if (!ctx) {
        return ESP_ERR_INVALID_ARG;
    }
    
    xSemaphoreTake(merge_state.mutex, portMAX_DELAY);
    
    // Invalidate all sources
    for (int i = 0; i < ctx->source_count; i++) {
        ctx->sources[i].is_valid = false;
    }
    
    // Clear output
    memset(ctx->merged_data, 0, 512);
    ctx->output_active = false;
    
    ESP_LOGI(TAG, "Port %d blackout", port);
    
    xSemaphoreGive(merge_state.mutex);
    
    return ESP_OK;
}

uint8_t merge_engine_get_active_sources(uint8_t port, 
                                       dmx_source_data_t *sources,
                                       uint8_t max_sources)
{
    if (!merge_state.initialized || !sources || max_sources == 0) {
        return 0;
    }
    
    merge_context_t *ctx = get_port_context(port);
    if (!ctx) {
        return 0;
    }
    
    xSemaphoreTake(merge_state.mutex, portMAX_DELAY);
    
    cleanup_timeout_sources(ctx);
    
    uint8_t count = 0;
    for (int i = 0; i < ctx->source_count && count < max_sources; i++) {
        if (!is_source_timeout(&ctx->sources[i], ctx->timeout_us)) {
            memcpy(&sources[count], &ctx->sources[i], sizeof(dmx_source_data_t));
            count++;
        }
    }
    
    xSemaphoreGive(merge_state.mutex);
    
    return count;
}

esp_err_t merge_engine_get_stats(uint8_t port, merge_stats_t *stats)
{
    if (!merge_state.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    merge_context_t *ctx = get_port_context(port);
    if (!ctx || !stats) {
        return ESP_ERR_INVALID_ARG;
    }
    
    xSemaphoreTake(merge_state.mutex, portMAX_DELAY);
    
    memcpy(stats, &ctx->stats, sizeof(merge_stats_t));
    stats->active_sources = count_active_sources(ctx);
    
    xSemaphoreGive(merge_state.mutex);
    
    return ESP_OK;
}

esp_err_t merge_engine_reset_stats(uint8_t port)
{
    if (!merge_state.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    merge_context_t *ctx = get_port_context(port);
    if (!ctx) {
        return ESP_ERR_INVALID_ARG;
    }
    
    xSemaphoreTake(merge_state.mutex, portMAX_DELAY);
    memset(&ctx->stats, 0, sizeof(merge_stats_t));
    xSemaphoreGive(merge_state.mutex);
    
    ESP_LOGI(TAG, "Port %d statistics reset", port);
    
    return ESP_OK;
}

// ============================================================================
// Internal Merge Functions
// ============================================================================

/**
 * @brief Perform merge based on configured mode
 */
static void perform_merge(merge_context_t *ctx)
{
    ctx->stats.total_merges++;
    ctx->stats.active_sources = count_active_sources(ctx);
    
    switch (ctx->mode) {
        case MERGE_MODE_HTP:
            merge_htp(ctx);
            ctx->stats.htp_merges++;
            break;
            
        case MERGE_MODE_LTP:
            merge_ltp(ctx);
            ctx->stats.ltp_merges++;
            break;
            
        case MERGE_MODE_LAST:
            merge_last(ctx);
            ctx->stats.last_merges++;
            break;
            
        case MERGE_MODE_BACKUP:
            merge_backup(ctx);
            break;
            
        case MERGE_MODE_DISABLE:
        default:
            merge_disable(ctx);
            break;
    }
    
    ctx->last_merge_time_us = get_time_us();
}

/**
 * @brief HTP (Highest Takes Precedence) merge
 * Takes the maximum value for each channel across all active sources
 */
static void merge_htp(merge_context_t *ctx)
{
    memset(ctx->merged_data, 0, 512);
    ctx->output_active = false;
    
    for (int i = 0; i < ctx->source_count; i++) {
        if (!is_source_timeout(&ctx->sources[i], ctx->timeout_us)) {
            ctx->output_active = true;
            
            for (int ch = 0; ch < 512; ch++) {
                if (ctx->sources[i].data[ch] > ctx->merged_data[ch]) {
                    ctx->merged_data[ch] = ctx->sources[i].data[ch];
                }
            }
        }
    }
}

/**
 * @brief LTP (Lowest Takes Precedence) merge
 * Takes the minimum value for each channel across all active sources
 */
static void merge_ltp(merge_context_t *ctx)
{
    memset(ctx->merged_data, 255, 512);
    ctx->output_active = false;
    
    for (int i = 0; i < ctx->source_count; i++) {
        if (!is_source_timeout(&ctx->sources[i], ctx->timeout_us)) {
            ctx->output_active = true;
            
            for (int ch = 0; ch < 512; ch++) {
                if (ctx->sources[i].data[ch] < ctx->merged_data[ch]) {
                    ctx->merged_data[ch] = ctx->sources[i].data[ch];
                }
            }
        }
    }
    
    // If no sources, output 0 instead of 255
    if (!ctx->output_active) {
        memset(ctx->merged_data, 0, 512);
    }
}

/**
 * @brief LAST (Latest Takes Precedence) merge
 * Uses the entire frame from the most recently updated source
 */
static void merge_last(merge_context_t *ctx)
{
    uint64_t latest_time = 0;
    dmx_source_data_t *latest_source = NULL;
    
    // Find the most recent source
    for (int i = 0; i < ctx->source_count; i++) {
        if (!is_source_timeout(&ctx->sources[i], ctx->timeout_us)) {
            if (ctx->sources[i].timestamp_us > latest_time) {
                latest_time = ctx->sources[i].timestamp_us;
                latest_source = &ctx->sources[i];
            }
        }
    }
    
    if (latest_source) {
        memcpy(ctx->merged_data, latest_source->data, 512);
        ctx->output_active = true;
    } else {
        memset(ctx->merged_data, 0, 512);
        ctx->output_active = false;
    }
}

/**
 * @brief BACKUP merge
 * Uses primary source, switches to backup on timeout
 */
static void merge_backup(merge_context_t *ctx)
{
    // If primary source index is valid and not timeout, use it
    if (ctx->primary_source_index >= 0 && 
        ctx->primary_source_index < ctx->source_count) {
        
        dmx_source_data_t *primary = &ctx->sources[ctx->primary_source_index];
        
        if (!is_source_timeout(primary, ctx->timeout_us)) {
            memcpy(ctx->merged_data, primary->data, 512);
            ctx->output_active = true;
            return;
        }
    }
    
    // Primary timeout or not set - find any active source as backup
    for (int i = 0; i < ctx->source_count; i++) {
        if (!is_source_timeout(&ctx->sources[i], ctx->timeout_us)) {
            memcpy(ctx->merged_data, ctx->sources[i].data, 512);
            ctx->primary_source_index = i;  // This becomes new primary
            ctx->output_active = true;
            ctx->stats.backup_switches++;
            ESP_LOGI(TAG, "Port %d: Switched to backup source %d", 
                     ctx->port_num, i);
            return;
        }
    }
    
    // No sources available
    memset(ctx->merged_data, 0, 512);
    ctx->output_active = false;
    ctx->primary_source_index = -1;
}

/**
 * @brief DISABLE merge
 * No merging - uses first active source only
 */
static void merge_disable(merge_context_t *ctx)
{
    // Find first active source
    for (int i = 0; i < ctx->source_count; i++) {
        if (!is_source_timeout(&ctx->sources[i], ctx->timeout_us)) {
            memcpy(ctx->merged_data, ctx->sources[i].data, 512);
            ctx->output_active = true;
            return;
        }
    }
    
    // No active source
    memset(ctx->merged_data, 0, 512);
    ctx->output_active = false;
}
