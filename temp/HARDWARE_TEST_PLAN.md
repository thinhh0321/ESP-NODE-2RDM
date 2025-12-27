# Hardware Testing Plan - ESP-NODE-2RDM

**Version:** 1.0  
**Date:** 2025-12-27  
**Target:** Phase 8 Hardware Integration

---

## Purpose

This document provides a comprehensive, step-by-step procedure for testing the ESP-NODE-2RDM firmware on actual hardware. Follow this plan to verify all functionality before production deployment.

---

## Prerequisites

### Hardware Requirements

| Item | Description | Quantity | Notes |
|------|-------------|----------|-------|
| ESP32-S3 Board | With W5500 Ethernet | 1 | 16MB Flash, 8MB PSRAM |
| DMX Fixtures | LED lights, dimmers, etc. | 2+ | Different addresses |
| DMX Cables | XLR cables | 3+ | Good quality, not damaged |
| DMX Terminator | 120Ω resistor | 1 | For end of DMX chain |
| Ethernet Switch | Gigabit preferred | 1 | Or connect to router |
| Ethernet Cable | Cat5e/Cat6 | 1 | Tested cable |
| USB Cable | For serial monitor | 1 | USB-C or USB-Micro |
| Power Supply | 5V DC, 2A+ | 1 | Stable power required |
| Multimeter | Optional | 1 | For signal debugging |

### Software Requirements

| Software | Version | Purpose |
|----------|---------|---------|
| ESP-IDF | v5.2.6 | Building and flashing |
| QLC+ | Latest | Art-Net/sACN testing |
| Wireshark | Latest | Network packet analysis |
| Serial Terminal | Any | PuTTY, screen, minicom |
| Web Browser | Modern | Chrome, Firefox, Edge |

### Network Setup

```
┌─────────────┐
│   PC/Laptop │ (Art-Net/sACN Controller)
│   QLC+      │
└──────┬──────┘
       │
┌──────┴──────┐
│   Switch    │
│  Ethernet   │
└──────┬──────┘
       │
┌──────┴──────┐
│ ESP-NODE    │ (Device Under Test)
│   2RDM      │
└──────┬──────┘
       │
┌──────┴──────┐
│ DMX Fixture │
│   Port 1    │
└─────────────┘
```

**Network Configuration:**
- Subnet: 192.168.1.0/24 or 10.0.0.0/24
- PC: Static IP (e.g., 192.168.1.100)
- ESP-NODE: DHCP or static (configured in firmware)
- No firewall blocking UDP ports 6454 (Art-Net) or 5568 (sACN)

---

## Phase 1: Initial Bring-Up (15 minutes)

### Test 1.1: Power On and Boot

**Objective:** Verify hardware powers on and firmware boots

**Steps:**
1. Connect USB serial cable to PC
2. Open serial terminal (115200 baud, 8N1)
3. Connect 5V power supply
4. Observe serial output

**Expected Results:**
```
ESP-ROM:esp32s3-20210327
Build:Mar 27 2021
...
I (500) main: ESP-NODE-2RDM Firmware v0.1.0
I (501) main: ESP-IDF Version: v5.2.6
I (520) storage_manager: Storage initialized
I (530) config_manager: Configuration loaded
I (540) main: Node: ESP-NODE
I (550) led_manager: LED Manager initialized
```

**Pass Criteria:**
- ✅ No bootloop (continuous resets)
- ✅ Firmware version printed
- ✅ All components initialize successfully
- ✅ No panic or abort messages

**If Failed:**
- Check serial baud rate (must be 115200)
- Check power supply (minimum 2A)
- Reflash firmware
- Check for hardware short circuits

---

### Test 1.2: LED Status Indicator

**Objective:** Verify WS2812 LED works and shows correct status

**Steps:**
1. Observe LED immediately after power on
2. Wait for network initialization
3. Note LED color changes

**Expected LED Behavior:**

| State | Color | Pattern | Duration |
|-------|-------|---------|----------|
| Boot | Cyan | Solid | 2-5s |
| Ethernet Connecting | Yellow | Slow pulse | 5-10s |
| Ethernet Connected | Green | Solid | Continuous |
| WiFi STA Connected | Blue | Solid | Continuous |
| WiFi AP Active | Magenta | Solid | Continuous |
| Error | Red | Fast blink | Continuous |

**Pass Criteria:**
- ✅ LED lights up at boot (cyan)
- ✅ LED changes based on network state
- ✅ Colors are correct and visible
- ✅ No flickering or wrong colors

**If Failed:**
- Check GPIO 48 connection to WS2812
- Check WS2812 power supply
- Verify LED_MANAGER enabled in config
- Check for damaged LED

---

### Test 1.3: Serial Monitor Logs

**Objective:** Verify comprehensive logging and diagnostics

**Steps:**
1. Keep serial monitor open
2. Wait for full boot sequence (30 seconds)
3. Review all log messages
4. Check for warnings or errors

**Expected Log Sections:**
```
✅ NVS Flash initialization
✅ Storage manager init
✅ Config manager init and load
✅ LED manager init
✅ Network manager init
✅ Network start with fallback
✅ DMX handler init
✅ DMX port configuration
✅ Merge engine init
✅ Art-Net receiver init
✅ sACN receiver init
✅ Web server init
✅ System initialized successfully
```

**Pass Criteria:**
- ✅ All components initialize (I logs)
- ✅ No critical errors (E logs)
- ✅ Warnings acceptable if explained
- ✅ "System initialized successfully" printed

---

## Phase 2: Network Testing (20 minutes)

### Test 2.1: Ethernet Connection

**Objective:** Verify W5500 Ethernet module works

**Steps:**
1. Connect Ethernet cable to board
2. Connect other end to switch/router
3. Watch serial monitor for network logs
4. Check LED status (should be green)

**Expected Serial Output:**
```
I (2000) network_manager: Starting Ethernet...
I (2500) w5500: Chip initialized
I (3000) network_manager: Ethernet link up
I (3500) network_manager: IP acquired: 192.168.1.xxx
I (3501) main: Network connected - IP: 192.168.1.xxx
```

**Pass Criteria:**
- ✅ Link up within 5 seconds
- ✅ DHCP IP acquired (or static IP set)
- ✅ LED turns green
- ✅ Can ping device from PC

**Testing:**
```bash
# From PC
ping 192.168.1.xxx
# Should get replies
```

**If Failed:**
- Check Ethernet cable (try different cable)
- Verify W5500 SPI connections
- Check router DHCP is enabled
- Try static IP configuration

---

### Test 2.2: WiFi Fallback (Optional)

**Objective:** Verify WiFi works if Ethernet unavailable

**Steps:**
1. Disconnect Ethernet cable
2. Wait 10 seconds for fallback
3. Check for WiFi AP mode
4. Try to connect to "ArtnetNode-XXXX"

**Expected Behavior:**
```
W (5000) network_manager: Ethernet failed, trying WiFi STA
I (6000) network_manager: WiFi STA connecting...
W (15000) network_manager: WiFi STA failed, starting AP
I (16000) network_manager: WiFi AP started
I (16001) network_manager: AP SSID: ArtnetNode-ABCD
```

**Pass Criteria:**
- ✅ Automatic fallback to WiFi
- ✅ AP mode starts if STA fails
- ✅ Can see WiFi network on phone/laptop
- ✅ Can connect to AP (password: "12345678" default)

---

### Test 2.3: Web Interface Access

**Objective:** Verify web server is accessible

**Steps:**
1. Ensure device has IP address (Ethernet or WiFi)
2. Open web browser on PC
3. Navigate to `http://[device-ip]/`
4. Try all API endpoints

**Expected Results:**
- ✅ Web page loads (HTML dashboard)
- ✅ System info displayed
- ✅ Buttons work (Get Statistics, etc.)
- ✅ No 404 or 500 errors

**API Tests:**
```bash
# Get system info
curl http://192.168.1.xxx/api/system/info | jq .

# Get configuration
curl http://192.168.1.xxx/api/config | jq .

# Get network status
curl http://192.168.1.xxx/api/network/status | jq .

# Get ports status
curl http://192.168.1.xxx/api/ports/status | jq .
```

**Pass Criteria:**
- ✅ All endpoints respond with valid JSON
- ✅ HTTP 200 OK status
- ✅ Data makes sense (correct IPs, versions, etc.)

---

## Phase 3: DMX Output Testing (30 minutes)

### Test 3.1: DMX Port 1 Output

**Objective:** Verify DMX output on Port 1

**Preparation:**
1. Connect DMX fixture to Port 1 output
2. Set fixture to DMX address 1
3. Add DMX terminator at end of chain
4. Ensure fixture is powered on

**Steps:**
1. Configure Port 1 in web interface:
   - Mode: DMX Output
   - Universe: 0
   - Merge Mode: HTP
2. Save and restart device
3. Wait for DMX output to start

**Expected Results:**
```
I (10000) dmx_handler: Port 1 configured: Mode=OUTPUT
I (10001) dmx_handler: Starting DMX port 1
I (10100) dmx_handler: DMX port 1 started successfully
```

**Physical Verification:**
- ✅ DMX fixture responds
- ✅ No flickering or glitches
- ✅ Smooth dimming transitions

**Signal Testing (with oscilloscope):**
- ✅ Break: 88-120µs
- ✅ MAB: 8-16µs  
- ✅ Data rate: 250kbps (4µs per bit)
- ✅ Frame rate: ~44Hz (23ms period)

**If Failed:**
- Check RS485 transceiver connections
- Verify TX/RX/EN pins
- Check DMX cable for damage
- Try different fixture
- Reduce cable length (<300m total)

---

### Test 3.2: DMX Port 2 Output

**Objective:** Verify DMX output on Port 2 (independent from Port 1)

**Steps:**
Same as Test 3.1 but for Port 2
- Configure Universe: 1
- Connect second fixture

**Pass Criteria:**
- ✅ Port 2 works independently
- ✅ Both ports can run simultaneously
- ✅ No interference between ports

---

### Test 3.3: DMX Data Integrity

**Objective:** Verify DMX channels output correctly

**Test Procedure:**
```
Channel Test Pattern:
Ch 1: 255 (100%)
Ch 2: 192 (75%)
Ch 3: 128 (50%)
Ch 4: 64 (25%)
Ch 5: 0 (0%)
Ch 6-512: 0
```

**Verification:**
1. Use DMX sniffer or fixture display
2. Verify each channel value
3. Check all 512 channels can be set

**Pass Criteria:**
- ✅ All 512 channels work
- ✅ Values are accurate (±1 count acceptable)
- ✅ No random flickering
- ✅ Stable output for 5+ minutes

---

## Phase 4: Protocol Receiver Testing (45 minutes)

### Test 4.1: Art-Net Reception

**Objective:** Verify Art-Net v4 packet reception

**QLC+ Setup:**
1. Open QLC+
2. Go to Inputs/Outputs tab
3. Select Art-Net plugin
4. Set Universe 0 to IP: 192.168.1.255 (broadcast)
5. Create simple scene (slider for channels 1-10)

**Testing:**
1. Open QLC+ scene
2. Move sliders for channels 1-10
3. Observe DMX fixture responds
4. Check serial monitor for Art-Net stats

**Expected Serial Output:**
```
I (30000) artnet_receiver: Art-Net packet received
D (30001) main: Art-Net DMX received: Universe=0, Length=512, Seq=1
I (40000) main: Art-Net - Packets: 520, DMX: 520, Poll: 0
```

**Pass Criteria:**
- ✅ Fixture responds to QLC+ changes in real-time
- ✅ Art-Net packet count increases
- ✅ Sequence numbers increment
- ✅ < 20ms latency from QLC+ to fixture

**Wireshark Verification:**
```
Filter: udp.port == 6454
- Verify packets are received
- Check OpCode: 0x5000 (ArtDmx)
- Verify universe number
```

---

### Test 4.2: sACN (E1.31) Reception

**Objective:** Verify sACN multicast reception

**QLC+ Setup:**
1. Go to Inputs/Outputs tab
2. Select E1.31 plugin
3. Set Universe 1 to multicast (239.255.0.1)
4. Configure Port 2 to Universe 1

**Testing:**
1. Send E1.31 from QLC+ to Universe 1
2. Verify Port 2 fixture responds
3. Check serial monitor for sACN stats

**Expected Output:**
```
I (50000) sacn_receiver: sACN packet received
D (50001) main: sACN DMX received: Universe=1, Priority=100, Seq=1
I (60000) main: sACN - Packets: 440, Data: 440
```

**Pass Criteria:**
- ✅ Multicast subscription works
- ✅ Fixture on Port 2 responds
- ✅ Priority handled correctly
- ✅ No packet loss

---

### Test 4.3: ArtPoll Discovery

**Objective:** Verify device responds to ArtPoll

**Testing:**
1. Send ArtPoll packet from controller
2. Verify ArtPollReply received

**Manual Test (Python script):**
```python
import socket

# Send ArtPoll
sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)

artpoll = b'Art-Net\x00\x00\x20\x00\x0e\x00\x00'
sock.sendto(artpoll, ('255.255.255.255', 6454))

# Wait for ArtPollReply
sock.settimeout(2.0)
data, addr = sock.recvfrom(1024)
print(f"Reply from {addr}: {len(data)} bytes")
```

**Expected:**
```
Reply from ('192.168.1.xxx', 6454): 239 bytes
```

**Pass Criteria:**
- ✅ ArtPollReply sent within 1 second
- ✅ Reply contains correct node info
- ✅ IP address correct
- ✅ Port info correct

---

## Phase 5: Merge Engine Testing (30 minutes)

### Test 5.1: HTP Merge Mode

**Objective:** Verify Highest Takes Precedence merge

**Setup:**
1. Configure Port 1: Universe 0, Merge Mode: HTP
2. Start two QLC+ instances (or two universes)
3. Both sending to Universe 0

**Test Pattern:**
```
Source A: Ch1=255, Ch2=0,   Ch3=128
Source B: Ch1=128, Ch2=255, Ch3=64

Expected: Ch1=255, Ch2=255, Ch3=128 (highest values)
```

**Pass Criteria:**
- ✅ Higher values win for each channel
- ✅ Smooth transitions between sources
- ✅ No flickering

---

### Test 5.2: Source Timeout

**Objective:** Verify merge engine handles source loss

**Steps:**
1. Start sending DMX from QLC+
2. Verify fixture is lit
3. Close QLC+ (stop sending)
4. Wait for timeout (default 5 seconds)
5. Verify fixture goes to blackout

**Expected Behavior:**
```
I (70000) merge_engine: Source timed out: IP=192.168.1.100
I (70001) merge_engine: Port 1 - Active sources: 0
```

**Pass Criteria:**
- ✅ Fixture blacks out after timeout
- ✅ No stuck channels
- ✅ Recovers when source returns

---

### Test 5.3: Multiple Sources

**Objective:** Test with 2+ simultaneous sources

**Setup:**
- 2 QLC+ instances with different IP addresses
- Both sending Art-Net to Universe 0
- Different channel patterns

**Pass Criteria:**
- ✅ Both sources tracked
- ✅ Merge algorithm correct (HTP/LTP)
- ✅ Statistics show 2 active sources
- ✅ No packet loss

---

## Phase 6: Web Control Testing (20 minutes)

### Test 6.1: Configuration API

**Test:** Update configuration via POST

```bash
curl -X POST http://192.168.1.xxx/api/config \
  -H "Content-Type: application/json" \
  -d '{
    "port1": {
      "mode": 1,
      "universe_primary": 5,
      "merge_mode": 0
    }
  }'
```

**Pass Criteria:**
- ✅ Returns 200 OK
- ✅ Configuration updated
- ✅ Restart applies changes

---

### Test 6.2: Blackout Control

**Test:** Force blackout on port

```bash
curl -X POST http://192.168.1.xxx/api/ports/1/blackout
```

**Physical Verification:**
- ✅ DMX fixture immediately goes dark
- ✅ All 512 channels = 0
- ✅ Recovers when blackout released

---

### Test 6.3: WebSocket Real-Time

**Test:** Connect WebSocket and receive updates

```javascript
const ws = new WebSocket('ws://192.168.1.xxx/ws/status');
ws.onmessage = (event) => {
  console.log('Status:', event.data);
};
```

**Pass Criteria:**
- ✅ WebSocket connects
- ✅ Receives messages
- ✅ Can send commands
- ✅ Handles disconnect gracefully

---

## Phase 7: Stress Testing (30 minutes)

### Test 7.1: Continuous Operation

**Objective:** Verify stability under normal load

**Test Duration:** 30 minutes minimum

**Procedure:**
1. Start Art-Net streaming at 44 Hz
2. Monitor serial output
3. Check web interface periodically
4. Verify fixture stays lit

**Monitoring:**
```
Watch for:
- Memory leaks (free heap decreasing)
- Task watchdog triggers
- Network disconnects
- DMX frame glitches
```

**Pass Criteria:**
- ✅ No crashes or reboots
- ✅ Free heap stable (±1KB variation OK)
- ✅ DMX output consistent
- ✅ Network stays connected
- ✅ Web interface remains responsive

---

### Test 7.2: Rapid Universe Changes

**Objective:** Test dynamic reconfiguration

**Steps:**
1. Send DMX to Universe 0 (Port 1)
2. Change Port 1 to Universe 1 via API
3. Subscribe to Universe 1
4. Send DMX to Universe 1
5. Verify fixture switches universes

**Pass Criteria:**
- ✅ No crashes during change
- ✅ Universe switch works
- ✅ No memory leaks

---

### Test 7.3: Network Interruption Recovery

**Objective:** Test network resilience

**Steps:**
1. Unplug Ethernet cable during operation
2. Wait 30 seconds
3. Reconnect Ethernet
4. Verify system recovers

**Expected Behavior:**
```
W (80000) network_manager: Ethernet link down
I (80500) network_manager: Starting fallback...
I (81000) network_manager: WiFi AP started
...
I (90000) network_manager: Ethernet link up
I (90500) network_manager: IP acquired
```

**Pass Criteria:**
- ✅ Fallback to WiFi AP
- ✅ Recovers when Ethernet returns
- ✅ No data corruption
- ✅ Web interface accessible throughout

---

## Phase 8: Performance Benchmarking (20 minutes)

### Test 8.1: DMX Output Timing

**Measurement:** Actual DMX frame rate

**Tools:** Oscilloscope on DMX+ line

**Procedure:**
1. Measure time between DMX breaks
2. Calculate frame rate
3. Verify timing consistency

**Expected Results:**
- Frame period: 23ms ± 1ms
- Frame rate: 44 Hz ± 2Hz
- Jitter: < 1ms

---

### Test 8.2: Network Latency

**Measurement:** Time from packet RX to DMX TX

**Procedure:**
1. Timestamp Art-Net packet transmission (PC)
2. Timestamp DMX output (oscilloscope trigger)
3. Calculate latency

**Target:**
- Latency: < 20ms (good)
- Latency: < 10ms (excellent)

---

### Test 8.3: Memory Usage

**Measurement:** Heap and task stack usage

**Procedure:**
```bash
# Check via serial monitor
I (100000) main: Free heap: 125432 bytes
I (100001) main: Minimum free heap: 119280 bytes
```

**Expected:**
- Free heap: > 100 KB
- Minimum free heap: > 80 KB
- No heap fragmentation

---

## Phase 9: Edge Cases and Error Handling (20 minutes)

### Test 9.1: Invalid DMX Universe

**Test:** Send DMX to non-subscribed universe

**Expected:**
- Packets received but not processed
- No crashes
- Correct error handling

---

### Test 9.2: Malformed Packets

**Test:** Send invalid Art-Net/sACN packets

**Examples:**
- Wrong header
- Invalid length
- Corrupted data
- Wrong protocol version

**Pass Criteria:**
- ✅ Invalid packets rejected
- ✅ Stats show invalid count
- ✅ No crashes
- ✅ Normal packets still work

---

### Test 9.3: Power Cycle During Operation

**Test:** Reset device while actively streaming

**Procedure:**
1. Start Art-Net streaming
2. Pull power plug
3. Reconnect power immediately
4. Verify clean boot

**Pass Criteria:**
- ✅ Boots normally
- ✅ Configuration preserved
- ✅ Resumes operation
- ✅ No corruption

---

## Test Results Template

### Test Session Information

```
Date: _______________
Firmware Version: _______________
Hardware Revision: _______________
Tester Name: _______________

Hardware:
- ESP32-S3 Board: _______________
- DMX Fixtures: _______________
- Test Environment: _______________
```

### Results Summary

| Phase | Test | Result | Notes |
|-------|------|--------|-------|
| 1.1 | Power On | ☐ Pass ☐ Fail | |
| 1.2 | LED Status | ☐ Pass ☐ Fail | |
| 1.3 | Serial Logs | ☐ Pass ☐ Fail | |
| 2.1 | Ethernet | ☐ Pass ☐ Fail | |
| 2.2 | WiFi Fallback | ☐ Pass ☐ Fail | |
| 2.3 | Web Interface | ☐ Pass ☐ Fail | |
| 3.1 | DMX Port 1 | ☐ Pass ☐ Fail | |
| 3.2 | DMX Port 2 | ☐ Pass ☐ Fail | |
| 3.3 | DMX Integrity | ☐ Pass ☐ Fail | |
| 4.1 | Art-Net RX | ☐ Pass ☐ Fail | |
| 4.2 | sACN RX | ☐ Pass ☐ Fail | |
| 4.3 | ArtPoll | ☐ Pass ☐ Fail | |
| 5.1 | HTP Merge | ☐ Pass ☐ Fail | |
| 5.2 | Source Timeout | ☐ Pass ☐ Fail | |
| 5.3 | Multiple Sources | ☐ Pass ☐ Fail | |
| 6.1 | Config API | ☐ Pass ☐ Fail | |
| 6.2 | Blackout | ☐ Pass ☐ Fail | |
| 6.3 | WebSocket | ☐ Pass ☐ Fail | |
| 7.1 | Continuous Op | ☐ Pass ☐ Fail | |
| 7.2 | Universe Change | ☐ Pass ☐ Fail | |
| 7.3 | Network Recovery | ☐ Pass ☐ Fail | |
| 8.1 | DMX Timing | ☐ Pass ☐ Fail | |
| 8.2 | Latency | ☐ Pass ☐ Fail | |
| 8.3 | Memory Usage | ☐ Pass ☐ Fail | |
| 9.1 | Invalid Universe | ☐ Pass ☐ Fail | |
| 9.2 | Malformed Packets | ☐ Pass ☐ Fail | |
| 9.3 | Power Cycle | ☐ Pass ☐ Fail | |

**Overall Result:** ☐ PASS ☐ FAIL

**Signature:** ___________________ **Date:** _______________

---

## Troubleshooting Guide

### Issue: Device won't boot

**Symptoms:** Continuous reset, no serial output

**Solutions:**
1. Check power supply (minimum 2A)
2. Try different USB cable
3. Reflash bootloader
4. Check for hardware shorts
5. Try factory reset (erase flash)

### Issue: No Ethernet connection

**Symptoms:** Link never comes up, no IP address

**Solutions:**
1. Check Ethernet cable
2. Verify W5500 SPI connections:
   - MOSI, MISO, SCK, CS pins
3. Check W5500 reset pin
4. Try different switch/router port
5. Enable debug logs for W5500

### Issue: DMX output not working

**Symptoms:** Fixture doesn't respond

**Solutions:**
1. Check DMX cable connections
2. Add DMX terminator (120Ω)
3. Test with different fixture
4. Verify fixture DMX address
5. Check RS485 transceiver:
   - TX, RX, enable pins
   - Power supply
6. Measure DMX signal with scope
7. Try lower baud rate test

### Issue: Network packets not received

**Symptoms:** Art-Net/sACN stats show 0 packets

**Solutions:**
1. Check PC and device on same subnet
2. Disable firewall on PC
3. Verify UDP ports not blocked
4. Use Wireshark to verify packets sent
5. Check multicast subscription (sACN)
6. Try broadcast instead of unicast

### Issue: Web interface not accessible

**Symptoms:** Cannot load web page

**Solutions:**
1. Verify device IP address (serial monitor)
2. Ping device from PC
3. Check browser URL (http not https)
4. Try different browser
5. Clear browser cache
6. Check web server logs

### Issue: Memory leak or crash

**Symptoms:** Free heap decreases, device resets

**Solutions:**
1. Check serial logs for panic
2. Enable heap tracing
3. Review recent code changes
4. Run with minimal features enabled
5. Check for buffer overflows
6. Verify mutex/semaphore usage

---

## Appendix A: Quick Test Commands

### Flash Firmware
```bash
cd /path/to/ESP-NODE-2RDM
idf.py build
idf.py -p /dev/ttyUSB0 flash monitor
```

### Serial Monitor
```bash
screen /dev/ttyUSB0 115200
# or
minicom -D /dev/ttyUSB0 -b 115200
# or
idf.py -p /dev/ttyUSB0 monitor
```

### Network Tests
```bash
# Ping device
ping 192.168.1.100

# Scan network
nmap -sn 192.168.1.0/24

# Test web interface
curl http://192.168.1.100/api/system/info
```

### Art-Net Send (Python)
```python
import socket

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
artnet_dmx = (
    b'Art-Net\x00' +  # Header
    b'\x00\x50' +      # OpDmx
    b'\x00\x0e' +      # Protocol version
    b'\x00' +          # Sequence
    b'\x00' +          # Physical
    b'\x00\x00' +      # Universe
    b'\x02\x00' +      # Length (512 big-endian)
    bytes([255] * 512) # DMX data
)
sock.sendto(artnet_dmx, ('192.168.1.100', 6454))
```

---

## Appendix B: Test Equipment Checklist

### Before Starting Testing

☐ ESP32-S3 board flashed with latest firmware  
☐ DMX fixtures powered on and configured  
☐ DMX cables checked for continuity  
☐ DMX terminator installed at end of chain  
☐ Ethernet switch powered on  
☐ Network cables connected  
☐ PC with QLC+ installed and configured  
☐ Serial terminal ready (115200 baud)  
☐ Web browser open  
☐ Wireshark installed (optional)  
☐ Multimeter available (optional)  
☐ Oscilloscope available (optional)  
☐ Test results template printed  

---

## Appendix C: Success Criteria

### Minimum Requirements for Release

**Critical (Must Pass):**
- ✅ Device boots reliably
- ✅ Ethernet connection works
- ✅ DMX output functional on both ports
- ✅ Art-Net reception works
- ✅ sACN reception works
- ✅ Web interface accessible
- ✅ Configuration can be saved
- ✅ No crashes in 30-minute stress test

**Important (Should Pass):**
- ✅ LED status indicator works
- ✅ WiFi fallback works
- ✅ Merge engine functions correctly
- ✅ ArtPoll responses work
- ✅ Network recovery works
- ✅ Power cycle recovery

**Nice to Have (Can Defer):**
- RDM functionality
- Advanced web features
- Performance optimizations

---

**End of Hardware Test Plan**

**Next Steps After Testing:**
1. Document all test results
2. File bugs for any failures
3. Implement fixes
4. Retest failed items
5. Proceed to production deployment
