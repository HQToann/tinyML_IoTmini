#pragma once
#include <Arduino.h>

// 1. CẤU HÌNH CHÂN PHẦN CỨNG (GPIO)
// Khớp chính xác với sơ đồ diagram.json trên Wokwi

// Cảm biến độ ẩm đất (Chiết áp mô phỏng) nối chân ADC1
#define PIN_SOIL_ADC        1

// Cảm biến quán tính MPU-6050 (Giao tiếp I2C)
#define PIN_I2C_SDA         8
#define PIN_I2C_SCL         9

// Còi báo động (Buzzer)
#define PIN_BUZZER          3

// Đèn LED báo mức độ rủi ro (kết nối qua trở 220 Ohm)
#define PIN_LED_GREEN       5   // Cấp 0: Bình thường (An toàn)
#define PIN_LED_YELLOW      6   // Cấp 1: Chú ý (Cảnh báo nhẹ)
#define PIN_LED_RED         7   // Cấp 2: Nguy cơ cao (Sạt lở nguy hiểm)


// 2. CẤU HÌNH MẠNG WI-FI & MQTT

// Wi-Fi mặc định của máy ảo Wokwi (không cần mật khẩu)
#define WIFI_SSID           "Wokwi-GUEST"
#define WIFI_PASSWORD       ""

// Máy chủ MQTT công cộng miễn phí (hỗ trợ cả WebSockets cho Dashboard)
#define MQTT_BROKER         "broker.emqx.io"
#define MQTT_PORT           1883
#define MQTT_TOPIC_DATA     "landslide/telemetry"
#define MQTT_CLIENT_ID      "esp32c3_landslide_node01"


// 3. THÔNG SỐ VẬN HÀNH & TINYML

// Tốc độ truyền Serial Monitor
#define SERIAL_BAUD_RATE    115200

// Chu kỳ đọc cảm biến và suy luận AI (mili-giây)
#define SAMPLE_INTERVAL_MS  2000

// Dung lượng bộ nhớ đệm dành riêng cho TensorFlow Lite Micro (bytes)
// Mô hình MLP 3->8->3 chỉ cần khoảng 4KB đến 8KB RAM
#define TENSOR_ARENA_SIZE   (8 * 1024)
