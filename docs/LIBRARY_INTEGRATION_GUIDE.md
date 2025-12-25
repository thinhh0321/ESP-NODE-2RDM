# HƯỚNG DẪN TÍCH HỢP THƯ VIỆN
**Dự án: ESP-NODE-2RDM**

**Phiên bản:** 1.0  
**Ngày:** 25/12/2025

---

## MỤC LỤC
1. [Tổng quan](#1-tổng-quan)
2. [Thư viện ESP-IDF chính thức](#2-thư-viện-esp-idf-chính-thức)
3. [esp-dmx - DMX512/RDM](#3-esp-dmx---dmx512rdm)
4. [libe131 - sACN Receiver](#4-libe131---sacn-receiver)
5. [Art-Net Implementation](#5-art-net-implementation)
6. [LittleFS Integration](#6-littlefs-integration)
7. [Dependency Management](#7-dependency-management)

---

## 1. TỔNG QUAN

Tài liệu này mô tả chi tiết cách tích hợp từng thư viện vào dự án, bao gồm:
- Cách thêm thư viện vào project
- Cấu hình CMakeLists.txt
- API sử dụng chính
- Ví dụ code
- Troubleshooting

### 1.1. Nguyên tắc chọn thư viện

✅ **Ưu tiên cao nhất:**
1. Thư viện chính thức ESP-IDF
2. Thư viện được maintain tốt (commit trong 6 tháng gần)
3. Thư viện có documentation đầy đủ
4. Thư viện đã được test với ESP32-S3

❌ **Tránh:**
1. Thư viện Arduino-only (khó port)
2. Thư viện abandoned (> 2 năm không update)
3. Thư viện có dependency phức tạp
4. Thư viện closed-source

### 1.2. Cấu trúc thư viện trong project

```
ESP-NODE-2RDM/
├── components/                         # Custom components
│   ├── config_manager/
│   ├── network_manager/
│   └── ...
├── managed_components/                 # IDF Component Manager
│   └── (auto-generated)
└── external_components/                # External libraries
    ├── esp-dmx/                        # Git submodule or clone
    ├── libe131/
    └── artnet/
```

---

## 2. THƯ VIỆN ESP-IDF CHÍNH THỨC

Các thư viện này đã có sẵn khi cài ESP-IDF, chỉ cần khai báo trong CMakeLists.txt.

### 2.1. esp_eth - Ethernet Driver

**Mục đích:** Driver cho W5500 Ethernet controller qua SPI

**Thêm vào CMakeLists.txt:**
```cmake
idf_component_register(
    SRCS "network_manager.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_eth esp_netif driver
)
```

**API chính:**
```c
#include "esp_eth.h"
#include "esp_eth_driver.h"

// W5500 configuration
eth_w5500_config_t w5500_config = ETH_W5500_DEFAULT_CONFIG(SPI_HOST, GPIO_NUM_10);
w5500_config.int_gpio_num = GPIO_NUM_9;

eth_mac_config_t mac_config = ETH_MAC_DEFAULT_CONFIG();
eth_phy_config_t phy_config = ETH_PHY_DEFAULT_CONFIG();

esp_eth_mac_t *mac = esp_eth_mac_new_w5500(&w5500_config, &mac_config);
esp_eth_phy_t *phy = esp_eth_phy_new_w5500(&phy_config);

esp_eth_config_t eth_config = ETH_DEFAULT_CONFIG(mac, phy);
esp_eth_handle_t eth_handle = NULL;
esp_eth_driver_install(&eth_config, &eth_handle);

// Start
esp_eth_start(eth_handle);
```

**Configuration (sdkconfig):**
```ini
CONFIG_ETH_USE_SPI_ETHERNET=y
CONFIG_ETH_SPI_ETHERNET_W5500=y
CONFIG_ETH_ENABLED=y
```

**Documentation:**
https://docs.espressif.com/projects/esp-idf/en/v5.2.6/esp32s3/api-reference/network/esp_eth.html

---

### 2.2. esp_wifi - WiFi Driver

**Mục đích:** WiFi Station và Access Point modes

**Thêm vào CMakeLists.txt:**
```cmake
REQUIRES esp_wifi esp_netif nvs_flash
```

**API chính (STA mode):**
```c
#include "esp_wifi.h"

wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
esp_wifi_init(&cfg);

wifi_config_t wifi_config = {
    .sta = {
        .ssid = "MyNetwork",
        .password = "MyPassword",
    },
};

esp_wifi_set_mode(WIFI_MODE_STA);
esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
esp_wifi_start();
esp_wifi_connect();
```

**API chính (AP mode):**
```c
wifi_config_t ap_config = {
    .ap = {
        .ssid = "ArtnetNode-0000",
        .password = "12345678",
        .max_connection = 4,
        .authmode = WIFI_AUTH_WPA2_PSK
    },
};

esp_wifi_set_mode(WIFI_MODE_AP);
esp_wifi_set_config(WIFI_IF_AP, &ap_config);
esp_wifi_start();
```

**Documentation:**
https://docs.espressif.com/projects/esp-idf/en/v5.2.6/esp32s3/api-reference/network/esp_wifi.html

---

### 2.3. esp_http_server - HTTP Server + WebSocket

**Mục đích:** Web server cho configuration interface

**Thêm vào CMakeLists.txt:**
```cmake
REQUIRES esp_http_server
```

**API chính:**
```c
#include "esp_http_server.h"

httpd_handle_t server = NULL;
httpd_config_t config = HTTPD_DEFAULT_CONFIG();
config.uri_match_fn = httpd_uri_match_wildcard;

httpd_start(&server, &config);

// Register URI handler
httpd_uri_t uri_get = {
    .uri = "/config",
    .method = HTTP_GET,
    .handler = config_get_handler,
    .user_ctx = NULL
};
httpd_register_uri_handler(server, &uri_get);
```

**WebSocket handler:**
```c
httpd_uri_t ws = {
    .uri = "/ws",
    .method = HTTP_GET,
    .handler = ws_handler,
    .user_ctx = NULL,
    .is_websocket = true,
    .handle_ws_control_frames = true
};

esp_err_t ws_handler(httpd_req_t *req) {
    if (req->method == HTTP_GET) {
        ESP_LOGI(TAG, "WebSocket handshake");
        return ESP_OK;
    }
    
    httpd_ws_frame_t ws_pkt;
    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
    ws_pkt.type = HTTPD_WS_TYPE_TEXT;
    
    esp_err_t ret = httpd_ws_recv_frame(req, &ws_pkt, 0);
    if (ret != ESP_OK) return ret;
    
    // Process message
    return httpd_ws_send_frame(req, &ws_pkt);
}
```

**Configuration:**
```ini
CONFIG_HTTPD_MAX_REQ_HDR_LEN=1024
CONFIG_HTTPD_MAX_URI_LEN=512
CONFIG_HTTPD_WS_SUPPORT=y
```

**Documentation:**
https://docs.espressif.com/projects/esp-idf/en/v5.2.6/esp32s3/api-reference/protocols/esp_http_server.html

---

### 2.4. led_strip - WS2812 LED Control

**Mục đích:** Điều khiển WS2812 LED status

**Thêm vào CMakeLists.txt:**
```cmake
REQUIRES led_strip
```

**API chính:**
```c
#include "led_strip.h"

led_strip_handle_t led_strip;
led_strip_config_t strip_config = {
    .strip_gpio_num = GPIO_NUM_48,
    .max_leds = 1,
    .led_pixel_format = LED_PIXEL_FORMAT_GRB,
    .led_model = LED_MODEL_WS2812,
};

led_strip_rmt_config_t rmt_config = {
    .clk_src = RMT_CLK_SRC_DEFAULT,
    .resolution_hz = 10 * 1000 * 1000, // 10MHz
};

led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip);

// Set color (R, G, B)
led_strip_set_pixel(led_strip, 0, 255, 0, 0); // Red
led_strip_refresh(led_strip);

// Clear
led_strip_clear(led_strip);
```

**Documentation:**
https://github.com/espressif/idf-extra-components/tree/master/led_strip

---

### 2.5. cJSON - JSON Parsing

**Mục đích:** Parse và generate JSON cho configuration

**Thêm vào CMakeLists.txt:**
```cmake
REQUIRES json
```

**API chính:**
```c
#include "cJSON.h"

// Parse JSON
const char *json_str = "{\"name\":\"ArtnetNode\",\"port\":6454}";
cJSON *root = cJSON_Parse(json_str);
cJSON *name = cJSON_GetObjectItem(root, "name");
printf("Name: %s\n", name->valuestring);
cJSON_Delete(root);

// Generate JSON
cJSON *root = cJSON_CreateObject();
cJSON_AddStringToObject(root, "name", "ArtnetNode");
cJSON_AddNumberToObject(root, "port", 6454);

char *json_str = cJSON_Print(root);
printf("%s\n", json_str);
free(json_str);
cJSON_Delete(root);
```

**Documentation:**
https://github.com/DaveGamble/cJSON

---

### 2.6. nvs_flash - Non-Volatile Storage

**Mục đích:** Lưu trữ config cơ bản (fallback nếu LittleFS lỗi)

**Thêm vào CMakeLists.txt:**
```cmake
REQUIRES nvs_flash
```

**API chính:**
```c
#include "nvs_flash.h"
#include "nvs.h"

// Initialize
nvs_flash_init();

// Write
nvs_handle_t handle;
nvs_open("storage", NVS_READWRITE, &handle);
nvs_set_str(handle, "ssid", "MyNetwork");
nvs_commit(handle);
nvs_close(handle);

// Read
nvs_open("storage", NVS_READONLY, &handle);
size_t len;
nvs_get_str(handle, "ssid", NULL, &len);
char *ssid = malloc(len);
nvs_get_str(handle, "ssid", ssid, &len);
nvs_close(handle);
```

**Documentation:**
https://docs.espressif.com/projects/esp-idf/en/v5.2.6/esp32s3/api-reference/storage/nvs_flash.html

---

## 3. ESP-DMX - DMX512/RDM

**Repository:** https://github.com/someweisguy/esp-dmx  
**License:** MIT  
**Maintainer:** someweisguy (active)  
**ESP-IDF Version:** v5.0+

### 3.1. Installation

**Method 1: Git clone (Recommended)**
```bash
cd components
git clone https://github.com/someweisguy/esp-dmx.git esp-dmx
```

**Method 2: Git submodule**
```bash
git submodule add https://github.com/someweisguy/esp-dmx.git components/esp-dmx
git submodule update --init --recursive
```

### 3.2. CMakeLists.txt

Trong component sử dụng DMX (e.g., `dmx_handler/CMakeLists.txt`):
```cmake
idf_component_register(
    SRCS "dmx_handler.c"
    INCLUDE_DIRS "include"
    REQUIRES esp-dmx driver
)
```

### 3.3. API Overview

**Initialize DMX driver:**
```c
#include "esp_dmx.h"

// Configuration
dmx_config_t config = DMX_CONFIG_DEFAULT;
dmx_personality_t personality = {
    .footprint = 512,
    .start_address = 1
};

// Install driver for port 1
dmx_driver_install(DMX_NUM_1, &config, &personality, DMX_INTR_FLAGS_DEFAULT);

// Set GPIO pins (TX, RX, EN)
dmx_set_pin(DMX_NUM_1, GPIO_NUM_17, GPIO_NUM_16, GPIO_NUM_21);
```

**DMX Output:**
```c
uint8_t dmx_data[512] = {0};
dmx_data[0] = 255; // Channel 1 = 255

// Write data to driver
dmx_write(DMX_NUM_1, dmx_data, 512);

// Send packet
dmx_send(DMX_NUM_1, 512);

// Wait for send complete
dmx_wait_sent(DMX_NUM_1, DMX_TIMEOUT_TICK);
```

**DMX Input:**
```c
dmx_packet_t packet;

// Wait for incoming packet
dmx_receive(DMX_NUM_1, &packet, DMX_TIMEOUT_TICK);

// Read data
uint8_t dmx_data[512];
dmx_read(DMX_NUM_1, dmx_data, 512);

printf("Received %d slots, break: %d us\n", 
       packet.size, packet.break_len);
```

**RDM Discovery:**
```c
rdm_disc_unique_branch_t branch = {
    .lower_bound = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    .upper_bound = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff}
};

rdm_disc_mute_t mute;
size_t num_devices = 0;

// Discover devices
rdm_discover_devices_simple(DMX_NUM_1, &branch, &mute, 
                             &num_devices, DMX_TIMEOUT_TICK);

printf("Found %d RDM devices\n", num_devices);
```

**RDM Get/Set:**
```c
rdm_uid_t uid = {0x0123, 0x45678901}; // Manufacturer ID + Device ID

// Get device info
rdm_parameter_t param;
rdm_get(DMX_NUM_1, &uid, RDM_PID_DEVICE_INFO, &param, 
        sizeof(param), DMX_TIMEOUT_TICK);

// Set DMX start address
uint16_t start_addr = 100;
rdm_set(DMX_NUM_1, &uid, RDM_PID_DMX_START_ADDRESS, 
        &start_addr, sizeof(start_addr), DMX_TIMEOUT_TICK);
```

### 3.4. Configuration

**Kconfig options:**
```ini
# In sdkconfig
CONFIG_DMX_ISR_IN_IRAM=y
CONFIG_DMX_STATISTIC=y
```

### 3.5. Example Task

```c
static void dmx_output_task(void *arg) {
    uint8_t dmx_buffer[512];
    dmx_packet_t packet;
    
    while (1) {
        // Get merged data from merge engine
        merge_engine_get_output(1, dmx_buffer);
        
        // Write and send
        dmx_write(DMX_NUM_1, dmx_buffer, 512);
        dmx_send(DMX_NUM_1, 512);
        
        // Wait for completion
        dmx_wait_sent(DMX_NUM_1, DMX_TIMEOUT_TICK);
        
        // ~44Hz refresh rate
        vTaskDelay(pdMS_TO_TICKS(23));
    }
}
```

### 3.6. Troubleshooting

**Problem:** DMX không output
- Check GPIO configuration
- Verify RS485 transceiver wiring
- Check TX enable pin (EN/DIR)
- Use oscilloscope để verify signal

**Problem:** RDM không hoạt động
- RDM cần bidirectional communication
- Check RX pin connection
- Verify timing (esp-dmx handles this)
- Some devices không support RDM

**Documentation:**
https://github.com/someweisguy/esp-dmx

---

## 4. LIBE131 - sACN RECEIVER

**Repository:** https://github.com/hhromic/libe131  
**License:** Apache 2.0  
**Language:** C  
**Dependencies:** Standard sockets (lwip)

### 4.1. Installation

```bash
cd components
git clone https://github.com/hhromic/libe131.git libe131
```

### 4.2. CMakeLists.txt Setup

Tạo `components/libe131/CMakeLists.txt`:
```cmake
idf_component_register(
    SRCS "src/e131.c"
    INCLUDE_DIRS "src"
    REQUIRES lwip
)
```

### 4.3. API Overview

**Initialize socket:**
```c
#include "e131.h"

e131_socket_t socket;
e131_packet_t packet;

// Initialize
if (e131_init(&socket) != E131_OK) {
    ESP_LOGE(TAG, "Failed to init E1.31 socket");
    return;
}

// Join multicast group for universe 1
// Multicast address: 239.255.0.1
if (e131_join_multicast(&socket, 1) != E131_OK) {
    ESP_LOGE(TAG, "Failed to join multicast");
    return;
}
```

**Receive packets:**
```c
while (1) {
    int result = e131_recv(&socket, &packet);
    
    if (result == E131_OK) {
        // Validate packet
        if (e131_pkt_validate(&packet) == E131_OK) {
            uint16_t universe = packet.universe;
            uint8_t priority = packet.priority;
            uint8_t *dmx_data = packet.dmp.data;
            
            // Process DMX data (512 channels)
            on_sacn_data(universe, dmx_data, 512, priority);
            
            ESP_LOGI(TAG, "Received sACN universe %d, priority %d", 
                     universe, priority);
        }
    }
}
```

**Subscribe to multiple universes:**
```c
// Universe 1: 239.255.0.1
e131_join_multicast(&socket, 1);

// Universe 2: 239.255.0.2
e131_join_multicast(&socket, 2);

// Universe 256: 239.255.1.0
e131_join_multicast(&socket, 256);
```

**Leave multicast:**
```c
e131_leave_multicast(&socket, 1);
```

### 4.4. Multicast Address Calculation

sACN multicast addresses:
- Base: `239.255.0.0`
- Universe N: `239.255.(N >> 8).(N & 0xFF)`

Examples:
- Universe 1 → 239.255.0.1
- Universe 255 → 239.255.0.255
- Universe 256 → 239.255.1.0

### 4.5. Example Task

```c
static void sacn_receiver_task(void *arg) {
    e131_socket_t socket;
    e131_packet_t packet;
    
    // Init
    e131_init(&socket);
    
    // Subscribe to configured universes
    config_t *config = config_get_current();
    e131_join_multicast(&socket, config->ports.port1.universe_primary);
    e131_join_multicast(&socket, config->ports.port2.universe_primary);
    
    ESP_LOGI(TAG, "sACN receiver started");
    
    while (1) {
        if (e131_recv(&socket, &packet) == E131_OK) {
            if (e131_pkt_validate(&packet) == E131_OK) {
                // Add to merge engine
                uint8_t port = config_universe_to_port(packet.universe);
                merge_engine_add_source(port, packet.universe, 
                                       packet.dmp.data, 
                                       packet.priority);
                
                // Flash LED
                led_manager_pulse();
            }
        }
    }
}
```

### 4.6. Configuration

**lwIP settings (sdkconfig):**
```ini
CONFIG_LWIP_IGMP=y
CONFIG_LWIP_MAX_SOCKETS=16
CONFIG_LWIP_SO_REUSE=y
CONFIG_LWIP_SO_RCVBUF=y
CONFIG_LWIP_RECV_BUFSIZE_DEFAULT=16384
```

### 4.7. Troubleshooting

**Problem:** Không nhận được sACN packets
- Check multicast join (use Wireshark)
- Verify IGMP enabled in lwIP
- Check firewall/router settings
- Ensure source is sending to correct multicast address

**Problem:** Packet validation fails
- Check CID (Component ID)
- Verify sequence number
- Check priority field

**Documentation:**
https://github.com/hhromic/libe131

---

## 5. ART-NET IMPLEMENTATION

Art-Net đơn giản hơn sACN, có 2 options:

### Option 1: Port existing library (e.g., hideakitai/ArtNet)
### Option 2: Custom minimal implementation (Recommended)

### 5.1. Art-Net Protocol Basics

**UDP Port:** 6454  
**Packet format:**

```c
typedef struct {
    uint8_t id[8];        // "Art-Net\0"
    uint16_t opcode;      // 0x5000 = ArtDmx
    uint16_t version;     // 14 (Art-Net v4)
    uint8_t sequence;     // Sequence number
    uint8_t physical;     // Physical port
    uint16_t universe;    // Net (7 bits) + Sub-net (4 bits) + Universe (4 bits)
    uint16_t length;      // DMX data length (2-512)
    uint8_t data[512];    // DMX data
} __attribute__((packed)) artnet_dmx_packet_t;
```

### 5.2. Minimal Custom Implementation

**artnet_receiver.h:**
```c
#ifndef ARTNET_RECEIVER_H
#define ARTNET_RECEIVER_H

#include <stdint.h>
#include "esp_err.h"

#define ARTNET_PORT 6454
#define ARTNET_OPCODE_DMX 0x5000
#define ARTNET_OPCODE_POLL 0x2000
#define ARTNET_OPCODE_POLL_REPLY 0x2100

typedef void (*artnet_dmx_callback_t)(uint16_t universe, 
                                       const uint8_t *data, 
                                       uint16_t length, 
                                       void *ctx);

esp_err_t artnet_receiver_init(void);
esp_err_t artnet_receiver_start(void);
esp_err_t artnet_receiver_stop(void);
esp_err_t artnet_receiver_set_callback(artnet_dmx_callback_t callback, void *ctx);

#endif
```

**artnet_receiver.c:**
```c
#include "artnet_receiver.h"
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "artnet";
static int artnet_socket = -1;
static artnet_dmx_callback_t dmx_callback = NULL;
static void *callback_ctx = NULL;

typedef struct {
    uint8_t id[8];
    uint16_t opcode;
    uint16_t version;
    uint8_t sequence;
    uint8_t physical;
    uint16_t universe;
    uint16_t length;
    uint8_t data[512];
} __attribute__((packed)) artnet_dmx_t;

static void artnet_task(void *arg) {
    uint8_t buffer[1024];
    struct sockaddr_in addr;
    socklen_t addr_len = sizeof(addr);
    
    while (1) {
        int len = recvfrom(artnet_socket, buffer, sizeof(buffer), 0,
                          (struct sockaddr*)&addr, &addr_len);
        
        if (len > 0 && len >= 18) {
            artnet_dmx_t *packet = (artnet_dmx_t*)buffer;
            
            // Verify Art-Net header
            if (memcmp(packet->id, "Art-Net\0", 8) == 0) {
                uint16_t opcode = packet->opcode;
                
                if (opcode == ARTNET_OPCODE_DMX && dmx_callback) {
                    uint16_t universe = packet->universe;
                    uint16_t length = (packet->length >> 8) | (packet->length << 8); // Big-endian
                    
                    if (length > 512) length = 512;
                    
                    dmx_callback(universe, packet->data, length, callback_ctx);
                    
                    ESP_LOGD(TAG, "Received ArtDmx: universe=%d, len=%d", 
                            universe, length);
                }
                else if (opcode == ARTNET_OPCODE_POLL) {
                    // TODO: Send ArtPollReply
                    ESP_LOGI(TAG, "Received ArtPoll");
                }
            }
        }
    }
}

esp_err_t artnet_receiver_init(void) {
    artnet_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (artnet_socket < 0) {
        ESP_LOGE(TAG, "Failed to create socket");
        return ESP_FAIL;
    }
    
    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port = htons(ARTNET_PORT),
        .sin_addr.s_addr = INADDR_ANY
    };
    
    if (bind(artnet_socket, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        ESP_LOGE(TAG, "Failed to bind socket");
        close(artnet_socket);
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "Art-Net receiver initialized on port %d", ARTNET_PORT);
    return ESP_OK;
}

esp_err_t artnet_receiver_start(void) {
    xTaskCreate(artnet_task, "artnet_task", 4096, NULL, 5, NULL);
    return ESP_OK;
}

esp_err_t artnet_receiver_set_callback(artnet_dmx_callback_t callback, void *ctx) {
    dmx_callback = callback;
    callback_ctx = ctx;
    return ESP_OK;
}
```

### 5.3. Usage Example

```c
void on_artnet_dmx(uint16_t universe, const uint8_t *data, 
                   uint16_t length, void *ctx) {
    uint8_t port = config_universe_to_port(universe);
    merge_engine_add_source(port, universe, data, 100);
    led_manager_pulse();
}

void app_main(void) {
    // ... network init ...
    
    artnet_receiver_init();
    artnet_receiver_set_callback(on_artnet_dmx, NULL);
    artnet_receiver_start();
}
```

### 5.4. Troubleshooting

**Problem:** Không nhận Art-Net packets
- Verify UDP port 6454 open
- Check broadcast/unicast address
- Use Wireshark để capture packets
- Some software gửi broadcast (255.255.255.255)

---

## 6. LITTLEFS INTEGRATION

**Component:** `esp_littlefs`  
**Repository:** https://github.com/joltwallet/esp_littlefs

### 6.1. Installation

**IDF Component Manager (Recommended):**
```bash
# Add to main/idf_component.yml
dependencies:
  esp_littlefs:
    git: https://github.com/joltwallet/esp_littlefs.git
```

**Or manual clone:**
```bash
cd components
git clone https://github.com/joltwallet/esp_littlefs.git esp_littlefs
```

### 6.2. Partition Table

**partitions.csv:**
```csv
# Name,     Type, SubType,  Offset,   Size,     Flags
nvs,        data, nvs,      0x9000,   0x4000,
phy_init,   data, phy,      0xf000,   0x1000,
factory,    app,  factory,  0x10000,  0x200000,
littlefs,   data, spiffs,   0x210000, 0x100000,
```

### 6.3. API Usage

```c
#include "esp_littlefs.h"

// Mount
esp_vfs_littlefs_conf_t conf = {
    .base_path = "/littlefs",
    .partition_label = "littlefs",
    .format_if_mount_failed = true,
    .dont_mount = false,
};

esp_err_t ret = esp_vfs_littlefs_register(&conf);
if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to mount LittleFS");
    return ret;
}

// Use standard file I/O
FILE *f = fopen("/littlefs/config.json", "r");
if (f) {
    fread(buffer, 1, sizeof(buffer), f);
    fclose(f);
}

// Write
FILE *f = fopen("/littlefs/config.json", "w");
fprintf(f, "{\"name\":\"test\"}");
fclose(f);

// Unmount
esp_vfs_littlefs_unregister("littlefs");
```

### 6.4. Configuration

```ini
CONFIG_LITTLEFS_MAX_PARTITIONS=3
CONFIG_LITTLEFS_PAGE_SIZE=256
CONFIG_LITTLEFS_OBJ_NAME_LEN=64
CONFIG_LITTLEFS_USE_MTIME=y
```

---

## 7. DEPENDENCY MANAGEMENT

### 7.1. Component Dependencies Graph

```
main
├── config_manager (REQUIRES: json nvs_flash storage_manager)
├── storage_manager (REQUIRES: esp_littlefs)
├── network_manager (REQUIRES: esp_eth esp_wifi esp_netif lwip)
├── led_manager (REQUIRES: led_strip driver)
├── dmx_handler (REQUIRES: esp-dmx driver)
├── artnet_receiver (REQUIRES: lwip)
├── sacn_receiver (REQUIRES: libe131 lwip)
├── merge_engine (REQUIRES: dmx_handler)
└── web_server (REQUIRES: esp_http_server json)
```

### 7.2. Root CMakeLists.txt

```cmake
cmake_minimum_required(VERSION 3.16)

# Add external components
list(APPEND EXTRA_COMPONENT_DIRS
    "external_components/esp-dmx"
    "external_components/libe131"
)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(esp-node-2rdm)
```

### 7.3. Main CMakeLists.txt

```cmake
idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES
        config_manager
        storage_manager
        network_manager
        led_manager
        dmx_handler
        artnet_receiver
        sacn_receiver
        merge_engine
        web_server
)
```

### 7.4. Update Dependencies

```bash
# Update all git submodules
git submodule update --remote --merge

# Or individual components
cd components/esp-dmx
git pull origin main
```

---

## PHỤ LỤC: CHECKLIST TÍCH HỢP THƯ VIỆN

- [ ] esp-dmx cloned vào components/
- [ ] libe131 cloned và tạo CMakeLists.txt
- [ ] Partition table có LittleFS partition
- [ ] sdkconfig.defaults có đầy đủ config
- [ ] Tất cả REQUIRES được khai báo trong CMakeLists.txt
- [ ] Build thành công không có warnings
- [ ] Test từng thư viện độc lập
- [ ] Verify API documentation

---

**Tài liệu này cập nhật theo tiến độ phát triển.**
