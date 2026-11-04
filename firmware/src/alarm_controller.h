#pragma once
#include <Arduino.h>

class AlarmController {
public:
    AlarmController();
    void begin();
    void update(int risk_level);
    void allOff();

private:
    int _current_risk_level;
    int _last_applied_risk_level;
    unsigned long _last_toggle_ms;
    bool _toggle_state;
};
