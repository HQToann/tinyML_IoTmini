#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <Preferences.h>
#include <PubSubClient.h>
#include "sensor_reader.h"
#include "tinyml_runner.h"

class NetworkMQTT {
public:
    NetworkMQTT();
    void begin();
    void loop();

    // Chuyển sang chế độ phát AP và mở trang cài đặt Wi-Fi
    void startSetupMode();

    // Kiểm tra thiết bị có đang ở chế độ cài đặt Wi-Fi không
    bool isSetupMode() const;

    // Kiểm tra nút bấm vật lý (ví dụ nút BOOT) để kích hoạt chế độ cài đặt
    void checkSetupButton(int pin, unsigned long hold_time_ms);

    // Gửi dữ liệu Telemetry lên MQTT Broker
    bool publishTelemetry(const SensorData& sensor_data, const PredictionResult& pred);

    // Trạng thái kết nối
    bool isWiFiConnected();
    bool isMQTTConnected();

private:
    WebServer _server;
    DNSServer _dnsServer;
    Preferences _preferences;
    WiFiClient _wifiClient;
    PubSubClient _mqttClient;

    bool _setup_mode;
    unsigned long _button_press_start_ms;
    unsigned long _last_reconnect_attempt_ms;

    void _connectWiFiOrStartSetup();
    void _setupWebServerRoutes();
    void _reconnectMQTT();
};
