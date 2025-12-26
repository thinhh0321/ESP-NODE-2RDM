# Network Manager Testing Guide

## Overview

This guide provides step-by-step instructions for testing the Network Manager component of ESP-NODE-2RDM.

## Prerequisites

### Hardware
- ESP32-S3 development board (with ≥16MB Flash, 8MB PSRAM)
- W5500 Ethernet module (for Ethernet testing)
- Jumper wires
- USB cable for programming and power
- Ethernet cable and router/switch
- WiFi access point (for STA testing)
- Second WiFi device (phone/laptop for AP testing)

### Software
- ESP-IDF v5.2.6 installed and configured
- Serial terminal (minicom, screen, or ESP-IDF monitor)

### Wiring for W5500

| W5500 Pin | ESP32-S3 Pin | GPIO |
|-----------|--------------|------|
| CS        | GPIO10       | 10   |
| MOSI      | GPIO11       | 11   |
| MISO      | GPIO13       | 13   |
| SCK       | GPIO12       | 12   |
| INT       | GPIO9        | 9    |
| VCC       | 3.3V         | -    |
| GND       | GND          | -    |

**Important**: Ensure W5500 is powered with 3.3V, NOT 5V!

## Building the Firmware

```bash
# Navigate to project directory
cd /path/to/ESP-NODE-2RDM

# Set up ESP-IDF environment
. $IDF_PATH/export.sh

# Configure project (if needed)
idf.py menuconfig

# Build
idf.py build

# Flash and monitor
idf.py -p /dev/ttyUSB0 flash monitor
```

## Test Plan

### Test 1: Basic Initialization

**Objective**: Verify network manager initializes correctly

**Steps**:
1. Flash firmware
2. Open serial monitor
3. Observe boot sequence

**Expected Output**:
```
I (xxx) main: ESP-NODE-2RDM Firmware v0.1.0
I (xxx) main: ESP-IDF Version: v5.2.6
I (xxx) storage: LittleFS mounted
I (xxx) config: Configuration loaded
I (xxx) main: Node: ArtNet-Node
I (xxx) led_mgr: LED Manager initialized
I (xxx) network_mgr: Initializing network manager...
I (xxx) network_mgr: Network manager initialized
I (xxx) main: Starting network with auto-fallback...
I (xxx) network_fallback: Starting network auto-fallback sequence...
```

**Success Criteria**: No errors, initialization completes

---

### Test 2: Ethernet Connection (DHCP)

**Prerequisites**: W5500 connected, Ethernet cable plugged into router

**Configuration**:
Edit `config.json` (via web interface later, or manually in storage):
```json
{
  "network": {
    "use_ethernet": true,
    "eth_use_static_ip": false
  }
}
```

**Steps**:
1. Flash firmware
2. Monitor serial output
3. Observe LED (should turn green)
4. Note IP address in logs

**Expected Output**:
```
I (xxx) network_fallback: Attempting Ethernet connection...
I (xxx) network_fallback: Ethernet attempt 1/3
I (xxx) network_mgr: Ethernet Started
I (xxx) network_mgr: Ethernet Link Up
I (xxx) network_mgr: Ethernet Got IP: 192.168.1.xxx
I (xxx) main: Network state changed: 1
I (xxx) led_mgr: Setting LED state: ETHERNET_OK
I (xxx) network_fallback: Ethernet connected successfully
I (xxx) main: Network connected - IP: 192.168.1.xxx
```

**Verification**:
- Ping ESP32 from computer: `ping 192.168.1.xxx`
- LED should be solid green
- Connection should be stable

---

### Test 3: Ethernet Connection (Static IP)

**Configuration**:
```json
{
  "network": {
    "use_ethernet": true,
    "eth_use_static_ip": true,
    "eth_static_ip": "192.168.1.100",
    "eth_netmask": "255.255.255.0",
    "eth_gateway": "192.168.1.1"
  }
}
```

**Steps**:
1. Update configuration
2. Reboot device
3. Verify assigned IP

**Expected Output**:
```
I (xxx) network_mgr: Using static IP: 192.168.1.100
I (xxx) network_mgr: Ethernet Got IP: 192.168.1.100
```

**Verification**:
- IP should be exactly 192.168.1.100
- Ping should work from computer

---

### Test 4: WiFi Station Connection (Single Profile)

**Prerequisites**: Remove Ethernet cable or disable Ethernet in config

**Configuration**:
```json
{
  "network": {
    "use_ethernet": false,
    "wifi_profiles": [
      {
        "ssid": "YourWiFiSSID",
        "password": "YourPassword",
        "priority": 0,
        "use_static_ip": false
      }
    ]
  }
}
```

**Steps**:
1. Configure WiFi profile
2. Reboot device
3. Monitor connection

**Expected Output**:
```
I (xxx) network_fallback: Ethernet connection failed after 3 attempts
I (xxx) network_fallback: Attempting WiFi Station connection...
I (xxx) network_fallback: Trying WiFi profile: YourWiFiSSID (priority 0)
I (xxx) network_mgr: Connecting to WiFi SSID: YourWiFiSSID
I (xxx) network_mgr: WiFi STA Started
I (xxx) network_mgr: WiFi Connected
I (xxx) network_mgr: WiFi Got IP: 192.168.1.xxx
I (xxx) main: Network state changed: 2
I (xxx) led_mgr: Setting LED state: WIFI_STA_OK
I (xxx) network_fallback: WiFi Station connected to: YourWiFiSSID
```

**Verification**:
- LED should blink green (slow)
- Can ping ESP32
- Check WiFi router to see device connected

---

### Test 5: WiFi Multiple Profiles (Priority)

**Configuration**:
```json
{
  "network": {
    "use_ethernet": false,
    "wifi_profiles": [
      {
        "ssid": "HighPriorityWiFi",
        "password": "pass1",
        "priority": 0
      },
      {
        "ssid": "LowPriorityWiFi",
        "password": "pass2",
        "priority": 1
      }
    ]
  }
}
```

**Test Cases**:

**Case A**: Both networks available
- Should connect to "HighPriorityWiFi" (priority 0)

**Case B**: Only "LowPriorityWiFi" available
- Should skip "HighPriorityWiFi" and connect to "LowPriorityWiFi"

**Case C**: Both unavailable
- Should fallback to WiFi AP mode

---

### Test 6: WiFi AP Mode (Fallback)

**Prerequisites**: No Ethernet, no valid WiFi profiles

**Configuration**:
```json
{
  "network": {
    "use_ethernet": false,
    "wifi_profiles": [],
    "ap_ssid": "ESP-NODE-2RDM",
    "ap_password": "12345678",
    "ap_channel": 6
  }
}
```

**Steps**:
1. Configure as above
2. Reboot device
3. Search for WiFi network from phone/laptop

**Expected Output**:
```
I (xxx) network_fallback: Falling back to WiFi AP mode...
I (xxx) network_mgr: Starting WiFi AP: ESP-NODE-2RDM
I (xxx) network_mgr: WiFi AP Started
I (xxx) main: Network state changed: 3
I (xxx) led_mgr: Setting LED state: WIFI_AP
I (xxx) network_fallback: WiFi AP started successfully
I (xxx) network_fallback: SSID: ESP-NODE-2RDM
I (xxx) network_fallback: IP: 192.168.4.1
```

**Verification**:
- LED should be purple/magenta
- SSID "ESP-NODE-2RDM" visible on phone
- Can connect with password "12345678"
- ESP32 should have IP 192.168.4.1
- Can ping 192.168.4.1 from connected device

---

### Test 7: Auto-Fallback Sequence

**Objective**: Test complete fallback: Ethernet → WiFi STA → WiFi AP

**Configuration**:
```json
{
  "network": {
    "use_ethernet": true,
    "eth_use_static_ip": false,
    "wifi_profiles": [
      {
        "ssid": "NonExistentNetwork",
        "password": "wrongpass",
        "priority": 0
      }
    ],
    "ap_ssid": "ESP-NODE-FALLBACK",
    "ap_password": "testtest"
  }
}
```

**Steps**:
1. Disconnect Ethernet cable (or don't connect W5500)
2. Ensure "NonExistentNetwork" doesn't exist
3. Flash and monitor

**Expected Sequence**:
```
I (xxx) network_fallback: Starting network auto-fallback sequence...
I (xxx) network_fallback: Attempting Ethernet connection...
I (xxx) network_fallback: Ethernet attempt 1/3
[wait ~10 seconds]
I (xxx) network_fallback: Ethernet connection failed, retrying...
[repeat 2 more times]
I (xxx) network_fallback: Ethernet connection failed after 3 attempts
I (xxx) network_fallback: Attempting WiFi Station connection...
I (xxx) network_fallback: Trying WiFi profile: NonExistentNetwork
[wait ~15 seconds]
W (xxx) network_fallback: Failed to connect to: NonExistentNetwork
I (xxx) network_fallback: WiFi Station connection failed for all profiles
I (xxx) network_fallback: Falling back to WiFi AP mode...
I (xxx) network_mgr: Starting WiFi AP: ESP-NODE-FALLBACK
I (xxx) network_mgr: WiFi AP Started
I (xxx) led_mgr: Setting LED state: WIFI_AP
```

**Success Criteria**:
- All three modes attempted in order
- Appropriate timeouts respected
- Finally settles on WiFi AP mode
- LED shows correct state for each phase

---

### Test 8: Network Reconnection

**Objective**: Test automatic reconnection after network loss

**For Ethernet**:
1. Connect Ethernet successfully
2. Unplug Ethernet cable
3. Wait for "Link Down" message
4. Plug cable back in
5. Should reconnect automatically

**For WiFi STA**:
1. Connect to WiFi successfully
2. Turn off WiFi router
3. Wait for disconnection
4. Turn router back on
5. Should reconnect automatically (with retries)

**Expected Behavior**: Device reconnects within ~30 seconds

---

### Test 9: WiFi Scan

**Test Code** (add to main.c temporarily):
```c
wifi_ap_record_t ap_records[20];
uint16_t ap_count = network_wifi_scan(ap_records, 20);
ESP_LOGI(TAG, "Found %d access points:", ap_count);
for (int i = 0; i < ap_count; i++) {
    ESP_LOGI(TAG, "  %s (RSSI: %d)", 
             ap_records[i].ssid, ap_records[i].rssi);
}
```

**Expected Output**: List of nearby WiFi networks

---

### Test 10: Stress Test

**Objective**: Test stability under repeated connections

**Steps**:
1. Configure auto-fallback with working Ethernet
2. Create loop that disconnects/reconnects every 60 seconds
3. Run for 1 hour
4. Monitor for memory leaks or crashes

**Success Criteria**:
- No crashes
- No memory leaks (check heap size)
- Consistent reconnection times

---

## LED Status Reference

During testing, observe LED colors:

| LED Color | State | Meaning |
|-----------|-------|---------|
| Light Blue | BOOT | System booting |
| Solid Green | ETHERNET_OK | Ethernet connected |
| Blinking Green | WIFI_STA_OK | WiFi Station connected |
| Purple | WIFI_AP | WiFi AP mode active |
| Red (fast blink) | ERROR | Network error |

---

## Troubleshooting

### Ethernet not working

**Symptom**: "Ethernet connection failed after 3 attempts"

**Checks**:
1. Verify W5500 wiring (especially SPI pins)
2. Check W5500 power (must be 3.3V)
3. Verify Ethernet cable is connected
4. Check router/switch is powered on
5. Try different Ethernet cable
6. Check SPI communication with logic analyzer

**Debug Commands** (add to code):
```c
// Check SPI communication
uint8_t version;
esp_eth_ioctl(s_eth_handle, ETH_CMD_G_PHY_ADDR, &version);
ESP_LOGI(TAG, "W5500 version: 0x%02x", version); // Should be 0x04
```

### WiFi won't connect

**Symptom**: "Failed to connect to: YourSSID"

**Checks**:
1. Verify SSID spelling (case-sensitive)
2. Check password is correct
3. Ensure using 2.4GHz network (ESP32-S3 doesn't support 5GHz)
4. Check WiFi router is in range (RSSI > -70 dBm)
5. Try open network first (no password) to isolate auth issues
6. Check for MAC filtering on router

### WiFi AP not visible

**Symptom**: Can't find ESP-NODE-2RDM network

**Checks**:
1. Verify AP actually started (check logs)
2. Check channel isn't blocked by regulatory domain
3. Try different channel (1, 6, or 11 recommended)
4. Check if device supports 2.4GHz scanning
5. Try searching closer to ESP32

### Memory issues

**Symptom**: Crashes, heap corruption

**Checks**:
```bash
# Monitor heap size
idf.py monitor | grep "free heap"
```

Expected free heap: >100KB after initialization

### Build errors

**Common issues**:

1. **Missing esp_netif**: Ensure ESP-IDF v5.2.6
2. **SPI errors**: Check sdkconfig for SPI enabled
3. **WiFi errors**: Ensure WiFi component enabled in menuconfig

---

## Performance Benchmarks

Expected performance metrics:

| Metric | Target | Acceptable Range |
|--------|--------|------------------|
| Ethernet connection time | 3-5s | 2-10s |
| WiFi STA connection time | 8-12s | 5-20s |
| WiFi AP start time | 1-2s | 1-3s |
| Ethernet throughput | ~10 Mbps | 8-12 Mbps |
| WiFi throughput | ~25 Mbps | 15-35 Mbps |
| Ethernet latency | <1ms | <5ms |
| WiFi latency | 5ms | 2-15ms |

### Throughput Test

Use `iperf3` to test throughput:

```bash
# On PC (server mode)
iperf3 -s

# On ESP32 (client mode - add to code)
// Use lwIP socket API to connect and send data
```

### Latency Test

```bash
# Ping test
ping -c 100 <ESP32_IP>
```

Expected: <10ms average for WiFi, <1ms for Ethernet

---

## Test Report Template

```
Network Manager Test Report
Date: ___________
Tester: ___________
Hardware: ESP32-S3 + W5500
Firmware Version: v0.1.0

Test Results:
[ ] Test 1: Basic Initialization - PASS/FAIL
[ ] Test 2: Ethernet DHCP - PASS/FAIL/N/A
[ ] Test 3: Ethernet Static IP - PASS/FAIL/N/A
[ ] Test 4: WiFi STA Single Profile - PASS/FAIL
[ ] Test 5: WiFi Multiple Profiles - PASS/FAIL
[ ] Test 6: WiFi AP Fallback - PASS/FAIL
[ ] Test 7: Auto-Fallback Sequence - PASS/FAIL
[ ] Test 8: Network Reconnection - PASS/FAIL
[ ] Test 9: WiFi Scan - PASS/FAIL
[ ] Test 10: Stress Test - PASS/FAIL

Issues Found:
1. _________________________________
2. _________________________________
3. _________________________________

Overall Status: PASS / FAIL
Notes: _________________________________
```

---

## Next Steps

After successful network testing:
1. Proceed to Phase 4: DMX/RDM Handler
2. Integrate protocol receivers (Art-Net, sACN)
3. Implement web server for configuration
4. Full system integration testing

---

## References

- [ESP-IDF WiFi API](https://docs.espressif.com/projects/esp-idf/en/v5.2.6/esp32s3/api-reference/network/esp_wifi.html)
- [ESP-IDF Ethernet API](https://docs.espressif.com/projects/esp-idf/en/v5.2.6/esp32s3/api-reference/network/esp_eth.html)
- [W5500 Datasheet](https://www.wiznet.io/product-item/w5500/)
