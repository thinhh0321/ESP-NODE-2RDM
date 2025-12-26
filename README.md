# ESP-NODE-2RDM

**Art-Net / sACN to DMX512/RDM Converter**  
2 Independent DMX/RDM Ports | ESP32-S3 | Ethernet + WiFi

---

## 📚 Tài liệu dự án

Dự án bao gồm tài liệu thiết kế chi tiết, kế hoạch phát triển và quy chuẩn lập trình:

### Kế hoạch phát triển

- **[📋 DEVELOPMENT_SUMMARY.md](docs/DEVELOPMENT_SUMMARY.md)** - **ĐỌC ĐẦU TIÊN** - Tóm tắt tổng quan kế hoạch
- **[📘 FIRMWARE_DEVELOPMENT_PLAN.md](docs/FIRMWARE_DEVELOPMENT_PLAN.md)** - Kế hoạch chi tiết, cấu trúc project, roadmap
- **[🔧 LIBRARY_INTEGRATION_GUIDE.md](docs/LIBRARY_INTEGRATION_GUIDE.md)** - Hướng dẫn tích hợp thư viện
- **[🗺️ IMPLEMENTATION_ROADMAP.md](docs/IMPLEMENTATION_ROADMAP.md)** - Lộ trình triển khai từng sprint
- **[🔀 ALTERNATIVE_APPROACHES.md](docs/ALTERNATIVE_APPROACHES.md)** - So sánh phương án thay thế

### Tổng quan hệ thống

- **[TongQuan.md](TongQuan.md)** - Tài liệu tổng quan toàn bộ hệ thống firmware

### Thiết kế Module (Design Documents)

Tài liệu thiết kế chi tiết cho từng module độc lập:

1. **[Configuration Module](docs/modules/DESIGN_MODULE_Configuration.md)**

   - Quản lý cấu hình hệ thống
   - NVS và LittleFS storage
   - JSON serialization

2. **[Network Module](docs/modules/DESIGN_MODULE_Network.md)**

   - Ethernet W5500 (SPI)
   - WiFi Station/AP modes
   - Auto-fallback mechanism

3. **[LED Manager Module](docs/modules/DESIGN_MODULE_LED_Manager.md)**

   - WS2812 status LED control
   - State indication
   - Event-based triggers

4. **[DMX/RDM Handler Module](docs/modules/DESIGN_MODULE_DMX_RDM_Handler.md)**

   - DMX512 Output/Input
   - RDM Master/Responder
   - 2 independent ports

5. **[Merge Engine Module](docs/modules/DESIGN_MODULE_Merge_Engine.md)**

   - HTP/LTP/LAST/BACKUP modes
   - Multi-source merging
   - Timeout handling

6. **[Art-Net Receiver Module](docs/modules/DESIGN_MODULE_ArtNet_Receiver.md)**

   - Art-Net v4 protocol
   - ArtPoll/ArtPollReply
   - Universe routing

7. **[sACN Receiver Module](docs/modules/DESIGN_MODULE_sACN_Receiver.md)**

   - E1.31 (Streaming ACN)
   - Multicast reception
   - Priority handling

8. **[Web Server Module](docs/modules/DESIGN_MODULE_Web_Server.md)**

   - HTTP REST API
   - WebSocket real-time updates
   - Configuration interface

9. **[Storage Module](docs/modules/DESIGN_MODULE_Storage.md)**
   - LittleFS file system
   - NVS backup storage
   - Config persistence

### Quy chuẩn và Testing

- **[CODING_STANDARDS.md](docs/CODING_STANDARDS.md)** - Quy chuẩn lập trình bắt buộc

  - Naming conventions
  - Code formatting
  - Error handling
  - Thread safety
  - Performance best practices

- **[TESTING_GUIDE.md](docs/TESTING_GUIDE.md)** - Hướng dẫn testing vật lý
  - Module testing procedures
  - Integration testing
  - Performance testing
  - Troubleshooting guide

---

## 🎯 Tính năng chính

### Giao thức mạng

- ✅ **Art-Net v4** - UDP port 6454
- ✅ **sACN (E1.31)** - Multicast support
- ✅ Protocol priority và merge modes

### DMX512 & RDM

- ✅ 2 cổng DMX512 độc lập (configurable)
- ✅ DMX Output (~44 Hz refresh rate)
- ✅ DMX Input (monitoring)
- ✅ RDM Master (discovery, get/set parameters)
- ✅ RDM Responder mode

### Merge Engine

- ✅ HTP (Highest Takes Precedence)
- ✅ LTP (Lowest Takes Precedence)
- ✅ LAST (Last packet wins)
- ✅ BACKUP (Primary + Backup source)
- ✅ Multi-source support (up to 4 sources)

### Network

- ✅ Ethernet W5500 (primary, SPI interface)
- ✅ WiFi Station mode (multiple profiles)
- ✅ WiFi Access Point mode (fallback)
- ✅ Auto-fallback: Ethernet → WiFi STA → WiFi AP

### Web Interface

- ✅ Full configuration interface
- ✅ Real-time DMX monitoring (WebSocket)
- ✅ RDM device control
- ✅ Network status & statistics
- ✅ Firmware OTA update

### Hardware

- **MCU:** ESP32-S3-WROOM-1-N16R8 (16MB Flash, 8MB PSRAM)
- **Network:** W5500 Ethernet SPI
- **DMX:** RS485 transceivers (2 ports)
- **LED:** WS2812 status indicator
- **Development:** ESP-IDF v5.2.6

---

## 🚀 Quick Start

### Build Firmware

```bash
# Clone repository
git clone https://github.com/thinhh0321/ESP-NODE-2RDM.git
cd ESP-NODE-2RDM

# Set up ESP-IDF v5.2.6
. $IDF_PATH/export.sh

# Configure, build and flash
idf.py menuconfig
idf.py build
idf.py -p COM3 flash monitor
```

### Initial Configuration

1. **Power on** device → LED màu xanh dương
2. **Connect Ethernet** → LED chuyển xanh lá (connected)
3. **Access web interface**: http://[device_ip]
4. **Configure**:
   - Network settings
   - DMX port modes và universes
   - Protocol và merge modes

### Testing

Xem chi tiết tại [TESTING_GUIDE.md](docs/TESTING_GUIDE.md)

---

## 📋 Requirements

### Development

- ESP-IDF v5.2.6
- CMake 3.16+
- Python 3.8+

### Hardware

- ESP32-S3 module với ≥16MB Flash
- W5500 Ethernet module
- RS485 transceivers (MAX485 hoặc tương tự)
- WS2812 LED (optional, cho status)

---

## 📖 Documentation Structure

```
ESP-NODE-2RDM/
├── README.md                          # This file
├── TongQuan.md                        # System overview (Vietnamese)
├── docs/
│   ├── CODING_STANDARDS.md            # Coding standards & best practices
│   ├── TESTING_GUIDE.md               # Physical testing guide
│   └── modules/                       # Module design documents
│       ├── DESIGN_MODULE_Configuration.md
│       ├── DESIGN_MODULE_Network.md
│       ├── DESIGN_MODULE_LED_Manager.md
│       ├── DESIGN_MODULE_DMX_RDM_Handler.md
│       ├── DESIGN_MODULE_Merge_Engine.md
│       ├── DESIGN_MODULE_ArtNet_Receiver.md
│       ├── DESIGN_MODULE_sACN_Receiver.md
│       ├── DESIGN_MODULE_Web_Server.md
│       └── DESIGN_MODULE_Storage.md
└── [source code directories - to be added]
```

---

## 🤝 Contributing

Khi contribute code, vui lòng:

1. Đọc và tuân thủ [CODING_STANDARDS.md](docs/CODING_STANDARDS.md)
2. Test theo [TESTING_GUIDE.md](docs/TESTING_GUIDE.md)
3. Document code theo format quy định
4. Không commit code có compiler warnings

---

## 📄 License

[MIT License](LICENSE)

---

## 👤 Author

**thinhh0321**

---

## 🔗 References

- [Art-Net Protocol](https://art-net.org.uk/)
- [sACN (E1.31) Standard](https://tsp.esta.org/tsp/documents/docs/ANSI_E1-31-2018.pdf)
- [DMX512 Standard](https://tsp.esta.org/tsp/documents/docs/ANSI-ESTA_E1-11_2008R2018.pdf)
- [RDM Standard](https://tsp.esta.org/tsp/documents/docs/ANSI-ESTA_E1-20_2010.pdf)
- [ESP-IDF Documentation](https://docs.espressif.com/projects/esp-idf/en/v5.2.6/)
