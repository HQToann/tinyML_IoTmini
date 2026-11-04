#include "alarm_controller.h"
#include "config.h"

AlarmController::AlarmController() 
    : _current_risk_level(-1), _last_applied_risk_level(-1), _last_toggle_ms(0), _toggle_state(false) {}

void AlarmController::begin() {
    pinMode(PIN_LED_GREEN, OUTPUT);
    pinMode(PIN_LED_YELLOW, OUTPUT);
    pinMode(PIN_LED_RED, OUTPUT);
    pinMode(PIN_BUZZER, OUTPUT);

    allOff();
    update(0); // Khởi động ở trạng thái Bình thường (LED Xanh)
    Serial.println("[OK] Đèn LED và Còi báo động đã sẵn sàng!");
}

void AlarmController::allOff() {
    digitalWrite(PIN_LED_GREEN, LOW);
    digitalWrite(PIN_LED_YELLOW, LOW);
    digitalWrite(PIN_LED_RED, LOW);
    noTone(PIN_BUZZER);
    digitalWrite(PIN_BUZZER, LOW);
    _last_applied_risk_level = -1;
}

void AlarmController::update(int risk_level) {
    _current_risk_level = risk_level;
    unsigned long now_ms = millis();

    // 1. Chỉ cập nhật phần cứng LED và còi khi CẤP RỦI RO THAY ĐỔI (tránh ghi đè GPIO liên tục trong loop)
    if (risk_level != _last_applied_risk_level) {
        _last_applied_risk_level = risk_level;
        _last_toggle_ms = now_ms;

        switch (risk_level) {
            case 0:
                // Cấp 0: Bình thường (An toàn)
                digitalWrite(PIN_LED_GREEN, HIGH);
                digitalWrite(PIN_LED_YELLOW, LOW);
                digitalWrite(PIN_LED_RED, LOW);
                noTone(PIN_BUZZER);
                break;

            case 1:
                // Cấp 1: Chú ý (Bất thường nhẹ)
                digitalWrite(PIN_LED_GREEN, LOW);
                digitalWrite(PIN_LED_YELLOW, HIGH);
                digitalWrite(PIN_LED_RED, LOW);
                tone(PIN_BUZZER, 1200);  // Bíp ngay tiếng đầu tiên
                break;

            case 2:
                // Cấp 2: Nguy cơ cao trong mô hình
                digitalWrite(PIN_LED_GREEN, LOW);
                digitalWrite(PIN_LED_YELLOW, LOW);
                digitalWrite(PIN_LED_RED, HIGH);
                tone(PIN_BUZZER, 2000);  // Bật còi liên tục tần số 2000 Hz
                break;

            default:
                allOff();
                break;
        }
    }

    // 2. Nhịp bíp ngắt quãng chỉ kích hoạt khi đang ở Cấp 1 (100ms kêu, 1400ms nghỉ)
    if (risk_level == 1) {
        unsigned long elapsed = now_ms - _last_toggle_ms;
        if (elapsed >= 1500) {
            _last_toggle_ms = now_ms;
            tone(PIN_BUZZER, 1200);
        } else if (elapsed >= 100) {
            noTone(PIN_BUZZER);
        }
    }
}
