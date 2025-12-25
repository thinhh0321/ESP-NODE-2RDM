Tài Liệu Dự Án: Artnet-Node-2RDM trên ESP32 S3 N16R8
1. Giới Thiệu Dự Án
Dự án "Artnet-Node-2RDM" là thiết bị điều khiển DMX/RDM dựa trên ESP32 S3 N16R8, hoạt động như một Art-Net node với hỗ trợ bổ sung sACN (Streaming ACN). Thiết bị có 2 cổng DMX/RDM độc lập, giao diện web để cấu hình và test, và sử dụng FreeRTOS để phân chia tác vụ giữa hai core. Nó phù hợp cho các ứng dụng sân khấu, chiếu sáng, và hệ thống DMX chuyên nghiệp.

Tính Năng Chính
2 Cổng DMX/RDM: Mỗi cổng có thể cấu hình độc lập (DMX-out, DMX-in, RDM).
Hỗ trợ Giao Thức: Art-Net (port 6454) và sACN (port 5568, multicast).
Merge Engine: Hỗ trợ LTP (Latest Takes Precedence), HTP (Highest Takes Precedence), hoặc Disable cho mỗi cổng.
Giao Diện Web: Cấu hình mạng, cổng, sACN, và Merge Engine; giao diện ảo để test tín hiệu DMX (hiển thị kênh, điều khiển slider).
Kết Nối Mạng: Ethernet (W5500), WiFi STA, hoặc WiFi AP.
FreeRTOS: Core 0 xử lý giao thức mạng và web; Core 1 xử lý xuất DMX và báo lỗi qua LED WS2812 (GPIO 48).
Thư Viện Chính: esp-dmx cho DMX/RDM, ESPAsyncWebServer cho web, ESP32-sACN cho sACN.
Model ESP32: ESP32 S3 N16R8 (16MB flash, 8MB PSRAM) – đủ tài nguyên cho xử lý nặng mà không lag.
Yêu Cầu Hệ Thống
Kiến thức cơ bản về lập trình ESP32 (Arduino IDE hoặc ESP-IDF).
Phần mềm: Arduino IDE với ESP32 board support.
Tài nguyên: Nguồn 5V/2A, cáp DMX, và thiết bị test (như DMX controller hoặc QLC+).
2. Phần Cứng
Linh Kiện Chính
Bộ Vi Điều Khiển: ESP32 S3 N16R8 (ESP32-S3 với 16MB flash và 8MB PSRAM). GPIO đủ cho 2 cổng DMX, Ethernet, và LED.
Cổng DMX/RDM (2 cổng):
IC Driver: SN75176 hoặc MAX485 (cho RS-485).
Kết nối GPIO: UART1 (GPIO 16/17) cho cổng 1, UART2 (GPIO 18/19) cho cổng 2.
Bảo vệ: Optocoupler (6N137) để chống nhiễu.
Kết nối vật lý: Cổng XLR 5-pin hoặc RJ45.
Kết Nối Mạng:
W5500 Ethernet module (SPI: GPIO 12/13/14/15).
WiFi tích hợp trong ESP32 S3.
LED Báo Lỗi: WS2812 (1-2 LED) tại GPIO 48, với resistor 470Ω và capacitor 100nF.
Linh Kiện Bổ Sung:
Nguồn điện: Bộ chuyển đổi 5V (từ USB hoặc 12V).
LED chỉ báo trạng thái (cho mạng và cổng).
Nút reset/boot.
PCB tùy chỉnh (thiết kế bằng KiCad hoặc EasyEDA).
Sơ Đồ Mạch (Tóm Tắt)
ESP32 S3 kết nối UART đến driver DMX.
W5500 SPI đến ESP32.
WS2812 đến GPIO 48 với tín hiệu PWM.
Nguồn chung 5V, với decoupling capacitors.
Ước tính Chi Phí: 50-100 USD (ESP32 ~10 USD, W5500 ~5 USD, linh kiện DMX ~20 USD/cổng).

3. Phần Mềm và Thư Viện
Môi Trường Phát Triển
Arduino IDE với ESP32 core (hoặc ESP-IDF).
Ngôn ngữ: C++.
Thư Viện Chính
esp-dmx: Xử lý DMX/RDM (GitHub: https://github.com/someone-somename/esp-dmx).
ESPAsyncWebServer & AsyncTCP: Web server không đồng bộ.
ESP32-sACN: Hỗ trợ sACN (GitHub: https://github.com/hpwit/Esp32-sACN).
Adafruit_NeoPixel: Điều khiển LED WS2812.
ArduinoJson: Xử lý JSON cho cấu hình.
FreeRTOS: Tích hợp sẵn (sử dụng xTaskCreatePinnedToCore).
Cấu Trúc Code
Tasks FreeRTOS:
Core 0 (networkProtocolTask): Xử lý Art-Net, sACN, web server, cấu hình mạng. Sử dụng queue để gửi dữ liệu DMX đến Core 1.
Core 1 (dmxOutputTask): Xử lý xuất DMX qua esp-dmx và cập nhật LED WS2812. Nhận dữ liệu từ queue, báo lỗi (xanh: OK, đỏ: lỗi DMX, vàng: cảnh báo mạng).
Modules Chính:
NetworkManager: Quản lý Ethernet/WiFi STA/AP, multicast cho sACN.
DMXHandler: Xử lý 2 cổng với esp-dmx, tích hợp Merge Engine (LTP/HTP).
sACNHandler: Nhận/gửi DMX qua sACN, merge với Art-Net.
MergeEngine: Logic hợp nhất dữ liệu từ nhiều nguồn (Art-Net/sACN).
WebServer: Phục vụ giao diện web (HTML/CSS/JS với Bootstrap).
LEDManager: Cập nhật WS2812 dựa trên trạng thái.
RDMManager: Xử lý RDM (discovery, set parameters).
Lưu Trữ: SPIFFS/LittleFS cho cấu hình JSON (mạng, cổng, sACN, merge).
4. Giao Diện Web
Cấu Trúc
Framework: HTML/CSS/JS (Bootstrap cho UI), WebSockets cho real-time.
Trang Chính: Dashboard trạng thái cổng, mạng, liên kết đến trang con.
Trang Cấu Hình:
Mạng: Dropdown chế độ (Ethernet/WiFi STA/AP), input IP/subnet/gateway.
Giao Thức: Dropdown chọn Art-Net hoặc sACN (hoặc cả hai).
sACN Settings: Universe (1-63999), multicast address, priority.
Merge Engine: Dropdown cho mỗi cổng (LTP/HTP/Disable).
Cổng DMX: Chế độ (out/in/RDM), universe.
Trang Test/Kiểm Tra:
Hiển thị tín hiệu DMX: Bảng kênh (0-255), đồ thị real-time, nguồn (Art-Net/sACN).
Điều khiển ảo: Slider cho kênh DMX (chỉ cho DMX-out), nút test merge.
RDM: Nút discovery, form set parameters.
Bảo Mật: Basic auth hoặc token.
5. Kết Nối Mạng
Chế Độ:
Ethernet (W5500): DHCP/static IP.
WiFi STA: Kết nối router (SSID/password qua web).
WiFi AP: Hotspot (SSID "ArtNet-Node").
Giao Thức:
Art-Net: UDP port 6454.
sACN: Multicast UDP port 5568 (239.255.x.x).
Merge: Hợp nhất dữ liệu từ Art-Net/sACN dựa trên cấu hình.
6. Cấu Hình Cổng Điều Khiển
Mỗi cổng (1/2) cấu hình độc lập qua web:
DMX-out: Gửi DMX đã merge.
DMX-in: Nhận và chuyển tiếp qua mạng.
RDM: Discovery/control thiết bị.
Merge Engine áp dụng khi enable (LTP/HTP).
7. Triển Khai và Test
Bước Triển Khai
Thiết kế/hàn PCB.
Cài đặt Arduino IDE và thư viện.
Viết code theo modules, test FreeRTOS tasks.
Tích hợp sACN và merge.
Cập nhật web server.
Test Từng Phần
Mạng: Ping IP, multicast sACN.
DMX: Output/input với DMX tester.
Merge: Gửi từ hai nguồn, kiểm tra LTP/HTP.
LED: Simulate lỗi, kiểm tra màu sắc.
Web: Cấu hình và test giao diện.
Tích Hợp: Chạy với QLC+, monitor CPU.
Thời Gian: 5-8 tuần.

8. Lưu Ý và Mở Rộng
Tuân Thủ: Art-Net, sACN (ANSI E1.31), DMX/RDM standards.
Tối Ưu: Core 0 cho I/O mạng, Core 1 cho timing DMX (44μs/kênh).
Mở Rộng: Thêm PoE, hoặc tích hợp với cloud nếu cần.
Debug: Serial log, ESP-IDF tools.
9. Tài Liệu Tham Khảo
ESP32 S3: https://docs.espressif.com/projects/esp-idf/en/latest/
esp-dmx: https://github.com/someone-somename/esp-dmx
sACN: https://tsp.esta.org/tsp/documents/docs/ANSI_E1-31-2018.pdf
FreeRTOS: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/freertos.html
Art-Net: https://art-net.org.uk/
Tài liệu này là bản hoàn chỉnh. 