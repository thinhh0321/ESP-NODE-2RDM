# THIẾT KẾ MODULE: Configuration

**Dự án:** Artnet-Node-2RDM  
**Module:** Configuration Management  
**Phiên bản:** 1.0  
**Ngày tạo:** 25/12/2025

---

## 1. Mục đích

Module Configuration chịu trách nhiệm quản lý toàn bộ thông số cấu hình của hệ thống, bao gồm:
- Thông số mạng (Ethernet, WiFi)
- Cấu hình cổng DMX/RDM
- Cấu hình giao thức (Art-Net, sACN)
- Cấu hình Merge Engine
- Thông tin thiết bị (node name, ID)

---

## 2. Kiến trúc Module

### 2.1. Cấu trúc dữ liệu chính

```c
typedef struct {
    // Network Configuration
    bool use_ethernet;
    wifi_profile_t wifi_profiles[5];
    char ap_ssid[32];
    char ap_password[64];
    uint8_t ap_channel;
    
    // IP Configuration
    ip_config_t ethernet_ip;
    ip_config_t wifi_sta_ip;
    ip_config_t wifi_ap_ip;
    
    // DMX Port Configuration
    dmx_mode_t port1_mode;
    dmx_mode_t port2_mode;
    uint16_t universe_primary_port1;
    uint16_t universe_primary_port2;
    int16_t universe_secondary_port1;
    int16_t universe_secondary_port2;
    int16_t universe_offset_port1;
    int16_t universe_offset_port2;
    
    // Protocol Configuration
    protocol_mode_t protocol_mode_port1;
    protocol_mode_t protocol_mode_port2;
    
    // Merge Configuration
    merge_mode_t merge_mode_port1;
    merge_mode_t merge_mode_port2;
    uint8_t merge_timeout_seconds;
    
    // RDM Configuration
    bool rdm_enabled_port1;
    bool rdm_enabled_port2;
    
    // Node Information
    char node_short_name[18];
    char node_long_name[64];
    uint8_t node_mac[6];
    
} system_config_t;
```

### 2.2. Enumerations

```c
typedef enum {
    DMX_OUT = 0,
    DMX_IN = 1,
    RDM_MASTER = 2,
    RDM_RESPONDER = 3,
    DISABLED = 4
} dmx_mode_t;

typedef enum {
    ARTNET_ONLY = 0,
    SACN_ONLY = 1,
    ARTNET_PRIORITY = 2,
    SACN_PRIORITY = 3,
    MERGE_BOTH = 4
} protocol_mode_t;

typedef enum {
    HTP = 0,      // Highest Takes Precedence
    LTP = 1,      // Lowest Takes Precedence
    LAST = 2,     // Last packet wins
    BACKUP = 3,   // Primary + Backup source
    DISABLE = 4   // No merge
} merge_mode_t;
```

---

## 3. API Public

### 3.1. Khởi tạo và Load Configuration

```c
/**
 * Khởi tạo module Configuration
 * @return ESP_OK nếu thành công
 */
esp_err_t config_init(void);

/**
 * Load cấu hình từ storage (NVS hoặc LittleFS)
 * @return ESP_OK nếu thành công
 */
esp_err_t config_load(void);

/**
 * Load cấu hình mặc định
 */
void config_load_defaults(void);
```

### 3.2. Save Configuration

```c
/**
 * Lưu cấu hình vào storage
 * @return ESP_OK nếu thành công
 */
esp_err_t config_save(void);

/**
 * Lưu cấu hình vào file JSON
 * @param filename Tên file
 * @return ESP_OK nếu thành công
 */
esp_err_t config_save_to_json(const char *filename);
```

### 3.3. Get/Set Configuration

```c
/**
 * Lấy con trỏ đến cấu hình hiện tại (read-only)
 * @return Pointer to system_config_t
 */
const system_config_t* config_get(void);

/**
 * Lấy bản sao cấu hình hiện tại
 * @param config Pointer để lưu cấu hình
 * @return ESP_OK nếu thành công
 */
esp_err_t config_get_copy(system_config_t *config);

/**
 * Cập nhật toàn bộ cấu hình
 * @param config Cấu hình mới
 * @return ESP_OK nếu thành công
 */
esp_err_t config_set(const system_config_t *config);

/**
 * Cập nhật cấu hình một cổng DMX
 * @param port_num Số cổng (1 hoặc 2)
 * @param mode DMX mode
 * @param primary_universe Universe chính
 * @param secondary_universe Universe phụ (-1 để tắt)
 * @param offset Universe offset
 * @return ESP_OK nếu thành công
 */
esp_err_t config_set_port(uint8_t port_num, 
                          dmx_mode_t mode,
                          uint16_t primary_universe,
                          int16_t secondary_universe,
                          int16_t offset);
```

### 3.4. Validation

```c
/**
 * Kiểm tra tính hợp lệ của cấu hình
 * @param config Cấu hình cần kiểm tra
 * @return true nếu hợp lệ
 */
bool config_validate(const system_config_t *config);

/**
 * Kiểm tra tính hợp lệ của WiFi profile
 * @param profile WiFi profile
 * @return true nếu hợp lệ
 */
bool config_validate_wifi_profile(const wifi_profile_t *profile);
```

---

## 4. Luồng hoạt động

### 4.1. Boot sequence

```
1. config_init()
   └─> Khởi tạo mutex/semaphore
   └─> Cấp phát bộ nhớ cho config

2. config_load()
   └─> Thử đọc từ NVS
   └─> Nếu fail, thử đọc từ LittleFS (config.json)
   └─> Nếu fail, load defaults
   └─> Validate config
   └─> Nếu invalid, load defaults

3. Các module khác gọi config_get() để lấy cấu hình
```

### 4.2. Runtime update

```
1. Web Server nhận request thay đổi config
2. Parse JSON từ HTTP POST
3. Validate config mới
4. config_set() để update
5. config_save() để lưu vào storage
6. Trigger các module liên quan reload config
```

---

## 5. Thread Safety

- Module sử dụng **mutex** để bảo vệ truy cập đồng thời
- Các hàm get/set đều thread-safe
- config_get() trả về const pointer → không cho phép sửa trực tiếp
- Mọi thay đổi phải qua config_set()

---

## 6. Storage Backend

### 6.1. NVS (Non-Volatile Storage)

- Lưu trữ nhị phân, nhanh
- Sử dụng namespace: "config"
- Key: "system_config"

### 6.2. LittleFS (JSON)

- Lưu trữ dạng text (JSON), dễ debug
- File: `/littlefs/config.json`
- Backup khi save: `config.json.bak`

### 6.3. Priority

1. Thử đọc NVS trước
2. Nếu NVS không có hoặc lỗi, đọc LittleFS
3. Nếu cả hai đều fail, dùng defaults

---

## 7. Cấu hình mặc định

```c
static const system_config_t DEFAULT_CONFIG = {
    .use_ethernet = true,
    .ap_ssid = "ArtnetNode-XXXX",
    .ap_password = "12345678",
    .ap_channel = 1,
    
    .port1_mode = DMX_OUT,
    .port2_mode = DMX_OUT,
    .universe_primary_port1 = 0,
    .universe_primary_port2 = 1,
    .universe_secondary_port1 = -1,
    .universe_secondary_port2 = -1,
    .universe_offset_port1 = 0,
    .universe_offset_port2 = 0,
    
    .protocol_mode_port1 = ARTNET_PRIORITY,
    .protocol_mode_port2 = ARTNET_PRIORITY,
    
    .merge_mode_port1 = HTP,
    .merge_mode_port2 = HTP,
    .merge_timeout_seconds = 3,
    
    .rdm_enabled_port1 = true,
    .rdm_enabled_port2 = true,
    
    .node_short_name = "ArtNode-2RDM",
    .node_long_name = "ESP32 ArtNet Node 2 Port RDM"
};
```

---

## 8. Xử lý lỗi

| Lỗi | Hành động |
|------|-----------|
| NVS read fail | Thử đọc LittleFS |
| LittleFS read fail | Load defaults |
| Config invalid | Load defaults |
| Save fail | Retry 3 lần, log error |
| JSON parse error | Load defaults |

---

## 9. Dependencies

- **ESP-IDF NVS**: Lưu trữ nhị phân
- **LittleFS**: File system
- **cJSON**: Parse/generate JSON
- **FreeRTOS**: Mutex, semaphore

---

## 10. Testing Points

1. Load defaults khi không có config
2. Load từ NVS thành công
3. Load từ LittleFS khi NVS fail
4. Validate config - reject invalid values
5. Save/Load cycle - data integrity
6. Thread-safe concurrent access
7. JSON serialization/deserialization
8. Backup/restore config file

---

## 11. Memory Usage

- Struct size: ~400 bytes
- JSON file: ~2-4 KB
- Heap usage: ~10 KB (temporary during parse/save)

---

## 12. Performance

- Load time: < 100ms
- Save time: < 200ms
- Get config: < 1µs (pointer access)
