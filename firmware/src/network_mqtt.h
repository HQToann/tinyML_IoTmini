#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "sensor_reader.h"
#include "tinyml_runner.h"

class NetworkMQTT {
public:
    NetworkMQTT();

    // Khởi tạo Wi-Fi và MQTT Client
    bool begin();

    // Duy trì kết nối non-blocking trong vòng lặp loop
    void loop();

    // Kiểm tra trạng thái kết nối MQTT
    bool isConnected();

    // Đóng gói JSON và gửi telemetry lên topic
    bool publishTelemetry(const SensorData& data, const PredictionResult& pred);

private:
    WiFiClient _espClient;
    PubSubClient _mqttClient;
    unsigned long _last_reconnect_attempt;

    bool reconnect();
};
