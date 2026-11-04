#include "tinyml_runner.h"
#include "config.h"
#include "norm_params.h"
#include "model_data.h"

#if __has_include(<TensorFlowLite_ESP32.h>)
  #include <TensorFlowLite_ESP32.h>
#elif __has_include(<Chirale_TensorFlowLite.h>)
  #include <Chirale_TensorFlowLite.h>
#endif

#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/schema/schema_generated.h"

#if __has_include("tensorflow/lite/micro/micro_error_reporter.h")
  #include "tensorflow/lite/micro/micro_error_reporter.h"
  #define HAS_MICRO_ERROR_REPORTER 1
#endif
#include <cmath>

// Vùng nhớ tĩnh (Tensor Arena) dành cho TFLite Micro được căn chỉnh 16-byte
alignas(16) static uint8_t tensor_arena[TENSOR_ARENA_SIZE];

static const tflite::Model* model = nullptr;
static tflite::MicroInterpreter* interpreter = nullptr;
static TfLiteTensor* input_tensor = nullptr;
static TfLiteTensor* output_tensor = nullptr;
static int model_feature_count = FEATURE_COUNT;

TinyMLRunner::TinyMLRunner() : _is_initialized(false) {}

bool TinyMLRunner::begin() {
    // 1. Kiểm tra cấu trúc mô hình từ model_data.h
    model = tflite::GetModel(g_model);
    if (model == nullptr) {
        Serial.println("[LỖI] Không thể đọc mô hình TFLite!");
        return false;
    }

    if (model->version() != TFLITE_SCHEMA_VERSION) {
        Serial.printf("[LỖI] Schema TFLite không khớp! Cần %d, hiện tại %u\r\n",
                      TFLITE_SCHEMA_VERSION, (unsigned int)model->version());
        return false;
    }

    // 2. Khởi tạo resolver gồm 3 toán tử: FullyConnected, ReLU, Softmax
    static tflite::MicroMutableOpResolver<3> resolver;
    if (resolver.AddFullyConnected() != kTfLiteOk ||
        resolver.AddRelu() != kTfLiteOk ||
        resolver.AddSoftmax() != kTfLiteOk) {
        Serial.println("[LỖI] Đăng ký toán tử resolver thất bại!");
        return false;
    }

    // 3. Khởi tạo bộ thông dịch MicroInterpreter
#ifdef HAS_MICRO_ERROR_REPORTER
    static tflite::MicroErrorReporter micro_error_reporter;
    static tflite::MicroInterpreter static_interpreter(
        model, resolver, tensor_arena, sizeof(tensor_arena), &micro_error_reporter
    );
#else
    static tflite::MicroInterpreter static_interpreter(
        model, resolver, tensor_arena, sizeof(tensor_arena)
    );
#endif
    interpreter = &static_interpreter;

    // 4. Cấp phát vùng nhớ cho các tensor
    if (interpreter->AllocateTensors() != kTfLiteOk) {
        Serial.println("[LỖI] Không thể cấp phát bộ nhớ tensor arena!");
        return false;
    }

    // 5. Trích xuất con trỏ tensor đầu vào và đầu ra
    input_tensor = interpreter->input(0);
    output_tensor = interpreter->output(0);

    if (input_tensor == nullptr || output_tensor == nullptr ||
        input_tensor->type != kTfLiteInt8 || output_tensor->type != kTfLiteInt8) {
        Serial.println("[LỖI] Tensor đầu vào/đầu ra không khớp kiểu INT8!");
        return false;
    }

    // Tự động nhận diện số đặc trưng đầu vào (2 đặc trưng mới hoặc 3 đặc trưng cũ)
    model_feature_count = input_tensor->dims->data[input_tensor->dims->size - 1];
    Serial.printf("[OK] TinyML Int8 đã sẵn sàng! Số đặc trưng đầu vào: %d\r\n", model_feature_count);

    _is_initialized = true;
    return true;
}

PredictionResult TinyMLRunner::predict(const SensorData& sensor_data) {
    PredictionResult result = {0, CLASS_NAMES[0], {1.0f, 0.0f, 0.0f}};

    if (!_is_initialized || input_tensor == nullptr) {
        return result;
    }

    // Bước 1: Chuẩn hóa Z-score & Lượng tử hóa sang Int8
    if (model_feature_count == 2) {
        // Mô hình 2 đặc trưng: [goc_nghieng, toc_do_doi_goc]
        float raw[2] = { sensor_data.tilt_angle_deg, sensor_data.tilt_rate_deg_per_min };
        for (int i = 0; i < 2; i++) {
            float std_val = (NORM_STD[i] == 0.0f) ? 1.0f : NORM_STD[i];
            float z = (raw[i] - NORM_MEAN[i]) / std_val;
            int32_t q = (int32_t)roundf(z / INPUT_SCALE + INPUT_ZERO_POINT);
            input_tensor->data.int8[i] = (int8_t)constrain(q, -128, 127);
        }
    } else {
        // Tương thích ngược với mô hình 3 đặc trưng cũ: [do_am_mac_dinh, goc_nghieng, toc_do_doi_goc]
        float raw[3] = { 47.411724f, sensor_data.tilt_angle_deg, sensor_data.tilt_rate_deg_per_min };
        float mean_3[3] = { 47.411724f, 5.565814f, 0.218661f };
        float std_3[3]  = { 16.465240f, 5.037641f, 0.224276f };
        for (int i = 0; i < 3; i++) {
            float z = (raw[i] - mean_3[i]) / std_3[i];
            int32_t q = (int32_t)roundf(z / INPUT_SCALE + INPUT_ZERO_POINT);
            input_tensor->data.int8[i] = (int8_t)constrain(q, -128, 127);
        }
    }

    // Bước 2: Thực thi suy luận mạng nơ-ron
    if (interpreter->Invoke() != kTfLiteOk) {
        Serial.println("[LỖI] Suy luận TinyML thất bại!");
        return result;
    }

    // Bước 3: Đọc kết quả và tìm lớp có xác suất cao nhất
    int best_class = 0;
    int8_t max_score = output_tensor->data.int8[0];

    // Dequantize xác suất mềm cho 3 lớp (Softmax Int8)
    float out_scale = output_tensor->params.scale;
    int32_t out_zero_point = output_tensor->params.zero_point;
    float sum_prob = 0.0f;

    for (int i = 0; i < 3; i++) {
        if (output_tensor->data.int8[i] > max_score) {
            max_score = output_tensor->data.int8[i];
            best_class = i;
        }
        float prob = (output_tensor->data.int8[i] - out_zero_point) * out_scale;
        result.probabilities[i] = (prob < 0.0f) ? 0.0f : prob;
        sum_prob += result.probabilities[i];
    }

    // Chuẩn hóa xác suất về tổng 1.0
    if (sum_prob > 0.0f) {
        for (int i = 0; i < 3; i++) {
            result.probabilities[i] /= sum_prob;
        }
    }

    result.predicted_class = best_class;
    result.class_name = CLASS_NAMES[best_class];

    return result;
}
