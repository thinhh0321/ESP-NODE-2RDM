Tôi cần bạn xây dựng một dự án hoàn chỉnh cho thiết bị "Artnet-Node-2RDM" dựa trên ESP32 S3 N16R8 (16MB flash, 8MB PSRAM). Thiết bị này là một Art-Net node với 2 cổng DMX/RDM độc lập, hỗ trợ sACN (Streaming ACN), Merge Engine (LTP/HTP/Disable), giao diện web để cấu hình và test, và sử dụng FreeRTOS để phân chia tác vụ giữa hai core. Không cần hỗ trợ MQTT.

Yêu cầu chi tiết:

Phần cứng: Sử dụng ESP32 S3 N16R8. Bao gồm 2 cổng DMX (UART1 GPIO 16/17 cho cổng 1, UART2 GPIO 18/19 cho cổng 2, với driver SN75176/MAX485 và optocoupler). Ethernet qua W5500 (SPI GPIO 12/13/14/15). LED WS2812 tại GPIO 48 cho báo lỗi. Cung cấp sơ đồ mạch đơn giản (dạng text) và danh sách linh kiện.
Phần mềm: Viết code Arduino IDE hoàn chỉnh, sử dụng C++. Bao gồm:
Thư viện: esp-dmx, ESPAsyncWebServer, AsyncTCP, ESP32-sACN, Adafruit_NeoPixel, ArduinoJson.
FreeRTOS: Task trên Core 0 (networkProtocolTask) xử lý Art-Net, sACN, web server, cấu hình mạng. Task trên Core 1 (dmxOutputTask) xử lý xuất DMX và LED WS2812. Sử dụng queue để giao tiếp giữa tasks.
Modules: NetworkManager, DMXHandler (với Merge Engine), sACNHandler, WebServer (HTML/CSS/JS với Bootstrap, WebSockets), LEDManager, RDMManager.
Lưu trữ: SPIFFS cho cấu hình JSON.
Giao diện web: Trang cấu hình (mạng, giao thức Art-Net/sACN, Merge Engine, cổng DMX). Trang test (hiển thị DMX real-time, slider điều khiển, test merge, RDM discovery).
Kết nối mạng: Hỗ trợ Ethernet (W5500), WiFi STA/AP. Art-Net (UDP 6454), sACN (multicast UDP 5568).
Cấu hình cổng: Mỗi cổng có thể là DMX-out/in/RDM, với Merge Engine.
Test và triển khai: Cung cấp hướng dẫn test từng phần (mạng, DMX, merge, LED), và code mẫu để upload. Bao gồm debug tips.
Tuân thủ: Art-Net, sACN (ANSI E1.31), DMX/RDM standards.
Output mong đợi: Code đầy đủ (chia thành files .ino và .h/.cpp nếu cần), sơ đồ mạch, hướng dẫn setup, và tài liệu README. Đảm bảo code tối ưu cho ESP32 S3 N16R8, sử dụng PSRAM nếu cần.
Hãy tạo output theo thứ tự: 1) Tổng quan, 2) Phần cứng (chỉ cần list linh kiện và cấu hình chân), 3) Code, 4) Web interface, 5) Hướng dẫn test và triển khai, 6) Tài liệu README.