# THIẾT KẾ MODULE: Web Server

**Dự án:** Artnet-Node-2RDM  
**Module:** HTTP Web Server & WebSocket  
**Phiên bản:** 1.0  
**Ngày tạo:** 25/12/2025

---

## 1. Mục đích

Module Web Server cung cấp giao diện web để:
- Cấu hình thiết bị
- Monitor trạng thái real-time
- Test DMX channels
- RDM device control
- WiFi management
- Firmware update (OTA)

---

## 2. Kiến trúc Module

### 2.1. Technology Stack

- **HTTP Server**: ESP-IDF HTTP Server component
- **WebSocket**: For real-time communication
- **Static Files**: Served from LittleFS
- **API**: RESTful JSON API

---

## 3. API Endpoints

### 3.1. Static Pages

| Path | Method | Description |
|------|--------|-------------|
| `/` | GET | Main HTML page |
| `/script.js` | GET | JavaScript file |
| `/style.css` | GET | CSS file |

### 3.2. Configuration APIs

| Path | Method | Description | Request Body | Response |
|------|--------|-------------|--------------|----------|
| `/api/config` | GET | Get full config | - | `system_config_t` JSON |
| `/api/config` | POST | Set full config | `system_config_t` JSON | Status |
| `/api/config/reset` | POST | Reset to defaults | - | Status |

### 3.3. Network APIs

| Path | Method | Description | Request Body | Response |
|------|--------|-------------|--------------|----------|
| `/api/network/status` | GET | Network status | - | `network_status_t` JSON |
| `/api/wifi/scan` | GET | Scan WiFi networks | - | Array of APs |
| `/api/wifi/profiles` | GET | Get WiFi profiles | - | Array of profiles |
| `/api/wifi/profiles` | POST | Add WiFi profile | `wifi_profile_t` JSON | Status |
| `/api/wifi/profiles/{id}` | DELETE | Delete profile | - | Status |
| `/api/ap/config` | GET | Get AP config | - | AP config JSON |
| `/api/ap/config` | POST | Set AP config | AP config JSON | Status |
| `/api/ethernet/config` | GET | Get Ethernet config | - | Ethernet config JSON |
| `/api/ethernet/config` | POST | Set Ethernet config | Ethernet config JSON | Status |

### 3.4. DMX/RDM Port APIs

| Path | Method | Description | Request Body | Response |
|------|--------|-------------|--------------|----------|
| `/api/ports/status` | GET | Get all ports status | - | Array of port status |
| `/api/ports/{id}/config` | GET | Get port config | - | Port config JSON |
| `/api/ports/{id}/config` | POST | Set port config | Port config JSON | Status |
| `/api/ports/{id}/levels` | GET | Get DMX levels | - | 512-byte array |
| `/api/ports/{id}/blackout` | POST | Force blackout | - | Status |

### 3.5. Protocol APIs

| Path | Method | Description | Request Body | Response |
|------|--------|-------------|--------------|----------|
| `/api/protocol/config` | GET | Get protocol config | - | Protocol config JSON |
| `/api/protocol/config` | POST | Set protocol config | Protocol config JSON | Status |
| `/api/merge/config` | GET | Get merge config | - | Merge config JSON |
| `/api/merge/config` | POST | Set merge config | Merge config JSON | Status |

### 3.6. RDM APIs

| Path | Method | Description | Request Body | Response |
|------|--------|-------------|--------------|----------|
| `/api/rdm/{port}/discover` | POST | Start RDM discovery | - | Status |
| `/api/rdm/{port}/devices` | GET | Get RDM devices | - | Array of devices |
| `/api/rdm/{port}/get` | POST | RDM GET command | `{uid, pid}` | RDM response |
| `/api/rdm/{port}/set` | POST | RDM SET command | `{uid, pid, data}` | Status |

### 3.7. System APIs

| Path | Method | Description | Request Body | Response |
|------|--------|-------------|--------------|----------|
| `/api/system/info` | GET | System information | - | System info JSON |
| `/api/system/stats` | GET | Statistics | - | Stats JSON |
| `/api/system/restart` | POST | Restart device | - | Status |
| `/api/system/factory-reset` | POST | Factory reset | - | Status |

### 3.8. Firmware Update (OTA)

| Path | Method | Description | Request Body | Response |
|------|--------|-------------|--------------|----------|
| `/api/firmware/version` | GET | Current version | - | Version string |
| `/api/firmware/upload` | POST | Upload firmware | Binary file | Status |

---

## 4. WebSocket Endpoints

### 4.1. Real-time DMX Monitoring

**Path:** `/ws/dmx/{port}`

**Messages from server:**
```json
{
  "type": "dmx_levels",
  "port": 1,
  "data": [0, 255, 128, ...],  // 512 bytes
  "timestamp": 1234567890
}
```

**Messages from client:**
```json
{
  "type": "dmx_test",
  "port": 1,
  "channel": 1,
  "value": 255
}
```

### 4.2. Real-time Status Monitoring

**Path:** `/ws/status`

**Messages from server:**
```json
{
  "type": "network_status",
  "state": "ETHERNET_CONNECTED",
  "ip": "192.168.1.100",
  "packets_per_sec": 44
}

{
  "type": "port_status",
  "port": 1,
  "frames_sent": 1000,
  "active_sources": 2
}
```

### 4.3. RDM Events

**Path:** `/ws/rdm/{port}`

**Messages from server:**
```json
{
  "type": "rdm_device_found",
  "port": 1,
  "uid": "1234:56789ABC",
  "model": "LED Fixture"
}

{
  "type": "rdm_response",
  "port": 1,
  "uid": "1234:56789ABC",
  "pid": 240,
  "data": [0, 1]
}
```

---

## 5. Implementation

### 5.1. HTTP Server Setup

```c
static httpd_handle_t server = NULL;

esp_err_t webserver_init(void) {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 80;
    config.max_uri_handlers = 32;
    config.max_open_sockets = 7;
    config.lru_purge_enable = true;
    config.stack_size = 8192;
    
    if (httpd_start(&server, &config) != ESP_OK) {
        return ESP_FAIL;
    }
    
    // Register handlers
    register_static_handlers();
    register_api_handlers();
    register_websocket_handlers();
    
    return ESP_OK;
}
```

### 5.2. Static File Handler

```c
static esp_err_t static_file_handler(httpd_req_t *req) {
    const char *filepath = req->uri;
    
    // Default to index.html
    if (strcmp(filepath, "/") == 0) {
        filepath = "/index.html";
    }
    
    // Read from LittleFS
    FILE *f = fopen(filepath, "r");
    if (!f) {
        httpd_resp_send_404(req);
        return ESP_FAIL;
    }
    
    // Set content type
    const char *content_type = get_content_type(filepath);
    httpd_resp_set_type(req, content_type);
    
    // Enable caching
    httpd_resp_set_hdr(req, "Cache-Control", "public, max-age=86400");
    
    // Send file
    char buffer[1024];
    size_t read_len;
    while ((read_len = fread(buffer, 1, sizeof(buffer), f)) > 0) {
        httpd_resp_send_chunk(req, buffer, read_len);
    }
    
    fclose(f);
    httpd_resp_send_chunk(req, NULL, 0);  // End response
    
    return ESP_OK;
}
```

### 5.3. JSON API Handler Example

```c
static esp_err_t api_config_get_handler(httpd_req_t *req) {
    const system_config_t *config = config_get();
    
    // Serialize to JSON
    cJSON *root = cJSON_CreateObject();
    config_to_json(config, root);
    
    char *json_str = cJSON_PrintUnformatted(root);
    
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_sendstr(req, json_str);
    
    free(json_str);
    cJSON_Delete(root);
    
    return ESP_OK;
}

static esp_err_t api_config_post_handler(httpd_req_t *req) {
    char buffer[2048];
    int ret = httpd_req_recv(req, buffer, sizeof(buffer));
    if (ret <= 0) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    buffer[ret] = '\0';
    
    // Parse JSON
    cJSON *root = cJSON_Parse(buffer);
    if (!root) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_FAIL;
    }
    
    // Update config
    system_config_t new_config;
    if (json_to_config(root, &new_config) != ESP_OK) {
        cJSON_Delete(root);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid config");
        return ESP_FAIL;
    }
    
    config_set(&new_config);
    config_save();
    
    cJSON_Delete(root);
    
    httpd_resp_sendstr(req, "{\"status\":\"ok\"}");
    return ESP_OK;
}
```

### 5.4. WebSocket Handler

```c
static esp_err_t ws_dmx_handler(httpd_req_t *req) {
    if (req->method == HTTP_GET) {
        // WebSocket handshake
        return ESP_OK;
    }
    
    httpd_ws_frame_t ws_pkt;
    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
    ws_pkt.type = HTTPD_WS_TYPE_TEXT;
    
    // Receive frame
    esp_err_t ret = httpd_ws_recv_frame(req, &ws_pkt, 0);
    if (ret != ESP_OK) {
        return ret;
    }
    
    if (ws_pkt.len) {
        uint8_t *buf = malloc(ws_pkt.len + 1);
        ws_pkt.payload = buf;
        ret = httpd_ws_recv_frame(req, &ws_pkt, ws_pkt.len);
        
        if (ret == ESP_OK) {
            // Parse message
            buf[ws_pkt.len] = '\0';
            handle_ws_dmx_message(req, (char*)buf);
        }
        
        free(buf);
    }
    
    return ESP_OK;
}

static void handle_ws_dmx_message(httpd_req_t *req, const char *message) {
    cJSON *root = cJSON_Parse(message);
    if (!root) return;
    
    const char *type = cJSON_GetObjectItem(root, "type")->valuestring;
    
    if (strcmp(type, "dmx_test") == 0) {
        int port = cJSON_GetObjectItem(root, "port")->valueint;
        int channel = cJSON_GetObjectItem(root, "channel")->valueint;
        int value = cJSON_GetObjectItem(root, "value")->valueint;
        
        // Set DMX channel
        dmx_set_channel(port, channel, value);
    }
    
    cJSON_Delete(root);
}
```

### 5.5. WebSocket Broadcast

```c
static void ws_broadcast_dmx_levels(uint8_t port, const uint8_t *levels) {
    // Build JSON message
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "type", "dmx_levels");
    cJSON_AddNumberToObject(root, "port", port);
    
    cJSON *data_array = cJSON_CreateIntArray((int*)levels, 512);
    cJSON_AddItemToObject(root, "data", data_array);
    
    char *json_str = cJSON_PrintUnformatted(root);
    
    // Send to all connected clients
    httpd_ws_frame_t ws_pkt;
    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
    ws_pkt.type = HTTPD_WS_TYPE_TEXT;
    ws_pkt.payload = (uint8_t*)json_str;
    ws_pkt.len = strlen(json_str);
    
    for (int i = 0; i < ws_client_count; i++) {
        httpd_ws_send_frame_async(server, ws_clients[i], &ws_pkt);
    }
    
    free(json_str);
    cJSON_Delete(root);
}
```

---

## 6. Security

### 6.1. Authentication (Optional)

```c
// Basic HTTP authentication
static esp_err_t basic_auth_handler(httpd_req_t *req) {
    char *auth_header = NULL;
    size_t auth_len = httpd_req_get_hdr_value_len(req, "Authorization") + 1;
    
    if (auth_len > 1) {
        auth_header = malloc(auth_len);
        httpd_req_get_hdr_value_str(req, "Authorization", auth_header, auth_len);
        
        // Check credentials
        if (verify_credentials(auth_header)) {
            free(auth_header);
            return ESP_OK;
        }
        free(auth_header);
    }
    
    // Request authentication
    httpd_resp_set_hdr(req, "WWW-Authenticate", "Basic realm=\"ArtNet Node\"");
    httpd_resp_send_err(req, HTTPD_401_UNAUTHORIZED, "Unauthorized");
    return ESP_FAIL;
}
```

### 6.2. CORS Headers

```c
static void set_cors_headers(httpd_req_t *req) {
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Methods", 
                      "GET, POST, PUT, DELETE, OPTIONS");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Headers", 
                      "Content-Type, Authorization");
}
```

---

## 7. Threading Model

- **HTTP Server Task** (managed by ESP-IDF)
  - Handles HTTP requests
  - Manages WebSocket connections

- **WebSocket Broadcast Task** (Priority: 5, Core: 0, Stack: 4KB)
  - Periodically broadcast status updates
  - Send DMX levels (max 10 Hz to avoid flooding)

---

## 8. Error Handling

| Error | Action |
|-------|--------|
| Server start fail | Log error, retry |
| Handler registration fail | Log warning |
| JSON parse error | Return 400 Bad Request |
| Invalid parameter | Return 400 Bad Request |
| Internal error | Return 500 Internal Server Error |

---

## 9. Dependencies

- **ESP-IDF Components**:
  - esp_http_server
  - cJSON
- **Internal Modules**:
  - Storage (static files)
  - Configuration
  - Network
  - DMX Handler
  - RDM Handler
  - Merge Engine

---

## 10. Testing Points

1. GET static pages
2. GET/POST config APIs
3. WiFi scan API
4. DMX levels API
5. RDM discovery API
6. WebSocket connection
7. WebSocket message exchange
8. WebSocket broadcast
9. CORS headers
10. Error responses (400, 404, 500)
11. Concurrent connections
12. Large JSON handling

---

## 11. Memory Usage

- HTTP server: ~20 KB
- WebSocket buffers: 8 KB
- JSON buffers: 4 KB
- **Total: ~32 KB**

---

## 12. Performance

- Max concurrent connections: 7
- Request latency: < 50ms
- WebSocket update rate: 10 Hz
- JSON parse time: < 5ms

---

## 13. Content Types

```c
static const char* get_content_type(const char *path) {
    if (strstr(path, ".html")) return "text/html";
    if (strstr(path, ".css")) return "text/css";
    if (strstr(path, ".js")) return "application/javascript";
    if (strstr(path, ".json")) return "application/json";
    if (strstr(path, ".png")) return "image/png";
    if (strstr(path, ".jpg")) return "image/jpeg";
    if (strstr(path, ".ico")) return "image/x-icon";
    return "text/plain";
}
```
