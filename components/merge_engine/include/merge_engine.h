#ifndef MERGE_ENGINE_H
#define MERGE_ENGINE_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "config_manager.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Merge Engine Module
 * 
 * This module merges DMX data from multiple sources using various algorithms.
 * It supports Art-Net, sACN, and DMX input sources with timeout management.
 * 
 * Features:
 * - Multiple merge modes (HTP, LTP, LAST, BACKUP, DISABLE)
 * - Multi-source support (up to 4 sources per port)
 * - Timeout detection and handling
 * - Source tracking (protocol, IP, name, priority)
 * - Thread-safe operation
 */

// Maximum sources per port
#define MERGE_MAX_SOURCES 4

// Default timeout (2.5 seconds in microseconds)
#define MERGE_DEFAULT_TIMEOUT_US 2500000

/**
 * @brief Source protocol type
 */
typedef enum {
    SOURCE_PROTOCOL_ARTNET = 0,    /**< Art-Net source */
    SOURCE_PROTOCOL_SACN = 1,      /**< sACN source */
    SOURCE_PROTOCOL_DMX_IN = 2     /**< DMX input source */
} source_protocol_t;

/**
 * @brief DMX source data structure
 */
typedef struct {
    uint8_t data[512];             /**< DMX data (512 channels) */
    uint64_t timestamp_us;         /**< Timestamp in microseconds */
    uint32_t sequence;             /**< Sequence number */
    uint8_t priority;              /**< Source priority (0-200, sACN only) */
    char source_name[64];          /**< Source identifier */
    uint32_t source_ip;            /**< Source IP address */
    source_protocol_t protocol;    /**< Protocol type */
    bool is_valid;                 /**< Data valid flag */
} dmx_source_data_t;

/**
 * @brief Merge engine statistics
 */
typedef struct {
    uint32_t total_merges;         /**< Total merge operations */
    uint32_t htp_merges;           /**< HTP merge count */
    uint32_t ltp_merges;           /**< LTP merge count */
    uint32_t last_merges;          /**< LAST merge count */
    uint32_t backup_switches;      /**< Backup failover count */
    uint32_t source_timeouts;      /**< Source timeout count */
    uint32_t active_sources;       /**< Currently active sources */
} merge_stats_t;

/**
 * @brief Initialize merge engine
 * 
 * Initializes the merge engine module. Must be called before any other functions.
 * 
 * @return
 *     - ESP_OK on success
 *     - ESP_ERR_NO_MEM if memory allocation failed
 *     - ESP_ERR_INVALID_STATE if already initialized
 */
esp_err_t merge_engine_init(void);

/**
 * @brief Deinitialize merge engine
 * 
 * Stops the merge engine and frees resources.
 * 
 * @return
 *     - ESP_OK on success
 */
esp_err_t merge_engine_deinit(void);

/**
 * @brief Configure merge engine for a port
 * 
 * Configures merge mode and timeout for the specified port.
 * 
 * @param port Port number (1 or 2)
 * @param mode Merge mode from config_manager
 * @param timeout_ms Timeout in milliseconds (0 = use default)
 * @return
 *     - ESP_OK on success
 *     - ESP_ERR_INVALID_ARG if port invalid
 *     - ESP_ERR_INVALID_STATE if not initialized
 */
esp_err_t merge_engine_config(uint8_t port, merge_mode_t mode, uint32_t timeout_ms);

/**
 * @brief Push Art-Net data to merge engine
 * 
 * Adds or updates Art-Net source data for merging.
 * 
 * @param port Port number (1 or 2)
 * @param universe Universe number
 * @param data DMX data (512 channels)
 * @param sequence Sequence number
 * @param source_ip Source IP address
 * @return
 *     - ESP_OK on success
 *     - ESP_ERR_INVALID_ARG if parameters invalid
 *     - ESP_ERR_INVALID_STATE if not initialized
 *     - ESP_ERR_NO_MEM if maximum sources reached
 */
esp_err_t merge_engine_push_artnet(uint8_t port, uint16_t universe,
                                   const uint8_t *data, uint8_t sequence,
                                   uint32_t source_ip);

/**
 * @brief Push sACN data to merge engine
 * 
 * Adds or updates sACN source data for merging.
 * 
 * @param port Port number (1 or 2)
 * @param universe Universe number
 * @param data DMX data (512 channels)
 * @param sequence Sequence number
 * @param priority Priority (0-200)
 * @param source_name Source name
 * @param source_ip Source IP address
 * @return
 *     - ESP_OK on success
 *     - ESP_ERR_INVALID_ARG if parameters invalid
 *     - ESP_ERR_INVALID_STATE if not initialized
 *     - ESP_ERR_NO_MEM if maximum sources reached
 */
esp_err_t merge_engine_push_sacn(uint8_t port, uint16_t universe,
                                 const uint8_t *data, uint8_t sequence,
                                 uint8_t priority, const char *source_name,
                                 uint32_t source_ip);

/**
 * @brief Push DMX input data to merge engine
 * 
 * Adds or updates DMX input source data for merging.
 * 
 * @param port Port number (1 or 2)
 * @param data DMX data (512 channels)
 * @return
 *     - ESP_OK on success
 *     - ESP_ERR_INVALID_ARG if parameters invalid
 *     - ESP_ERR_INVALID_STATE if not initialized
 */
esp_err_t merge_engine_push_dmx_in(uint8_t port, const uint8_t *data);

/**
 * @brief Get merged output data
 * 
 * Retrieves the merged DMX data for the specified port.
 * Performs merge operation if needed.
 * 
 * @param port Port number (1 or 2)
 * @param data Buffer to store merged data (512 channels)
 * @return
 *     - ESP_OK on success
 *     - ESP_ERR_INVALID_ARG if parameters invalid
 *     - ESP_ERR_INVALID_STATE if not initialized
 *     - ESP_ERR_TIMEOUT if no active sources
 */
esp_err_t merge_engine_get_output(uint8_t port, uint8_t *data);

/**
 * @brief Check if output is active
 * 
 * Checks if there are any active sources within the timeout period.
 * 
 * @param port Port number (1 or 2)
 * @return true if output active, false otherwise
 */
bool merge_engine_is_output_active(uint8_t port);

/**
 * @brief Force blackout
 * 
 * Clears all sources and outputs zero for all channels.
 * 
 * @param port Port number (1 or 2)
 * @return
 *     - ESP_OK on success
 *     - ESP_ERR_INVALID_ARG if port invalid
 *     - ESP_ERR_INVALID_STATE if not initialized
 */
esp_err_t merge_engine_blackout(uint8_t port);

/**
 * @brief Get active sources
 * 
 * Retrieves information about currently active sources.
 * 
 * @param port Port number (1 or 2)
 * @param sources Buffer to store source information
 * @param max_sources Maximum number of sources to return
 * @return Number of active sources
 */
uint8_t merge_engine_get_active_sources(uint8_t port, 
                                       dmx_source_data_t *sources,
                                       uint8_t max_sources);

/**
 * @brief Get merge statistics
 * 
 * Retrieves merge engine statistics for the specified port.
 * 
 * @param port Port number (1 or 2)
 * @param stats Pointer to statistics structure
 * @return
 *     - ESP_OK on success
 *     - ESP_ERR_INVALID_ARG if parameters invalid
 *     - ESP_ERR_INVALID_STATE if not initialized
 */
esp_err_t merge_engine_get_stats(uint8_t port, merge_stats_t *stats);

/**
 * @brief Reset statistics
 * 
 * Resets merge statistics for the specified port.
 * 
 * @param port Port number (1 or 2)
 * @return
 *     - ESP_OK on success
 *     - ESP_ERR_INVALID_ARG if port invalid
 *     - ESP_ERR_INVALID_STATE if not initialized
 */
esp_err_t merge_engine_reset_stats(uint8_t port);

#ifdef __cplusplus
}
#endif

#endif // MERGE_ENGINE_H
