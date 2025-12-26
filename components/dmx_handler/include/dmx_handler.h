#ifndef DMX_HANDLER_H
#define DMX_HANDLER_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "config_manager.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief DMX/RDM Handler Module
 * 
 * This module manages 2 independent DMX512/RDM ports using the esp-dmx library.
 * It provides unified interface for DMX output, input, and RDM operations.
 * 
 * Hardware:
 * - Port 1: TX=GPIO17, RX=GPIO16, DIR=GPIO21, UART_NUM_1
 * - Port 2: TX=GPIO19, RX=GPIO18, DIR=GPIO20, UART_NUM_2
 * 
 * Features:
 * - DMX512 output at ~44Hz refresh rate
 * - DMX512 input monitoring
 * - RDM master (discovery, get/set parameters)
 * - RDM responder mode
 * - Configurable per-port modes
 * - Thread-safe operation
 */

// Port numbers
#define DMX_PORT_1  1
#define DMX_PORT_2  2
#define DMX_PORT_MAX 2

// DMX constants
#define DMX_CHANNEL_COUNT 512
#define DMX_FRAME_SIZE (DMX_CHANNEL_COUNT + 1)  // Including start code
#define DMX_MAX_DEVICES 32

// RDM UIDs (6 bytes)
typedef uint8_t rdm_uid_t[6];

// Broadcast UID for RDM
#define RDM_BROADCAST_UID {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}

/**
 * @brief RDM device information
 */
typedef struct {
    rdm_uid_t uid;              /**< RDM Unique ID (6 bytes) */
    uint16_t device_model;      /**< Device model ID */
    uint32_t software_version;  /**< Software version */
    uint16_t dmx_footprint;     /**< DMX footprint (channels used) */
    uint16_t dmx_start_address; /**< DMX start address */
    uint8_t personality_count;  /**< Number of personalities */
    uint8_t current_personality;/**< Current personality */
    char manufacturer_label[33];/**< Manufacturer name */
    char device_label[33];      /**< Device label */
    char device_model_desc[33]; /**< Device model description */
} rdm_device_t;

/**
 * @brief DMX port statistics
 */
typedef struct {
    uint32_t frames_sent;       /**< Total DMX frames sent */
    uint32_t frames_received;   /**< Total DMX frames received */
    uint32_t rdm_requests_sent; /**< Total RDM requests sent */
    uint32_t rdm_responses_rx;  /**< Total RDM responses received */
    uint32_t error_count;       /**< Total errors */
    uint32_t last_frame_time_ms;/**< Last frame timestamp (ms) */
} dmx_port_stats_t;

/**
 * @brief DMX port status
 */
typedef struct {
    dmx_mode_t mode;            /**< Current port mode */
    bool is_active;             /**< Port is active */
    uint16_t universe;          /**< Primary universe */
    dmx_port_stats_t stats;     /**< Port statistics */
    uint8_t rdm_device_count;   /**< Number of RDM devices found */
} dmx_port_status_t;

/**
 * @brief DMX frame received callback
 * @param port Port number (1 or 2)
 * @param data DMX data (512 channels)
 * @param size Data size (should be 512)
 * @param user_data User data pointer
 */
typedef void (*dmx_rx_callback_t)(uint8_t port, const uint8_t *data, size_t size, void *user_data);

/**
 * @brief RDM discovery completed callback
 * @param port Port number (1 or 2)
 * @param device_count Number of devices discovered
 * @param user_data User data pointer
 */
typedef void (*rdm_discovery_callback_t)(uint8_t port, uint8_t device_count, void *user_data);

/**
 * @brief Initialize DMX handler module
 * 
 * Initializes the DMX/RDM handler with both ports disabled.
 * Must be called before any other dmx_handler functions.
 * 
 * @return
 *     - ESP_OK on success
 *     - ESP_ERR_NO_MEM if memory allocation failed
 *     - ESP_ERR_INVALID_STATE if already initialized
 */
esp_err_t dmx_handler_init(void);

/**
 * @brief Deinitialize DMX handler module
 * 
 * Stops all ports and frees resources.
 * 
 * @return
 *     - ESP_OK on success
 */
esp_err_t dmx_handler_deinit(void);

/**
 * @brief Configure a DMX port
 * 
 * Configures port mode, universe assignment, and RDM settings.
 * If port is already running, it will be reconfigured.
 * 
 * @param port Port number (1 or 2)
 * @param config Port configuration from config_manager
 * @return
 *     - ESP_OK on success
 *     - ESP_ERR_INVALID_ARG if port number invalid
 *     - ESP_FAIL if configuration failed
 */
esp_err_t dmx_handler_configure_port(uint8_t port, const port_config_t *config);

/**
 * @brief Start a DMX port
 * 
 * Starts the port in the configured mode.
 * Port must be configured first using dmx_handler_configure_port.
 * 
 * @param port Port number (1 or 2)
 * @return
 *     - ESP_OK on success
 *     - ESP_ERR_INVALID_ARG if port number invalid
 *     - ESP_ERR_INVALID_STATE if port not configured
 */
esp_err_t dmx_handler_start_port(uint8_t port);

/**
 * @brief Stop a DMX port
 * 
 * Stops the port and disables output/input.
 * 
 * @param port Port number (1 or 2)
 * @return
 *     - ESP_OK on success
 *     - ESP_ERR_INVALID_ARG if port number invalid
 */
esp_err_t dmx_handler_stop_port(uint8_t port);

/**
 * @brief Send DMX frame
 * 
 * Sends 512 channels of DMX data on the specified port.
 * Port must be in DMX_MODE_OUTPUT or DMX_MODE_RDM_MASTER mode.
 * 
 * @param port Port number (1 or 2)
 * @param data Pointer to 512-byte DMX data
 * @return
 *     - ESP_OK on success
 *     - ESP_ERR_INVALID_ARG if port or data invalid
 *     - ESP_ERR_INVALID_STATE if port not in output mode
 *     - ESP_FAIL if send failed
 */
esp_err_t dmx_handler_send_dmx(uint8_t port, const uint8_t *data);

/**
 * @brief Read DMX frame
 * 
 * Reads the last received DMX frame.
 * Port must be in DMX_MODE_INPUT mode.
 * 
 * @param port Port number (1 or 2)
 * @param data Buffer to store 512-byte DMX data
 * @param timeout_ms Timeout in milliseconds (0 = no wait)
 * @return
 *     - ESP_OK on success
 *     - ESP_ERR_INVALID_ARG if port or data invalid
 *     - ESP_ERR_INVALID_STATE if port not in input mode
 *     - ESP_ERR_TIMEOUT if no frame received within timeout
 */
esp_err_t dmx_handler_read_dmx(uint8_t port, uint8_t *data, uint32_t timeout_ms);

/**
 * @brief Set single DMX channel
 * 
 * Updates a single DMX channel value.
 * Useful for testing or simple control.
 * 
 * @param port Port number (1 or 2)
 * @param channel Channel number (1-512)
 * @param value Channel value (0-255)
 * @return
 *     - ESP_OK on success
 *     - ESP_ERR_INVALID_ARG if port or channel invalid
 *     - ESP_ERR_INVALID_STATE if port not in output mode
 */
esp_err_t dmx_handler_set_channel(uint8_t port, uint16_t channel, uint8_t value);

/**
 * @brief Set multiple DMX channels
 * 
 * Updates a range of DMX channels.
 * 
 * @param port Port number (1 or 2)
 * @param start_channel Starting channel (1-512)
 * @param data Pointer to channel data
 * @param length Number of channels to set
 * @return
 *     - ESP_OK on success
 *     - ESP_ERR_INVALID_ARG if parameters invalid
 *     - ESP_ERR_INVALID_STATE if port not in output mode
 */
esp_err_t dmx_handler_set_channels(uint8_t port, uint16_t start_channel, 
                                   const uint8_t *data, uint16_t length);

/**
 * @brief Blackout (set all channels to 0)
 * 
 * Sets all 512 DMX channels to 0.
 * 
 * @param port Port number (1 or 2)
 * @return
 *     - ESP_OK on success
 *     - ESP_ERR_INVALID_ARG if port invalid
 *     - ESP_ERR_INVALID_STATE if port not in output mode
 */
esp_err_t dmx_handler_blackout(uint8_t port);

/**
 * @brief Start RDM discovery
 * 
 * Initiates RDM device discovery on the specified port.
 * Port must be in DMX_MODE_RDM_MASTER mode.
 * Discovery runs asynchronously. Use callback or get_rdm_devices to retrieve results.
 * 
 * @param port Port number (1 or 2)
 * @return
 *     - ESP_OK on success (discovery started)
 *     - ESP_ERR_INVALID_ARG if port invalid
 *     - ESP_ERR_INVALID_STATE if port not in RDM master mode or discovery already running
 */
esp_err_t dmx_handler_rdm_discover(uint8_t port);

/**
 * @brief Get discovered RDM devices
 * 
 * Retrieves the list of discovered RDM devices.
 * 
 * @param port Port number (1 or 2)
 * @param devices Array to store device information
 * @param count Input: array size, Output: number of devices found
 * @return
 *     - ESP_OK on success
 *     - ESP_ERR_INVALID_ARG if port or parameters invalid
 *     - ESP_ERR_INVALID_STATE if port not in RDM master mode
 */
esp_err_t dmx_handler_get_rdm_devices(uint8_t port, rdm_device_t *devices, size_t *count);

/**
 * @brief Send RDM GET command
 * 
 * Sends an RDM GET command to a specific device.
 * 
 * @param port Port number (1 or 2)
 * @param uid Target device UID
 * @param pid Parameter ID (e.g., DMX_START_ADDRESS)
 * @param response_data Buffer for response data
 * @param response_size Input: buffer size, Output: response size
 * @return
 *     - ESP_OK on success
 *     - ESP_ERR_INVALID_ARG if parameters invalid
 *     - ESP_ERR_INVALID_STATE if port not in RDM master mode
 *     - ESP_ERR_TIMEOUT if no response
 */
esp_err_t dmx_handler_rdm_get(uint8_t port, const rdm_uid_t uid, uint16_t pid, 
                              uint8_t *response_data, size_t *response_size);

/**
 * @brief Send RDM SET command
 * 
 * Sends an RDM SET command to a specific device.
 * 
 * @param port Port number (1 or 2)
 * @param uid Target device UID
 * @param pid Parameter ID (e.g., DMX_START_ADDRESS)
 * @param data Parameter data
 * @param size Data size
 * @return
 *     - ESP_OK on success
 *     - ESP_ERR_INVALID_ARG if parameters invalid
 *     - ESP_ERR_INVALID_STATE if port not in RDM master mode
 *     - ESP_ERR_TIMEOUT if no response
 */
esp_err_t dmx_handler_rdm_set(uint8_t port, const rdm_uid_t uid, uint16_t pid, 
                              const uint8_t *data, size_t size);

/**
 * @brief Get port status
 * 
 * Retrieves current port status and statistics.
 * 
 * @param port Port number (1 or 2)
 * @param status Pointer to status structure
 * @return
 *     - ESP_OK on success
 *     - ESP_ERR_INVALID_ARG if port or status invalid
 */
esp_err_t dmx_handler_get_port_status(uint8_t port, dmx_port_status_t *status);

/**
 * @brief Register DMX receive callback
 * 
 * Registers a callback to be called when a DMX frame is received.
 * Only applicable for ports in DMX_MODE_INPUT mode.
 * 
 * @param port Port number (1 or 2)
 * @param callback Callback function
 * @param user_data User data pointer passed to callback
 * @return
 *     - ESP_OK on success
 *     - ESP_ERR_INVALID_ARG if port invalid
 */
esp_err_t dmx_handler_register_rx_callback(uint8_t port, dmx_rx_callback_t callback, 
                                           void *user_data);

/**
 * @brief Register RDM discovery callback
 * 
 * Registers a callback to be called when RDM discovery completes.
 * 
 * @param port Port number (1 or 2)
 * @param callback Callback function
 * @param user_data User data pointer passed to callback
 * @return
 *     - ESP_OK on success
 *     - ESP_ERR_INVALID_ARG if port invalid
 */
esp_err_t dmx_handler_register_discovery_callback(uint8_t port, 
                                                  rdm_discovery_callback_t callback,
                                                  void *user_data);

#ifdef __cplusplus
}
#endif

#endif // DMX_HANDLER_H
