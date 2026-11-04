#pragma once
#include <Arduino.h>
#include "sensor_reader.h"

// Kết quả dự đoán của mô hình TinyML
struct PredictionResult {
    int predicted_class;          // 0: Bình thường, 1: Chú ý, 2: Nguy cơ cao
    const char* class_name;       // Chuỗi mô tả trạng thái
    float probabilities[3];       // Xác suất của từng lớp (0.0 đến 1.0)
};

class TinyMLRunner {
public:
    TinyMLRunner();

    // Khởi tạo runtime TFLite Micro và nạp model Int8
    bool begin();

    // Chuẩn hóa dữ liệu đầu vào và chạy suy luận mạng nơ-ron
    PredictionResult predict(const SensorData& sensor_data);

private:
    bool _is_initialized;
};
