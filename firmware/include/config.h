#pragma once
#include <Arduino.h>

// ==============================================================================
// 1. CẤU HÌNH CHÂN PHẦN CỨNG (GPIO) - ESP32-C3 SUPER MINI (NHÀ MÔ HÌNH)
// ==============================================================================

// Giao tiếp I2C với cảm biến MPU-6050
#define PIN_I2C_SDA         4   // SDA của MPU-6050
#define PIN_I2C_SCL         5   // SCL của MPU-6050

// Nút bấm kích hoạt chế độ cấu hình Wi-Fi (nút BOOT tích hợp sẵn trên ESP32-C3)
#define PIN_BOOT_BTN        9
#define HOLD_BUTTON_TIME_MS 5000UL

// Còi báo động (Buzzer)
#define PIN_BUZZER          3

// Đèn LED báo mức độ rủi ro (kết nối qua điện trở 220Ω)
#define PIN_LED_GREEN       6   // Cấp 0: Bình thường (An toàn)
#define PIN_LED_YELLOW      7   // Cấp 1: Chú ý (Bất thường nhẹ)
#define PIN_LED_RED         8   // Cấp 2: Nguy cơ cao trong mô hình

// ==============================================================================
// 2. CẤU HÌNH ĐIỂM TRUY CẬP WI-FI (CAPTIVE PORTAL) & MQTT
// ==============================================================================

// Thông tin Access Point phát ra khi thiết bị ở chế độ cài đặt Wi-Fi
#define WIFI_AP_SSID        "NhaMoHinh-Setup"
#define WIFI_AP_PASSWORD    "NhaMoHinh2026"

// Máy chủ MQTT công cộng miễn phí (đồng bộ dữ liệu với Dashboard giám sát)
#define MQTT_BROKER         "broker.emqx.io"
#define MQTT_PORT           1883
#define MQTT_TOPIC_DATA     "landslide/telemetry"
#define MQTT_CLIENT_ID      "esp32c3_nhamohinh_node01"

// ==============================================================================
// 3. THÔNG SỐ VẬN HÀNH & TINYML
// ==============================================================================

// Tốc độ truyền Serial Monitor
#define SERIAL_BAUD_RATE    115200

// Chu kỳ đọc cảm biến và suy luận AI (mili-giây)
#define SAMPLE_INTERVAL_MS  1000UL

// Cửa sổ thời gian tính tốc độ thay đổi góc nghiêng (mili-giây)
#define SPEED_WINDOW_MS     60000UL

// Dung lượng bộ nhớ đệm dành riêng cho TensorFlow Lite Micro (bytes)
#define TENSOR_ARENA_SIZE   (16 * 1024)
