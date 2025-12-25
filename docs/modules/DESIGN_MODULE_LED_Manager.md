# THIẾT KẾ MODULE: LED Manager

**Dự án:** Artnet-Node-2RDM  
**Module:** LED Status Manager  
**Phiên bản:** 1.0  
**Ngày tạo:** 25/12/2025

---

## 1. Mục đích

Module LED Manager điều khiển WS2812 LED để hiển thị trạng thái hoạt động của thiết bị:
- Trạng thái boot
- Trạng thái mạng (Ethernet, WiFi STA, WiFi AP)
- Hoạt động nhận/gửi DMX
- RDM discovery
- Lỗi hệ thống

---

## 2. Hardware Interface

```c
#define WS2812_GPIO     GPIO_NUM_48
#define WS2812_COUNT    1            // Số lượng LED
#define WS2812_RMT_CHANNEL  0        // RMT channel
```

**Driver:** ESP-IDF RMT (Remote Control) peripheral

---

## 3. Kiến trúc Module

### 3.1. Cấu trúc dữ liệu

```c
typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
} rgb_color_t;

typedef enum {
    LED_STATE_BOOT = 0,
    LED_STATE_ETHERNET_CONNECTED,
    LED_STATE_WIFI_STA_CONNECTED,
    LED_STATE_WIFI_AP_ACTIVE,
    LED_STATE_DATA_RECEIVING,
    LED_STATE_RDM_DISCOVERY,
    LED_STATE_ERROR
} led_state_t;

typedef enum {
    LED_BEHAVIOR_STATIC = 0,        // Tĩnh
    LED_BEHAVIOR_SLOW_BLINK,        // Nhấp chậm 1Hz
    LED_BEHAVIOR_FAST_BLINK,        // Nhấp nhanh 5Hz
    LED_BEHAVIOR_PULSE,             // Nhấp 1 lần khi có event
    LED_BEHAVIOR_BREATH             // Breathing effect
} led_behavior_t;

typedef struct {
    led_state_t state;
    rgb_color_t color;
    led_behavior_t behavior;
    uint32_t duration_ms;           // Thời gian hiển thị (0 = vô hạn)
    uint8_t priority;               // 0-255, càng thấp càng ưu tiên
} led_event_t;
```

---

## 4. API Public

### 4.1. Khởi tạo

```c
/**
 * Khởi tạo LED Manager
 * @return ESP_OK nếu thành công
 */
esp_err_t led_manager_init(void);

/**
 * Dừng LED Manager
 * @return ESP_OK nếu thành công
 */
esp_err_t led_manager_deinit(void);
```

### 4.2. Set LED State

```c
/**
 * Set trạng thái LED (sử dụng preset)
 * @param state Trạng thái LED
 * @return ESP_OK nếu thành công
 */
esp_err_t led_manager_set_state(led_state_t state);

/**
 * Set LED với màu và behavior tùy chỉnh
 * @param color Màu RGB
 * @param behavior Hành vi (static, blink, pulse, etc.)
 * @param duration_ms Thời gian hiển thị (0 = vô hạn)
 * @param priority Priority (0 = cao nhất)
 * @return ESP_OK nếu thành công
 */
esp_err_t led_manager_set_custom(rgb_color_t color,
                                 led_behavior_t behavior,
                                 uint32_t duration_ms,
                                 uint8_t priority);
```

### 4.3. Event-based Control

```c
/**
 * Trigger LED pulse (nhấp nháy 1 lần ngắn)
 * Dùng khi có data packet, RDM response, etc.
 * @param color Màu pulse
 * @return ESP_OK nếu thành công
 */
esp_err_t led_manager_pulse(rgb_color_t color);

/**
 * Trigger error blink (nhấp nhanh màu đỏ)
 * @param duration_ms Thời gian blink
 * @return ESP_OK nếu thành công
 */
esp_err_t led_manager_error_blink(uint32_t duration_ms);
```

### 4.4. Direct Control

```c
/**
 * Set màu LED trực tiếp (bypass state machine)
 * @param color Màu RGB
 * @return ESP_OK nếu thành công
 */
esp_err_t led_manager_set_color_direct(rgb_color_t color);

/**
 * Tắt LED
 * @return ESP_OK nếu thành công
 */
esp_err_t led_manager_off(void);
```

---

## 5. Preset LED States

| State | Màu RGB | Behavior | Mô tả |
|-------|---------|----------|-------|
| BOOT | (0, 50, 100) | Static | Xanh dương nhạt |
| ETHERNET_CONNECTED | (0, 255, 0) | Static | Xanh lá tĩnh |
| WIFI_STA_CONNECTED | (0, 200, 0) | Slow Blink | Xanh lá nhấp chậm |
| WIFI_AP_ACTIVE | (128, 0, 128) | Static | Tím tĩnh |
| DATA_RECEIVING | (255, 255, 255) | Pulse | Trắng nhấp 1 lần |
| RDM_DISCOVERY | (255, 200, 0) | Slow Blink | Vàng nhấp chậm |
| ERROR | (255, 0, 0) | Fast Blink | Đỏ nhấp nhanh |

---

## 6. Luồng hoạt động

### 6.1. State Machine

```
1. Boot:
   └─> LED = BOOT (xanh dương)
   
2. Network connecting:
   └─> LED giữ nguyên BOOT
   
3. Ethernet connected:
   └─> LED = ETHERNET_CONNECTED (xanh lá tĩnh)
   
4. WiFi STA connected:
   └─> LED = WIFI_STA_CONNECTED (xanh lá nhấp)
   
5. WiFi AP active:
   └─> LED = WIFI_AP_ACTIVE (tím)
   
6. Nhận Art-Net/sACN packet:
   └─> LED pulse trắng 50ms (max 10Hz để tránh nhấp quá nhanh)
   └─> Return to network state
   
7. RDM discovery:
   └─> LED = RDM_DISCOVERY (vàng nhấp) trong suốt quá trình
   └─> Return to network state khi xong
   
8. Error:
   └─> LED = ERROR (đỏ nhấp nhanh)
   └─> Auto return to network state sau 5s
```

### 6.2. Priority System

- LED state có priority
- Event với priority cao hơn (số nhỏ hơn) sẽ override event hiện tại
- Event với duration > 0 sẽ tự động expire và trở về state trước đó

**Priority levels:**
- 0: Error (cao nhất)
- 1: RDM Discovery
- 2: Data Receiving (pulse)
- 3: Network states
- 4: Boot

---

## 7. Threading Model

- **LED Task** (Priority: 3, Core: 1):
  - Chạy liên tục
  - Process LED state queue
  - Update WS2812 via RMT
  - Handle blinking/breathing timing

- **Queue**:
  - Size: 10 events
  - Timeout: 100ms

---

## 8. Timing Parameters

```c
#define LED_PULSE_DURATION_MS       50      // Pulse duration
#define LED_SLOW_BLINK_PERIOD_MS    1000    // 1Hz
#define LED_FAST_BLINK_PERIOD_MS    200     // 5Hz
#define LED_BREATH_PERIOD_MS        2000    // 0.5Hz
#define LED_DATA_PULSE_MAX_RATE_HZ  10      // Max pulse rate
```

---

## 9. Implementation Details

### 9.1. WS2812 Driver

```c
/**
 * Initialize RMT for WS2812
 */
static esp_err_t ws2812_init(void);

/**
 * Set WS2812 color
 * @param r Red (0-255)
 * @param g Green (0-255)
 * @param b Blue (0-255)
 */
static void ws2812_set_color(uint8_t r, uint8_t g, uint8_t b);

/**
 * WS2812 timing (T0H, T0L, T1H, T1L)
 * Based on WS2812 datasheet
 */
#define WS2812_T0H_NS   350
#define WS2812_T0L_NS   900
#define WS2812_T1H_NS   900
#define WS2812_T1L_NS   350
#define WS2812_RESET_US 50
```

### 9.2. Blink Implementation

```c
static void led_task(void *pvParameters) {
    led_event_t current_event;
    uint32_t blink_state = 0;
    uint32_t last_update = 0;
    
    while (1) {
        // Check for new events
        if (xQueueReceive(led_queue, &current_event, pdMS_TO_TICKS(10))) {
            // Process new event
        }
        
        // Update LED based on behavior
        uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;
        
        switch (current_event.behavior) {
            case LED_BEHAVIOR_STATIC:
                // Just set color once
                break;
                
            case LED_BEHAVIOR_SLOW_BLINK:
            case LED_BEHAVIOR_FAST_BLINK:
                // Toggle on/off based on period
                if (now - last_update > period / 2) {
                    blink_state = !blink_state;
                    ws2812_set_color(blink_state ? r : 0, 
                                     blink_state ? g : 0, 
                                     blink_state ? b : 0);
                    last_update = now;
                }
                break;
                
            case LED_BEHAVIOR_PULSE:
                // One-shot pulse
                ws2812_set_color(r, g, b);
                vTaskDelay(pdMS_TO_TICKS(duration));
                // Return to previous state
                break;
                
            case LED_BEHAVIOR_BREATH:
                // Sinusoidal brightness
                float brightness = (sin(2 * PI * now / period) + 1) / 2;
                ws2812_set_color(r * brightness, g * brightness, b * brightness);
                break;
        }
        
        // Check for expiration
        if (current_event.duration_ms > 0) {
            if (now - event_start_time > current_event.duration_ms) {
                // Restore previous state
            }
        }
    }
}
```

---

## 10. Rate Limiting

- Data pulse events được rate-limited để tránh nhấp nháy quá nhanh
- Max rate: 10 Hz (100ms giữa các pulse)
- Sử dụng timestamp để track pulse cuối

```c
static uint32_t last_data_pulse_time = 0;

esp_err_t led_manager_pulse(rgb_color_t color) {
    uint32_t now = esp_timer_get_time() / 1000; // ms
    
    if (now - last_data_pulse_time < 100) {
        // Skip pulse - too soon
        return ESP_OK;
    }
    
    last_data_pulse_time = now;
    // Trigger pulse
    return ESP_OK;
}
```

---

## 11. Error Handling

| Lỗi | Hành động |
|-----|-----------|
| RMT init fail | Log error, LED disabled |
| WS2812 not responding | Log warning, continue |
| Queue full | Drop oldest event |

---

## 12. Dependencies

- **ESP-IDF Components**:
  - driver/rmt
  - freertos/task
  - freertos/queue

---

## 13. Testing Points

1. Boot LED color
2. Ethernet connected state
3. WiFi STA connected state  
4. WiFi AP state
5. Data pulse on packet receive
6. Data pulse rate limiting
7. RDM discovery blink
8. Error blink
9. Priority override
10. Timed event expiration
11. Blink frequency accuracy
12. Color accuracy

---

## 14. Memory Usage

- Heap: ~2 KB
- Stack (task): 2 KB
- Queue: 10 events × 24 bytes = 240 bytes

---

## 15. Performance

- LED update rate: up to 100 Hz
- Latency: < 10ms từ event đến LED update
- CPU usage: < 1%

---

## 16. Configuration

```c
// LED brightness (0-255)
#define LED_BRIGHTNESS_BOOT       80
#define LED_BRIGHTNESS_NORMAL     255
#define LED_BRIGHTNESS_ERROR      255
```

Brightness có thể điều chỉnh để giảm độ sáng nếu cần.
