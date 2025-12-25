# THIẾT KẾ MODULE: DMX/RDM Handler

**Dự án:** Artnet-Node-2RDM  
**Module:** DMX512 & RDM Protocol Handler  
**Phiên bản:** 1.0  
**Ngày tạo:** 25/12/2025

---

## 1. Mục đích

Module DMX/RDM Handler quản lý 2 cổng DMX512/RDM độc lập, hỗ trợ:
- DMX512 Output (gửi DMX frame)
- DMX512 Input (nhận DMX frame)
- RDM Master mode (discovery, get/set parameters)
- RDM Responder mode (trả lời RDM requests)
- Port disable

---

## 2. Hardware Interface

### 2.1. GPIO Mapping

```c
// Port 1
#define DMX_PORT1_TX_PIN    GPIO_NUM_17
#define DMX_PORT1_RX_PIN    GPIO_NUM_16
#define DMX_PORT1_DIR_PIN   GPIO_NUM_21  // HIGH = TX, LOW = RX

// Port 2
#define DMX_PORT2_TX_PIN    GPIO_NUM_19
#define DMX_PORT2_RX_PIN    GPIO_NUM_18
#define DMX_PORT2_DIR_PIN   GPIO_NUM_20  // HIGH = TX, LOW = RX

// UART Assignment
#define DMX_PORT1_UART      UART_NUM_1
#define DMX_PORT2_UART      UART_NUM_2
```

### 2.2. UART Configuration

```c
// DMX512 standard
#define DMX_BAUD_RATE       250000
#define DMX_DATA_BITS       UART_DATA_8_BITS
#define DMX_PARITY          UART_PARITY_DISABLE
#define DMX_STOP_BITS       UART_STOP_BITS_2
#define DMX_FLOW_CTRL       UART_HW_FLOWCTRL_DISABLE

// Buffer sizes
#define DMX_TX_BUFFER_SIZE  513  // Start code + 512 channels
#define DMX_RX_BUFFER_SIZE  1024
```

---

## 3. Kiến trúc Module

### 3.1. Cấu trúc dữ liệu

```c
typedef enum {
    DMX_MODE_OUT = 0,
    DMX_MODE_IN = 1,
    RDM_MODE_MASTER = 2,
    RDM_MODE_RESPONDER = 3,
    DMX_MODE_DISABLED = 4
} dmx_mode_t;

typedef struct {
    uint8_t start_code;         // 0x00 for DMX, 0xCC for RDM
    uint8_t data[512];          // DMX data channels
} dmx_frame_t;

typedef struct {
    uint8_t port_num;           // 1 or 2
    dmx_mode_t mode;
    uint16_t universe;          // Primary universe
    int16_t universe_secondary; // Secondary universe (-1 = disabled)
    int16_t universe_offset;    // Offset within universe (-512 to +512)
    
    // Runtime state
    bool is_active;
    uint32_t frames_sent;
    uint32_t frames_received;
    uint32_t last_frame_time;   // Timestamp
    dmx_frame_t current_frame;
} dmx_port_t;

typedef struct {
    uint8_t uid[6];             // RDM Unique ID
    uint16_t device_model;
    uint32_t software_version;
    uint16_t dmx_start_address;
    uint8_t rdm_protocol_version;
} rdm_device_t;

typedef struct {
    uint8_t uid[6];             // Source/Dest UID
    uint8_t command_class;      // DISCOVERY, GET, SET
    uint16_t pid;               // Parameter ID
    uint8_t data[231];          // Parameter data
    uint8_t data_len;
} rdm_message_t;
```

---

## 4. API Public

### 4.1. Khởi tạo

```c
/**
 * Khởi tạo DMX/RDM module
 * @return ESP_OK nếu thành công
 */
esp_err_t dmx_init(void);

/**
 * Khởi tạo một port DMX
 * @param port Port number (1 or 2)
 * @param mode DMX mode
 * @param universe Primary universe
 * @return ESP_OK nếu thành công
 */
esp_err_t dmx_port_init(uint8_t port, dmx_mode_t mode, uint16_t universe);

/**
 * Dừng một port DMX
 * @param port Port number (1 or 2)
 * @return ESP_OK nếu thành công
 */
esp_err_t dmx_port_deinit(uint8_t port);
```

### 4.2. DMX Output

```c
/**
 * Gửi DMX frame
 * @param port Port number (1 or 2)
 * @param data Pointer to 512-byte DMX data
 * @return ESP_OK nếu thành công
 */
esp_err_t dmx_send_frame(uint8_t port, const uint8_t *data);

/**
 * Update một kênh DMX
 * @param port Port number
 * @param channel Channel number (1-512)
 * @param value Channel value (0-255)
 * @return ESP_OK nếu thành công
 */
esp_err_t dmx_set_channel(uint8_t port, uint16_t channel, uint8_t value);

/**
 * Update nhiều kênh DMX
 * @param port Port number
 * @param start_channel Start channel (1-512)
 * @param data Pointer to data
 * @param length Number of channels
 * @return ESP_OK nếu thành công
 */
esp_err_t dmx_set_channels(uint8_t port, uint16_t start_channel, 
                           const uint8_t *data, uint16_t length);

/**
 * Blackout (set all channels to 0)
 * @param port Port number
 * @return ESP_OK nếu thành công
 */
esp_err_t dmx_blackout(uint8_t port);
```

### 4.3. DMX Input

```c
/**
 * Đọc DMX frame từ input
 * @param port Port number (1 or 2)
 * @param data Buffer to store 512-byte DMX data
 * @param timeout_ms Timeout in milliseconds
 * @return ESP_OK nếu có frame mới
 */
esp_err_t dmx_receive_frame(uint8_t port, uint8_t *data, uint32_t timeout_ms);

/**
 * Đăng ký callback khi nhận được DMX frame
 * @param port Port number
 * @param callback Callback function
 * @param user_data User data
 */
void dmx_register_rx_callback(uint8_t port, dmx_rx_callback_t callback, 
                              void *user_data);
```

### 4.4. RDM Master

```c
/**
 * Bắt đầu RDM discovery trên port
 * @param port Port number
 * @param callback Callback khi tìm thấy device
 * @return ESP_OK nếu thành công
 */
esp_err_t rdm_start_discovery(uint8_t port, rdm_discovery_callback_t callback);

/**
 * RDM GET command
 * @param port Port number
 * @param uid Target device UID
 * @param pid Parameter ID
 * @param response Buffer to store response
 * @return ESP_OK nếu thành công
 */
esp_err_t rdm_get(uint8_t port, const uint8_t uid[6], uint16_t pid, 
                 rdm_message_t *response);

/**
 * RDM SET command
 * @param port Port number
 * @param uid Target device UID
 * @param pid Parameter ID
 * @param data Parameter data
 * @param data_len Data length
 * @return ESP_OK nếu thành công
 */
esp_err_t rdm_set(uint8_t port, const uint8_t uid[6], uint16_t pid,
                 const uint8_t *data, uint8_t data_len);

/**
 * Lấy danh sách RDM devices đã discover
 * @param port Port number
 * @param devices Buffer to store devices
 * @param max_devices Maximum number of devices
 * @return Number of devices found
 */
uint16_t rdm_get_devices(uint8_t port, rdm_device_t *devices, 
                        uint16_t max_devices);
```

### 4.5. RDM Responder

```c
/**
 * Set RDM Responder UID
 * @param port Port number
 * @param uid Device UID (6 bytes)
 * @return ESP_OK nếu thành công
 */
esp_err_t rdm_responder_set_uid(uint8_t port, const uint8_t uid[6]);

/**
 * Set RDM Responder parameter
 * @param port Port number
 * @param pid Parameter ID
 * @param data Parameter data
 * @param data_len Data length
 * @return ESP_OK nếu thành công
 */
esp_err_t rdm_responder_set_param(uint8_t port, uint16_t pid,
                                  const uint8_t *data, uint8_t data_len);

/**
 * Đăng ký handler cho RDM GET request
 * @param port Port number
 * @param pid Parameter ID
 * @param handler Handler function
 */
void rdm_responder_register_get_handler(uint8_t port, uint16_t pid,
                                       rdm_get_handler_t handler);

/**
 * Đăng ký handler cho RDM SET request
 * @param port Port number
 * @param pid Parameter ID
 * @param handler Handler function
 */
void rdm_responder_register_set_handler(uint8_t port, uint16_t pid,
                                       rdm_set_handler_t handler);
```

### 4.6. Status & Statistics

```c
/**
 * Lấy trạng thái port
 * @param port Port number
 * @param status Buffer to store status
 * @return ESP_OK nếu thành công
 */
esp_err_t dmx_get_port_status(uint8_t port, dmx_port_t *status);

/**
 * Reset statistics counter
 * @param port Port number
 * @return ESP_OK nếu thành công
 */
esp_err_t dmx_reset_stats(uint8_t port);
```

---

## 5. DMX512 Protocol

### 5.1. DMX Frame Structure

```
+-------+-------------+-------------+-----+-------------+
| BREAK | MAB | START | DATA[0]     | ... | DATA[511]   |
+-------+-----+-------+-------------+-----+-------------+
  88µs   8µs   0x00    Channel 1         Channel 512

BREAK: 88-1000µs LOW
MAB (Mark After Break): 8-1000µs HIGH
Start Code: 0x00 (DMX), 0xCC (RDM)
Data: 512 bytes (8 bits each)
MTBP (Mark Time Between Packets): ≥ 0µs
```

### 5.2. Timing

- Frame rate: ~44 Hz (22.7ms per frame)
- Break time: 176µs (2 × 88µs minimum)
- MAB time: 12µs
- Channel time: 44µs per byte
- Total frame time: ~22.7ms (176µs + 12µs + 513 × 44µs)

### 5.3. DMX Output Implementation

```c
static void dmx_output_task(void *pvParameters) {
    uint8_t port = (uint8_t)pvParameters;
    dmx_port_t *port_ctx = &dmx_ports[port - 1];
    
    while (1) {
        // Wait for new data or timeout
        if (xQueueReceive(port_ctx->tx_queue, &frame, pdMS_TO_TICKS(50))) {
            // Send BREAK
            gpio_set_level(port_ctx->dir_pin, 1);  // TX mode
            uart_set_line_inverse(port_ctx->uart, UART_SIGNAL_TXD_INV);
            uart_wait_tx_done(port_ctx->uart, pdMS_TO_TICKS(10));
            ets_delay_us(176);  // BREAK time
            
            // Send MAB
            uart_set_line_inverse(port_ctx->uart, 0);
            ets_delay_us(12);   // MAB time
            
            // Send Start Code + Data
            uint8_t start_code = 0x00;
            uart_write_bytes(port_ctx->uart, &start_code, 1);
            uart_write_bytes(port_ctx->uart, frame.data, 512);
            uart_wait_tx_done(port_ctx->uart, pdMS_TO_TICKS(50));
            
            port_ctx->frames_sent++;
            port_ctx->last_frame_time = esp_timer_get_time();
        } else {
            // Timeout - send last frame again (maintain DMX output)
            // Or blackout if merge timeout exceeded
        }
    }
}
```

### 5.4. DMX Input Implementation

```c
static void dmx_input_task(void *pvParameters) {
    uint8_t port = (uint8_t)pvParameters;
    dmx_port_t *port_ctx = &dmx_ports[port - 1];
    
    gpio_set_level(port_ctx->dir_pin, 0);  // RX mode
    
    uint8_t buffer[513];
    int len;
    
    while (1) {
        // Wait for BREAK detection
        uart_event_t event;
        if (xQueueReceive(port_ctx->uart_queue, &event, portMAX_DELAY)) {
            if (event.type == UART_BREAK) {
                // BREAK detected, read frame
                len = uart_read_bytes(port_ctx->uart, buffer, 513, 
                                     pdMS_TO_TICKS(50));
                
                if (len >= 513 && buffer[0] == 0x00) {
                    // Valid DMX frame
                    memcpy(port_ctx->current_frame.data, &buffer[1], 512);
                    port_ctx->frames_received++;
                    port_ctx->last_frame_time = esp_timer_get_time();
                    
                    // Trigger callback
                    if (port_ctx->rx_callback) {
                        port_ctx->rx_callback(port, &buffer[1], 512);
                    }
                } else if (len >= 1 && buffer[0] == 0xCC) {
                    // RDM packet
                    rdm_process_packet(port, &buffer[1], len - 1);
                }
            }
        }
    }
}
```

---

## 6. RDM Protocol

### 6.1. RDM Packet Structure

```
+----+-----+-----+-------+-----+------+-----+----------+----------+
| SC | SUB | ML  | DST   | SRC | TNO  | ... | PDL      | DATA     |
+----+-----+-----+-------+-----+------+-----+----------+----------+
0xCC  0x01  24+   6bytes  6B   1B     ...   1B         0-231B

SC: Start Code (0xCC)
SUB START CODE: 0x01
Message Length: 24 + PDL
Destination UID: 6 bytes
Source UID: 6 bytes
Transaction Number: 1 byte
Port ID: 1 byte
Message Count: 1 byte
Sub-Device: 2 bytes
Command Class: 1 byte (DISCOVERY/GET/SET/SET_RESPONSE/etc.)
Parameter ID: 2 bytes
PDL: Parameter Data Length
PD: Parameter Data (0-231 bytes)
Checksum: 2 bytes
```

### 6.2. Common RDM PIDs

| PID | Name | Description |
|-----|------|-------------|
| 0x0001 | DISC_UNIQUE_BRANCH | Discovery command |
| 0x0010 | DEVICE_INFO | Device information |
| 0x0020 | SOFTWARE_VERSION_LABEL | Software version |
| 0x0050 | DEVICE_MODEL_DESCRIPTION | Model name |
| 0x0051 | MANUFACTURER_LABEL | Manufacturer name |
| 0x00F0 | DMX_START_ADDRESS | DMX start address |
| 0x0080 | IDENTIFY_DEVICE | Identify mode on/off |

### 6.3. RDM Discovery

```c
static esp_err_t rdm_discover_devices(uint8_t port) {
    uint8_t lower_uid[6] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    uint8_t upper_uid[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    
    rdm_discovery_recursive(port, lower_uid, upper_uid);
    return ESP_OK;
}

static void rdm_discovery_recursive(uint8_t port, 
                                   uint8_t lower[6], 
                                   uint8_t upper[6]) {
    // Send DISC_UNIQUE_BRANCH
    rdm_message_t msg;
    msg.command_class = RDM_CC_DISCOVERY_COMMAND;
    msg.pid = RDM_PID_DISC_UNIQUE_BRANCH;
    memcpy(msg.data, lower, 6);
    memcpy(msg.data + 6, upper, 6);
    msg.data_len = 12;
    
    rdm_send_message(port, BROADCAST_UID, &msg);
    
    // Wait for response
    vTaskDelay(pdMS_TO_TICKS(3));  // RDM response time
    
    // Check response
    uint8_t response[24];
    int len = rdm_read_response(port, response, 24);
    
    if (len > 0) {
        // Device found
        uint8_t uid[6];
        rdm_decode_discovery_response(response, uid);
        
        if (memcmp(lower, upper, 6) == 0) {
            // Single device found
            rdm_add_device(port, uid);
        } else {
            // Multiple devices - split range
            uint8_t mid[6];
            rdm_uid_midpoint(lower, upper, mid);
            rdm_discovery_recursive(port, lower, mid);
            rdm_discovery_recursive(port, mid, upper);
        }
    }
}
```

---

## 7. Threading Model

- **DMX Output Task** (Priority: 10, Core: 1, Stack: 4KB) × 2 ports
  - Liên tục gửi DMX frames
  - Rate: ~44 Hz

- **DMX Input Task** (Priority: 10, Core: 1, Stack: 4KB) × 2 ports
  - Nhận DMX frames từ UART
  - Process RDM packets

- **RDM Discovery Task** (Priority: 8, Core: 1, Stack: 8KB)
  - Chạy khi triggered
  - Tự động terminate khi xong

---

## 8. Error Handling

| Lỗi | Hành động |
|-----|-----------|
| UART init fail | Log error, disable port |
| DMX framing error | Skip frame, continue |
| RDM checksum error | Drop packet, log warning |
| RDM timeout | Retry up to 3 times |
| Buffer overflow | Drop oldest data |

---

## 9. Dependencies

- **ESP-IDF Components**:
  - driver/uart
  - driver/gpio
  - freertos/task
  - freertos/queue

---

## 10. Testing Points

1. DMX Output - frame timing
2. DMX Output - data accuracy
3. DMX Input - frame reception
4. DMX Input - break detection
5. RDM Discovery - single device
6. RDM Discovery - multiple devices
7. RDM GET command
8. RDM SET command
9. RDM Responder - reply to discovery
10. RDM Responder - handle GET/SET
11. Port mode switching
12. Blackout functionality
13. Statistics counters

---

## 11. Memory Usage

- Port context: 1 KB × 2 = 2 KB
- TX/RX buffers: 2 KB × 2 = 4 KB
- Task stacks: 12 KB × 2 ports = 24 KB
- RDM device list: 32 devices × 32 bytes = 1 KB
- **Total: ~31 KB**

---

## 12. Performance

- DMX frame rate: 44 Hz
- DMX latency: < 1ms (from data to TX)
- RDM response time: 2-5ms
- RDM discovery time: ~2s for 32 devices
