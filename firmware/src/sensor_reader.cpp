#include "sensor_reader.h"
#include "config.h"
#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <cmath>

static Adafruit_MPU6050 mpu;

SensorReader::SensorReader() 
    : _last_tilt_angle(0.0f), _last_read_ms(0), _is_first_read(true) {}

bool SensorReader::begin() {
    pinMode(PIN_SOIL_ADC, INPUT);
    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);

    if (!mpu.begin(0x68, &Wire)) {
        Serial.println("[LỖI] Không tìm thấy MPU-6050!");
        return false;
    }

    mpu.setAccelerometerRange(MPU6050_RANGE_4_G);
    mpu.setGyroRange(MPU6050_RANGE_250_DEG);
    mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);

    Serial.println("[OK] MPU-6050 & ADC đã sẵn sàng!");
    return true;
}

SensorData SensorReader::read() {
    SensorData data;
    unsigned long now_ms = millis();

    // 1. Độ ẩm đất (ADC 12-bit: 0 - 4095)
    int raw_adc = analogRead(PIN_SOIL_ADC);
    data.soil_moisture_pct = constrain((raw_adc / 4095.0f) * 100.0f, 0.0f, 100.0f);

    // 2. Góc nghiêng (độ) từ MPU-6050
    sensors_event_t a, g, temp;
    mpu.getEvent(&a, &g, &temp);

    float ax = a.acceleration.x;
    float ay = a.acceleration.y;
    float az = a.acceleration.z;
    float norm = sqrt(ax * ax + ay * ay + az * az);

    if (norm > 0.001f) {
        float cos_theta = constrain(az / norm, -1.0f, 1.0f);
        data.tilt_angle_deg = acos(cos_theta) * (180.0f / M_PI);
    } else {
        data.tilt_angle_deg = 0.0f;
    }

    // 3. Tốc độ biến thiên góc nghiêng (độ/phút)
    // Tích hợp từ Gyroscope (thanh ROTATION trên Wokwi) và biến thiên góc nghiêng theo thời gian
    float gyro_dps = sqrt(g.gyro.x * g.gyro.x + g.gyro.y * g.gyro.y) * (180.0f / M_PI); // độ/giây
    float gyro_rate_deg_per_min = gyro_dps * 0.012f; // Quy đổi về thang đo phù hợp với TinyML (0 - 1.2°/phút)

    float accel_rate_deg_per_min = 0.0f;
    if (_is_first_read) {
        _is_first_read = false;
    } else {
        float delta_time_min = (now_ms - _last_read_ms) / 60000.0f;
        if (delta_time_min > 0.0001f) {
            float delta_angle = fabs(data.tilt_angle_deg - _last_tilt_angle);
            accel_rate_deg_per_min = delta_angle / delta_time_min;
        }
    }

    // Nếu người dùng chỉnh thanh ROTATION trên Wokwi, cảm biến Gyro sẽ phản hồi ngay lập tức
    if (gyro_rate_deg_per_min > 0.05f) {
        data.tilt_rate_deg_per_min = constrain(gyro_rate_deg_per_min, 0.0f, 1.2f);
    } else {
        data.tilt_rate_deg_per_min = constrain(accel_rate_deg_per_min, 0.0f, 1.2f);
    }

    _last_tilt_angle = data.tilt_angle_deg;
    _last_read_ms = now_ms;

    return data;
}
