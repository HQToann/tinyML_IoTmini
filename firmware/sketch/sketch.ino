#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <TensorFlowLite_ESP32.h>
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"
#include "tensorflow/lite/micro/micro_error_reporter.h"
#include "model_data.h"
#include "norm_params.h"

// Chân & Cấu hình ESP32-C3
const int XANH = 21, VANG = 20, DO = 10, COI = 8, BTN = 9;
String sv = "http://192.168.1.187:8000/api/readings/";
Preferences pref;
// MPU6050: X huong len (thang dung), Y ngang, Z dam vao trong.
// Neu mui ten X cua chip huong xuong thi doi X_SIGN thanh -1.0f
const float X_SIGN = 1.0f;
float zero_r = 0.0f, zero_p = 0.0f;   // moc 0 cua roll (trai/phai) va pitch (vao/ra)
bool da_calib = false, loi_hw = false, coi_dang_keu = false;
unsigned long t_doc = 0, t_btn = 0;
const char* tt_str[] = { "Binh thuong", "Chu y", "Nguy co cao" };

// Engine TinyML
alignas(16) uint8_t arena[16 * 1024];
tflite::MicroMutableOpResolver<3> res;
tflite::MicroInterpreter* interp = nullptr;
TfLiteTensor *in = nullptr, *out = nullptr;

// Điều khiển còi an toàn: chỉ gọi tone/noTone khi đổi trạng thái (chống lỗi LEDC not initialized)
void set_coi(bool bat) {
  if (bat && !coi_dang_keu) {
    tone(COI, 3000);
    coi_dang_keu = true;
  } else if (!bat && coi_dang_keu) {
    noTone(COI);
    coi_dang_keu = false;
  }
}

void set_st(int tt) {
  digitalWrite(XANH, tt == 0); digitalWrite(VANG, tt == 1); digitalWrite(DO, tt == 2);
  set_coi(tt == 2);
}

void w_i2c(uint8_t r, uint8_t v) { Wire.beginTransmission(0x68); Wire.write(r); Wire.write(v); Wire.endTransmission(); }

bool init_mpu() {
  delay(100); Wire.beginTransmission(0x68); if (Wire.endTransmission() != 0) return false;
  w_i2c(0x6B, 0); w_i2c(0x1A, 4); w_i2c(0x1C, 0); return true;
}

// roll  = nghieng trai/phai (X ngang ve phia Y), pitch = nghieng vao/ra (X ngang ve phia Z)
bool read_angle(float& roll, float& pitch) {
  Wire.beginTransmission(0x68); Wire.write(0x3B);
  if (Wire.endTransmission(false) != 0 || Wire.requestFrom(0x68, 6) != 6) return false;
  float ax = (int16_t)((Wire.read() << 8) | Wire.read()) / 16384.0f * X_SIGN;
  float ay = (int16_t)((Wire.read() << 8) | Wire.read()) / 16384.0f;
  float az = (int16_t)((Wire.read() << 8) | Wire.read()) / 16384.0f;
  roll  = atan2f(ay, ax) * 180.0f / PI;
  pitch = atan2f(az, ax) * 180.0f / PI;
  return isfinite(roll) && isfinite(pitch);
}

// ch: 0 = roll, 1 = pitch, 2 = tong. ch < 0: xoa lich su (goi sau khi doi moc 0)
float get_rate(int ch, float a) {
  static float h[3][60]; static int i[3] = {0, 0, 0}, n[3] = {0, 0, 0};
  if (ch < 0) { for (int k = 0; k < 3; ++k) { i[k] = 0; n[k] = 0; } return 0.0f; }
  float r = (n[ch] >= 5) ? fabsf(a - h[ch][n[ch] < 60 ? 0 : i[ch]]) / (n[ch] / 60.0f) : 0.0f;
  h[ch][i[ch]] = a; i[ch] = (i[ch] + 1) % 60; if (n[ch] < 60) ++n[ch];
  return r;
}

void set_zero(float r, float p, bool c = true) {
  zero_r = r; zero_p = p; da_calib = c;
  pref.begin("goc", false);
  pref.putBool("c", c); pref.putFloat("zr", r); pref.putFloat("zp", p);
  pref.end();
  get_rate(-1, 0);
  if (c) set_st(0); else { digitalWrite(XANH, 0); digitalWrite(VANG, 1); digitalWrite(DO, 0); set_coi(false); }
  Serial.printf(">>> MOC 0: roll %.2f | pitch %.2f (calib=%d)\n", r, p, c);
}

void do_tare() {
  float sr = 0, sp = 0; int n = 0;
  for (int i = 0; i < 20; ++i) { float r, p; if (read_angle(r, p)) { sr += r; sp += p; ++n; } delay(25); }
  if (n >= 15) set_zero(sr / n, sp / n, true);
}

bool init_ai() {
  const tflite::Model* m = tflite::GetModel(g_model);
  if (!m || m->version() != TFLITE_SCHEMA_VERSION) return false;
  res.AddFullyConnected(); res.AddRelu(); res.AddSoftmax();
  static tflite::MicroErrorReporter err;
  static tflite::MicroInterpreter it(m, res, arena, sizeof(arena), &err);
  interp = &it;
  if (interp->AllocateTensors() != kTfLiteOk) return false;
  in = interp->input(0); out = interp->output(0);
  return (in && out);
}

int classify(float g, float spd) {
  float f[2] = { g, spd };
  for (int i = 0; i < 2; ++i) {
    float std_v = (NORM_STD[i] == 0) ? 1.0f : NORM_STD[i];
    in->data.int8[i] = (int8_t)constrain(roundf(((f[i] - NORM_MEAN[i]) / std_v) / INPUT_SCALE + INPUT_ZERO_POINT), -128, 127);
  }
  if (interp->Invoke() != kTfLiteOk) return -1;
  int b = 0; int8_t mx = out->data.int8[0];
  for (int i = 1; i < 3; ++i) if (out->data.int8[i] > mx) { mx = out->data.int8[i]; b = i; }
  return b;
}

void send_telemetry(float a, float r, int s, float roll, float pitch) {
  if (WiFi.status() != WL_CONNECTED) return;
  HTTPClient http; if (!http.begin(sv)) return;
  http.setTimeout(3000); http.addHeader("Content-Type", "application/json"); http.addHeader("X-API-Key", "demo-satlo-2026");
  String p = "{\"device_id\":\"esp32c3_node01\",\"tilt_angle_deg\":" + String(a, 2)
           + ",\"roll_deg\":" + String(roll, 2) + ",\"pitch_deg\":" + String(pitch, 2)
           + ",\"tilt_rate_deg_per_min\":" + String(r, 3) + ",\"status\":" + String(s)
           + ",\"is_calibrated\":" + (da_calib ? "true" : "false") + "}";
  int code = http.POST(p);
  bool c_tare = false, c_rst = false;
  if (code > 0) {
    String resp = http.getString();
    c_tare = (resp.indexOf("tare_command\":true") >= 0 || resp.indexOf("tare_command\": true") >= 0);
    c_rst = (resp.indexOf("reset_tare_command\":true") >= 0 || resp.indexOf("reset_tare_command\": true") >= 0);
  }
  http.end();
  if (c_tare) do_tare(); else if (c_rst) set_zero(0, 0, false);
}

void setup() {
  Serial.begin(115200);
  pinMode(XANH, OUTPUT); pinMode(VANG, OUTPUT); pinMode(DO, OUTPUT); pinMode(COI, OUTPUT); pinMode(BTN, INPUT_PULLUP);
  set_coi(false);
  Wire.begin(0, 1); Wire.setClock(100000);
  if (!init_mpu() || !init_ai()) { loi_hw = true; return; }

  pref.begin("goc", true); da_calib = pref.getBool("c", false);
  zero_r = pref.getFloat("zr", 0); zero_p = pref.getFloat("zp", 0);
  if (da_calib && !pref.isKey("zr")) da_calib = false;  // moc 0 cu (1 truc) khong dung duoc, can tare lai
  String s_url = pref.getString("sv", "");
  if (s_url.length() > 0) sv = s_url;
  pref.end();
  if (da_calib) set_st(0); else digitalWrite(VANG, 1);

  WiFiManager wm;
  WiFiManagerParameter p_sv("server", "Dashboard URL", sv.c_str(), 100);
  wm.addParameter(&p_sv);
  if (!wm.autoConnect("NhaMoHinh-Setup", "00000000")) { delay(3000); ESP.restart(); }
  if (strlen(p_sv.getValue()) > 0 && String(p_sv.getValue()) != sv) {
    sv = String(p_sv.getValue()); pref.begin("goc", false); pref.putString("sv", sv); pref.end();
  }
}

void loop() {
  if (digitalRead(BTN) == LOW) {
    if (!t_btn) t_btn = millis();
    digitalWrite(XANH, (millis()/100)%2); digitalWrite(VANG, (millis()/100)%2); digitalWrite(DO, (millis()/100)%2);
    if (millis() - t_btn >= 5000) {
      tone(COI, 3000, 200); delay(250);
      WiFiManager wm; wm.resetSettings();
      pref.begin("goc", false); pref.remove("sv"); pref.end();
      ESP.restart();
    }
    return;
  } else t_btn = 0;

  if (loi_hw) { digitalWrite(VANG, (millis()/300)%2); digitalWrite(DO, (millis()/300)%2); return; }

  if (!da_calib) {
    digitalWrite(VANG, (millis()/500)%2);
    if (millis() - t_doc >= 1000) {
      t_doc = millis(); float r = 0, p = 0; read_angle(r, p);
      send_telemetry(sqrtf(r * r + p * p), 0, -1, r, p);
      Serial.printf("Dang cho hieu chuan tren web | roll %.2f | pitch %.2f\n", r, p);
    }
    return;
  }

  if (millis() - t_doc >= 1000) {
    t_doc = millis();
    float r, p; if (!read_angle(r, p)) return;
    float gr = r - zero_r, gp = p - zero_p;
    float gt = sqrtf(gr * gr + gp * gp);

    float sr = get_rate(0, gr), sp = get_rate(1, gp), st = get_rate(2, gt);
    // Model chi hoc goc duong (0..90 do) nen dua do lon goc vao, khong dua goc co dau
    int t1 = classify(fabsf(gr), sr), t2 = classify(fabsf(gp), sp), t3 = classify(gt, st);
    if (t1 < 0 || t2 < 0 || t3 < 0) return;

    int tt = max(t1, max(t2, t3));
    float spd = max(sr, max(sp, st));
    set_st(tt);

    const char* h_r = fabsf(gr) < 0.5f ? "-" : (gr > 0 ? "sang +Y" : "sang -Y");
    const char* h_p = fabsf(gp) < 0.5f ? "-" : (gp > 0 ? "vao trong" : "ra ngoai");
    Serial.printf("Roll %.2f (%s) | Pitch %.2f (%s) | Tong %.2f | Toc do %.2f do/phut | TT: %s\n",
                  gr, h_r, gp, h_p, gt, spd, tt_str[tt]);
    send_telemetry(gt, spd, tt, gr, gp);
  }
}
