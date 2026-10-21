#include "alarm_controller.h"
#include "config.h"

AlarmController::AlarmController() 
    : _current_risk_level(0), _last_toggle_ms(0), _toggle_state(false) {}

void AlarmController::begin() {
    pinMode(PIN_LED_GREEN, OUTPUT);
    pinMode(PIN_LED_YELLOW, OUTPUT);
    pinMode(PIN_LED_RED, OUTPUT);
    pinMode(PIN_BUZZER, OUTPUT);

    allOff();
    // Khởi động bật LED Xanh mặc định (trạng thái an toàn)
    digitalWrite(PIN_LED_GREEN, HIGH);
    Serial.println("[OK] Đèn LED và Còi báo động đã sẵn sàng!");
}

void AlarmController::allOff() {
    digitalWrite(PIN_LED_GREEN, LOW);
    digitalWrite(PIN_LED_YELLOW, LOW);
    digitalWrite(PIN_LED_RED, LOW);
    noTone(PIN_BUZZER);
    digitalWrite(PIN_BUZZER, LOW);
}

void AlarmController::update(int risk_level) {
    _current_risk_level = risk_level;
    unsigned long now_ms = millis();

    switch (risk_level) {
        case 0:
            // Cấp 0: BÌNH THƯỜNG (AN TOÀN)
            // LED Xanh sáng cố định, còi tắt
            digitalWrite(PIN_LED_GREEN, HIGH);
            digitalWrite(PIN_LED_YELLOW, LOW);
            digitalWrite(PIN_LED_RED, LOW);
            noTone(PIN_BUZZER);
            digitalWrite(PIN_BUZZER, LOW);
            break;

        case 1:
            // Cấp 1: CHÚ Ý (CÓ BẤT THƯỜNG)
            // LED Vàng sáng cố định, còi bíp ngắn ngắt quãng chậm (1000Hz, kêu 100ms mỗi 1 giây)
            digitalWrite(PIN_LED_GREEN, LOW);
            digitalWrite(PIN_LED_YELLOW, HIGH);
            digitalWrite(PIN_LED_RED, LOW);

            if (now_ms - _last_toggle_ms >= 1000) {
                _last_toggle_ms = now_ms;
                tone(PIN_BUZZER, 1000); // Phát âm tần số 1000 Hz
            } else if (now_ms - _last_toggle_ms >= 100) {
                noTone(PIN_BUZZER);
            }
            break;

        case 2:
            // Cấp 2: NGUY CƠ CAO (SẠT LỞ NGUY HIỂM)
            // LED Đỏ và Còi hú dồn dập tần số cao 2400Hz (chu kỳ bật tắt 200ms)
            digitalWrite(PIN_LED_GREEN, LOW);
            digitalWrite(PIN_LED_YELLOW, LOW);

            if (now_ms - _last_toggle_ms >= 200) {
                _last_toggle_ms = now_ms;
                _toggle_state = !_toggle_state;

                digitalWrite(PIN_LED_RED, _toggle_state ? HIGH : LOW);
                if (_toggle_state) {
                    tone(PIN_BUZZER, 2400); // Còi hú khẩn cấp tần số 2400 Hz
                } else {
                    noTone(PIN_BUZZER);
                }
            }
            break;

        default:
            allOff();
            break;
    }
}
