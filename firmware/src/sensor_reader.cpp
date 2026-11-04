#include "sensor_reader.h"
#include "config.h"
#include <Wire.h>
#include <cmath>

// Địa chỉ I2C và thanh ghi MPU-6050
#define MPU_ADDR        0x68
#define REG_PWR_MGMT_1  0x6B
#define REG_ACCEL_CFG   0x1C
#define REG_CONFIG      0x1A
#define REG_WHO_AM_I    0x75
#define REG_ACCEL_XOUT  0x3B

// ==============================================================================
// Giao tiếp I2C trực tiếp (tương thích chip GY-521 clone)
// ==============================================================================

static void mpu_write(uint8_t reg, uint8_t val) {
    Wire.beginTransmission(MPU_ADDR);
    Wire.write(reg);
    Wire.write(val);
    Wire.endTransmission();
}

// Đọc chính xác 6 byte gia tốc kế tuần tự để tránh lỗi đảo thứ tự byte (Undefined evaluation order)
static bool mpu_read_accel(float& ax, float& ay, float& az) {
    Wire.beginTransmission(MPU_ADDR);
    Wire.write(REG_ACCEL_XOUT);
    if (Wire.endTransmission(false) != 0) return false;
    
    if (Wire.requestFrom((uint8_t)MPU_ADDR, (uint8_t)6) < 6) return false;
    
    uint8_t buf[6];
    for (int i = 0; i < 6; i++) {
        buf[i] = Wire.read();
    }

    int16_t rx = (int16_t)((buf[0] << 8) | buf[1]);
    int16_t ry = (int16_t)((buf[2] << 8) | buf[3]);
    int16_t rz = (int16_t)((buf[4] << 8) | buf[5]);

    // Thang đo ±2g (16384 LSB/g) -> chuyển đổi sang m/s²
    const float scale = 9.80665f / 16384.0f;
    ax = rx * scale;
    ay = ry * scale;
    az = rz * scale;
    return true;
}

// Đọc trung bình nhiều mẫu để loại bỏ triệt để nhiễu rung và xung nhọn
static bool mpu_read_accel_filtered(float& ax, float& ay, float& az, int num_samples = 5) {
    float sum_x = 0.0f, sum_y = 0.0f, sum_z = 0.0f;
    int valid = 0;

    for (int i = 0; i < num_samples; i++) {
        float x, y, z;
        if (mpu_read_accel(x, y, z)) {
            sum_x += x;
            sum_y += y;
            sum_z += z;
            valid++;
        }
        delay(2);
    }

    if (valid == 0) return false;

    ax = sum_x / valid;
    ay = sum_y / valid;
    az = sum_z / valid;
    return true;
}

// ==============================================================================

SensorReader::SensorReader()
    : _base_tilt_angle(0.0f), _last_tilt_angle(0.0f), _current_tilt_rate(0.0f),
      _calib_gx(0.0f), _calib_gy(0.0f), _calib_gz(1.0f),
      _base_time_ms(0), _last_read_ms(0),
      _has_base_mark(false), _mpu_ready(false) {}

bool SensorReader::begin() {
    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
    Wire.setClock(100000);  // 100kHz chuẩn I2C
    delay(100);

    // Chẩn đoán WHO_AM_I
    Wire.beginTransmission(MPU_ADDR);
    Wire.write(REG_WHO_AM_I);
    Wire.endTransmission(false);
    Wire.requestFrom((uint8_t)MPU_ADDR, (uint8_t)1);
    uint8_t who_val = Wire.available() ? Wire.read() : 0xFF;
    Serial.printf("[I2C] WHO_AM_I = 0x%02X%s\r\n", who_val,
                  who_val == 0x68 ? " (chip chuẩn)" : " (chip clone - bỏ qua kiểm tra)");

    // Cấu hình thanh ghi MPU-6050
    mpu_write(REG_PWR_MGMT_1, 0x80);  // Reset toàn bộ thanh ghi
    delay(100);
    mpu_write(REG_PWR_MGMT_1, 0x00);  // Đánh thức cảm biến (wake up)
    delay(50);
    mpu_write(REG_ACCEL_CFG, 0x00);   // Thang đo ±2g
    // DLPF ~10Hz (chống rung cơ học và khử nhiễu tần số cao của còi/môi trường)
    mpu_write(REG_CONFIG, 0x05);       
    delay(50);

    // Đọc thử cảm biến
    float ax, ay, az;
    if (!mpu_read_accel_filtered(ax, ay, az, 3)) {
        Serial.println("[LỖI] Không đọc được dữ liệu gia tốc kế từ MPU-6050!");
        return false;
    }

    // Tự động hiệu chuẩn mốc Vector 3D ban đầu (30 mẫu trung bình)
    float sum_x = 0.0f, sum_y = 0.0f, sum_z = 0.0f;
    int samples = 0;
    for (int i = 0; i < 30; i++) {
        if (mpu_read_accel(ax, ay, az)) {
            sum_x += ax;
            sum_y += ay;
            sum_z += az;
            samples++;
        }
        delay(15);
    }
    
    if (samples > 0) {
        float avg_x = sum_x / samples;
        float avg_y = sum_y / samples;
        float avg_z = sum_z / samples;
        float norm = sqrtf(avg_x * avg_x + avg_y * avg_y + avg_z * avg_z);
        if (norm > 0.001f) {
            _calib_gx = avg_x / norm;
            _calib_gy = avg_y / norm;
            _calib_gz = avg_z / norm;
        }
    }

    // Tính góc lệch tĩnh của mặt bàn ban đầu để hiển thị thông tin
    float initial_tilt = acosf(constrain(_calib_gz, -1.0f, 1.0f)) * 180.0f / (float)M_PI;
    Serial.printf("[OK] MPU-6050 đã sẵn sàng! (WHO=0x%02X) | Mốc Vector 3D: [%.3f, %.3f, %.3f] (Lệch tĩnh: %.2f°)\r\n",
                  who_val, _calib_gx, _calib_gy, _calib_gz, initial_tilt);
    _mpu_ready = true;
    return true;
}

SensorData SensorReader::read() {
    SensorData data;
    data.tilt_angle_deg = 0.0f;
    data.tilt_rate_deg_per_min = 0.0f;
    data.soil_moisture_pct = 0.0f;

    if (!_mpu_ready) return data;

    unsigned long now_ms = millis();

    // 1. Đọc trung bình 5 mẫu để loại trừ xung nhiễu ngẫu nhiên
    float ax, ay, az;
    if (!mpu_read_accel_filtered(ax, ay, az, 5)) {
        data.tilt_angle_deg = _last_tilt_angle;
        data.tilt_rate_deg_per_min = _current_tilt_rate;
        return data;
    }

    // 2. Tính góc nghiêng không gian 3D qua Tích vô hướng (Vector Dot Product)
    float cur_norm = sqrtf(ax * ax + ay * ay + az * az);
    if (cur_norm < 0.001f) {
        data.tilt_angle_deg = _last_tilt_angle;
    } else {
        float nx = ax / cur_norm;
        float ny = ay / cur_norm;
        float nz = az / cur_norm;

        // cos(theta) = g0 . gt
        float dot = _calib_gx * nx + _calib_gy * ny + _calib_gz * nz;
        dot = constrain(dot, -1.0f, 1.0f);
        float calc_angle = acosf(dot) * 180.0f / (float)M_PI;

        if (!isfinite(calc_angle)) {
            calc_angle = _last_tilt_angle;
        }
        
        // Vùng trễ (Deadband) chống rung: dưới 0.25° thì coi như nằm yên tuyệt đối = 0.00°
        if (calc_angle < 0.25f) {
            calc_angle = 0.0f;
        }

        // Bộ lọc làm mịn EMA (Exponential Moving Average): 60% giá trị cũ + 40% giá trị mới
        if (!_has_base_mark) {
            data.tilt_angle_deg = calc_angle;
        } else {
            data.tilt_angle_deg = 0.6f * _last_tilt_angle + 0.4f * calc_angle;
            if (data.tilt_angle_deg < 0.15f) data.tilt_angle_deg = 0.0f;
        }
    }
        } else {
            data.tilt_angle_deg = 0.6f * _last_tilt_angle + 0.4f * calc_angle;
            if (data.tilt_angle_deg < 0.15f) data.tilt_angle_deg = 0.0f;
        }
    }

    // 3. Tính tốc độ biến thiên góc nghiêng (độ/phút) với cơ chế giải phóng (decay) khi đứng yên
    if (!_has_base_mark) {
        _base_tilt_angle = data.tilt_angle_deg;
        _base_time_ms = now_ms;
        _has_base_mark = true;
        _current_tilt_rate = 0.0f;
    } else {
        unsigned long elapsed_ms = now_ms - _base_time_ms;
        float dt_min = (now_ms - _last_read_ms) / 60000.0f;
        float instant_delta = fabsf(data.tilt_angle_deg - _last_tilt_angle);

        if (elapsed_ms >= SPEED_WINDOW_MS) {
            float minutes = elapsed_ms / 60000.0f;
            if (minutes > 0.0f) {
                _current_tilt_rate = fabsf(data.tilt_angle_deg - _base_tilt_angle) / minutes;
            }
            _base_tilt_angle = data.tilt_angle_deg;
            _base_time_ms = now_ms;
        } else if (dt_min > 0.001f) {
            if (instant_delta > 0.35f) {
                // Đang chuyển động rõ rệt: tính tốc độ tức thời và làm mịn
                float instant_rate = instant_delta / dt_min;
                _current_tilt_rate = 0.6f * _current_tilt_rate + 0.4f * instant_rate;
            } else {
                // ĐÃ ĐỨNG YÊN: Giảm dần tốc độ về 0 ngay lập tức (Decay) thay vì giữ nguyên giá trị cao!
                _current_tilt_rate = _current_tilt_rate * 0.4f;
                if (_current_tilt_rate < 0.1f) {
                    _current_tilt_rate = 0.0f;
                }
            }
        }
    }

    data.tilt_rate_deg_per_min = _current_tilt_rate;
    _last_tilt_angle = data.tilt_angle_deg;
    _last_read_ms = now_ms;

    return data;
}

bool SensorReader::isReady() const {
    return _mpu_ready;
}
