#include "tinyml_runner.h"
#include "config.h"
#include "norm_params.h"
#include "model_data.h"

#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/schema/schema_generated.h"
#include <cmath>

// Vùng nhớ tĩnh (Tensor Arena) dành cho TFLite Micro
static uint8_t tensor_arena[TENSOR_ARENA_SIZE] __attribute__((aligned(16)));

static const tflite::Model* model = nullptr;
static tflite::MicroInterpreter* interpreter = nullptr;
static TfLiteTensor* input_tensor = nullptr;
static TfLiteTensor* output_tensor = nullptr;

static const char* CLASS_LABELS[3] = {
    "Bình thường",
    "Chú ý",
    "Nguy cơ cao"
};

TinyMLRunner::TinyMLRunner() : _is_initialized(false) {}

bool TinyMLRunner::begin() {
    // 1. Kiểm tra cấu trúc mô hình từ model_data.h
    model = tflite::GetModel(g_model);
    if (model->version() != TFLITE_SCHEMA_VERSION) {
        Serial.printf("[LỖI] Phiên bản schema TFLite không khớp! Cần %d, hiện tại %u\r\n",
                      TFLITE_SCHEMA_VERSION, (unsigned int)model->version());
        return false;
    }

    // 2. Khởi tạo resolver: Chỉ đăng ký đúng 2 toán tử mà mô hình MLP sử dụng (FullyConnected và Softmax)
    // Giúp giảm dung lượng Flash từ 1.76MB xuống chỉ còn ~350KB!
    static tflite::MicroMutableOpResolver<2> resolver;
    resolver.AddFullyConnected();
    resolver.AddSoftmax();

    // 3. Khởi tạo bộ thông dịch MicroInterpreter
    static tflite::MicroInterpreter static_interpreter(
        model, resolver, tensor_arena, TENSOR_ARENA_SIZE
    );
    interpreter = &static_interpreter;

    // 4. Cấp phát vùng nhớ cho các tensor
    TfLiteStatus allocate_status = interpreter->AllocateTensors();
    if (allocate_status != kTfLiteOk) {
        Serial.println("[LỖI] Không thể cấp phát bộ nhớ tensor arena! Hãy tăng TENSOR_ARENA_SIZE trong config.h");
        return false;
    }

    // 5. Trích xuất con trỏ tensor đầu vào và đầu ra
    input_tensor = interpreter->input(0);
    output_tensor = interpreter->output(0);

    Serial.println("[OK] Mô hình TinyML MLP Int8 đã khởi tạo thành công trên ESP32-C3!");
    _is_initialized = true;
    return true;
}

PredictionResult TinyMLRunner::predict(const SensorData& sensor_data) {
    PredictionResult result = {0, "Không xác định", {0.0f, 0.0f, 0.0f}};

    if (!_is_initialized || input_tensor == nullptr) {
        return result;
    }

    // Bước 1: Chuẩn hóa Z-score & Lượng tử hóa sang Int8
    // Thứ tự 3 đặc trưng: [độ ẩm đất, góc nghiêng, tốc độ đổi góc]
    float raw_features[3] = {
        sensor_data.soil_moisture_pct,
        sensor_data.tilt_angle_deg,
        sensor_data.tilt_rate_deg_per_min
    };

    for (int i = 0; i < 3; i++) {
        // Z-score: z = (x - mean) / std
        float z = (raw_features[i] - NORM_MEAN[i]) / NORM_STD[i];

        // Int8 Quantize: q = round(z / scale) + zero_point
        int32_t q = round(z / INPUT_SCALE) + INPUT_ZERO_POINT;
        input_tensor->data.int8[i] = (int8_t)constrain(q, -128, 127);
    }

    // Bước 2: Thực thi suy luận mạng nơ-ron
    if (interpreter->Invoke() != kTfLiteOk) {
        Serial.println("[LỖI] Suy luận TinyML thất bại!");
        return result;
    }

    // Bước 3: Đọc kết quả và giải lượng tử hóa (Dequantize)
    float out_scale = output_tensor->params.scale;
    int32_t out_zero_point = output_tensor->params.zero_point;

    int best_class = 0;
    float max_prob = -1.0f;

    for (int i = 0; i < 3; i++) {
        int8_t val = output_tensor->data.int8[i];
        float prob = (val - out_zero_point) * out_scale;
        result.probabilities[i] = prob;

        if (prob > max_prob) {
            max_prob = prob;
            best_class = i;
        }
    }

    result.predicted_class = best_class;
    result.class_name = CLASS_LABELS[best_class];

    return result;
}
