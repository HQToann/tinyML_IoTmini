#pragma once
#include <Arduino.h>

// Dữ liệu đọc từ cảm biến cho nhà mô hình
struct SensorData {
    float tilt_angle_deg;         // Góc nghiêng (độ)
    float tilt_rate_deg_per_min;  // Tốc độ đổi góc (độ/phút)
    float soil_moisture_pct;      // Giữ giá trị 0.0f để tương thích telemetry cũ
};

class SensorReader {
public:
    SensorReader();
    bool begin();
    SensorData read();

    bool isReady() const;

private:
    float _base_tilt_angle;
    float _last_tilt_angle;
    float _current_tilt_rate;
    float _calib_gx;
    float _calib_gy;
    float _calib_gz;
    unsigned long _base_time_ms;
    unsigned long _last_read_ms;
    bool _has_base_mark;
    bool _mpu_ready;   // false nếu MPU-6050 không tìm thấy khi khởi tạo
};
