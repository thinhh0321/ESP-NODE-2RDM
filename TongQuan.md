# TÀI LIỆU THIẾT KẾ FIRMWARE  
**DỰ ÁN: Artnet-Node-2RDM**  
**Nền tảng: ESP32-S3-WROOM-1-N16R8 (16MB Flash, 8MB PSRAM)**  
**Môi trường phát triển: ESP-IDF v5.2.6**

**Phiên bản: 1.3**  
**Ngày lập: 25/12/2025**

---

## 1. Tổng quan hệ thống

**Artnet-Node-2RDM** là thiết bị chuyển đổi giao thức mạng thành tín hiệu DMX512/RDM với 2 cổng độc lập.  
Thiết bị hỗ trợ:

- **Giao thức nguồn**: Art-Net v4, sACN (E1.31)
- **Chế độ DMX**: Out, In, RDM Master, RDM Responder, Disabled (riêng cho từng cổng)
- **Merge Engine**: HTP, LTP, Last, Backup
- **Kết nối mạng**: Ethernet W5500 (ưu tiên), fallback WiFi STA/AP
- **RDM**: Hỗ trợ discovery, get/set (bidirectional)
- **Giao diện web**: Cấu hình, test DMX, RDM control, status
- **LED trạng thái**: WS2812 (GPIO 48)
- **Lưu trữ**: LittleFS (config.json + web files)

---

## 2. Thông số phần cứng cố định

| Chức năng                  | GPIO / Chân                  | Ghi chú                                                                 |
|----------------------------|------------------------------|-------------------------------------------------------------------------|
| WS2812 LED                 | GPIO 48                      | Data pin                                                                |
| DMX Port 1 TX              | GPIO 17                      | UART TX (hoặc GPIO matrix)                                              |
| DMX Port 1 RX              | GPIO 16                      | UART RX                                                                 |
| DMX Port 1 DIR             | GPIO 21                      | HIGH = TX, LOW = RX                                                     |
| DMX Port 2 TX              | GPIO 19                      | UART TX                                                                 |
| DMX Port 2 RX              | GPIO 18                      | UART RX                                                                 |
| DMX Port 2 DIR             | GPIO 20                      | HIGH = TX, LOW = RX                                                     |
| W5500 SPI CS               | GPIO 10                      | Chip Select                                                             |
| W5500 SPI MOSI             | GPIO 11                      | VSPI MOSI                                                               |
| W5500 SPI MISO             | GPIO 13                      | VSPI MISO                                                               |
| W5500 SPI SCK              | GPIO 12                      | VSPI SCK                                                                |
| W5500 INT (optional)       | GPIO 9                       | Interrupt                                                               |

---

## 3. Cấu trúc module hệ thống

| Module                     | Trách nhiệm chính                                                                 | Phụ thuộc chính                     |
|----------------------------|-----------------------------------------------------------------------------------|-------------------------------------|
| **Configuration**          | Quản lý toàn bộ thông số cấu hình                                                 | Storage                             |
| **Network**                | Ethernet W5500 + WiFi STA/AP                                                      | —                                   |
| **LED Manager**            | Điều khiển WS2812 báo trạng thái                                                  | —                                   |
| **DMX/RDM Handler**        | Quản lý 2 cổng DMX512/RDM (In/Out)                                                | Configuration                       |
| **Merge Engine**           | Hợp nhất dữ liệu từ nhiều nguồn                                                   | DMX/RDM Handler                     |
| **Art-Net Receiver**       | Nhận & xử lý Art-Net UDP                                                          | Network                             |
| **sACN Receiver**          | Nhận & xử lý sACN multicast                                                       | Network                             |
| **Web Server**             | Giao diện web (HTTP + WebSocket)                                                  | Configuration, DMX/RDM, Network     |
| **Storage**                | LittleFS – lưu config.json & web files                                            | —                                   |

---

## 4. Yêu cầu chức năng chi tiết từng module

### 4.1. Configuration

**Thông số cấu hình chính**:

| Nhóm                  | Thông số                                      | Kiểu dữ liệu | Mặc định / Phạm vi                                   |
|-----------------------|-----------------------------------------------|--------------|------------------------------------------------------|
| Network               | use_ethernet                                  | bool         | true                                                 |
|                       | wifi_profiles[5]                              | array        | [{ssid, pass, priority}]                             |
|                       | ap_ssid                                       | string       | "ArtnetNode-XXXX"                                    |
|                       | ap_password                                   | string       | "12345678"                                           |
|                       | ap_channel                                    | uint8_t      | 1–13                                                 |
| DMX Ports             | port1_mode                                    | enum         | DMX_OUT / DMX_IN / RDM_MASTER / RDM_RESPONDER / DISABLED |
|                       | port2_mode                                    | enum         | ...                                                  |
|                       | universe_primary_port1 / _port2               | uint16_t     | 0–32767                                              |
|                       | universe_secondary_port1 / _port2             | int16_t      | -1 (tắt) hoặc 0–32767                                |
|                       | universe_offset_port1 / _port2                | int16_t      | -512 đến +512                                        |
| Protocol              | protocol_mode_port1 / _port2                  | enum         | ARTNET_ONLY / SACN_ONLY / ARTNET_PRIORITY / SACN_PRIORITY / MERGE_BOTH |
| Merge                 | merge_mode_port1 / _port2                     | enum         | HTP / LTP / LAST / BACKUP / DISABLE                  |
|                       | merge_timeout_seconds                         | uint8_t      | 2–10 (giây)                                          |
| RDM                   | rdm_enabled_port1 / _port2                    | bool         | true                                                 |
| Node Info             | node_short_name                               | string       | max 18 ký tự                                         |
|                       | node_long_name                                | string       | max 64 ký tự                                         |

### 4.2. Network

- Ưu tiên Ethernet W5500 (thử 3 lần, timeout 10s mỗi lần)
- Nếu Ethernet fail → thử WiFi STA theo thứ tự priority của profiles
- Nếu WiFi STA fail → chuyển sang WiFi AP
- Hỗ trợ IP tĩnh cho Ethernet & WiFi STA

### 4.3. LED Manager

**Trạng thái LED**:

| Trạng thái                        | Màu RGB            | Hành vi                          |
|-----------------------------------|--------------------|----------------------------------|
| Boot                              | Xanh dương nhạt    | Tĩnh                             |
| Ethernet OK                       | Xanh lá            | Tĩnh                             |
| WiFi STA OK                       | Xanh lá + nhấp     | Nhấp nháy chậm                   |
| WiFi AP active                    | Tím                | Tĩnh                             |
| Nhận Art-Net/sACN                 | Trắng              | Nhấp 1 lần (max 10 Hz)           |
| RDM discovery đang diễn ra        | Vàng               | Nhấp nháy chậm                   |
| Lỗi nghiêm trọng                  | Đỏ                 | Nhấp nhanh 200ms                 |

### 4.4. DMX/RDM Handler

- Chế độ DMX Out: gửi DMX frame 512 kênh + start code 0x00
- Chế độ DMX In: đọc DMX input, gửi lên mạng (Art-Net/sACN)
- Chế độ RDM Master: hỗ trợ discovery, get/set PID
- Chế độ RDM Responder: trả lời RDM request
- Tần suất DMX Out: ~44 Hz
- Blackout nếu không có dữ liệu mới trong merge_timeout

### 4.5. Merge Engine

**Chế độ merge**:

| Chế độ   | Mô tả chi tiết                                                                 |
|----------|--------------------------------------------------------------------------------|
| HTP      | Highest Takes Precedence                                                       |
| LTP      | Lowest Takes Precedence                                                        |
| Last     | Gói tin nhận sau cùng thắng                                                    |
| Backup   | Nguồn chính + backup (chuyển backup nếu chính mất > timeout)                  |
| Disable  | Không merge, chỉ dùng nguồn ưu tiên                                            |

### 4.6. Art-Net Receiver

- Nhận ArtDmx, ArtPoll, ArtPollReply, ArtSync, ArtAddress
- Hỗ trợ Net/Sub/Universe format

### 4.7. sACN Receiver

- Nhận multicast E1.31 Data packet
- Kiểm tra Sequence Number, Source Name, Priority
- Join multicast group theo universe

### 4.8. Web Server

**Endpoint chính**:

| Đường dẫn                          | Method | Mô tả                                                                 |
|------------------------------------|--------|-----------------------------------------------------------------------|
| `/`                                | GET    | Trang chính (index.html)                                              |
| `/config`                          | GET/POST | Lấy/đặt toàn bộ cấu hình                                              |
| `/status`                          | GET    | Trạng thái: IP, link, packet count, DMX levels                        |
| `/wifi/scan`                       | GET    | Quét mạng WiFi                                                        |
| `/wifi/profiles`                   | GET/POST/DELETE | Quản lý profile WiFi STA                                              |
| `/ap/config`                       | GET/POST | Cấu hình WiFi AP                                                      |
| `/ports/config`                    | GET/POST | Cấu hình chế độ DMX/RDM & universe cho từng cổng                      |
| `/protocol/config`                 | GET/POST | Cấu hình Art-Net / sACN mode                                          |
| `/merge/config`                    | GET/POST | Cấu hình Merge Engine                                                 |
| `/test/dmx/port1` / `/port2`       | WS     | WebSocket real-time DMX levels & test                                 |
| `/rdm/discover/port1` / `/port2`   | POST   | Bắt đầu RDM discovery                                                 |
| `/rdm/devices/port1` / `/port2`    | GET    | Danh sách thiết bị RDM                                                |
| `/rdm/set/port1` / `/port2`        | POST   | Set RDM parameter                                                     |
| `/rdm/get/port1` / `/port2`        | POST   | Get RDM parameter                                                     |

**WebSocket payload**:

- `dmx_update`: levels 512 kênh
- `dmx_test`: client gửi kênh + giá trị
- `rdm_event`: device_found, error, response
- `status_update`: packet rate, link status

### 4.9. Storage

- LittleFS partition ~1MB
- File: `config.json`, `index.html`, `script.js`, `style.css`

---

## 5. Luồng hoạt động tổng thể

1. **Boot** → NVS → LittleFS → Load config → LED xanh dương
2. **Network init** → Ethernet (3 lần) → WiFi STA → AP → LED tương ứng
3. **DMX/RDM init** → cấu hình theo port_mode, universe
4. **Protocol init** → Join multicast sACN, bind UDP Art-Net
5. **Task Network (Core 0)** → Nhận Art-Net/sACN → Merge Engine → Queue DMX
6. **Task DMX Output (Core 1)** → Đọc queue → Gửi DMX frame → Xử lý RDM
7. **Web Server** → Phục vụ HTTP/WebSocket → Cập nhật cấu hình → Test DMX/RDM
8. **LED** → Cập nhật trạng thái liên tục

---

Tài liệu này bao quát **toàn bộ thiết kế firmware**
