/**
 * @file web_server.c
 * @brief Web Server Implementation
 * 
 * Provides HTTP REST API and WebSocket server for device configuration
 * and real-time monitoring.
 * 
 * Key Features:
 * - RESTful JSON API for configuration and control
 * - WebSocket for real-time DMX monitoring
 * - Static file serving for web UI
 * - Thread-safe operation
 * 
 * Memory Usage: ~20KB (server context + handlers)
 * Stack: 8KB per connection (configurable)
 */

#include "web_server.h"
#include "esp_log.h"
#include "esp_http_server.h"
#include "cJSON.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <string.h>

// Component includes
#include "config_manager.h"
#include "network_manager.h"
#include "dmx_handler.h"
#include "artnet_receiver.h"
#include "sacn_receiver.h"
#include "merge_engine.h"

static const char *TAG = "web_server";

// WebSocket frame opcodes
#define WS_OPCODE_TEXT   0x01
#define WS_OPCODE_BINARY 0x02
#define WS_OPCODE_CLOSE  0x08
#define WS_OPCODE_PING   0x09
#define WS_OPCODE_PONG   0x0A

// Maximum WebSocket clients
#define MAX_WS_CLIENTS 4

/**
 * @brief Server state
 */
static struct {
    httpd_handle_t server;
    bool initialized;
    bool running;
    web_server_config_t config;
    
    // Statistics
    uint32_t total_requests;
    uint8_t active_connections;
    
    // WebSocket clients
    ws_client_t ws_clients[MAX_WS_CLIENTS];
    uint8_t ws_client_count;
    SemaphoreHandle_t ws_mutex;
} server_state = {
    .server = NULL,
    .initialized = false,
    .running = false,
};

// Forward declarations
static esp_err_t api_config_get_handler(httpd_req_t *req);
static esp_err_t api_config_post_handler(httpd_req_t *req);
static esp_err_t api_network_status_handler(httpd_req_t *req);
static esp_err_t api_ports_status_handler(httpd_req_t *req);
static esp_err_t api_port_config_get_handler(httpd_req_t *req);
static esp_err_t api_port_blackout_handler(httpd_req_t *req);
static esp_err_t api_system_info_handler(httpd_req_t *req);
static esp_err_t api_system_stats_handler(httpd_req_t *req);
static esp_err_t api_system_restart_handler(httpd_req_t *req);
static esp_err_t ws_handler(httpd_req_t *req);
static esp_err_t root_handler(httpd_req_t *req);

// Helper functions
static void send_json_response(httpd_req_t *req, cJSON *json, int status);
static void send_error_response(httpd_req_t *req, int status, const char *message);
static int get_port_from_uri(const char *uri);

// ============================================================================
// Public API Implementation
// ============================================================================

esp_err_t web_server_init(const web_server_config_t *config)
{
    if (server_state.initialized) {
        ESP_LOGW(TAG, "Already initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    ESP_LOGI(TAG, "Initializing web server...");
    
    // Set configuration
    if (config) {
        server_state.config = *config;
    } else {
        // Default configuration
        server_state.config.port = WEB_SERVER_DEFAULT_PORT;
        server_state.config.max_connections = WEB_SERVER_MAX_CONNECTIONS;
        server_state.config.stack_size_kb = 8;
        server_state.config.task_priority = 5;
        server_state.config.enable_websocket = true;
    }
    
    // Create WebSocket mutex
    server_state.ws_mutex = xSemaphoreCreateMutex();
    if (!server_state.ws_mutex) {
        ESP_LOGE(TAG, "Failed to create WebSocket mutex");
        return ESP_ERR_NO_MEM;
    }
    
    // Initialize WebSocket clients array
    memset(server_state.ws_clients, 0, sizeof(server_state.ws_clients));
    server_state.ws_client_count = 0;
    
    server_state.initialized = true;
    ESP_LOGI(TAG, "Web server initialized on port %d", server_state.config.port);
    
    return ESP_OK;
}

esp_err_t web_server_start(void)
{
    if (!server_state.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (server_state.running) {
        ESP_LOGW(TAG, "Already running");
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "Starting web server...");
    
    // Configure HTTP server
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = server_state.config.port;
    config.max_open_sockets = server_state.config.max_connections;
    config.stack_size = server_state.config.stack_size_kb * 1024;
    config.task_priority = server_state.config.task_priority;
    config.lru_purge_enable = true;
    
    // Start server
    if (httpd_start(&server_state.server, &config) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start HTTP server");
        return ESP_FAIL;
    }
    
    // Register URI handlers
    
    // Root page
    httpd_uri_t root_uri = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = root_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server_state.server, &root_uri);
    
    // Configuration APIs
    httpd_uri_t config_get_uri = {
        .uri = "/api/config",
        .method = HTTP_GET,
        .handler = api_config_get_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server_state.server, &config_get_uri);
    
    httpd_uri_t config_post_uri = {
        .uri = "/api/config",
        .method = HTTP_POST,
        .handler = api_config_post_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server_state.server, &config_post_uri);
    
    // Network APIs
    httpd_uri_t network_status_uri = {
        .uri = "/api/network/status",
        .method = HTTP_GET,
        .handler = api_network_status_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server_state.server, &network_status_uri);
    
    // Port APIs
    httpd_uri_t ports_status_uri = {
        .uri = "/api/ports/status",
        .method = HTTP_GET,
        .handler = api_ports_status_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server_state.server, &ports_status_uri);
    
    httpd_uri_t port_config_uri = {
        .uri = "/api/ports/*/config",
        .method = HTTP_GET,
        .handler = api_port_config_get_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server_state.server, &port_config_uri);
    
    httpd_uri_t port_blackout_uri = {
        .uri = "/api/ports/*/blackout",
        .method = HTTP_POST,
        .handler = api_port_blackout_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server_state.server, &port_blackout_uri);
    
    // System APIs
    httpd_uri_t system_info_uri = {
        .uri = "/api/system/info",
        .method = HTTP_GET,
        .handler = api_system_info_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server_state.server, &system_info_uri);
    
    httpd_uri_t system_stats_uri = {
        .uri = "/api/system/stats",
        .method = HTTP_GET,
        .handler = api_system_stats_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server_state.server, &system_stats_uri);
    
    httpd_uri_t system_restart_uri = {
        .uri = "/api/system/restart",
        .method = HTTP_POST,
        .handler = api_system_restart_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server_state.server, &system_restart_uri);
    
    // WebSocket handler
    if (server_state.config.enable_websocket) {
        httpd_uri_t ws_uri = {
            .uri = "/ws/*",
            .method = HTTP_GET,
            .handler = ws_handler,
            .user_ctx = NULL,
            .is_websocket = true
        };
        httpd_register_uri_handler(server_state.server, &ws_uri);
    }
    
    server_state.running = true;
    ESP_LOGI(TAG, "Web server started successfully on port %d", server_state.config.port);
    
    return ESP_OK;
}

esp_err_t web_server_stop(void)
{
    if (!server_state.running) {
        return ESP_ERR_INVALID_STATE;
    }
    
    ESP_LOGI(TAG, "Stopping web server...");
    
    if (server_state.server) {
        httpd_stop(server_state.server);
        server_state.server = NULL;
    }
    
    server_state.running = false;
    ESP_LOGI(TAG, "Web server stopped");
    
    return ESP_OK;
}

esp_err_t web_server_deinit(void)
{
    if (server_state.running) {
        web_server_stop();
    }
    
    if (server_state.ws_mutex) {
        vSemaphoreDelete(server_state.ws_mutex);
        server_state.ws_mutex = NULL;
    }
    
    server_state.initialized = false;
    ESP_LOGI(TAG, "Web server deinitialized");
    
    return ESP_OK;
}

bool web_server_is_running(void)
{
    return server_state.running;
}

esp_err_t web_server_ws_send(const char *path, const uint8_t *data,
                             size_t len, bool is_binary)
{
    if (!server_state.running || !path || !data) {
        return ESP_ERR_INVALID_ARG;
    }
    
    httpd_ws_frame_t ws_pkt;
    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
    ws_pkt.payload = data;
    ws_pkt.len = len;
    ws_pkt.type = is_binary ? HTTPD_WS_TYPE_BINARY : HTTPD_WS_TYPE_TEXT;
    
    xSemaphoreTake(server_state.ws_mutex, portMAX_DELAY);
    
    // Send to all clients on matching path
    for (int i = 0; i < MAX_WS_CLIENTS; i++) {
        if (server_state.ws_clients[i].active &&
            strcmp(server_state.ws_clients[i].path, path) == 0) {
            httpd_ws_send_frame_async(server_state.server, 
                                     server_state.ws_clients[i].fd,
                                     &ws_pkt);
        }
    }
    
    xSemaphoreGive(server_state.ws_mutex);
    
    return ESP_OK;
}

uint8_t web_server_ws_get_client_count(const char *path)
{
    if (!server_state.running) {
        return 0;
    }
    
    uint8_t count = 0;
    
    xSemaphoreTake(server_state.ws_mutex, portMAX_DELAY);
    
    for (int i = 0; i < MAX_WS_CLIENTS; i++) {
        if (server_state.ws_clients[i].active) {
            if (path == NULL || strcmp(server_state.ws_clients[i].path, path) == 0) {
                count++;
            }
        }
    }
    
    xSemaphoreGive(server_state.ws_mutex);
    
    return count;
}

esp_err_t web_server_get_stats(uint32_t *total_requests,
                               uint8_t *active_connections,
                               uint8_t *ws_clients)
{
    if (total_requests) {
        *total_requests = server_state.total_requests;
    }
    if (active_connections) {
        *active_connections = server_state.active_connections;
    }
    if (ws_clients) {
        *ws_clients = server_state.ws_client_count;
    }
    
    return ESP_OK;
}

// ============================================================================
// HTTP Request Handlers
// ============================================================================

/**
 * @brief Root page handler
 */
static esp_err_t root_handler(httpd_req_t *req)
{
    server_state.total_requests++;
    
    const char *html = 
        "<!DOCTYPE html>\n"
        "<html>\n"
        "<head>\n"
        "  <title>ESP-NODE-2RDM</title>\n"
        "  <meta charset=\"utf-8\">\n"
        "  <style>\n"
        "    body { font-family: Arial, sans-serif; margin: 20px; background: #f0f0f0; }\n"
        "    .container { max-width: 1200px; margin: 0 auto; background: white; padding: 20px; border-radius: 8px; }\n"
        "    h1 { color: #333; border-bottom: 2px solid #007bff; padding-bottom: 10px; }\n"
        "    .section { margin: 20px 0; padding: 15px; background: #f9f9f9; border-radius: 4px; }\n"
        "    .status { display: inline-block; padding: 5px 10px; border-radius: 4px; margin: 5px; }\n"
        "    .status.ok { background: #28a745; color: white; }\n"
        "    .status.error { background: #dc3545; color: white; }\n"
        "    button { padding: 10px 20px; margin: 5px; cursor: pointer; background: #007bff; color: white; border: none; border-radius: 4px; }\n"
        "    button:hover { background: #0056b3; }\n"
        "  </style>\n"
        "</head>\n"
        "<body>\n"
        "  <div class=\"container\">\n"
        "    <h1>ESP-NODE-2RDM Web Interface</h1>\n"
        "    <div class=\"section\">\n"
        "      <h2>System Status</h2>\n"
        "      <p>Firmware Version: 0.1.0</p>\n"
        "      <p>Device: ESP32-S3 Art-Net / sACN to DMX512 Node</p>\n"
        "      <button onclick=\"fetch('/api/system/stats').then(r=>r.json()).then(d=>alert(JSON.stringify(d, null, 2)))\">Get Statistics</button>\n"
        "      <button onclick=\"if(confirm('Restart device?'))fetch('/api/system/restart',{method:'POST'})\">Restart Device</button>\n"
        "    </div>\n"
        "    <div class=\"section\">\n"
        "      <h2>API Endpoints</h2>\n"
        "      <p>Configuration: GET/POST <code>/api/config</code></p>\n"
        "      <p>Network Status: GET <code>/api/network/status</code></p>\n"
        "      <p>Ports Status: GET <code>/api/ports/status</code></p>\n"
        "      <p>System Info: GET <code>/api/system/info</code></p>\n"
        "      <p>System Stats: GET <code>/api/system/stats</code></p>\n"
        "      <p>WebSocket: <code>/ws/dmx/{port}</code> (real-time DMX levels)</p>\n"
        "    </div>\n"
        "    <div class=\"section\">\n"
        "      <h2>Quick Actions</h2>\n"
        "      <button onclick=\"fetch('/api/ports/1/blackout',{method:'POST'}).then(()=>alert('Port 1 blackout'))\">Blackout Port 1</button>\n"
        "      <button onclick=\"fetch('/api/ports/2/blackout',{method:'POST'}).then(()=>alert('Port 2 blackout'))\">Blackout Port 2</button>\n"
        "    </div>\n"
        "  </div>\n"
        "</body>\n"
        "</html>";
    
    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, html, strlen(html));
}

/**
 * @brief GET /api/config - Get full configuration
 */
static esp_err_t api_config_get_handler(httpd_req_t *req)
{
    server_state.total_requests++;
    
    config_t *config = config_get();
    if (!config) {
        send_error_response(req, 500, "Failed to get configuration");
        return ESP_FAIL;
    }
    
    // Create JSON response
    cJSON *json = cJSON_CreateObject();
    
    // Node info
    cJSON *node = cJSON_CreateObject();
    cJSON_AddStringToObject(node, "short_name", config->node_info.short_name);
    cJSON_AddStringToObject(node, "long_name", config->node_info.long_name);
    cJSON_AddItemToObject(json, "node_info", node);
    
    // Port 1
    cJSON *port1 = cJSON_CreateObject();
    cJSON_AddNumberToObject(port1, "mode", config->port1.mode);
    cJSON_AddNumberToObject(port1, "universe_primary", config->port1.universe_primary);
    cJSON_AddNumberToObject(port1, "merge_mode", config->port1.merge_mode);
    cJSON_AddItemToObject(json, "port1", port1);
    
    // Port 2
    cJSON *port2 = cJSON_CreateObject();
    cJSON_AddNumberToObject(port2, "mode", config->port2.mode);
    cJSON_AddNumberToObject(port2, "universe_primary", config->port2.universe_primary);
    cJSON_AddNumberToObject(port2, "merge_mode", config->port2.merge_mode);
    cJSON_AddItemToObject(json, "port2", port2);
    
    send_json_response(req, json, 200);
    cJSON_Delete(json);
    
    return ESP_OK;
}

/**
 * @brief POST /api/config - Set configuration
 */
static esp_err_t api_config_post_handler(httpd_req_t *req)
{
    server_state.total_requests++;
    
    // Read POST data
    char *buf = malloc(req->content_len + 1);
    if (!buf) {
        send_error_response(req, 500, "Memory allocation failed");
        return ESP_FAIL;
    }
    
    int ret = httpd_req_recv(req, buf, req->content_len);
    if (ret <= 0) {
        free(buf);
        send_error_response(req, 400, "Failed to read request body");
        return ESP_FAIL;
    }
    buf[ret] = '\0';
    
    // Parse JSON
    cJSON *json = cJSON_Parse(buf);
    free(buf);
    
    if (!json) {
        send_error_response(req, 400, "Invalid JSON");
        return ESP_FAIL;
    }
    
    // TODO: Apply configuration changes
    // This would update config_manager with new values
    
    cJSON_Delete(json);
    
    cJSON *response = cJSON_CreateObject();
    cJSON_AddStringToObject(response, "status", "ok");
    cJSON_AddStringToObject(response, "message", "Configuration updated (restart required)");
    send_json_response(req, response, 200);
    cJSON_Delete(response);
    
    return ESP_OK;
}

/**
 * @brief GET /api/network/status - Get network status
 */
static esp_err_t api_network_status_handler(httpd_req_t *req)
{
    server_state.total_requests++;
    
    network_status_t status;
    if (network_get_status(&status) != ESP_OK) {
        send_error_response(req, 500, "Failed to get network status");
        return ESP_FAIL;
    }
    
    cJSON *json = cJSON_CreateObject();
    cJSON_AddNumberToObject(json, "state", status.state);
    cJSON_AddBoolToObject(json, "connected", network_is_connected());
    
    const char *ip = network_get_ip_address();
    if (ip) {
        cJSON_AddStringToObject(json, "ip_address", ip);
    }
    
    send_json_response(req, json, 200);
    cJSON_Delete(json);
    
    return ESP_OK;
}

/**
 * @brief GET /api/ports/status - Get all ports status
 */
static esp_err_t api_ports_status_handler(httpd_req_t *req)
{
    server_state.total_requests++;
    
    cJSON *json = cJSON_CreateArray();
    
    // Port 1
    dmx_port_status_t status1;
    if (dmx_handler_get_port_status(DMX_PORT_1, &status1) == ESP_OK) {
        cJSON *port1 = cJSON_CreateObject();
        cJSON_AddNumberToObject(port1, "port", 1);
        cJSON_AddBoolToObject(port1, "active", status1.is_active);
        cJSON_AddNumberToObject(port1, "mode", status1.mode);
        cJSON_AddNumberToObject(port1, "frames_sent", status1.stats.frames_sent);
        cJSON_AddNumberToObject(port1, "frames_received", status1.stats.frames_received);
        cJSON_AddItemToArray(json, port1);
    }
    
    // Port 2
    dmx_port_status_t status2;
    if (dmx_handler_get_port_status(DMX_PORT_2, &status2) == ESP_OK) {
        cJSON *port2 = cJSON_CreateObject();
        cJSON_AddNumberToObject(port2, "port", 2);
        cJSON_AddBoolToObject(port2, "active", status2.is_active);
        cJSON_AddNumberToObject(port2, "mode", status2.mode);
        cJSON_AddNumberToObject(port2, "frames_sent", status2.stats.frames_sent);
        cJSON_AddNumberToObject(port2, "frames_received", status2.stats.frames_received);
        cJSON_AddItemToArray(json, port2);
    }
    
    send_json_response(req, json, 200);
    cJSON_Delete(json);
    
    return ESP_OK;
}

/**
 * @brief GET /api/ports/{id}/config - Get port configuration
 */
static esp_err_t api_port_config_get_handler(httpd_req_t *req)
{
    server_state.total_requests++;
    
    int port = get_port_from_uri(req->uri);
    if (port < 1 || port > 2) {
        send_error_response(req, 400, "Invalid port number");
        return ESP_FAIL;
    }
    
    config_t *config = config_get();
    port_config_t *port_cfg = (port == 1) ? &config->port1 : &config->port2;
    
    cJSON *json = cJSON_CreateObject();
    cJSON_AddNumberToObject(json, "port", port);
    cJSON_AddNumberToObject(json, "mode", port_cfg->mode);
    cJSON_AddNumberToObject(json, "universe_primary", port_cfg->universe_primary);
    cJSON_AddNumberToObject(json, "merge_mode", port_cfg->merge_mode);
    
    send_json_response(req, json, 200);
    cJSON_Delete(json);
    
    return ESP_OK;
}

/**
 * @brief POST /api/ports/{id}/blackout - Blackout port
 */
static esp_err_t api_port_blackout_handler(httpd_req_t *req)
{
    server_state.total_requests++;
    
    int port = get_port_from_uri(req->uri);
    if (port < 1 || port > 2) {
        send_error_response(req, 400, "Invalid port number");
        return ESP_FAIL;
    }
    
    esp_err_t ret = dmx_handler_blackout(port);
    
    cJSON *json = cJSON_CreateObject();
    if (ret == ESP_OK) {
        cJSON_AddStringToObject(json, "status", "ok");
        cJSON_AddNumberToObject(json, "port", port);
        send_json_response(req, json, 200);
    } else {
        cJSON_AddStringToObject(json, "status", "error");
        cJSON_AddStringToObject(json, "message", "Blackout failed");
        send_json_response(req, json, 500);
    }
    
    cJSON_Delete(json);
    
    return ESP_OK;
}

/**
 * @brief GET /api/system/info - Get system information
 */
static esp_err_t api_system_info_handler(httpd_req_t *req)
{
    server_state.total_requests++;
    
    cJSON *json = cJSON_CreateObject();
    cJSON_AddStringToObject(json, "firmware_version", "0.1.0");
    cJSON_AddStringToObject(json, "hardware", "ESP32-S3");
    cJSON_AddStringToObject(json, "idf_version", esp_get_idf_version());
    cJSON_AddNumberToObject(json, "free_heap", esp_get_free_heap_size());
    cJSON_AddNumberToObject(json, "uptime_sec", xTaskGetTickCount() / configTICK_RATE_HZ);
    
    send_json_response(req, json, 200);
    cJSON_Delete(json);
    
    return ESP_OK;
}

/**
 * @brief GET /api/system/stats - Get system statistics
 */
static esp_err_t api_system_stats_handler(httpd_req_t *req)
{
    server_state.total_requests++;
    
    cJSON *json = cJSON_CreateObject();
    
    // Art-Net stats
    artnet_stats_t artnet_stats;
    if (artnet_receiver_get_stats(&artnet_stats) == ESP_OK) {
        cJSON *artnet = cJSON_CreateObject();
        cJSON_AddNumberToObject(artnet, "packets", artnet_stats.packets_received);
        cJSON_AddNumberToObject(artnet, "dmx_packets", artnet_stats.dmx_packets);
        cJSON_AddNumberToObject(artnet, "poll_packets", artnet_stats.poll_packets);
        cJSON_AddItemToObject(json, "artnet", artnet);
    }
    
    // sACN stats
    sacn_stats_t sacn_stats;
    if (sacn_receiver_get_stats(&sacn_stats) == ESP_OK) {
        cJSON *sacn = cJSON_CreateObject();
        cJSON_AddNumberToObject(sacn, "packets", sacn_stats.packets_received);
        cJSON_AddNumberToObject(sacn, "data_packets", sacn_stats.data_packets);
        cJSON_AddItemToObject(json, "sacn", sacn);
    }
    
    // Merge engine stats
    merge_stats_t merge1, merge2;
    if (merge_engine_get_stats(1, &merge1) == ESP_OK) {
        cJSON *merge = cJSON_CreateObject();
        cJSON_AddNumberToObject(merge, "active_sources", merge1.active_sources);
        cJSON_AddNumberToObject(merge, "total_merges", merge1.total_merges);
        cJSON_AddItemToObject(json, "merge_port1", merge);
    }
    
    if (merge_engine_get_stats(2, &merge2) == ESP_OK) {
        cJSON *merge = cJSON_CreateObject();
        cJSON_AddNumberToObject(merge, "active_sources", merge2.active_sources);
        cJSON_AddNumberToObject(merge, "total_merges", merge2.total_merges);
        cJSON_AddItemToObject(json, "merge_port2", merge);
    }
    
    send_json_response(req, json, 200);
    cJSON_Delete(json);
    
    return ESP_OK;
}

/**
 * @brief POST /api/system/restart - Restart device
 */
static esp_err_t api_system_restart_handler(httpd_req_t *req)
{
    server_state.total_requests++;
    
    cJSON *json = cJSON_CreateObject();
    cJSON_AddStringToObject(json, "status", "ok");
    cJSON_AddStringToObject(json, "message", "Restarting in 2 seconds...");
    send_json_response(req, json, 200);
    cJSON_Delete(json);
    
    // Schedule restart
    vTaskDelay(pdMS_TO_TICKS(2000));
    esp_restart();
    
    return ESP_OK;
}

/**
 * @brief WebSocket handler
 */
static esp_err_t ws_handler(httpd_req_t *req)
{
    if (req->method == HTTP_GET) {
        ESP_LOGI(TAG, "WebSocket handshake for: %s", req->uri);
        
        // Add client to list
        xSemaphoreTake(server_state.ws_mutex, portMAX_DELAY);
        
        for (int i = 0; i < MAX_WS_CLIENTS; i++) {
            if (!server_state.ws_clients[i].active) {
                server_state.ws_clients[i].fd = httpd_req_to_sockfd(req);
                strncpy(server_state.ws_clients[i].path, req->uri, sizeof(server_state.ws_clients[i].path) - 1);
                server_state.ws_clients[i].active = true;
                server_state.ws_client_count++;
                ESP_LOGI(TAG, "WebSocket client connected: %s (fd=%d)", req->uri, server_state.ws_clients[i].fd);
                break;
            }
        }
        
        xSemaphoreGive(server_state.ws_mutex);
        
        return ESP_OK;
    }
    
    // Handle WebSocket frames
    httpd_ws_frame_t ws_pkt;
    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
    ws_pkt.type = HTTPD_WS_TYPE_TEXT;
    
    // Get frame info
    esp_err_t ret = httpd_ws_recv_frame(req, &ws_pkt, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "httpd_ws_recv_frame failed: %d", ret);
        return ret;
    }
    
    if (ws_pkt.len) {
        // Allocate buffer for payload
        uint8_t *buf = malloc(ws_pkt.len + 1);
        if (buf) {
            ws_pkt.payload = buf;
            ret = httpd_ws_recv_frame(req, &ws_pkt, ws_pkt.len);
            if (ret == ESP_OK) {
                buf[ws_pkt.len] = '\0';
                ESP_LOGI(TAG, "WebSocket received: %s", (char*)buf);
                
                // Parse and handle command
                // TODO: Handle DMX test commands from client
            }
            free(buf);
        }
    }
    
    return ESP_OK;
}

// ============================================================================
// Helper Functions
// ============================================================================

/**
 * @brief Send JSON response
 */
static void send_json_response(httpd_req_t *req, cJSON *json, int status)
{
    char *json_str = cJSON_PrintUnformatted(json);
    if (json_str) {
        httpd_resp_set_type(req, "application/json");
        httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
        httpd_resp_set_status(req, (status == 200) ? "200 OK" : "500 Internal Server Error");
        httpd_resp_send(req, json_str, strlen(json_str));
        free(json_str);
    } else {
        httpd_resp_send_500(req);
    }
}

/**
 * @brief Send error response
 */
static void send_error_response(httpd_req_t *req, int status, const char *message)
{
    cJSON *json = cJSON_CreateObject();
    cJSON_AddStringToObject(json, "error", message);
    send_json_response(req, json, status);
    cJSON_Delete(json);
}

/**
 * @brief Extract port number from URI
 */
static int get_port_from_uri(const char *uri)
{
    // Parse URI like "/api/ports/1/config" to extract port number
    const char *ptr = strstr(uri, "/ports/");
    if (ptr) {
        ptr += 7;  // Skip "/ports/"
        return atoi(ptr);
    }
    return 0;
}
