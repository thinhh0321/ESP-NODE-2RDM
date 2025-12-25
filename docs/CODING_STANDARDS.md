# QUY CHUẨN LẬP TRÌNH FIRMWARE

**Dự án:** Artnet-Node-2RDM  
**Tài liệu:** Coding Standards & Best Practices  
**Phiên bản:** 1.0  
**Ngày tạo:** 25/12/2025

---

## MỤC ĐÍCH

Tài liệu này quy định các tiêu chuẩn lập trình bắt buộc cho dự án ESP-NODE-2RDM nhằm:
- Đảm bảo code quality và maintainability
- Tránh các lỗi phổ biến và bugs
- Đồng nhất coding style trong toàn bộ dự án
- Tối ưu performance và memory usage
- Đảm bảo thread safety và real-time performance

---

## 1. NAMING CONVENTIONS

### 1.1. Files

```c
// Modules: lowercase với underscore
config_manager.c
config_manager.h
dmx_handler.c
network_manager.c

// Headers: .h extension
// Source: .c extension
// Private headers: _priv.h
dmx_handler_priv.h
```

### 1.2. Functions

```c
// Public API: module_action_object()
esp_err_t config_init(void);
esp_err_t dmx_send_frame(uint8_t port, const uint8_t *data);
bool network_is_connected(void);

// Private/Static: lowercase với underscore
static void parse_artnet_packet(const uint8_t *buffer, size_t len);
static int calculate_checksum(const uint8_t *data, size_t len);

// Getters: module_get_xxx()
const system_config_t* config_get(void);
uint16_t dmx_get_universe(uint8_t port);

// Setters: module_set_xxx()
esp_err_t config_set(const system_config_t *config);
esp_err_t dmx_set_channel(uint8_t port, uint16_t channel, uint8_t value);
```

### 1.3. Variables

```c
// Global variables: g_prefix (TRÁNH SỬ DỤNG KHI CÓ THỂ)
static system_config_t g_config;
static bool g_is_initialized = false;

// Local variables: lowercase với underscore
uint8_t dmx_buffer[512];
int packet_count = 0;
bool is_connected = false;

// Constants: UPPERCASE với underscore
#define MAX_DMX_CHANNELS     512
#define ARTNET_PORT          6454
#define DEFAULT_TIMEOUT_MS   3000
```

### 1.4. Types

```c
// Structs: xxx_t suffix
typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
} rgb_color_t;

typedef struct {
    uint16_t universe;
    uint8_t data[512];
} dmx_frame_t;

// Enums: xxx_t suffix, values UPPERCASE
typedef enum {
    DMX_MODE_OUT = 0,
    DMX_MODE_IN = 1,
    RDM_MODE_MASTER = 2,
    RDM_MODE_RESPONDER = 3
} dmx_mode_t;

// Callbacks: xxx_callback_t suffix
typedef void (*dmx_rx_callback_t)(uint8_t port, const uint8_t *data, 
                                  uint16_t length);
typedef void (*network_state_callback_t)(network_state_t new_state);
```

### 1.5. Macros

```c
// Function-like macros: UPPERCASE
#define MIN(a, b)  ((a) < (b) ? (a) : (b))
#define MAX(a, b)  ((a) > (b) ? (a) : (b))
#define ARRAY_SIZE(arr)  (sizeof(arr) / sizeof((arr)[0]))

// Configuration macros: MODULE_XXX
#define DMX_UART_NUM        UART_NUM_1
#define DMX_TX_PIN          GPIO_NUM_17
#define DMX_BAUD_RATE       250000

// Always use parentheses in macro expressions
#define CALCULATE_UNIVERSE(net, sub, uni) \
    (((net) << 8) | ((sub) << 4) | (uni))
```

---

## 2. CODE FORMATTING

### 2.1. Indentation

```c
// 4 spaces - KHÔNG dùng tabs
void function_name(void) {
    if (condition) {
        do_something();
    }
}
```

### 2.2. Braces

```c
// K&R style - opening brace trên cùng dòng
void function_name(void) {
    // code
}

if (condition) {
    // code
} else {
    // code
}

// Single line if - VẪN phải có braces
if (condition) {
    do_something();
}

// KHÔNG được viết:
if (condition) do_something();  // SAI!
```

### 2.3. Line Length

```c
// Maximum 100 characters per line
// Nếu dài hơn, break thành nhiều dòng

// Good:
esp_err_t dmx_send_frame(uint8_t port, 
                         const uint8_t *data,
                         uint16_t length);

// Bad:
esp_err_t dmx_send_frame(uint8_t port, const uint8_t *data, uint16_t length, bool force_update, uint32_t timeout);
```

### 2.4. Spacing

```c
// Operators: có space xung quanh
int result = a + b * c;
bool condition = (x == 5) && (y != 0);

// Function calls: không space trước (
function_name(arg1, arg2);

// Keywords: có space sau
if (condition)
for (int i = 0; i < 10; i++)
while (running)

// Pointers: * gần type
uint8_t *buffer;
const char *string;

// Function declarations: * gần function name
void (*callback)(int);
```

---

## 3. COMMENTS & DOCUMENTATION

### 3.1. File Headers

```c
/**
 * @file dmx_handler.c
 * @brief DMX512/RDM protocol handler implementation
 * 
 * This module handles DMX512 output, input and RDM protocol
 * for both ports with independent configuration.
 * 
 * @author Your Name
 * @date 25/12/2025
 */
```

### 3.2. Function Documentation

```c
/**
 * Send DMX frame to output port
 * 
 * @param port Port number (1 or 2)
 * @param data Pointer to 512-byte DMX data buffer
 * @param length Number of valid channels (1-512)
 * 
 * @return ESP_OK on success
 * @return ESP_ERR_INVALID_ARG if port invalid or data is NULL
 * @return ESP_ERR_INVALID_STATE if port not initialized
 * 
 * @note This function is thread-safe
 * @warning Data buffer must be at least 'length' bytes
 */
esp_err_t dmx_send_frame(uint8_t port, const uint8_t *data, uint16_t length);
```

### 3.3. Inline Comments

```c
// Good comments - explain WHY, not WHAT
// Use BREAK signal to reset DMX receivers before data
uart_set_line_inverse(uart, UART_SIGNAL_TXD_INV);
ets_delay_us(176);  // DMX BREAK time: minimum 88µs, we use 176µs

// Bad comments - stating the obvious
i++;  // Increment i  <- KHÔNG CẦN!
```

### 3.4. TODO/FIXME/NOTE

```c
// TODO(username): Add support for RDM bidirectional communication
// FIXME: Memory leak when config_save fails
// NOTE: This assumes little-endian byte order
// WARNING: Do not call from ISR context
```

---

## 4. ERROR HANDLING

### 4.1. Return Values

```c
// Always check return values
esp_err_t ret = config_save();
if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to save config: %s", esp_err_to_name(ret));
    return ret;
}

// KHÔNG được bỏ qua return value
config_save();  // SAI! Phải check return value
```

### 4.2. Parameter Validation

```c
esp_err_t dmx_send_frame(uint8_t port, const uint8_t *data, uint16_t length) {
    // Validate parameters FIRST
    if (port < 1 || port > 2) {
        ESP_LOGE(TAG, "Invalid port number: %d", port);
        return ESP_ERR_INVALID_ARG;
    }
    
    if (data == NULL) {
        ESP_LOGE(TAG, "Data pointer is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    
    if (length < 1 || length > 512) {
        ESP_LOGE(TAG, "Invalid length: %d", length);
        return ESP_ERR_INVALID_ARG;
    }
    
    // Proceed with implementation
    // ...
}
```

### 4.3. Assertions

```c
#include <assert.h>

// Use assertions for conditions that should NEVER happen
// Will crash in debug builds, noop in release
assert(buffer != NULL);
assert(port >= 1 && port <= 2);

// Use configASSERT for FreeRTOS
configASSERT(xSemaphoreTake(mutex, portMAX_DELAY) == pdTRUE);
```

### 4.4. Logging

```c
// Use appropriate log levels
ESP_LOGE(TAG, "Critical error: %s", error_msg);     // Error
ESP_LOGW(TAG, "Warning: retry %d/3", retry_count);  // Warning
ESP_LOGI(TAG, "Connected to %s", ssid);             // Info
ESP_LOGD(TAG, "Buffer size: %d", size);             // Debug
ESP_LOGV(TAG, "Entering function");                 // Verbose

// Always define TAG
static const char *TAG = "DMX_HANDLER";
```

---

## 5. MEMORY MANAGEMENT

### 5.1. Dynamic Allocation

```c
// TRÁNH malloc/free trong real-time tasks
// Allocate ở init phase, deallocate ở deinit

// Good: Allocate once at init
static uint8_t *dmx_buffer = NULL;

esp_err_t dmx_init(void) {
    dmx_buffer = malloc(512);
    if (dmx_buffer == NULL) {
        ESP_LOGE(TAG, "Failed to allocate DMX buffer");
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

// Always check malloc return value
uint8_t *buffer = malloc(size);
if (buffer == NULL) {
    // Handle error
    return ESP_ERR_NO_MEM;
}
```

### 5.2. Stack Usage

```c
// Tránh large arrays trên stack
void bad_function(void) {
    uint8_t buffer[4096];  // SAI! Too large for stack
    // ...
}

// Good: Use static or heap
void good_function(void) {
    static uint8_t buffer[4096];  // OK for static
    // hoặc
    uint8_t *buffer = malloc(4096);  // OK for heap
}
```

### 5.3. Buffer Overflows

```c
// Always check buffer bounds
void copy_data(uint8_t *dest, const uint8_t *src, size_t len, size_t max_len) {
    if (len > max_len) {
        ESP_LOGW(TAG, "Truncating data: %d > %d", len, max_len);
        len = max_len;
    }
    memcpy(dest, src, len);
}

// Use strncpy instead of strcpy
strncpy(dest, src, sizeof(dest) - 1);
dest[sizeof(dest) - 1] = '\0';  // Ensure null termination
```

### 5.4. Memory Leaks

```c
// Always free allocated memory
uint8_t *buffer = malloc(size);
// ... use buffer ...
free(buffer);
buffer = NULL;  // Good practice

// Be careful with early returns
esp_err_t process_data(void) {
    uint8_t *buffer = malloc(1024);
    if (buffer == NULL) {
        return ESP_ERR_NO_MEM;
    }
    
    if (some_error) {
        free(buffer);  // MUST free before return
        return ESP_ERR_INVALID_STATE;
    }
    
    // ... process ...
    
    free(buffer);
    return ESP_OK;
}
```

---

## 6. THREAD SAFETY

### 6.1. Mutexes

```c
static SemaphoreHandle_t config_mutex = NULL;

esp_err_t config_init(void) {
    config_mutex = xSemaphoreCreateMutex();
    if (config_mutex == NULL) {
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

esp_err_t config_set(const system_config_t *config) {
    if (xSemaphoreTake(config_mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        ESP_LOGE(TAG, "Failed to take mutex");
        return ESP_ERR_TIMEOUT;
    }
    
    // Critical section
    memcpy(&g_config, config, sizeof(system_config_t));
    
    xSemaphoreGive(config_mutex);
    return ESP_OK;
}
```

### 6.2. Atomic Operations

```c
// Use atomic operations for simple counters
#include <stdatomic.h>

static atomic_uint_fast32_t packet_count = 0;

void increment_packet_count(void) {
    atomic_fetch_add(&packet_count, 1);
}

uint32_t get_packet_count(void) {
    return atomic_load(&packet_count);
}
```

### 6.3. ISR Safety

```c
// Functions callable from ISR must be ISR-safe
void IRAM_ATTR gpio_isr_handler(void *arg) {
    // Use ISR-safe FreeRTOS functions
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xQueueSendFromISR(queue, &data, &xHigherPriorityTaskWoken);
    
    if (xHigherPriorityTaskWoken) {
        portYIELD_FROM_ISR();
    }
    
    // KHÔNG được gọi printf, malloc, ESP_LOG trong ISR
    // KHÔNG được block (delay, mutex)
}
```

---

## 7. PERFORMANCE

### 7.1. Optimization

```c
// Use const for read-only data
const uint8_t lookup_table[256] = { /* ... */ };

// Use inline for small frequently-called functions
static inline uint16_t calculate_universe(uint8_t net, uint8_t sub, uint8_t uni) {
    return ((net & 0x7F) << 8) | ((sub & 0x0F) << 4) | (uni & 0x0F);
}

// Mark functions with attributes when needed
__attribute__((always_inline)) static inline void critical_function(void) {
    // ...
}
```

### 7.2. Avoid Unnecessary Work

```c
// Cache frequently accessed values
static uint32_t cached_timestamp = 0;

uint32_t get_timestamp(void) {
    uint32_t now = esp_timer_get_time() / 1000;
    if (now != cached_timestamp) {
        // Update cache
        cached_timestamp = now;
    }
    return cached_timestamp;
}

// Early return for common cases
if (length == 0) {
    return ESP_OK;  // Nothing to do
}
```

### 7.3. Efficient Data Structures

```c
// Use appropriate data structures
// Array for indexed access: O(1)
// Linked list for insertions: O(1)
// Hash table for lookups: O(1) average

// Avoid linear search in hot paths
// Bad:
for (int i = 0; i < MAX_DEVICES; i++) {
    if (devices[i].uid == target_uid) {
        return &devices[i];
    }
}

// Better: Use hash table or binary search
```

---

## 8. CONFIGURATION

### 8.1. Magic Numbers

```c
// KHÔNG dùng magic numbers
if (port == 2) { ... }  // What is 2?

// Dùng constants hoặc enums
#define MAX_PORTS  2
if (port <= MAX_PORTS) { ... }

enum {
    PORT_1 = 1,
    PORT_2 = 2,
    MAX_PORTS = 2
};
```

### 8.2. Configuration Files

```c
// Centralize configuration in header files
// config_defaults.h

#define DEFAULT_AP_SSID            "ArtnetNode-XXXX"
#define DEFAULT_AP_PASSWORD        "12345678"
#define DEFAULT_MERGE_TIMEOUT_MS   3000
#define DEFAULT_DMX_RATE_HZ        44

// Compile-time options
#define ENABLE_RDMRESPONDER  1
#define ENABLE_DEBUG_LOGGING 0
```

---

## 9. TESTING & DEBUGGING

### 9.1. Testability

```c
// Write testable code - separate logic from I/O
// Bad:
void process_packet(void) {
    uint8_t buffer[1024];
    int len = recvfrom(socket, buffer, sizeof(buffer), 0, ...);
    // process buffer
}

// Good:
void process_packet(const uint8_t *buffer, size_t len) {
    // process buffer - can be tested without socket
}

void receive_task(void) {
    uint8_t buffer[1024];
    int len = recvfrom(socket, buffer, sizeof(buffer), 0, ...);
    if (len > 0) {
        process_packet(buffer, len);
    }
}
```

### 9.2. Debug Code

```c
// Use conditional compilation for debug code
#if CONFIG_DEBUG_MODE
    ESP_LOGD(TAG, "Entering function: %s", __func__);
    dump_buffer(buffer, length);
#endif

// Debug helper functions
#if CONFIG_DEBUG_MODE
static void dump_buffer(const uint8_t *buffer, size_t len) {
    for (size_t i = 0; i < len; i++) {
        printf("%02X ", buffer[i]);
        if ((i + 1) % 16 == 0) printf("\n");
    }
    printf("\n");
}
#endif
```

---

## 10. COMMON PITFALLS

### 10.1. Integer Overflow

```c
// Be careful with arithmetic
uint8_t a = 200;
uint8_t b = 100;
uint8_t sum = a + b;  // Overflow! sum = 44

// Use larger types for intermediate calculations
uint16_t sum = (uint16_t)a + (uint16_t)b;
```

### 10.2. Signed/Unsigned Comparison

```c
int count = get_count();
if (count < sizeof(buffer)) {  // Warning! signed vs unsigned
    // ...
}

// Fix: Cast appropriately
if ((size_t)count < sizeof(buffer)) {
    // ...
}
```

### 10.3. Uninitialized Variables

```c
// Always initialize variables
uint8_t status;  // SAI! Uninitialized
if (status == 0) { ... }

// Good:
uint8_t status = 0;
```

### 10.4. Race Conditions

```c
// Bad:
if (g_is_ready) {
    g_is_ready = false;  // Race condition!
    process();
}

// Good:
if (xSemaphoreTake(mutex, portMAX_DELAY) == pdTRUE) {
    if (g_is_ready) {
        g_is_ready = false;
        process();
    }
    xSemaphoreGive(mutex);
}
```

---

## 11. VERSION CONTROL

### 11.1. Commit Messages

```
Short summary (50 chars or less)

More detailed explanation if needed. Wrap at 72 characters.
Explain the problem that this commit is solving and why this 
approach was chosen.

- Bullet points are okay
- Use present tense: "Add feature" not "Added feature"

Closes #123
```

### 11.2. Code Review Checklist

- [ ] Code follows style guidelines
- [ ] All functions documented
- [ ] Parameters validated
- [ ] Return values checked
- [ ] No memory leaks
- [ ] Thread-safe where needed
- [ ] Tested on hardware
- [ ] No compiler warnings

---

## 12. BUILD & COMPILATION

### 12.1. Compiler Warnings

```cmake
# Enable all warnings in CMakeLists.txt
idf_component_register(
    SRCS "dmx_handler.c"
    INCLUDE_DIRS "."
    PRIV_REQUIRES esp_timer driver
)

target_compile_options(${COMPONENT_LIB} PRIVATE 
    -Wall 
    -Wextra 
    -Werror
)
```

### 12.2. No Warnings Policy

**QUY ĐỊNH: KHÔNG được commit code có compiler warnings!**

Tất cả warnings phải được fix trước khi commit.

---

## SUMMARY

Tuân thủ các quy chuẩn này sẽ đảm bảo:
- ✅ Code dễ đọc, dễ maintain
- ✅ Tránh bugs phổ biến
- ✅ Performance tối ưu
- ✅ Thread-safe và stable
- ✅ Dễ dàng debug và test

**LƯU Ý:** Tất cả code mới PHẢI tuân thủ 100% các quy chuẩn này!
