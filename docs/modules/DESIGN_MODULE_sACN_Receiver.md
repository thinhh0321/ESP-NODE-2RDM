# THIẾT KẾ MODULE: sACN Receiver

**Dự án:** Artnet-Node-2RDM  
**Module:** sACN (E1.31) Protocol Receiver  
**Phiên bản:** 1.0  
**Ngày tạo:** 25/12/2025

---

## 1. Mục đích

Module sACN Receiver nhận và xử lý các gói tin sACN (Streaming ACN / E1.31) từ mạng:
- Nhận DMX data qua multicast
- Hỗ trợ priority
- Hỗ trợ preview data
- Source name tracking
- Sequence number checking

---

## 2. sACN Protocol Overview

**Standard:** ANSI E1.31 (Streaming ACN)  
**Port:** UDP 5568  
**Multicast:** 239.255.0.0 - 239.255.255.255

---

## 3. Packet Structure

### 3.1. sACN Data Packet

```c
// Root Layer
typedef struct {
    uint16_t preamble_size;     // 0x0010
    uint16_t postamble_size;    // 0x0000
    uint8_t acn_pid[12];        // ACN Packet Identifier
    uint16_t flags_length;      // Flags + Length
    uint32_t vector;            // 0x00000004 (DATA)
    uint8_t cid[16];            // Component ID (UUID)
} sacn_root_layer_t;

// Framing Layer
typedef struct {
    uint16_t flags_length;      // Flags + Length
    uint32_t vector;            // 0x00000002 (DMP)
    char source_name[64];       // Source name (UTF-8)
    uint8_t priority;           // Priority (0-200)
    uint16_t sync_address;      // Sync universe
    uint8_t sequence_number;    // Sequence number
    uint8_t options;            // Options (preview, terminated)
    uint16_t universe;          // Universe (1-63999)
} sacn_framing_layer_t;

// DMP Layer
typedef struct {
    uint16_t flags_length;      // Flags + Length
    uint8_t vector;             // 0x02
    uint8_t address_type;       // 0xA1
    uint16_t first_address;     // 0x0000
    uint16_t address_increment; // 0x0001
    uint16_t property_count;    // 1 + 512
    uint8_t start_code;         // 0x00 (DMX)
    uint8_t data[512];          // DMX data
} sacn_dmp_layer_t;

// Complete packet
typedef struct {
    sacn_root_layer_t root;
    sacn_framing_layer_t framing;
    sacn_dmp_layer_t dmp;
} sacn_packet_t;
```

---

## 4. API Public

### 4.1. Khởi tạo

```c
/**
 * Khởi tạo sACN Receiver
 * @return ESP_OK nếu thành công
 */
esp_err_t sacn_init(void);

/**
 * Start sACN receiver
 * @return ESP_OK nếu thành công
 */
esp_err_t sacn_start(void);

/**
 * Stop sACN receiver
 * @return ESP_OK nếu thành công
 */
esp_err_t sacn_stop(void);
```

### 4.2. Universe Management

```c
/**
 * Subscribe to a universe
 * @param universe Universe number (1-63999)
 * @return ESP_OK nếu thành công
 */
esp_err_t sacn_subscribe_universe(uint16_t universe);

/**
 * Unsubscribe from a universe
 * @param universe Universe number
 * @return ESP_OK nếu thành công
 */
esp_err_t sacn_unsubscribe_universe(uint16_t universe);

/**
 * Subscribe to universes for both ports
 * @return ESP_OK nếu thành công
 */
esp_err_t sacn_subscribe_configured_universes(void);
```

### 4.3. Callbacks

```c
/**
 * Đăng ký callback khi nhận sACN data
 * @param callback Callback function
 * @param user_data User data
 */
void sacn_register_data_callback(sacn_data_callback_t callback,
                                 void *user_data);
```

### 4.4. Statistics

```c
/**
 * Lấy thống kê sACN
 * @param stats Buffer to store statistics
 * @return ESP_OK nếu thành công
 */
esp_err_t sacn_get_stats(sacn_stats_t *stats);

/**
 * Reset statistics
 * @return ESP_OK nếu thành công
 */
esp_err_t sacn_reset_stats(void);

/**
 * Get active sources for a universe
 * @param universe Universe number
 * @param sources Buffer to store source info
 * @param max_sources Maximum sources
 * @return Number of active sources
 */
uint16_t sacn_get_active_sources(uint16_t universe, 
                                sacn_source_info_t *sources,
                                uint16_t max_sources);
```

---

## 5. Implementation

### 5.1. Multicast Setup

```c
static int sacn_socket = -1;

static esp_err_t sacn_create_socket(void) {
    sacn_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sacn_socket < 0) {
        return ESP_FAIL;
    }
    
    // Set socket options
    int reuse = 1;
    setsockopt(sacn_socket, SOL_SOCKET, SO_REUSEADDR,
               &reuse, sizeof(reuse));
    
    // Bind to sACN port
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(SACN_PORT);
    
    if (bind(sacn_socket, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close(sacn_socket);
        return ESP_FAIL;
    }
    
    return ESP_OK;
}

static esp_err_t sacn_join_multicast(uint16_t universe) {
    if (universe < 1 || universe > 63999) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // Calculate multicast address
    // 239.255.0.0 + universe (1-63999)
    // Universe 1 = 239.255.0.1
    // Universe 256 = 239.255.1.0
    struct ip_mreq mreq;
    mreq.imr_multiaddr.s_addr = htonl(0xEFFF0000 | universe);
    mreq.imr_interface.s_addr = INADDR_ANY;
    
    if (setsockopt(sacn_socket, IPPROTO_IP, IP_ADD_MEMBERSHIP,
                   &mreq, sizeof(mreq)) < 0) {
        return ESP_FAIL;
    }
    
    return ESP_OK;
}

static esp_err_t sacn_leave_multicast(uint16_t universe) {
    struct ip_mreq mreq;
    mreq.imr_multiaddr.s_addr = htonl(0xEFFF0000 | universe);
    mreq.imr_interface.s_addr = INADDR_ANY;
    
    setsockopt(sacn_socket, IPPROTO_IP, IP_DROP_MEMBERSHIP,
               &mreq, sizeof(mreq));
    
    return ESP_OK;
}
```

### 5.2. Receive Task

```c
static void sacn_receive_task(void *pvParameters) {
    uint8_t buffer[SACN_MAX_PACKET_SIZE];
    struct sockaddr_in source_addr;
    socklen_t addr_len = sizeof(source_addr);
    
    while (1) {
        int len = recvfrom(sacn_socket, buffer, sizeof(buffer), 0,
                          (struct sockaddr*)&source_addr, &addr_len);
        
        if (len > 0) {
            sacn_stats.total_packets++;
            
            // Verify sACN packet
            if (!sacn_validate_packet(buffer, len)) {
                sacn_stats.invalid_packets++;
                continue;
            }
            
            // Parse packet
            sacn_packet_t *packet = (sacn_packet_t*)buffer;
            
            // Extract info
            uint16_t universe = ntohs(packet->framing.universe);
            uint8_t sequence = packet->framing.sequence_number;
            uint8_t priority = packet->framing.priority;
            uint8_t options = packet->framing.options;
            
            // Check preview flag
            bool is_preview = (options & SACN_OPTION_PREVIEW) != 0;
            
            // Check terminated flag
            bool is_terminated = (options & SACN_OPTION_TERMINATED) != 0;
            
            if (is_terminated) {
                // Source terminated - remove from tracking
                sacn_remove_source(universe, source_addr.sin_addr.s_addr);
                continue;
            }
            
            // Check if this universe is subscribed
            if (!sacn_is_subscribed(universe)) {
                continue;
            }
            
            // Handle data
            handle_sacn_data(universe, packet->dmp.data, sequence,
                           priority, packet->framing.source_name,
                           source_addr.sin_addr.s_addr, is_preview);
            
            sacn_stats.data_packets++;
            
            // LED pulse
            if (!is_preview) {
                led_manager_pulse((rgb_color_t){255, 255, 255});
            }
        }
    }
}
```

### 5.3. Packet Validation

```c
static bool sacn_validate_packet(uint8_t *buffer, int len) {
    if (len < sizeof(sacn_packet_t)) {
        return false;
    }
    
    sacn_packet_t *packet = (sacn_packet_t*)buffer;
    
    // Check preamble
    if (ntohs(packet->root.preamble_size) != 0x0010) {
        return false;
    }
    
    // Check ACN PID
    static const uint8_t acn_pid[12] = {
        0x41, 0x53, 0x43, 0x2D, 0x45, 0x31, 
        0x2E, 0x31, 0x37, 0x00, 0x00, 0x00
    };
    if (memcmp(packet->root.acn_pid, acn_pid, 12) != 0) {
        return false;
    }
    
    // Check root vector
    if (ntohl(packet->root.vector) != SACN_ROOT_VECTOR_DATA) {
        return false;
    }
    
    // Check framing vector
    if (ntohl(packet->framing.vector) != SACN_FRAMING_VECTOR_DATA) {
        return false;
    }
    
    // Check DMP vector
    if (packet->dmp.vector != SACN_DMP_VECTOR_DATA) {
        return false;
    }
    
    // Check start code
    if (packet->dmp.start_code != 0x00) {
        return false;  // Not DMX512
    }
    
    return true;
}
```

### 5.4. Data Handler

```c
static void handle_sacn_data(uint16_t universe, const uint8_t *data,
                            uint8_t sequence, uint8_t priority,
                            const char *source_name, uint32_t source_ip,
                            bool is_preview) {
    // Skip preview data (unless configured to use it)
    if (is_preview && !sacn_config.use_preview) {
        return;
    }
    
    // Check sequence number
    sacn_source_t *source = sacn_find_or_create_source(universe, source_ip);
    if (source) {
        // Check for sequence jump
        uint8_t expected_seq = (source->last_sequence + 1) & 0xFF;
        if (sequence != expected_seq && source->last_sequence != 0) {
            // Sequence jump detected
            sacn_stats.sequence_errors++;
        }
        source->last_sequence = sequence;
    }
    
    // Find which port this universe belongs to
    for (int port = 1; port <= 2; port++) {
        uint16_t primary = config_get()->universe_primary_port[port-1];
        int16_t secondary = config_get()->universe_secondary_port[port-1];
        
        if (universe == primary || universe == secondary) {
            // Push to merge engine
            merge_engine_push_sacn(port, universe, data, sequence,
                                  priority, source_name, source_ip);
            
            // Callback
            if (sacn_data_callback) {
                sacn_data_callback(port, universe, data, 512,
                                 priority, source_name);
            }
        }
    }
}
```

### 5.5. Source Tracking

```c
#define MAX_SOURCES_PER_UNIVERSE  8

typedef struct {
    uint32_t source_ip;
    char source_name[64];
    uint8_t cid[16];
    uint8_t last_sequence;
    uint64_t last_packet_time;
    uint8_t priority;
    bool is_active;
} sacn_source_t;

typedef struct {
    uint16_t universe;
    sacn_source_t sources[MAX_SOURCES_PER_UNIVERSE];
    uint8_t source_count;
} sacn_universe_t;

static sacn_source_t* sacn_find_or_create_source(uint16_t universe,
                                                 uint32_t source_ip) {
    sacn_universe_t *uni = sacn_find_universe(universe);
    if (!uni) {
        return NULL;
    }
    
    // Find existing source
    for (int i = 0; i < uni->source_count; i++) {
        if (uni->sources[i].source_ip == source_ip) {
            uni->sources[i].last_packet_time = esp_timer_get_time();
            uni->sources[i].is_active = true;
            return &uni->sources[i];
        }
    }
    
    // Create new source
    if (uni->source_count < MAX_SOURCES_PER_UNIVERSE) {
        sacn_source_t *src = &uni->sources[uni->source_count];
        src->source_ip = source_ip;
        src->last_packet_time = esp_timer_get_time();
        src->last_sequence = 0;
        src->is_active = true;
        uni->source_count++;
        return src;
    }
    
    return NULL;
}

static void sacn_cleanup_timeout_sources(void) {
    uint64_t now = esp_timer_get_time();
    uint64_t timeout = 2500000;  // 2.5 seconds (E1.31 spec)
    
    for (int i = 0; i < sacn_universe_count; i++) {
        sacn_universe_t *uni = &sacn_universes[i];
        
        for (int j = 0; j < uni->source_count; j++) {
            if (uni->sources[j].is_active) {
                uint64_t elapsed = now - uni->sources[j].last_packet_time;
                if (elapsed > timeout) {
                    uni->sources[j].is_active = false;
                }
            }
        }
    }
}
```

---

## 6. Priority Handling

sACN priority (0-200):
- 0: Lowest priority
- 100: Default priority
- 200: Highest priority

Priority được sử dụng trong Merge Engine để quyết định nguồn nào được ưu tiên.

---

## 7. Sequence Number

- 8-bit counter (0-255)
- Tăng dần cho mỗi packet
- Wrap around từ 255 về 0
- Dùng để phát hiện packet loss và out-of-order

---

## 8. Threading Model

- **sACN Receive Task** (Priority: 8, Core: 0, Stack: 4KB)
  - Receive multicast packets
  - Parse and validate
  - Dispatch to merge engine
  
- **sACN Cleanup Task** (Priority: 3, Core: 0, Stack: 2KB)
  - Cleanup timeout sources (every 1 second)

---

## 9. Error Handling

| Lỗi | Hành động |
|-----|-----------|
| Socket create fail | Retry 3 times, log error |
| Multicast join fail | Log error, continue |
| Invalid packet | Drop, increment counter |
| Sequence error | Log warning, accept packet |
| Source timeout | Mark inactive, don't remove |

---

## 10. Dependencies

- **ESP-IDF Components**:
  - lwip/sockets
  - freertos/task
- **Internal Modules**:
  - Network (interface)
  - Merge Engine (DMX data)
  - Configuration (universes)
  - LED Manager (pulse)

---

## 11. Testing Points

1. Receive sACN data - single universe
2. Receive sACN data - multiple universes
3. Multicast join/leave
4. Priority handling
5. Sequence number validation
6. Source tracking
7. Source timeout detection
8. Preview data handling
9. Terminated stream handling
10. Multiple sources same universe
11. Packet validation
12. Statistics accuracy

---

## 12. Memory Usage

- Socket buffers: 8 KB
- Task stacks: 6 KB
- Universe tracking: 10 universes × 512 bytes = 5 KB
- Source tracking: 10 uni × 8 src × 128 bytes = 10 KB
- **Total: ~29 KB**

---

## 13. Performance

- Max packet rate: ~1000 packets/sec
- Latency: < 2ms (from UDP to merge engine)
- CPU usage: ~5-10%

---

## 14. Constants

```c
#define SACN_PORT                   5568
#define SACN_MAX_PACKET_SIZE        638  // Root + Framing + DMP
#define SACN_MULTICAST_BASE         0xEFFF0000  // 239.255.0.0

#define SACN_ROOT_VECTOR_DATA       0x00000004
#define SACN_FRAMING_VECTOR_DATA    0x00000002
#define SACN_DMP_VECTOR_DATA        0x02

#define SACN_OPTION_PREVIEW         0x80
#define SACN_OPTION_TERMINATED      0x40
#define SACN_OPTION_FORCE_SYNC      0x20

#define SACN_PRIORITY_DEFAULT       100
#define SACN_PRIORITY_MIN           0
#define SACN_PRIORITY_MAX           200

#define SACN_SOURCE_TIMEOUT_MS      2500  // Per E1.31 spec
```
