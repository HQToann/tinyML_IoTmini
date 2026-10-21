/**
  Code chính của ESP32:
    1. Đọc cảm biến,
    2. Chạy model,
    3. Bật LED/còi
    4. Gửi MQTT
*/

#include <Arduino.h>

// put function declarations here:
int myFunction(int, int);

void setup() {
  // put your setup code here, to run once:
  int result = myFunction(2, 3);
}

void loop() {
  // put your main code here, to run repeatedly:
}

// put function definitions here:
int myFunction(int x, int y) {
  return x + y;
}