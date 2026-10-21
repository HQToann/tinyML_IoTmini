#pragma once
#include <Arduino.h>

class AlarmController {
public:
    AlarmController();

    // Khởi tạo các chân GPIO cho 3 LED và Buzzer
    void begin();

    // Cập nhật trạng thái báo động theo mức rủi ro (0, 1, 2)
    void update(int risk_level);

    // Tắt toàn bộ đèn và còi
    void allOff();

private:
    int _current_risk_level;
    unsigned long _last_toggle_ms;
    bool _toggle_state;
};
