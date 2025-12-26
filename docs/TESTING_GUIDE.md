# HƯỚNG DẪN TESTING VẬT LÝ

**Dự án:** Artnet-Node-2RDM  
**Tài liệu:** Physical Testing Guide  
**Phiên bản:** 1.0  
**Ngày tạo:** 25/12/2025

---

## MỤC ĐÍCH

Tài liệu này hướng dẫn chi tiết cách test vật lý firmware sau khi flash lên thiết bị, bao gồm:

- Test từng module độc lập
- Test tích hợp các module
- Verify chức năng end-to-end
- Performance testing
- Stress testing
- Troubleshooting

---

## CHUẨN BỊ

### Thiết bị cần thiết

1. **ESP32-S3 Board** với firmware đã flash
2. **DMX Controller/Console** (hoặc software như QLC+, Freestyler)
3. **DMX Fixtures** (đèn LED, moving heads, etc.) - ít nhất 2 fixtures
4. **DMX Cables** và XLR connectors
5. **Network Equipment**:
   - Router/Switch Ethernet
   - Ethernet cable Cat5e/Cat6
   - Computer với Art-Net/sACN software
6. **RDM Devices** (optional, cho RDM testing)
7. **Multimeter** (optional, để đo voltage/signal)
8. **Logic Analyzer** hoặc Oscilloscope (optional, để debug DMX timing)

### Software Tools

1. **Art-Net Software**:

   - QLC+ (free, cross-platform)
   - Freestyler DMX (Windows)
   - DMXControl 3 (Windows)
   - LightJams (Windows/Mac)

2. **sACN Software**:

   - QLC+ (supports E1.31)
   - OLA (Open Lighting Architecture)
   - ETC Eos offline

3. **Network Tools**:

   - Wireshark (packet capture)
   - Advanced IP Scanner
   - Angry IP Scanner

4. **Web Browser**:
   - Chrome/Firefox/Edge (để access web interface)

---

## 1. KIỂM TRA BAN ĐẦU (INITIAL CHECKS)

### 1.1. Power On Test

**Mục tiêu:** Verify thiết bị khởi động đúng

**Các bước:**

1. Kết nối nguồn 5V DC vào board
2. Quan sát LED status:
   - ✅ LED sáng màu **xanh dương nhạt** (Boot state)
   - ⏱️ Sau 2-5 giây, LED đổi màu tùy network state

**Kết quả mong đợi:**

- LED không nhấp nháy đỏ (error)
- Board không reset liên tục
- Có thể thấy WiFi AP "ArtnetNode-XXXX" nếu không có Ethernet

**Troubleshooting:**

- LED đỏ nhấp nhanh → Check serial log, có thể lỗi hardware hoặc firmware corrupt
- Không thấy LED → Check power supply, check GPIO 48 connection

### 1.2. Serial Monitor Check

**Mục tiêu:** Xem boot logs và debug info

**Các bước:**

1. Kết nối USB serial (115200 baud)
2. Mở serial monitor (PuTTY, screen, Arduino IDE)
3. Reset board (press EN button)
4. Quan sát boot sequence

**Log mong đợi:**

```
ESP-ROM:esp32s3-20210327
Build:Mar 27 2021
...
I (XXX) STORAGE: LittleFS mounted, total=1048576, used=XXXX
I (XXX) CONFIG: Configuration loaded
I (XXX) NETWORK: Initializing network...
I (XXX) NETWORK: Ethernet link up
I (XXX) NETWORK: IP address: 192.168.1.100
I (XXX) DMX: Port 1 initialized (mode: OUT, universe: 0)
I (XXX) DMX: Port 2 initialized (mode: OUT, universe: 1)
I (XXX) ARTNET: Listening on UDP port 6454
I (XXX) SACN: Listening on UDP port 5568
I (XXX) WEBSERVER: HTTP server started on port 80
I (XXX) SYSTEM: Boot completed in XXX ms
```

---

## 2. MODULE TESTING

### 2.1. Network Module

#### Test 2.1.1: Ethernet Connection (Priority)

**Các bước:**

1. Kết nối Ethernet cable từ board đến router/switch
2. Quan sát LED status:
   - ✅ Màu **xanh lá tĩnh** = Ethernet connected
3. Check serial log:
   ```
   I (XXX) NETWORK: Ethernet link up
   I (XXX) NETWORK: IP address: 192.168.1.XXX
   ```
4. Từ computer, ping IP address:
   ```bash
   ping 192.168.1.100
   ```
   - ✅ Nhận được reply

**Test cases:**

- [ ] Ethernet DHCP - nhận IP tự động
- [ ] Ethernet Static IP - cấu hình IP tĩnh
- [ ] Ethernet cable unplug/replug - reconnect trong 5s
- [ ] Ethernet link down - fallback to WiFi

#### Test 2.1.2: WiFi Station Mode

**Các bước:**

1. Không kết nối Ethernet
2. Cấu hình WiFi profile qua serial hoặc web
3. Reboot board
4. Quan sát LED:
   - ✅ Màu **xanh lá nhấp nháy chậm** = WiFi STA connected
5. Check IP address từ serial log

**Test cases:**

- [ ] WiFi STA connect - password đúng
- [ ] WiFi STA wrong password - skip và thử profile tiếp theo
- [ ] WiFi STA out of range - skip profile
- [ ] Multiple profiles - connect theo priority

#### Test 2.1.3: WiFi Access Point Mode

**Các bước:**

1. Không kết nối Ethernet và không có WiFi profile valid
2. Board sẽ tự động tạo WiFi AP
3. Quan sát LED:
   - ✅ Màu **tím tĩnh** = WiFi AP active
4. Từ phone/laptop, scan WiFi:
   - ✅ Thấy SSID: "ArtnetNode-XXXX"
5. Connect vào AP (password: "12345678" hoặc theo config)
6. Open browser, vào http://192.168.4.1

**Test cases:**

- [ ] AP mode activation khi không có network khác
- [ ] AP SSID hiển thị đúng
- [ ] AP password authentication
- [ ] Client device nhận IP từ DHCP
- [ ] Web interface accessible

---

### 2.2. LED Manager Module

#### Test 2.2.1: LED Status Indication

**Các bước:**

1. Test từng trạng thái network:

   - Boot: Xanh dương nhạt
   - Ethernet: Xanh lá tĩnh
   - WiFi STA: Xanh lá nhấp chậm
   - WiFi AP: Tím tĩnh

2. Test data receiving:

   - Gửi Art-Net/sACN packet
   - ✅ LED nhấp trắng 1 lần ngắn

3. Test error state:
   - Trigger error (disconnect critical component)
   - ✅ LED đỏ nhấp nhanh

**Test cases:**

- [ ] Boot LED color
- [ ] Network status LEDs
- [ ] Data pulse on packet (max 10 Hz)
- [ ] RDM discovery LED (vàng nhấp)
- [ ] Error LED (đỏ nhấp nhanh)

---

### 2.3. DMX/RDM Handler Module

#### Test 2.3.1: DMX Output Port 1

**Setup:**

- Kết nối DMX cable từ Port 1 đến DMX fixture
- Cấu hình Port 1: Mode = DMX_OUT, Universe = 0

**Các bước:**

1. Từ Art-Net controller, gửi DMX data đến Universe 0
2. Quan sát fixture:
   - ✅ Fixture nhận được DMX signal (LED indicator blink)
   - ✅ Fixture respond theo DMX values
3. Test các kênh:
   - Channel 1 = 255 → Fixture full brightness/value
   - Channel 1 = 0 → Fixture off/zero
   - Channel 1 = 128 → Fixture 50%

**Test cases:**

- [ ] DMX output active
- [ ] All 512 channels work correctly
- [ ] DMX timing (44 Hz frame rate)
- [ ] No flickering
- [ ] Blackout on timeout (3s no data)

#### Test 2.3.2: DMX Output Port 2

**Các bước:** Tương tự Port 1, nhưng:

- Kết nối Port 2
- Universe = 1 (hoặc theo config)
- Test độc lập với Port 1

**Test cases:**

- [ ] Port 2 hoạt động độc lập với Port 1
- [ ] Gửi data đến cả 2 ports đồng thời
- [ ] Verify không có crosstalk giữa 2 ports

#### Test 2.3.3: DMX Input Mode

**Setup:**

- Cấu hình Port 1: Mode = DMX_IN
- Kết nối DMX controller/console đến Port 1 input

**Các bước:**

1. Từ DMX console, gửi DMX data
2. Access web interface, check DMX levels page
3. Verify DMX values hiển thị chính xác

**Test cases:**

- [ ] DMX input detection
- [ ] All 512 channels read correctly
- [ ] Web interface updates in real-time
- [ ] DMX input → network output (if configured)

#### Test 2.3.4: RDM Discovery (Master Mode)

**Setup:**

- Cấu hình Port 1: Mode = RDM_MASTER
- Kết nối RDM-capable fixture đến Port 1

**Các bước:**

1. Access web interface → RDM page → Port 1
2. Click "Start Discovery"
3. Quan sát LED: Màu vàng nhấp nháy
4. Sau 5-10 giây, check danh sách devices
5. ✅ Fixture UID hiển thị trong danh sách

**Test cases:**

- [ ] RDM discovery tìm thấy devices
- [ ] Device information đầy đủ (UID, model, manufacturer)
- [ ] Discovery với multiple devices (2-5 devices)
- [ ] Re-discovery sau khi thêm/bớt device

#### Test 2.3.5: RDM Get/Set Parameters

**Setup:** Sau khi discovery thành công

**Các bước:**

1. Select device từ danh sách
2. **RDM GET**: Đọc DMX start address

   - Click "Get DMX Address"
   - ✅ Hiển thị địa chỉ hiện tại (e.g., 001)

3. **RDM SET**: Thay đổi DMX start address

   - Nhập địa chỉ mới: 010
   - Click "Set DMX Address"
   - ✅ Fixture cập nhật địa chỉ
   - Verify bằng cách GET lại

4. **Identify Device**:
   - Click "Identify"
   - ✅ Fixture blink/flash để identify

**Test cases:**

- [ ] GET DMX Start Address
- [ ] SET DMX Start Address
- [ ] GET Device Info
- [ ] GET Manufacturer Label
- [ ] GET Device Model Description
- [ ] Identify Device ON/OFF

---

### 2.4. Merge Engine Module

#### Test 2.4.1: HTP Merge Mode

**Setup:**

- Cấu hình Port 1: Merge Mode = HTP, Universe = 0
- 2 Art-Net sources gửi đến cùng Universe 0

**Các bước:**

1. Source 1 (IP .10): Ch1=100, Ch2=50
2. Source 2 (IP .20): Ch1=50, Ch2=200
3. Check output:
   - ✅ Ch1 = 100 (highest from Source 1)
   - ✅ Ch2 = 200 (highest from Source 2)

**Test cases:**

- [ ] HTP merge với 2 sources
- [ ] HTP merge với 4 sources
- [ ] One source timeout → use remaining source
- [ ] All sources timeout → blackout

#### Test 2.4.2: LTP Merge Mode

**Các bước:** Tương tự HTP, nhưng:

- Cấu hình Merge Mode = LTP
- Verify output = LOWEST values từ các sources

#### Test 2.4.3: LAST Merge Mode

**Các bước:**

1. Source 1 gửi: Ch1=100
2. Wait 1 second
3. Source 2 gửi: Ch1=50
4. ✅ Output = 50 (from last received source)

**Test cases:**

- [ ] Last packet wins
- [ ] Switching giữa các sources
- [ ] Timeout detection

#### Test 2.4.4: BACKUP Merge Mode

**Các bước:**

1. Source 1 (priority 100): Ch1=100
2. Source 2 (priority 50): Ch1=200
3. ✅ Output = 100 (priority source)
4. Disconnect Source 1
5. Wait 3 seconds (timeout)
6. ✅ Output = 200 (fallback to backup)

**Test cases:**

- [ ] Primary source active
- [ ] Backup source active when primary timeout
- [ ] Recovery khi primary trở lại

---

### 2.5. Art-Net Receiver Module

#### Test 2.5.1: Art-Net DMX Reception

**Setup:**

- QLC+ hoặc Art-Net software
- Cấu hình output Universe 0 → IP của ESP32

**Các bước:**

1. Từ QLC+, create fixture ở Universe 0
2. Set channel values
3. Start output
4. ✅ DMX fixture connected to Port 1 responds

**Test cases:**

- [ ] Receive Art-Net packets
- [ ] Correct universe routing
- [ ] Multiple universes (0, 1) → (Port 1, Port 2)
- [ ] Sequence number tracking
- [ ] Packet statistics accurate

#### Test 2.5.2: Art-Poll Reply

**Các bước:**

1. Từ Art-Net controller, send ArtPoll
2. Check controller's node list
3. ✅ "ArtNode-2RDM" hiển thị với đúng IP
4. Verify node information:
   - Short name
   - Long name
   - Number of ports (2)
   - Universe assignments

**Test cases:**

- [ ] ArtPoll response
- [ ] Node information correct
- [ ] Port status correct (active/inactive)

---

### 2.6. sACN Receiver Module

#### Test 2.6.1: sACN Multicast Reception

**Setup:**

- QLC+ với E1.31 (sACN) output enabled
- Universe 1 → Multicast 239.255.0.1

**Các bước:**

1. Enable E1.31 output trong QLC+
2. Set fixture values
3. ✅ DMX fixture responds

**Test cases:**

- [ ] Receive sACN multicast packets
- [ ] Correct universe routing
- [ ] Priority handling (0-200)
- [ ] Source name tracking
- [ ] Sequence number checking

#### Test 2.6.2: sACN Priority

**Các bước:**

1. Source 1 (priority 100): Ch1=100
2. Source 2 (priority 150): Ch1=200
3. ✅ Output = 200 (higher priority wins)

**Test cases:**

- [ ] Priority comparison
- [ ] Priority override
- [ ] Priority with merge modes

---

### 2.7. Web Server Module

#### Test 2.7.1: Web Interface Access

**Các bước:**

1. Open browser, navigate to http://[ESP32_IP]
2. ✅ Web page loads successfully
3. Verify UI elements:
   - Navigation menu
   - Status dashboard
   - Configuration pages

**Test cases:**

- [ ] HTTP server responds
- [ ] Static files load (HTML, CSS, JS)
- [ ] No 404 errors
- [ ] Responsive design (mobile/desktop)

#### Test 2.7.2: Configuration APIs

**Test GET /api/config:**

```bash
curl http://192.168.1.100/api/config
```

✅ Returns JSON with full configuration

**Test POST /api/config:**

```bash
curl -X POST http://192.168.1.100/api/config \
  -H "Content-Type: application/json" \
  -d '{"universe_primary_port1": 5}'
```

✅ Configuration updated

**Test cases:**

- [ ] GET config returns valid JSON
- [ ] POST config updates successfully
- [ ] Invalid JSON returns 400 error
- [ ] Config persists after reboot

#### Test 2.7.3: WiFi Management

**Các bước:**

1. Access /api/wifi/scan
2. ✅ Returns list of WiFi networks
3. Add WiFi profile:
   - SSID, password, priority
4. ✅ Profile saved
5. Reboot → ✅ Connect to new profile

**Test cases:**

- [ ] WiFi scan works
- [ ] Add/delete profiles
- [ ] Profile priority sorting
- [ ] Connect to configured profile

#### Test 2.7.4: WebSocket Real-time Updates

**Các bước:**

1. Open web interface → DMX Monitor page
2. Open WebSocket connection to /ws/dmx/1
3. Send DMX data từ Art-Net controller
4. ✅ DMX levels update real-time trên web page

**Test cases:**

- [ ] WebSocket connection established
- [ ] Real-time DMX level updates
- [ ] Multiple WebSocket clients
- [ ] WebSocket disconnect/reconnect

---

### 2.8. Storage Module

#### Test 2.8.1: Configuration Persistence

**Các bước:**

1. Change configuration qua web interface
2. Reboot device (power cycle)
3. ✅ Configuration retained sau reboot
4. Verify từ /api/config

**Test cases:**

- [ ] Config save to NVS
- [ ] Config save to LittleFS JSON
- [ ] Config load priority (NVS → File → Defaults)
- [ ] Config backup file creation

#### Test 2.8.2: Factory Reset

**Các bước:**

1. Call /api/system/factory-reset
2. Device reboots
3. ✅ All settings trở về mặc định
4. WiFi AP mode active (no saved profiles)

**Test cases:**

- [ ] Factory reset clears NVS
- [ ] Factory reset clears config file
- [ ] Default config loaded
- [ ] Web files intact (not deleted)

---

## 3. TÍCH HỢP TESTING (INTEGRATION)

### 3.1. Full System Test

**Scenario:** Complete Art-Net to DMX workflow

**Setup:**

- Ethernet connected
- 2 DMX fixtures on Port 1 and Port 2
- QLC+ with 2 universes

**Các bước:**

1. **Network**: Verify Ethernet connected
2. **Art-Net**: QLC+ sends to Universe 0, 1
3. **Merge**: Single source, no merge
4. **DMX Output**: Both fixtures respond correctly
5. **LED**: White pulse on packet receive
6. **Web**: Monitor DMX levels real-time

**Pass criteria:**

- [ ] End-to-end latency < 10ms
- [ ] No packet loss
- [ ] Stable operation > 30 minutes
- [ ] No memory leaks
- [ ] CPU usage < 50%

### 3.2. Multi-Source Merge Test

**Scenario:** 2 controllers merge HTP to single port

**Setup:**

- 2 Art-Net sources (QLC+ on 2 computers)
- Both send to Universe 0
- Merge Mode = HTP

**Các bước:**

1. Source 1: Ch1-10 = 100
2. Source 2: Ch11-20 = 200
3. ✅ Output: Ch1-10=100, Ch11-20=200
4. Source 1: Ch1=255
5. ✅ Output: Ch1=255 (higher wins)

**Pass criteria:**

- [ ] Correct HTP merge
- [ ] No flickering
- [ ] Smooth transitions
- [ ] Timeout handling correct

---

## 4. PERFORMANCE TESTING

### 4.1. Maximum Packet Rate

**Mục tiêu:** Verify device có thể handle high packet rate

**Test:**

1. QLC+ send Art-Net at max rate (44 Hz per universe)
2. Monitor với Wireshark
3. Check DMX output không bị drop frames

**Metrics:**

- Packet rate: > 40 Hz
- CPU usage: < 80%
- DMX frame rate: stable 44 Hz
- Latency: < 5ms

### 4.2. Multiple Universe Stress Test

**Test:**

1. Send data to Universe 0-9 (10 universes)
2. Device chỉ subscribe Universe 0, 1
3. ✅ Device không bị overload
4. Port 1, 2 vẫn stable

**Metrics:**

- CPU usage: < 70%
- Memory usage: stable
- No crashes
- Other universes ignored correctly

### 4.3. Long-term Stability Test

**Test:**

1. Chạy liên tục 24 giờ
2. Monitor memory usage
3. Check uptime

**Pass criteria:**

- [ ] No crashes
- [ ] No memory leaks
- [ ] Stable performance
- [ ] No unexpected reboots

---

## 5. ERROR HANDLING & RECOVERY

### 5.1. Network Disconnect Recovery

**Test:**

1. Ethernet connected, DMX output active
2. Unplug Ethernet cable
3. ✅ Fallback to WiFi trong 10s
4. DMX output continues
5. Replug Ethernet
6. ✅ Switch back to Ethernet

### 5.2. Power Failure Recovery

**Test:**

1. Device running
2. Ngắt nguồn đột ngột
3. Cấp nguồn lại
4. ✅ Device boots, loads config
5. Reconnects to network
6. Resumes DMX output

### 5.3. Invalid Data Handling

**Test:**

1. Send malformed Art-Net packet
2. ✅ Device drops packet, logs error
3. Device continues operating normally

---

## 6. TROUBLESHOOTING

### Common Issues

| Symptom                             | Possible Cause                 | Solution                                      |
| ----------------------------------- | ------------------------------ | --------------------------------------------- |
| LED đỏ nhấp nhanh                   | System error, boot fail        | Check serial log, reflash firmware            |
| Không có LED                        | Power issue, GPIO 48 fail      | Check power supply, check WS2812 connection   |
| Không connect WiFi                  | Wrong password, out of range   | Check WiFi credentials, move closer to AP     |
| Không nhận Art-Net                  | Wrong universe, firewall       | Check universe config, disable firewall       |
| DMX fixture không respond           | Cable issue, wrong universe    | Check DMX cable, verify universe mapping      |
| DMX flickering                      | Packet loss, merge timeout     | Check network quality, increase merge timeout |
| Web interface không load            | Network issue, server crash    | Check IP address, reboot device               |
| RDM discovery không tìm thấy device | RDM not supported, cable issue | Verify device supports RDM, check cable       |

### Debug Tools

**Serial Monitor:**

```bash
# Linux/Mac
screen COM3 115200

# Windows
PuTTY → Serial → COM3 → 115200
```

**Network Packet Capture:**

```bash
# Wireshark filter for Art-Net
udp.port == 6454

# Wireshark filter for sACN
udp.port == 5568
```

**Check Device IP:**

```bash
# Scan network
nmap -sn 192.168.1.0/24

# Or use Advanced IP Scanner (Windows)
```

---

## 7. TEST REPORT TEMPLATE

```markdown
# Test Report - ESP-NODE-2RDM

**Date:** [Date]
**Firmware Version:** [Version]
**Tester:** [Name]

## Test Summary

- Total Tests: XX
- Passed: XX
- Failed: XX
- Skipped: XX

## Module Test Results

### Network Module

- [x] Ethernet Connection - PASS
- [x] WiFi STA Mode - PASS
- [x] WiFi AP Mode - PASS

### DMX Module

- [x] Port 1 Output - PASS
- [x] Port 2 Output - PASS
- [ ] DMX Input - FAIL (see notes)

### RDM Module

- [x] Discovery - PASS
- [x] GET Parameters - PASS
- [x] SET Parameters - PASS

## Issues Found

1. [Issue description]
   - Severity: High/Medium/Low
   - Reproducible: Yes/No
   - Workaround: [If any]

## Notes

[Additional observations]

## Conclusion

[Overall assessment - PASS/FAIL]
```

---

## APPENDIX

### A. Quick Test Checklist

```
BASIC TESTS (Required)
□ Power on - LED boot color
□ Network connect - Ethernet or WiFi
□ Web interface accessible
□ DMX Port 1 output works
□ DMX Port 2 output works
□ Art-Net reception
□ Config save/load
□ Factory reset

ADVANCED TESTS (Optional)
□ RDM discovery
□ RDM get/set
□ sACN reception
□ Merge modes (HTP, LTP, LAST, BACKUP)
□ Multi-source merge
□ WebSocket real-time updates
□ Long-term stability (24h)

PERFORMANCE (Optional)
□ Max packet rate test
□ CPU usage < 80%
□ Memory stable
□ Latency < 10ms
```

### B. Test Equipment Recommendations

**Budget Setup ($100-200):**

- ESP32-S3 board
- 1-2 cheap DMX LED fixtures
- Home router
- Computer with QLC+ (free)

**Professional Setup ($500-1000):**

- ESP32-S3 board
- 2-4 RDM-capable fixtures
- Managed Ethernet switch
- DMX controller/console
- Logic analyzer
- Multiple test computers

---

**KẾT THÚC HƯỚNG DẪN TESTING**

Tuân thủ testing procedure này đảm bảo firmware hoạt động ổn định và đúng spec trước khi release!
