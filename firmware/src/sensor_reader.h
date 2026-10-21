#pragma once
#include <Arduino.h>

// 3 đặc trưng đầu vào cho mô hình TinyML
struct SensorData {
    float soil_moisture_pct;      // Độ ẩm đất (%)
    float tilt_angle_deg;         // Góc nghiêng (độ)
    float tilt_rate_deg_per_min;  // Tốc độ đổi góc (độ/phút)
};

class SensorReader {
public:
    SensorReader();
    bool begin();
    SensorData read();

private:
    float _last_tilt_angle;
    unsigned long _last_read_ms;
    bool _is_first_read;
};
