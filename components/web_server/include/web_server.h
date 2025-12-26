#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "esp_http_server.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Web Server Module
 * 
 * Provides HTTP REST API and WebSocket server for:
 * - Device configuration (GET/POST JSON)
 * - Real-time DMX monitoring (WebSocket)
 * - Network management (WiFi scan, profiles)
 * - RDM device control
 * - System information and statistics
 * - Firmware OTA updates
 * 
 * Features:
 * - RESTful JSON API
 * - WebSocket real-time communication
 * - Static file serving (HTML/CSS/JS)
 * - Thread-safe operation
 */

// Default HTTP port
#define WEB_SERVER_DEFAULT_PORT 80

// Maximum connections
#define WEB_SERVER_MAX_CONNECTIONS 4

/**
 * @brief Web server configuration
 */
typedef struct {
    uint16_t port;                     /**< HTTP port (default: 80) */
    uint8_t max_connections;           /**< Max simultaneous connections */
    uint16_t stack_size_kb;            /**< Task stack size in KB */
    uint8_t task_priority;             /**< Task priority */
    bool enable_websocket;             /**< Enable WebSocket support */
} web_server_config_t;

/**
 * @brief WebSocket client handle
 */
typedef struct {
    int fd;                            /**< Socket file descriptor */
    char path[64];                     /**< WebSocket path */
    bool active;                       /**< Client active flag */
} ws_client_t;

/**
 * @brief Initialize web server
 * 
 * Initializes the HTTP server with default configuration.
 * Must be called after network is initialized.
 * 
 * @param config Server configuration (NULL = use defaults)
 * @return
 *     - ESP_OK on success
 *     - ESP_ERR_NO_MEM if memory allocation failed
 *     - ESP_ERR_INVALID_STATE if already initialized
 */
esp_err_t web_server_init(const web_server_config_t *config);

/**
 * @brief Start web server
 * 
 * Starts the HTTP server and begins listening for connections.
 * 
 * @return
 *     - ESP_OK on success
 *     - ESP_ERR_INVALID_STATE if not initialized
 */
esp_err_t web_server_start(void);

/**
 * @brief Stop web server
 * 
 * Stops the HTTP server and closes all connections.
 * 
 * @return
 *     - ESP_OK on success
 *     - ESP_ERR_INVALID_STATE if not running
 */
esp_err_t web_server_stop(void);

/**
 * @brief Deinitialize web server
 * 
 * Stops the server (if running) and frees all resources.
 * 
 * @return ESP_OK on success
 */
esp_err_t web_server_deinit(void);

/**
 * @brief Check if server is running
 * 
 * @return true if server is running, false otherwise
 */
bool web_server_is_running(void);

/**
 * @brief Send WebSocket message to all clients on a path
 * 
 * Broadcasts a message to all WebSocket clients connected to the specified path.
 * 
 * @param path WebSocket path (e.g., "/ws/dmx/1")
 * @param data Message data
 * @param len Message length
 * @param is_binary true for binary frame, false for text
 * @return
 *     - ESP_OK on success
 *     - ESP_ERR_INVALID_ARG if parameters invalid
 *     - ESP_ERR_INVALID_STATE if server not running
 */
esp_err_t web_server_ws_send(const char *path, const uint8_t *data, 
                             size_t len, bool is_binary);

/**
 * @brief Get number of active WebSocket clients
 * 
 * @param path WebSocket path (NULL = all clients)
 * @return Number of active clients
 */
uint8_t web_server_ws_get_client_count(const char *path);

/**
 * @brief Get server statistics
 * 
 * Retrieves HTTP server statistics including request counts and errors.
 * 
 * @param total_requests Total HTTP requests served
 * @param active_connections Currently active connections
 * @param ws_clients Active WebSocket clients
 * @return ESP_OK on success
 */
esp_err_t web_server_get_stats(uint32_t *total_requests, 
                               uint8_t *active_connections,
                               uint8_t *ws_clients);

#ifdef __cplusplus
}
#endif

#endif // WEB_SERVER_H
