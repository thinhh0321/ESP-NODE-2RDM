# Network Manager Component

## Overview

The Network Manager component provides comprehensive network connectivity management for the ESP-NODE-2RDM project, supporting:

- **Ethernet W5500** (SPI-based) - Primary connection (Priority 1)
- **WiFi Station Mode** - Multiple profile support with priority (Priority 2)
- **WiFi AP Mode** - Fallback access point (Priority 3)
- **Auto-fallback** - Automatic transition between connection types with resource optimization

## Key Features

### High-Priority Event Task
- Dedicated `net_evt_task` on Core 0 with high priority
- Prevents network jitter from affecting Art-Net/DMX processing on Core 1
- Efficient event handling using ESP-IDF event loop

### Static Memory Allocation
- All critical components use static allocation (no heap)
- Event group created with `xEventGroupCreateStatic`
- Task created with `xTaskCreateStaticPinnedToCore`
- Stack size: 4096 bytes (statically allocated)

### Resource Optimization
- **WiFi RF and RAM freed when Ethernet is active**
- WiFi is only started when Ethernet is unavailable
- WiFi is stopped automatically when Ethernet connects
- Minimizes resource usage for optimal Art-Net/DMX performance

## Architecture

### Priority-Based Fallback

```
Priority 1 (Ethernet):
    Connect → Got IP → STOP WiFi (free RF/RAM) → Use Ethernet

Priority 2 (WiFi STA):
    Ethernet Failed → START WiFi → Try Profiles → Connect → Use WiFi STA

Priority 3 (WiFi AP):
    All Failed → START WiFi AP → Fallback Mode
```

### State Machine

```
DISCONNECTED → CONNECTING → ETHERNET_CONNECTED (WiFi stopped)
                         ↓
                    WIFI_STA_CONNECTED (Ethernet unavailable)
                         ↓
                    WIFI_AP_ACTIVE (Last resort)
                         ↓
                      ERROR
```

### Task Architecture

- **net_evt_task**: High-priority task on Core 0 for network events
  - Priority: `configMAX_PRIORITIES - 2`
  - Core: 0 (dedicated to network)
  - Stack: 4096 bytes (static)
  
- **Core 1**: Reserved for Art-Net/DMX processing (no network jitter)

### Hardware Configuration

#### W5500 Ethernet (SPI)
- CS Pin: GPIO10
- MOSI Pin: GPIO11
- MISO Pin: GPIO13
- SCK Pin: GPIO12
- INT Pin: GPIO9
- Clock Speed: 20 MHz

#### WiFi
- Internal antenna
- Power save: Disabled (for low latency)

## API Reference

### Initialization

```c
esp_err_t network_init(void);
```
Initialize the network manager. Must be called before any other network functions.

```c
esp_err_t network_start(void);
```
Start network connection. Basic startup without auto-fallback.

```c
esp_err_t network_start_with_fallback(void);
```
Start network with automatic fallback logic (recommended). This function:
- Creates a background task for the fallback sequence
- Returns immediately (non-blocking)
- Automatically tries: Ethernet → WiFi STA (by priority) → WiFi AP
- Notifies via state callbacks

```c
esp_err_t network_stop(void);
```
Stop all network interfaces.

### Ethernet Functions

```c
esp_err_t network_ethernet_connect(const ip_config_t *config);
```
Connect to Ethernet. Pass NULL for DHCP or provide static IP configuration.

```c
bool network_ethernet_is_link_up(void);
```
Check if Ethernet link is up.

### WiFi Station Functions

```c
esp_err_t network_wifi_sta_connect(const wifi_profile_t *profile);
```
Connect to WiFi using the specified profile.

```c
uint16_t network_wifi_scan(wifi_ap_record_t *scan_result, uint16_t max_aps);
```
Scan for available WiFi networks.

### WiFi AP Functions

```c
esp_err_t network_wifi_ap_start(const char *ssid, const char *password,
                                uint8_t channel, const ip_config_t *config);
```
Start WiFi Access Point.

### Status Functions

```c
esp_err_t network_get_status(network_status_t *status);
const char* network_get_ip_address(void);
bool network_is_connected(void);
```

### Callbacks

```c
void network_register_state_callback(network_state_callback_t callback,
                                    void *user_data);
```
Register a callback for network state changes.

## Usage Example

### Basic Usage with Auto-Fallback

```c
#include "network_manager.h"

// Callback function
void on_network_state_change(network_state_t state, void *user_data)
{
    switch (state) {
        case NETWORK_ETHERNET_CONNECTED:
            ESP_LOGI(TAG, "Ethernet connected");
            break;
        case NETWORK_WIFI_STA_CONNECTED:
            ESP_LOGI(TAG, "WiFi connected");
            break;
        case NETWORK_WIFI_AP_ACTIVE:
            ESP_LOGI(TAG, "AP mode active");
            break;
        default:
            break;
    }
}

void app_main(void)
{
    // Initialize
    ESP_ERROR_CHECK(network_init());
    
    // Register callback
    network_register_state_callback(on_network_state_change, NULL);
    
    // Start network with auto-fallback (recommended)
    ESP_ERROR_CHECK(network_start_with_fallback());
    
    // The fallback task will handle connection asynchronously
    // Continue with other initialization...
    
    // Later, get status
    if (network_is_connected()) {
        network_status_t status;
        network_get_status(&status);
        ESP_LOGI(TAG, "IP: %s", status.ip_address);
    }
}
```

### Manual Connection (No Auto-Fallback)

```c
void app_main(void)
{
    // Initialize
    ESP_ERROR_CHECK(network_init());
    
    // Manually connect to Ethernet
    ip_config_t eth_config = {
        .use_dhcp = true
    };
    ESP_ERROR_CHECK(network_ethernet_connect(&eth_config));
    
    // Wait for connection
    vTaskDelay(pdMS_TO_TICKS(5000));
    
    if (network_ethernet_is_link_up()) {
        ESP_LOGI(TAG, "Ethernet connected");
    }
}
```

## Auto-Fallback Sequence

The network manager implements an automatic fallback sequence with resource optimization:

1. **Try Ethernet (Priority 1)** (if enabled)
   - Retry: 3 attempts
   - Timeout: 10 seconds per attempt
   - On success: Stop WiFi (free RF/RAM), use Ethernet
   - On failure: Continue to WiFi STA

2. **Try WiFi Station (Priority 2)** (for each profile by priority)
   - Start WiFi (only if Ethernet failed)
   - Timeout: 15 seconds per profile
   - Max retries: 5 per profile
   - On success: Stop, use WiFi STA
   - On failure: Try next profile

3. **Fallback to WiFi AP (Priority 3)**
   - Start Access Point with configured SSID
   - Default IP: 192.168.4.1
   - Always succeeds (unless hardware failure)

**Resource Management**: WiFi is automatically stopped when Ethernet connects to free RF and RAM resources for Art-Net/DMX processing.

## Configuration

Network configuration is managed by the `config_manager` component. Example configuration:

```json
{
  "network": {
    "use_ethernet": true,
    "ethernet_ip": {
      "use_dhcp": true
    },
    "wifi_profiles": [
      {
        "ssid": "Office_WiFi",
        "password": "password123",
        "priority": 0,
        "use_static_ip": false
      },
      {
        "ssid": "Home_WiFi", 
        "password": "homepass",
        "priority": 1,
        "use_static_ip": true,
        "static_ip": {
          "ip": "192.168.1.100",
          "netmask": "255.255.255.0",
          "gateway": "192.168.1.1"
        }
      }
    ],
    "ap_ssid": "ESP-NODE-2RDM",
    "ap_password": "12345678",
    "ap_channel": 6
  }
}
```

## Event System

The network manager uses ESP-IDF's event system to handle:

- **ETH_EVENT**: Ethernet link up/down, start/stop
- **WIFI_EVENT**: WiFi connection, disconnection, AP events
- **IP_EVENT**: IP address assignment, loss

All events are processed internally and trigger state callbacks.

## Thread Safety

- All public APIs are thread-safe
- Internal state is protected by event groups (statically allocated)
- Callbacks are executed in the event loop context
- High-priority event task on Core 0 prevents jitter

## Memory Usage (Static Allocation)

- **Static allocation**: All critical components use static memory
- **Event group**: `StaticEventGroup_t` (no heap)
- **Task stack**: 4096 bytes (static array)
- **Task TCB**: `StaticTask_t` (no heap)
- **Total static**: ~6 KB (stack + structures)
- **WiFi buffers**: ~40 KB (managed by ESP-IDF, freed when Ethernet active)
- **W5500 buffers**: 32 KB (on-chip)

**Resource Optimization**: WiFi RF and RAM are freed when Ethernet is connected, reducing memory footprint during Ethernet operation.

## Performance

- **Ethernet throughput**: ~10 Mbps (practical)
- **WiFi throughput**: 20-30 Mbps (depending on conditions)
- **Connection time**:
  - Ethernet: 2-5 seconds
  - WiFi STA: 5-15 seconds
  - WiFi AP: 1-2 seconds
- **Latency**:
  - Ethernet: < 1 ms
  - WiFi: 2-10 ms

## Dependencies

- `esp_netif` - Network interface abstraction
- `esp_eth` - Ethernet driver
- `esp_wifi` - WiFi driver
- `esp_event` - Event loop system
- `driver` - SPI and GPIO drivers

## Testing

### Unit Testing

Test each function individually:

1. `network_init()` - Verify initialization
2. `network_ethernet_connect()` - Test with/without W5500
3. `network_wifi_sta_connect()` - Test with valid/invalid credentials
4. `network_wifi_ap_start()` - Verify AP creation

### Integration Testing

1. **Ethernet Connection**
   - Connect W5500 module
   - Verify link detection
   - Test DHCP IP assignment
   - Test static IP configuration

2. **WiFi STA Connection**
   - Configure multiple profiles
   - Test priority-based selection
   - Verify auto-reconnect
   - Test wrong password handling

3. **WiFi AP Fallback**
   - Disconnect all networks
   - Verify AP starts automatically
   - Test client connection to AP

4. **Auto-Fallback Sequence**
   - Disconnect Ethernet (if connected)
   - Verify WiFi STA attempt
   - Verify WiFi AP fallback

### Hardware Requirements

- ESP32-S3 development board
- W5500 Ethernet module (for Ethernet testing)
- WiFi access point (for STA testing)
- WiFi-capable device (for AP testing)

## Troubleshooting

### Ethernet not connecting

- Check W5500 SPI connections
- Verify W5500 power supply
- Check Ethernet cable
- Monitor logs for SPI errors

### WiFi STA not connecting

- Verify SSID and password
- Check WiFi signal strength
- Ensure 2.4GHz band (ESP32-S3 doesn't support 5GHz)
- Check for WPA2/WPA3 compatibility

### WiFi AP not visible

- Verify channel selection (1-13)
- Check for regulatory domain restrictions
- Ensure no conflicting WiFi AP on same channel

## Future Enhancements

- [ ] WiFi mesh support
- [ ] Multiple Ethernet interfaces
- [ ] Network statistics tracking
- [ ] Bandwidth monitoring
- [ ] Connection quality metrics
- [ ] SNMP support

## References

- [ESP-IDF Ethernet Documentation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/network/esp_eth.html)
- [ESP-IDF WiFi Documentation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/network/esp_wifi.html)
- [W5500 Datasheet](https://www.wiznet.io/product-item/w5500/)
