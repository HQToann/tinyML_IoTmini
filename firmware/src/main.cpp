/**
 * HỆ THỐNG IOT TINYML - GIÁM SÁT VÀ CẢNH BÁO SỚM CHO NHÀ MÔ HÌNH
 * Vi điều khiển: ESP32-C3 Super Mini
 * Cảm biến: MPU-6050 (Góc nghiêng, Tốc độ biến thiên góc)
 * Trí tuệ nhân tạo: TinyML TensorFlow Lite Micro Int8
 * Kết nối: Wi-Fi Captive Portal WebServer & MQTT Telemetry Cloud
 */

#include <Arduino.h>
#include "config.h"
#include "sensor_reader.h"
#include "tinyml_runner.h"
#include "alarm_controller.h"
#include "network_mqtt.h"

// Khởi tạo các module điều khiển
static SensorReader     sensorReader;
static TinyMLRunner     tinyMLRunner;
static AlarmController  alarmController;
static NetworkMQTT      networkMQTT;

// Biến quản lý chu kỳ lấy mẫu
static unsigned long last_sample_time_ms = 0;
static int current_risk_level = 0;
static bool system_ready = false;

void setup() {
    Serial.begin(SERIAL_BAUD_RATE);
    delay(500);

    Serial.println("\r\n=======================================================");
    Serial.println("   HỆ THỐNG TINYML GIÁM SÁT AN TOÀN NHÀ MÔ HÌNH        ");
    Serial.println("        Phần cứng: ESP32-C3 Super Mini (RISC-V)        ");
    Serial.println("=======================================================");

    // 1. Cấu hình nút nhấn BOOT (giữ 5s để cấu hình lại Wi-Fi)
    pinMode(PIN_BOOT_BTN, INPUT_PULLUP);

    // 2. Khởi tạo hệ thống đèn LED và còi báo động
    alarmController.begin();

    // 3. Khởi tạo kết nối Wi-Fi hoặc mở WebServer cài đặt
    networkMQTT.begin();

    // 4. Khởi tạo cảm biến MPU-6050
    bool sensor_ok = sensorReader.begin();
    if (!sensor_ok) {
        Serial.println("[CẢNH BÁO] Kiểm tra lại kết nối cảm biến MPU-6050!");
        // Nháy LED Đỏ 3 lần báo lỗi phần cứng
        for (int i = 0; i < 3; i++) {
            digitalWrite(PIN_LED_RED, HIGH); delay(200);
            digitalWrite(PIN_LED_RED, LOW);  delay(200);
        }
    }

    // 5. Khởi tạo TensorFlow Lite Micro Int8
    bool ml_ok = tinyMLRunner.begin();
    if (!ml_ok) {
        Serial.println("[LỖI] Không thể khởi động TinyML Runtime!");
    }

    // Hệ thống sẵn sàng CHỈ KHI cả sensor lẫn TinyML đều OK
    system_ready = sensor_ok && ml_ok;

    Serial.println(">> Hệ thống đã hoàn tất khởi động! <<\r\n");
}

void loop() {
    // 1. Duy trì mạng Wi-Fi, WebServer cài đặt và MQTT Broker
    networkMQTT.loop();

    // 2. Kiểm tra nút nhấn BOOT (nếu giữ 5 giây sẽ vào chế độ cấu hình Wi-Fi)
    networkMQTT.checkSetupButton(PIN_BOOT_BTN, HOLD_BUTTON_TIME_MS);

    // Nếu đang ở chế độ cài đặt Wi-Fi, nhường CPU cho Web Server
    if (networkMQTT.isSetupMode()) {
        alarmController.update(1); // Bật LED vàng báo hiệu đang cài đặt
        delay(10);
        return;
    }

    // Cập nhật trạng thái đèn LED và còi theo mức độ rủi ro hiện tại
    alarmController.update(current_risk_level);

    if (!system_ready) {
        delay(500);
        return;
    }

    // 3. Chu kỳ lấy mẫu và suy luận AI định kỳ
    unsigned long now_ms = millis();
    if (now_ms - last_sample_time_ms >= SAMPLE_INTERVAL_MS) {
        last_sample_time_ms = now_ms;

        // BƯỚC 1: Đọc cảm biến góc nghiêng và tốc độ đổi góc
        SensorData sensor_data = sensorReader.read();

        // BƯỚC 2: Chạy suy luận mạng nơ-ron TinyML Int8
        PredictionResult pred = tinyMLRunner.predict(sensor_data);
        current_risk_level = pred.predicted_class;

        // BƯỚC 3: Gửi dữ liệu Telemetry lên Dashboard qua MQTT
        bool mqtt_ok = networkMQTT.publishTelemetry(sensor_data, pred);

        // BƯỚC 4: In thông số giám sát ra Serial Monitor dạng thẻ trực quan
        static uint32_t cycle_count = 0;
        cycle_count++;

        const char* color_code = "\033[1;32m"; // Xanh lá
        const char* badge_text = "[ BÌNH THƯỜNG ]";

        if (pred.predicted_class == 1) {
            color_code = "\033[1;33m"; // Vàng
            badge_text = "[ CHÚ Ý ]";
        } else if (pred.predicted_class == 2) {
            color_code = "\033[1;31m"; // Đỏ
            badge_text = "[ NGUY CƠ CAO ]";
        }

        Serial.printf("\r\n---------------------------------------------------------------\r\n");
        Serial.printf("[CHU KỲ #%04u] ĐÁNH GIÁ: %s%s\033[0m (Cấp rủi ro %d)\r\n",
                      (unsigned int)cycle_count, color_code, badge_text, pred.predicted_class);
        Serial.printf("  * Cảm biến : Góc nghiêng: %5.2f° | Tốc độ đổi góc: %6.3f°/phút\r\n",
                      sensor_data.tilt_angle_deg,
                      sensor_data.tilt_rate_deg_per_min);
        Serial.printf("  * Xác suất : Bình thường: %3.0f%% | Chú ý: %3.0f%% | Nguy cơ cao: %3.0f%%\r\n",
                      pred.probabilities[0] * 100.0f,
                      pred.probabilities[1] * 100.0f,
                      pred.probabilities[2] * 100.0f);
        Serial.printf("  * Kết nối  : Wi-Fi: %s | MQTT: %s\r\n",
                      networkMQTT.isWiFiConnected() ? "[OK]" : "[MẤT KẾT NỐI]",
                      mqtt_ok ? "Đã gửi [OK]" : "Chưa gửi [X]");
        Serial.printf("---------------------------------------------------------------\r\n");
    }
}
