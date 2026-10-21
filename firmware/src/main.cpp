/**
 * ĐỒ ÁN IOT TINYML - GIÁM SÁT VÀ CẢNH BÁO SỚM SẠT LỞ ĐẤT
 * Vi điều khiển: ESP32-C3 DevKitM-1 (Mô phỏng Wokwi)
 * Tác vụ: Đọc cảm biến -> Chuẩn hóa Z-score -> TinyML Int8 -> LED/Còi -> MQTT
 */

#include <Arduino.h>
#include "config.h"
#include "sensor_reader.h"
#include "tinyml_runner.h"
#include "alarm_controller.h"
#include "network_mqtt.h"

// Khởi tạo các đối tượng điều khiển
static SensorReader     sensorReader;
static TinyMLRunner     tinyMLRunner;
static AlarmController  alarmController;
static NetworkMQTT      networkMQTT;

// Biến quản lý thời gian chu kỳ
static unsigned long last_sample_time_ms = 0;
static int current_risk_level = 0;

void setup() {
    // 1. Khởi động Serial Monitor
    Serial.begin(SERIAL_BAUD_RATE);
    delay(1000);

    Serial.println("\n=======================================================");
    Serial.println("  HỆ THỐNG IOT TINYML GIÁM SÁT & CẢNH BÁO SẠT LỞ ĐẤT   ");
    Serial.println("         Vi điều khiển: ESP32-C3 (RISC-V)              ");
    Serial.println("=======================================================");

    // 2. Khởi tạo hệ thống đèn LED và còi báo động
    alarmController.begin();

    // 3. Khởi tạo cảm biến độ ẩm đất và MPU-6050
    if (!sensorReader.begin()) {
        Serial.println("[CẢNH BÁO] Kiểm tra lại kết nối cảm biến!");
    }

    // 4. Khởi tạo runtime TensorFlow Lite Micro và nạp model Int8
    if (!tinyMLRunner.begin()) {
        Serial.println("[LỖI NGHIÊM TRỌNG] Không thể khởi động TinyML Runtime!");
    }

    // 5. Kết nối Wi-Fi và MQTT Broker
    networkMQTT.begin();

    Serial.println(">> Hệ thống đã sẵn sàng bước vào vòng lặp giám sát! <<\n");
}

void loop() {
    // Luôn duy trì kết nối mạng MQTT và kiểm tra tin nhắn trong nền
    networkMQTT.loop();

    // Cập nhật trạng thái đèn LED và còi (non-blocking)
    alarmController.update(current_risk_level);

    // Chu kỳ lấy mẫu và suy luận AI định kỳ
    unsigned long now_ms = millis();
    if (now_ms - last_sample_time_ms >= SAMPLE_INTERVAL_MS) {
        last_sample_time_ms = now_ms;

        // BƯỚC 1: Đọc cảm biến
        SensorData sensor_data = sensorReader.read();

        // BƯỚC 2: Chạy suy luận mạng nơ-ron TinyML
        PredictionResult pred = tinyMLRunner.predict(sensor_data);
        current_risk_level = pred.predicted_class;

        // BƯỚC 3: Gửi dữ liệu Telemetry lên Cloud/Dashboard qua MQTT
        bool mqtt_ok = networkMQTT.publishTelemetry(sensor_data, pred);

        // BƯỚC 4: In thông số giám sát ra Serial Monitor dạng thẻ trực quan
        static uint32_t cycle_count = 0;
        cycle_count++;

        // Màu ANSI trực quan: Xanh lá (An toàn), Vàng (Chú ý), Đỏ (Nguy hiểm)
        const char* color_code = "\033[1;32m"; // Xanh lá
        const char* badge_text = "[ AN TOÀN ]";

        if (pred.predicted_class == 1) {
            color_code = "\033[1;33m"; // Vàng
            badge_text = "[ CHÚ Ý ]";
        } else if (pred.predicted_class == 2) {
            color_code = "\033[1;31m"; // Đỏ
            badge_text = "[ NGUY HIỂM ]";
        }

        Serial.printf("\r\n---------------------------------------------------------------\r\n");
        Serial.printf("[CHU KỲ #%03u] ĐÁNH GIÁ: %s%s\033[0m (Cấp rủi ro %d)\r\n",
                      cycle_count, color_code, badge_text, pred.predicted_class);
        Serial.printf("  * Cảm biến : Độ ẩm: %5.1f%% | Góc nghiêng: %5.2f° | V_góc: %7.3f°/phút\r\n",
                      sensor_data.soil_moisture_pct,
                      sensor_data.tilt_angle_deg,
                      sensor_data.tilt_rate_deg_per_min);
        Serial.printf("  * Xác suất : An toàn: %3.0f%% | Chú ý: %3.0f%% | Nguy cơ cao: %3.0f%%\r\n",
                      pred.probabilities[0] * 100.0f,
                      pred.probabilities[1] * 100.0f,
                      pred.probabilities[2] * 100.0f);
        Serial.printf("  * Kết nối  : MQTT Telemetry -> %s\r\n",
                      mqtt_ok ? "Đã gửi [OK]" : "Mất kết nối [X]");
        Serial.printf("---------------------------------------------------------------\r\n");
    }
}
