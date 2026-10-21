#include "network_mqtt.h"
#include "config.h"

NetworkMQTT::NetworkMQTT() 
    : _mqttClient(_espClient), _last_reconnect_attempt(0) {}

bool NetworkMQTT::begin() {
    Serial.printf("[WIFI] Đang kết nối tới AP: %s...\r\n", WIFI_SSID);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    int timeout = 0;
    while (WiFi.status() != WL_CONNECTED && timeout < 20) {
        delay(300);
        Serial.print(".");
        timeout++;
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("\r\n[WIFI] Đã kết nối thành công! IP: %s\r\n", WiFi.localIP().toString().c_str());
    } else {
        Serial.println("\r\n[CẢNH BÁO] Kết nối Wi-Fi chưa sẵn sàng, sẽ tự động kết nối lại trong nền.");
    }

    _mqttClient.setServer(MQTT_BROKER, MQTT_PORT);
    _mqttClient.setBufferSize(512); // Đảm bảo bộ đệm đủ lớn cho chuỗi JSON

    return true;
}

bool NetworkMQTT::reconnect() {
    Serial.print("[MQTT] Đang kết nối tới Broker...");
    if (_mqttClient.connect(MQTT_CLIENT_ID)) {
        Serial.println(" THÀNH CÔNG!");
        return true;
    }
    Serial.printf(" THẤT BẠI (rc=%d), sẽ thử lại sau.\r\n", _mqttClient.state());
    return false;
}

void NetworkMQTT::loop() {
    // Nếu Wi-Fi mất kết nối thì bỏ qua
    if (WiFi.status() != WL_CONNECTED) {
        return;
    }

    // Nếu MQTT mất kết nối, tự động kết nối lại sau mỗi 5 giây (non-blocking)
    if (!_mqttClient.connected()) {
        unsigned long now_ms = millis();
        if (now_ms - _last_reconnect_attempt >= 5000) {
            _last_reconnect_attempt = now_ms;
            if (reconnect()) {
                _last_reconnect_attempt = 0;
            }
        }
    } else {
        _mqttClient.loop();
    }
}

bool NetworkMQTT::isConnected() {
    return _mqttClient.connected();
}

bool NetworkMQTT::publishTelemetry(const SensorData& data, const PredictionResult& pred) {
    if (!_mqttClient.connected()) {
        return false;
    }

    // Đóng gói dữ liệu telemetry bằng ArduinoJson
    JsonDocument doc;
    doc["node"] = MQTT_CLIENT_ID;
    doc["soil_moisture"] = round(data.soil_moisture_pct * 10.0f) / 10.0f;
    doc["tilt_angle"] = round(data.tilt_angle_deg * 100.0f) / 100.0f;
    doc["tilt_rate"] = round(data.tilt_rate_deg_per_min * 1000.0f) / 1000.0f;
    doc["risk_level"] = pred.predicted_class;
    doc["status"] = pred.class_name;
    doc["prob_normal"] = round(pred.probabilities[0] * 100.0f) / 100.0f;
    doc["prob_attention"] = round(pred.probabilities[1] * 100.0f) / 100.0f;
    doc["prob_high_risk"] = round(pred.probabilities[2] * 100.0f) / 100.0f;

    char buffer[384];
    serializeJson(doc, buffer, sizeof(buffer));

    bool success = _mqttClient.publish(MQTT_TOPIC_DATA, buffer, false);
    if (!success) {
        Serial.println("[MQTT LỖI] Không thể gửi gói tin!");
    }
    return success;
}
