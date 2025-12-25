# THIẾT KẾ MODULE: Art-Net Receiver

**Dự án:** Artnet-Node-2RDM  
**Module:** Art-Net Protocol Receiver  
**Phiên bản:** 1.0  
**Ngày tạo:** 25/12/2025

---

## 1. Mục đích

Module Art-Net Receiver nhận và xử lý các gói tin Art-Net từ mạng:
- ArtDmx: DMX data packets
- ArtPoll: Discovery requests
- ArtPollReply: Node information
- ArtSync: Synchronization
- ArtAddress: Remote configuration

---

## 2. Art-Net Protocol Overview

**Version:** Art-Net 4  
**Port:** UDP 6454  
**Broadcast:** 2.255.255.255 or 10.255.255.255

---

## 3. Packet Types

### 3.1. ArtDmx

```c
typedef struct {
    uint8_t id[8];          // "Art-Net\0"
    uint16_t opcode;        // 0x5000
    uint8_t prot_ver_hi;    // 0
    uint8_t prot_ver_lo;    // 14
    uint8_t sequence;       // Sequence number
    uint8_t physical;       // Physical port
    uint16_t universe;      // Universe (15 bit)
    uint16_t length;        // Data length (2-512)
    uint8_t data[512];      // DMX data
} artnet_dmx_t;
```

### 3.2. ArtPoll

```c
typedef struct {
    uint8_t id[8];          // "Art-Net\0"
    uint16_t opcode;        // 0x2000
    uint8_t prot_ver_hi;    // 0
    uint8_t prot_ver_lo;    // 14
    uint8_t flags;          // Talk flags
    uint8_t priority;       // Diagnostics priority
} artnet_poll_t;
```

### 3.3. ArtPollReply

```c
typedef struct {
    uint8_t id[8];              // "Art-Net\0"
    uint16_t opcode;            // 0x2100
    uint8_t ip[4];              // IP address
    uint16_t port;              // Port (6454)
    uint16_t version_info;      // Firmware version
    uint8_t net_switch;         // Net
    uint8_t sub_switch;         // Sub-Net
    uint16_t oem;               // OEM code
    uint8_t ubea_version;       // UBEA version
    uint8_t status1;            // Status 1
    uint16_t esta_man;          // ESTA manufacturer code
    char short_name[18];        // Short name
    char long_name[64];         // Long name
    char node_report[64];       // Node report
    uint16_t num_ports;         // Number of ports
    uint8_t port_types[4];      // Port types
    uint8_t good_input[4];      // Input status
    uint8_t good_output[4];     // Output status
    uint8_t swin[4];            // Input universe
    uint8_t swout[4];           // Output universe
    // ... more fields
} artnet_poll_reply_t;
```

---

## 4. API Public

### 4.1. Khởi tạo

```c
/**
 * Khởi tạo Art-Net Receiver
 * @return ESP_OK nếu thành công
 */
esp_err_t artnet_init(void);

/**
 * Start Art-Net receiver
 * @return ESP_OK nếu thành công
 */
esp_err_t artnet_start(void);

/**
 * Stop Art-Net receiver
 * @return ESP_OK nếu thành công
 */
esp_err_t artnet_stop(void);
```

### 4.2. Configuration

```c
/**
 * Set node information
 * @param short_name Short name (max 17 chars)
 * @param long_name Long name (max 63 chars)
 * @return ESP_OK nếu thành công
 */
esp_err_t artnet_set_node_info(const char *short_name, const char *long_name);

/**
 * Set output universe for a port
 * @param port Port number (1 or 2)
 * @param universe Universe (0-32767)
 * @return ESP_OK nếu thành công
 */
esp_err_t artnet_set_output_universe(uint8_t port, uint16_t universe);

/**
 * Enable/disable a port
 * @param port Port number (1 or 2)
 * @param enabled true to enable
 * @return ESP_OK nếu thành công
 */
esp_err_t artnet_set_port_enabled(uint8_t port, bool enabled);
```

### 4.3. Callbacks

```c
/**
 * Đăng ký callback khi nhận ArtDmx
 * @param callback Callback function
 * @param user_data User data
 */
void artnet_register_dmx_callback(artnet_dmx_callback_t callback,
                                  void *user_data);

/**
 * Đăng ký callback khi nhận ArtPoll
 * @param callback Callback function
 */
void artnet_register_poll_callback(artnet_poll_callback_t callback);
```

### 4.4. Statistics

```c
/**
 * Lấy thống kê Art-Net
 * @param stats Buffer to store statistics
 * @return ESP_OK nếu thành công
 */
esp_err_t artnet_get_stats(artnet_stats_t *stats);

/**
 * Reset statistics
 * @return ESP_OK nếu thành công
 */
esp_err_t artnet_reset_stats(void);
```

---

## 5. Implementation

### 5.1. UDP Socket Setup

```c
static int artnet_socket = -1;

static esp_err_t artnet_create_socket(void) {
    artnet_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (artnet_socket < 0) {
        return ESP_FAIL;
    }
    
    // Set socket options
    int broadcast = 1;
    setsockopt(artnet_socket, SOL_SOCKET, SO_BROADCAST, 
               &broadcast, sizeof(broadcast));
    
    int reuse = 1;
    setsockopt(artnet_socket, SOL_SOCKET, SO_REUSEADDR,
               &reuse, sizeof(reuse));
    
    // Bind to Art-Net port
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(ARTNET_PORT);
    
    if (bind(artnet_socket, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close(artnet_socket);
        return ESP_FAIL;
    }
    
    return ESP_OK;
}
```

### 5.2. Receive Task

```c
static void artnet_receive_task(void *pvParameters) {
    uint8_t buffer[ARTNET_MAX_PACKET_SIZE];
    struct sockaddr_in source_addr;
    socklen_t addr_len = sizeof(source_addr);
    
    while (1) {
        int len = recvfrom(artnet_socket, buffer, sizeof(buffer), 0,
                          (struct sockaddr*)&source_addr, &addr_len);
        
        if (len > 0) {
            artnet_packet_stats.total_packets++;
            
            // Verify Art-Net header
            if (len < 12 || memcmp(buffer, "Art-Net\0", 8) != 0) {
                artnet_packet_stats.invalid_packets++;
                continue;
            }
            
            // Get opcode (little endian)
            uint16_t opcode = buffer[8] | (buffer[9] << 8);
            
            // Dispatch based on opcode
            switch (opcode) {
                case ARTNET_OPCODE_DMX:
                    handle_artdmx(buffer, len, &source_addr);
                    break;
                    
                case ARTNET_OPCODE_POLL:
                    handle_artpoll(buffer, len, &source_addr);
                    break;
                    
                case ARTNET_OPCODE_SYNC:
                    handle_artsync(buffer, len);
                    break;
                    
                case ARTNET_OPCODE_ADDRESS:
                    handle_artaddress(buffer, len);
                    break;
                    
                default:
                    artnet_packet_stats.unknown_opcode++;
                    break;
            }
        }
    }
}
```

### 5.3. ArtDmx Handler

```c
static void handle_artdmx(uint8_t *buffer, int len, 
                         struct sockaddr_in *source_addr) {
    if (len < sizeof(artnet_dmx_t)) {
        return;
    }
    
    artnet_dmx_t *dmx = (artnet_dmx_t*)buffer;
    
    // Check protocol version
    if (dmx->prot_ver_hi != 0 || dmx->prot_ver_lo < 14) {
        return;
    }
    
    // Extract universe (15-bit)
    uint16_t universe = dmx->universe & 0x7FFF;
    
    // Extract Net/Sub-Net/Universe
    uint8_t net = (universe >> 8) & 0x7F;
    uint8_t subnet = (universe >> 4) & 0x0F;
    uint8_t uni = universe & 0x0F;
    
    // Check if this universe matches any of our ports
    for (int port = 1; port <= 2; port++) {
        if (artnet_config.port[port-1].enabled &&
            artnet_config.port[port-1].universe == universe) {
            
            // Push to merge engine
            merge_engine_push_artnet(port, universe, dmx->data,
                                    dmx->sequence,
                                    source_addr->sin_addr.s_addr);
            
            artnet_packet_stats.dmx_packets++;
            
            // Trigger LED pulse
            led_manager_pulse((rgb_color_t){255, 255, 255});
            
            // Callback
            if (artnet_dmx_callback) {
                artnet_dmx_callback(port, universe, dmx->data, 
                                   ntohs(dmx->length));
            }
        }
    }
}
```

### 5.4. ArtPoll Handler

```c
static void handle_artpoll(uint8_t *buffer, int len,
                          struct sockaddr_in *source_addr) {
    artnet_poll_t *poll = (artnet_poll_t*)buffer;
    
    // Send ArtPollReply
    send_artpoll_reply(source_addr);
    
    artnet_packet_stats.poll_packets++;
    
    // Callback
    if (artnet_poll_callback) {
        artnet_poll_callback();
    }
}

static void send_artpoll_reply(struct sockaddr_in *dest) {
    artnet_poll_reply_t reply;
    memset(&reply, 0, sizeof(reply));
    
    // Header
    memcpy(reply.id, "Art-Net\0", 8);
    reply.opcode = ARTNET_OPCODE_POLLREPLY;
    
    // IP info
    uint32_t ip = network_get_ip_u32();
    memcpy(reply.ip, &ip, 4);
    reply.port = htons(ARTNET_PORT);
    
    // Version
    reply.version_info = htons(FIRMWARE_VERSION);
    reply.oem = htons(ARTNET_OEM_CODE);
    reply.esta_man = htons(ESTA_MANUFACTURER_CODE);
    
    // Node info
    strncpy(reply.short_name, artnet_config.short_name, 17);
    strncpy(reply.long_name, artnet_config.long_name, 63);
    
    // Port info
    reply.num_ports = htons(2);
    
    for (int i = 0; i < 2; i++) {
        reply.port_types[i] = ARTNET_PORT_TYPE_DMX | 
                              ARTNET_PORT_TYPE_OUTPUT;
        reply.good_output[i] = artnet_config.port[i].enabled ? 
                              ARTNET_GOOD_OUTPUT_DATA : 0;
        reply.swout[i] = artnet_config.port[i].universe & 0x0F;
    }
    
    reply.net_switch = (artnet_config.port[0].universe >> 8) & 0x7F;
    reply.sub_switch = (artnet_config.port[0].universe >> 4) & 0x0F;
    
    // Status
    reply.status1 = ARTNET_STATUS1_INDICATOR_NORMAL |
                   ARTNET_STATUS1_PAP_NETWORK;
    
    // Node report
    snprintf(reply.node_report, 63, "#0001 [%04d] Power On",
            artnet_packet_stats.dmx_packets);
    
    // Send reply
    sendto(artnet_socket, &reply, sizeof(reply), 0,
          (struct sockaddr*)dest, sizeof(*dest));
}
```

---

## 6. Universe Mapping

### 6.1. Net/Sub-Net/Universe Format

```
15-bit Universe = [Net:7][Sub:4][Uni:4]

Example:
Net = 0, Sub = 1, Uni = 5
Universe = 0x0015 = 21
```

### 6.2. Configuration

```c
static uint16_t calculate_universe(uint8_t net, uint8_t subnet, uint8_t uni) {
    return ((net & 0x7F) << 8) | ((subnet & 0x0F) << 4) | (uni & 0x0F);
}

static void parse_universe(uint16_t universe, uint8_t *net, 
                          uint8_t *subnet, uint8_t *uni) {
    *net = (universe >> 8) & 0x7F;
    *subnet = (universe >> 4) & 0x0F;
    *uni = universe & 0x0F;
}
```

---

## 7. Threading Model

- **Art-Net Receive Task** (Priority: 8, Core: 0, Stack: 4KB)
  - Receive UDP packets
  - Parse and dispatch
  - Send replies

---

## 8. Error Handling

| Lỗi | Hành động |
|-----|-----------|
| Socket create fail | Retry 3 times, log error |
| Bind fail | Log error, disable Art-Net |
| Invalid packet | Drop, increment counter |
| Unknown opcode | Drop, increment counter |

---

## 9. Dependencies

- **ESP-IDF Components**:
  - lwip/sockets
  - freertos/task
- **Internal Modules**:
  - Network (IP address)
  - Merge Engine (DMX data)
  - Configuration (universes)
  - LED Manager (pulse)

---

## 10. Testing Points

1. Receive ArtDmx - single universe
2. Receive ArtDmx - multiple universes
3. Universe filtering
4. Net/Sub-Net/Universe parsing
5. ArtPoll - send reply
6. ArtPollReply content verification
7. Sequence number handling
8. Invalid packet handling
9. Packet rate measurement
10. Multiple sources same universe

---

## 11. Memory Usage

- Socket buffers: 8 KB
- Task stack: 4 KB
- Packet buffer: 1 KB
- **Total: ~13 KB**

---

## 12. Performance

- Max packet rate: ~1000 packets/sec
- Latency: < 2ms (from UDP to merge engine)
- CPU usage: ~5-10%

---

## 13. Constants

```c
#define ARTNET_PORT                 6454
#define ARTNET_MAX_PACKET_SIZE      1024

#define ARTNET_OPCODE_POLL          0x2000
#define ARTNET_OPCODE_POLLREPLY     0x2100
#define ARTNET_OPCODE_DMX           0x5000
#define ARTNET_OPCODE_SYNC          0x5200
#define ARTNET_OPCODE_ADDRESS       0x6000

#define ARTNET_OEM_CODE             0xFFFF  // Custom
#define ESTA_MANUFACTURER_CODE      0x7FF0  // Custom range

#define ARTNET_PORT_TYPE_DMX        0x00
#define ARTNET_PORT_TYPE_MIDI       0x01
#define ARTNET_PORT_TYPE_AVAB       0x02
#define ARTNET_PORT_TYPE_COLORTRAN  0x03
#define ARTNET_PORT_TYPE_ADB        0x04
#define ARTNET_PORT_TYPE_ARTNET     0x05
#define ARTNET_PORT_TYPE_OUTPUT     0x80
#define ARTNET_PORT_TYPE_INPUT      0x40
```
