# LỘ TRÌNH TRIỂN KHAI FIRMWARE
**Dự án: ESP-NODE-2RDM**

**Phiên bản:** 1.0  
**Ngày:** 25/12/2025

---

## MỤC LỤC
1. [Tổng quan](#1-tổng-quan)
2. [Sprint 0: Project Setup](#sprint-0-project-setup)
3. [Sprint 1: Storage & Configuration](#sprint-1-storage--configuration)
4. [Sprint 2: LED Manager](#sprint-2-led-manager)
5. [Sprint 3-4: Network Manager](#sprint-3-4-network-manager)
6. [Sprint 5-6: DMX/RDM Handler](#sprint-5-6-dmxrdm-handler)
7. [Sprint 7-8: Protocol Receivers](#sprint-7-8-protocol-receivers)
8. [Sprint 9-10: Merge Engine](#sprint-9-10-merge-engine)
9. [Sprint 11-12: Web Server](#sprint-11-12-web-server)
10. [Sprint 13: Integration](#sprint-13-integration)
11. [Sprint 14-15: Testing & Optimization](#sprint-14-15-testing--optimization)

---

## 1. TỔNG QUAN

### 1.1. Phương pháp phát triển

**Approach:** Bottom-up + Incremental  
**Duration:** 15 sprints (không ước lượng thời gian cụ thể)  
**Testing:** Continuous integration testing sau mỗi sprint

### 1.2. Mục tiêu từng phase

| Phase | Module | Deliverable |
|-------|--------|-------------|
| 0 | Setup | ESP-IDF project structure, build system |
| 1 | Storage | LittleFS + Config management |
| 2 | LED | WS2812 status indicator |
| 3-4 | Network | Ethernet + WiFi với fallback |
| 5-6 | DMX/RDM | 2 cổng DMX512/RDM functional |
| 7-8 | Protocols | Art-Net + sACN receivers |
| 9-10 | Merge | Data merging với HTP/LTP/etc |
| 11-12 | Web | HTTP + WebSocket interface |
| 13 | Integration | Full system integration |
| 14-15 | Testing | Performance tuning + stability |

### 1.3. Success Criteria

Sau mỗi sprint:
- ✅ Code builds without warnings
- ✅ Module tests passed
- ✅ Documentation updated
- ✅ Git commit với clear message

---

## SPRINT 0: PROJECT SETUP

### Mục tiêu
Tạo cấu trúc project ESP-IDF hoàn chỉnh, có thể build thành công.

### Công việc chi tiết

#### Task 0.1: Create ESP-IDF Project
```bash
# Navigate to workspace
cd /path/to/workspace

# Create new project
idf.py create-project esp-node-2rdm
cd esp-node-2rdm
```

#### Task 0.2: Setup Directory Structure
```bash
mkdir -p components/{config_manager,network_manager,led_manager,dmx_handler}
mkdir -p components/{merge_engine,artnet_receiver,sacn_receiver,web_server,storage_manager}
mkdir -p external_components
mkdir -p docs/modules
mkdir -p tools/test_scripts
```

#### Task 0.3: Create Partition Table

**Tạo `partitions.csv`:**
```csv
# Name,     Type, SubType,  Offset,   Size,     Flags
nvs,        data, nvs,      0x9000,   0x4000,
otadata,    data, ota,      0xd000,   0x2000,
phy_init,   data, phy,      0xf000,   0x1000,
factory,    app,  factory,  0x10000,  0x200000,
littlefs,   data, spiffs,   0x210000, 0x100000,
```

#### Task 0.4: Configure sdkconfig.defaults

**Tạo `sdkconfig.defaults`:**
```ini
# Target
CONFIG_IDF_TARGET="esp32s3"
CONFIG_ESPTOOLPY_FLASHSIZE_16MB=y

# Partition table
CONFIG_PARTITION_TABLE_CUSTOM=y
CONFIG_PARTITION_TABLE_CUSTOM_FILENAME="partitions.csv"
CONFIG_PARTITION_TABLE_FILENAME="partitions.csv"

# FreeRTOS
CONFIG_FREERTOS_HZ=1000
CONFIG_FREERTOS_UNICORE=n
CONFIG_FREERTOS_USE_TRACE_FACILITY=y
CONFIG_FREERTOS_USE_STATS_FORMATTING_FUNCTIONS=y

# PSRAM
CONFIG_SPIRAM=y
CONFIG_SPIRAM_MODE_OCT=y
CONFIG_SPIRAM_SPEED_80M=y
CONFIG_SPIRAM_MALLOC_ALWAYSINTERNAL=16384

# Ethernet
CONFIG_ETH_USE_SPI_ETHERNET=y
CONFIG_ETH_SPI_ETHERNET_W5500=y
CONFIG_ETH_ENABLED=y

# WiFi
CONFIG_ESP_WIFI_STATIC_RX_BUFFER_NUM=10
CONFIG_ESP_WIFI_DYNAMIC_RX_BUFFER_NUM=32
CONFIG_ESP_WIFI_DYNAMIC_TX_BUFFER_NUM=32

# HTTP Server
CONFIG_HTTPD_MAX_REQ_HDR_LEN=1024
CONFIG_HTTPD_MAX_URI_LEN=512
CONFIG_HTTPD_WS_SUPPORT=y

# LwIP
CONFIG_LWIP_MAX_SOCKETS=16
CONFIG_LWIP_SO_REUSE=y
CONFIG_LWIP_SO_RCVBUF=y
CONFIG_LWIP_RECV_BUFSIZE_DEFAULT=16384
CONFIG_LWIP_IGMP=y

# LittleFS
CONFIG_LITTLEFS_MAX_PARTITIONS=3
CONFIG_LITTLEFS_PAGE_SIZE=256

# Logging
CONFIG_LOG_DEFAULT_LEVEL_INFO=y
CONFIG_LOG_DEFAULT_LEVEL=3

# Compiler optimization
CONFIG_COMPILER_OPTIMIZATION_PERF=y
CONFIG_COMPILER_OPTIMIZATION_ASSERTIONS_DISABLE=n
```

#### Task 0.5: Setup Root CMakeLists.txt

**Tạo `CMakeLists.txt`:**
```cmake
# Minimum CMake version
cmake_minimum_required(VERSION 3.16)

# Add external component directories
set(EXTRA_COMPONENT_DIRS
    "${CMAKE_CURRENT_LIST_DIR}/external_components"
)

# Include ESP-IDF build system
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# Project definition
project(esp-node-2rdm)
```

#### Task 0.6: Create Main Component

**Tạo `main/CMakeLists.txt`:**
```cmake
idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
)
```

**Tạo `main/main.c`:**
```c
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "nvs_flash.h"

static const char *TAG = "main";

void app_main(void)
{
    ESP_LOGI(TAG, "ESP-NODE-2RDM Firmware v0.1.0");
    ESP_LOGI(TAG, "ESP-IDF Version: %s", esp_get_idf_version());
    
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    
    ESP_LOGI(TAG, "System initialized successfully");
    
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
```

#### Task 0.7: Setup .gitignore

**Tạo `.gitignore`:**
```gitignore
# ESP-IDF build artifacts
build/
sdkconfig
sdkconfig.old
dependencies.lock
managed_components/

# IDE
.vscode/
.idea/
*.swp
*.swo
*.swn

# OS
.DS_Store
Thumbs.db
*.bak

# Test artifacts
test_output/
*.log

# Temporary files
*~
*.tmp
```

#### Task 0.8: Initial Build Test

```bash
# Set up ESP-IDF environment
. $IDF_PATH/export.sh

# Build project
idf.py build

# Expected output: Build successful
```

### Deliverables
- [ ] Project structure created
- [ ] Partition table defined
- [ ] sdkconfig.defaults configured
- [ ] CMakeLists.txt setup complete
- [ ] Initial main.c builds successfully
- [ ] .gitignore configured

### Testing
```bash
# Clean build
idf.py fullclean
idf.py build

# Should complete without errors
# Check build output for warnings (should be zero)
```

---

## SPRINT 1: STORAGE & CONFIGURATION

### Mục tiêu
Triển khai LittleFS storage và configuration management với JSON.

### Công việc chi tiết

#### Task 1.1: Add LittleFS Component

```bash
cd external_components
git clone https://github.com/joltwallet/esp_littlefs.git esp_littlefs
```

#### Task 1.2: Create Storage Manager

**File: `components/storage_manager/CMakeLists.txt`:**
```cmake
idf_component_register(
    SRCS "storage_manager.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_littlefs
)
```

**File: `components/storage_manager/include/storage_manager.h`:**
```c
#ifndef STORAGE_MANAGER_H
#define STORAGE_MANAGER_H

#include "esp_err.h"
#include <stddef.h>
#include <stdbool.h>

#define STORAGE_BASE_PATH "/littlefs"

/**
 * @brief Initialize LittleFS storage
 * @return ESP_OK on success
 */
esp_err_t storage_init(void);

/**
 * @brief Deinitialize storage
 * @return ESP_OK on success
 */
esp_err_t storage_deinit(void);

/**
 * @brief Read file from storage
 * @param path File path relative to base path
 * @param buffer Output buffer
 * @param size Buffer size (in), bytes read (out)
 * @return ESP_OK on success
 */
esp_err_t storage_read_file(const char *path, char *buffer, size_t *size);

/**
 * @brief Write file to storage
 * @param path File path relative to base path
 * @param data Data to write
 * @param size Data size
 * @return ESP_OK on success
 */
esp_err_t storage_write_file(const char *path, const char *data, size_t size);

/**
 * @brief Check if file exists
 * @param path File path
 * @return true if exists
 */
bool storage_file_exists(const char *path);

/**
 * @brief Delete file
 * @param path File path
 * @return ESP_OK on success
 */
esp_err_t storage_delete_file(const char *path);

#endif
```

**File: `components/storage_manager/storage_manager.c`:**
```c
#include "storage_manager.h"
#include "esp_littlefs.h"
#include "esp_log.h"
#include <string.h>
#include <sys/stat.h>
#include <stdio.h>

static const char *TAG = "storage";
static bool initialized = false;

esp_err_t storage_init(void)
{
    if (initialized) {
        ESP_LOGW(TAG, "Already initialized");
        return ESP_OK;
    }
    
    esp_vfs_littlefs_conf_t conf = {
        .base_path = STORAGE_BASE_PATH,
        .partition_label = "littlefs",
        .format_if_mount_failed = true,
        .dont_mount = false,
    };
    
    esp_err_t ret = esp_vfs_littlefs_register(&conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to mount LittleFS: %s", esp_err_to_name(ret));
        return ret;
    }
    
    size_t total = 0, used = 0;
    ret = esp_littlefs_info("littlefs", &total, &used);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "LittleFS: total=%d KB, used=%d KB", total/1024, used/1024);
    }
    
    initialized = true;
    return ESP_OK;
}

esp_err_t storage_deinit(void)
{
    if (!initialized) return ESP_OK;
    
    esp_err_t ret = esp_vfs_littlefs_unregister("littlefs");
    initialized = false;
    return ret;
}

esp_err_t storage_read_file(const char *path, char *buffer, size_t *size)
{
    char full_path[128];
    snprintf(full_path, sizeof(full_path), "%s/%s", STORAGE_BASE_PATH, path);
    
    FILE *f = fopen(full_path, "r");
    if (!f) {
        ESP_LOGE(TAG, "Failed to open file: %s", full_path);
        return ESP_FAIL;
    }
    
    size_t read_size = fread(buffer, 1, *size - 1, f);
    buffer[read_size] = '\0';
    *size = read_size;
    
    fclose(f);
    ESP_LOGI(TAG, "Read %d bytes from %s", read_size, path);
    return ESP_OK;
}

esp_err_t storage_write_file(const char *path, const char *data, size_t size)
{
    char full_path[128];
    snprintf(full_path, sizeof(full_path), "%s/%s", STORAGE_BASE_PATH, path);
    
    FILE *f = fopen(full_path, "w");
    if (!f) {
        ESP_LOGE(TAG, "Failed to create file: %s", full_path);
        return ESP_FAIL;
    }
    
    size_t written = fwrite(data, 1, size, f);
    fclose(f);
    
    if (written != size) {
        ESP_LOGE(TAG, "Write size mismatch: %d != %d", written, size);
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "Wrote %d bytes to %s", written, path);
    return ESP_OK;
}

bool storage_file_exists(const char *path)
{
    char full_path[128];
    snprintf(full_path, sizeof(full_path), "%s/%s", STORAGE_BASE_PATH, path);
    
    struct stat st;
    return (stat(full_path, &st) == 0);
}

esp_err_t storage_delete_file(const char *path)
{
    char full_path[128];
    snprintf(full_path, sizeof(full_path), "%s/%s", STORAGE_BASE_PATH, path);
    
    if (unlink(full_path) == 0) {
        ESP_LOGI(TAG, "Deleted file: %s", path);
        return ESP_OK;
    }
    
    ESP_LOGE(TAG, "Failed to delete: %s", path);
    return ESP_FAIL;
}
```

#### Task 1.3: Create Configuration Manager

**File: `components/config_manager/CMakeLists.txt`:**
```cmake
idf_component_register(
    SRCS "config_manager.c"
    INCLUDE_DIRS "include"
    REQUIRES json nvs_flash storage_manager
)
```

**File: `components/config_manager/include/config_manager.h`:**
```c
#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

// DMX port modes
typedef enum {
    DMX_MODE_DISABLED = 0,
    DMX_MODE_OUTPUT,
    DMX_MODE_INPUT,
    DMX_MODE_RDM_MASTER,
    DMX_MODE_RDM_RESPONDER
} dmx_mode_t;

// Merge modes
typedef enum {
    MERGE_MODE_HTP = 0,
    MERGE_MODE_LTP,
    MERGE_MODE_LAST,
    MERGE_MODE_BACKUP,
    MERGE_MODE_DISABLE
} merge_mode_t;

// Protocol modes
typedef enum {
    PROTOCOL_ARTNET_ONLY = 0,
    PROTOCOL_SACN_ONLY,
    PROTOCOL_ARTNET_PRIORITY,
    PROTOCOL_SACN_PRIORITY,
    PROTOCOL_MERGE_BOTH
} protocol_mode_t;

// WiFi profile
typedef struct {
    char ssid[32];
    char password[64];
    uint8_t priority;
    bool use_static_ip;
    char static_ip[16];
    char gateway[16];
    char netmask[16];
} wifi_profile_t;

// Port configuration
typedef struct {
    dmx_mode_t mode;
    uint16_t universe_primary;
    int16_t universe_secondary;
    int16_t universe_offset;
    protocol_mode_t protocol_mode;
    merge_mode_t merge_mode;
    bool rdm_enabled;
} port_config_t;

// Main configuration
typedef struct {
    struct {
        bool use_ethernet;
        bool eth_use_static_ip;
        char eth_static_ip[16];
        char eth_gateway[16];
        char eth_netmask[16];
        wifi_profile_t wifi_profiles[5];
        uint8_t wifi_profile_count;
        char ap_ssid[32];
        char ap_password[64];
        uint8_t ap_channel;
    } network;
    
    port_config_t port1;
    port_config_t port2;
    
    struct {
        uint8_t timeout_seconds;
    } merge;
    
    struct {
        char short_name[18];
        char long_name[64];
    } node_info;
} config_t;

/**
 * @brief Initialize configuration manager
 * @return ESP_OK on success
 */
esp_err_t config_init(void);

/**
 * @brief Load configuration from storage
 * @return ESP_OK on success
 */
esp_err_t config_load(void);

/**
 * @brief Save configuration to storage
 * @return ESP_OK on success
 */
esp_err_t config_save(void);

/**
 * @brief Get current configuration
 * @return Pointer to config structure
 */
config_t* config_get(void);

/**
 * @brief Reset to default configuration
 * @return ESP_OK on success
 */
esp_err_t config_reset_to_defaults(void);

/**
 * @brief Export config as JSON string
 * @param json_str Output JSON string (caller must free)
 * @return ESP_OK on success
 */
esp_err_t config_to_json(char **json_str);

/**
 * @brief Import config from JSON string
 * @param json_str Input JSON string
 * @return ESP_OK on success
 */
esp_err_t config_from_json(const char *json_str);

#endif
```

**File: `components/config_manager/config_manager.c`:**
See implementation in FIRMWARE_DEVELOPMENT_PLAN.md Phase 1.

#### Task 1.4: Update main.c

```c
#include "storage_manager.h"
#include "config_manager.h"

void app_main(void)
{
    // ... NVS init ...
    
    // Initialize storage
    ESP_ERROR_CHECK(storage_init());
    
    // Initialize and load config
    ESP_ERROR_CHECK(config_init());
    ESP_ERROR_CHECK(config_load());
    
    config_t *config = config_get();
    ESP_LOGI(TAG, "Node: %s", config->node_info.short_name);
}
```

### Deliverables
- [ ] LittleFS integrated và mounted
- [ ] Storage manager API functional
- [ ] Config manager với full JSON support
- [ ] Default config loads successfully
- [ ] Save/load persistence verified

### Testing
```bash
# Build
idf.py build

# Flash and monitor
idf.py -p /dev/ttyUSB0 flash monitor

# Expected output:
# - LittleFS mounted
# - Default config created
# - Config loaded successfully

# Test persistence:
# 1. Flash firmware
# 2. Reboot → config should persist
# 3. Erase flash → default config recreated
```

---

## SPRINT 2: LED MANAGER

### Mục tiêu
Triển khai WS2812 LED status indicator.

### Công việc chi tiết

#### Task 2.1: Create LED Manager Component

**File: `components/led_manager/CMakeLists.txt`:**
```cmake
idf_component_register(
    SRCS "led_manager.c"
    INCLUDE_DIRS "include"
    REQUIRES led_strip driver
)
```

**File: `components/led_manager/include/led_manager.h`:**
```c
#ifndef LED_MANAGER_H
#define LED_MANAGER_H

#include "esp_err.h"
#include <stdint.h>

typedef enum {
    LED_STATE_BOOT,             // Light blue
    LED_STATE_ETHERNET_OK,      // Green
    LED_STATE_WIFI_STA_OK,      // Green blinking
    LED_STATE_WIFI_AP,          // Purple
    LED_STATE_RECEIVING,        // White flash
    LED_STATE_RDM_DISCOVERY,    // Yellow blinking
    LED_STATE_ERROR             // Red fast blink
} led_state_t;

esp_err_t led_manager_init(void);
esp_err_t led_manager_set_state(led_state_t state);
esp_err_t led_manager_pulse(void); // Quick flash for packet receive

#endif
```

**Implementation:** See FIRMWARE_DEVELOPMENT_PLAN.md Phase 2.

### Deliverables
- [ ] WS2812 LED functional
- [ ] All states implemented
- [ ] Smooth transitions
- [ ] Non-blocking operation

### Testing
```bash
# Test all LED states sequentially
# Verify colors với eyes or multimeter
# Check timing accuracy
```

---

## SPRINT 3-4: NETWORK MANAGER

### Mục tiêu
Triển khai Ethernet W5500 và WiFi với auto-fallback.

### Công việc chi tiết

See FIRMWARE_DEVELOPMENT_PLAN.md Phase 3 for detailed implementation.

### Deliverables
- [ ] W5500 Ethernet driver working
- [ ] WiFi STA mode functional
- [ ] WiFi AP mode functional
- [ ] Auto-fallback logic implemented
- [ ] IP configuration (DHCP + static)

### Testing
- Test Ethernet connection
- Test WiFi STA with multiple profiles
- Test WiFi AP fallback
- Verify fallback sequence
- Ping test from external device

---

## SPRINT 5-6: DMX/RDM HANDLER

### Mục tiêu
Triển khai 2 cổng DMX512/RDM độc lập.

### Công việc chi tiết

#### Task 5.1: Add esp-dmx Library
```bash
cd external_components
git clone https://github.com/someweisguy/esp-dmx.git esp-dmx
```

#### Task 5.2: Implement DMX Handler
See FIRMWARE_DEVELOPMENT_PLAN.md Phase 4 for detailed API.

### Deliverables
- [ ] Port 1 DMX output @ 44Hz
- [ ] Port 2 DMX output @ 44Hz
- [ ] RDM discovery functional
- [ ] RDM get/set working
- [ ] DMX input monitoring

### Testing
- DMX output với DMX tester/oscilloscope
- RDM discovery với real devices
- Timing verification
- Cross-port independence

---

## SPRINT 7-8: PROTOCOL RECEIVERS

### Mục tiêu
Triển khai Art-Net và sACN receivers.

### Công việc chi tiết

#### Task 7.1: Implement Art-Net Receiver
See LIBRARY_INTEGRATION_GUIDE.md Section 5.

#### Task 7.2: Add libe131
```bash
cd external_components
git clone https://github.com/hhromic/libe131.git libe131
```

#### Task 7.3: Implement sACN Receiver
See LIBRARY_INTEGRATION_GUIDE.md Section 4.

### Deliverables
- [ ] Art-Net receiver functional
- [ ] sACN receiver functional
- [ ] Universe routing correct
- [ ] Priority handling
- [ ] Multicast join/leave

### Testing
- Send Art-Net from QLC+
- Send sACN from software
- Verify universe mapping
- Test multiple sources
- Check packet loss

---

## SPRINT 9-10: MERGE ENGINE

### Mục tiêu
Triển khai merge logic cho multi-source.

### Công việc chi tiết
See FIRMWARE_DEVELOPMENT_PLAN.md Phase 6.

### Deliverables
- [ ] HTP mode working
- [ ] LTP mode working
- [ ] LAST mode working
- [ ] BACKUP mode working
- [ ] Timeout handling
- [ ] Performance < 5ms

### Testing
- Test HTP với 2 sources
- Test backup failover
- Timeout verification
- Performance profiling

---

## SPRINT 11-12: WEB SERVER

### Mục tiêu
Triển khai HTTP API và WebSocket interface.

### Công việc chi tiết
See FIRMWARE_DEVELOPMENT_PLAN.md Phase 7.

### Deliverables
- [ ] HTTP server running
- [ ] All REST endpoints functional
- [ ] WebSocket working
- [ ] Web UI (HTML/CSS/JS)
- [ ] Real-time DMX monitoring

### Testing
- Load web page in browser
- Test all API endpoints
- WebSocket connection
- DMX test via web
- Config download/upload

---

## SPRINT 13: INTEGRATION

### Mục tiêu
Tích hợp tất cả modules thành firmware hoàn chỉnh.

### Công việc chi tiết
See FIRMWARE_DEVELOPMENT_PLAN.md Phase 8.

### Deliverables
- [ ] All modules integrated
- [ ] Task allocation optimized
- [ ] End-to-end data flow working
- [ ] Error handling complete

### Testing
- Full integration test
- Art-Net → Merge → DMX
- sACN → Merge → DMX
- Web → Config → Apply
- RDM end-to-end

---

## SPRINT 14-15: TESTING & OPTIMIZATION

### Mục tiêu
Performance tuning, stability testing, documentation.

### Công việc chi tiết

#### Task 14.1: Performance Testing
- DMX refresh rate target: 40-44 Hz
- Merge processing: < 5ms
- Web response: < 200ms
- Packet loss: < 0.1%

#### Task 14.2: Stability Testing
- 24-hour continuous operation
- Memory leak detection
- Network reconnection
- Power cycle recovery

#### Task 14.3: Edge Case Testing
- Rapid config changes
- Network loss/recovery
- Multiple simultaneous sources
- RDM device hotplug

#### Task 14.4: Documentation
- Update API documentation
- Create user manual
- Hardware setup guide
- Troubleshooting guide

### Deliverables
- [ ] Performance targets met
- [ ] 24h stability passed
- [ ] All edge cases handled
- [ ] Documentation complete
- [ ] Ready for production

---

## PHỤ LỤC: TESTING CHECKLIST

### Per-Sprint Testing
- [ ] Code compiles without warnings
- [ ] Module unit tests pass
- [ ] No memory leaks detected
- [ ] Performance within targets
- [ ] Documentation updated

### Integration Testing
- [ ] All modules work together
- [ ] Error handling correct
- [ ] State transitions smooth
- [ ] Recovery from failures
- [ ] Config persistence

### Final Testing
- [ ] Full functional test
- [ ] Performance profiling
- [ ] Stability test (24h+)
- [ ] Edge cases covered
- [ ] Documentation reviewed

---

**Tài liệu này sẽ được cập nhật theo tiến độ thực tế.**
