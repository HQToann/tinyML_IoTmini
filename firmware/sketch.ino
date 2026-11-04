#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <Preferences.h>
#include <PubSubClient.h>
#include <math.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>

// Tương thích cả Arduino IDE và PlatformIO
#if __has_include(<TensorFlowLite_ESP32.h>)
  #include <TensorFlowLite_ESP32.h>
#elif __has_include(<Chirale_TensorFlowLite.h>)
  #include <Chirale_TensorFlowLite.h>
#endif 
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"
#include "tensorflow/lite/schema/schema_generated.h"
#if __has_include("tensorflow/lite/micro/micro_error_reporter.h")
  #include "tensorflow/lite/micro/micro_error_reporter.h"
  #define HAS_MICRO_ERROR_REPORTER 1
#endif

// File dữ liệu mô hình và thông số chuẩn hóa
#include "model_data.h"
#include "norm_params.h"

// ==============================================================================
// 1. CẤU HÌNH CHÂN (PINOUT) ESP32-C3 SUPER MINI (NHÀ MÔ HÌNH)
// ==============================================================================
constexpr int CHAN_SDA = 4;          // SDA của MPU-6050
constexpr int CHAN_SCL = 5;          // SCL của MPU-6050
constexpr int CHAN_COI = 3;          // Còi báo (Buzzer)
constexpr int CHAN_DEN_XANH = 6;     // LED Xanh (qua trở 220Ω)
constexpr int CHAN_DEN_VANG = 7;     // LED Vàng (qua trở 220Ω)
constexpr int CHAN_DEN_DO = 8;       // LED Đỏ (qua trở 220Ω)
constexpr int CHAN_BOOT_CAI_WIFI = 9; // Nút BOOT tích hợp sẵn trên ESP32-C3
constexpr unsigned long THOI_GIAN_GIU_NUT_MS = 5000UL;

// ==============================================================================
// 2. CẤU HÌNH MẠNG WI-FI & MQTT BROKER
// ==============================================================================
const char* TEN_WIFI_CAI_DAT = "NhaMoHinh-Setup";
const char* MAT_KHAU_WIFI_CAI_DAT = "NhaMoHinh2026";

const char* MQTT_BROKER = "broker.emqx.io";
const int   MQTT_PORT = 1883;
const char* MQTT_TOPIC_DATA = "landslide/telemetry";
const char* MQTT_CLIENT_ID = "esp32c3_nhamohinh_node01";

WebServer may_chu_cai_dat(80);
DNSServer may_chu_dns;
Preferences bo_nho_wifi;
WiFiClient esp_wifi_client;
PubSubClient client_mqtt(esp_wifi_client);

bool dang_o_che_do_cai_dat = false;
unsigned long luc_bat_dau_giu_nut = 0;
unsigned long lan_cuoi_reconnect_mqtt_ms = 0;

// ==============================================================================
// 3. THÔNG SỐ VẬN HÀNH & TINYML
// ==============================================================================
constexpr unsigned long CHU_KY_DOC_MS = 1000UL;
constexpr unsigned long CUA_SO_TOC_DO_MS = 60000UL;

// Căn chỉnh 16-byte cho vùng nhớ Tensor Arena (16 KB)
alignas(16) uint8_t vung_nho_tensor[16 * 1024];

Adafruit_MPU6050 mpu;
tflite::MicroMutableOpResolver<3> bo_toan_tu;
const tflite::Model* mo_hinh = nullptr;
tflite::MicroInterpreter* bo_chay_model = nullptr;
TfLiteTensor* dau_vao = nullptr;
TfLiteTensor* dau_ra = nullptr;
int so_dac_trung_model = FEATURE_COUNT;

float goc_moc_toc_do = 0.0f;
float goc_truoc_ms = 0.0f;
float toc_do_doi_goc = 0.0f;
float goc_hieu_chinh_zero = 0.0f;  // Mốc góc 0° tự động hiệu chuẩn khi khởi động
unsigned long moc_toc_do_ms = 0;
unsigned long lan_doc_truoc_ms = 0;
bool da_co_moc_toc_do = false;
bool he_thong_san_sang = false;

const char* ten_trang_thai[] = {
  "Binh thuong",
  "Chu y",
  "Nguy co cao trong mo hinh"
};

// ==============================================================================
// 4. GIAO DIỆN WEB CÀI ĐẶT WI-FI (CAPTIVE PORTAL HTML)
// ==============================================================================
const char TRANG_CAI_DAT[] PROGMEM = R"HTML(
<!doctype html>
<html lang="vi">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width,initial-scale=1">
  <title>Cài đặt Wi-Fi Nhà Mô Hình</title>
  <style>
    * { box-sizing: border-box; margin: 0; padding: 0; font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif; }
    body { background: #f8fafc; color: #1e293b; display: flex; justify-content: center; align-items: center; min-height: 100vh; padding: 16px; }
    .card { background: #fff; width: 100%; max-width: 420px; padding: 28px 20px; border-radius: 12px; box-shadow: 0 10px 25px rgba(0,0,0,0.06); border: 1px solid #e2e8f0; }
    .badge { display: inline-block; padding: 4px 8px; border-radius: 6px; font-size: 12px; background: #eff6ff; color: #2563eb; font-weight: 600; margin-bottom: 12px; }
    h2 { font-size: 20px; font-weight: 700; color: #0f172a; margin-bottom: 6px; }
    p { font-size: 14px; color: #64748b; margin-bottom: 18px; }
    label { display: block; font-size: 13px; font-weight: 600; margin-bottom: 6px; color: #334155; }
    select, input { width: 100%; padding: 12px; border: 1px solid #cbd5e1; border-radius: 8px; font-size: 14px; margin-bottom: 14px; outline: none; }
    select:focus, input:focus { border-color: #2563eb; }
    button { width: 100%; padding: 12px; border-radius: 8px; font-size: 15px; font-weight: 600; cursor: pointer; border: none; }
    .btn-submit { background: #2563eb; color: #fff; margin-top: 10px; }
    .btn-submit:hover { background: #1d4ed8; }
    .btn-scan { background: #f1f5f9; color: #334155; border: 1px solid #e2e8f0; margin-bottom: 14px; }
    .btn-scan:hover { background: #e2e8f0; }
    .status { font-size: 13px; color: #64748b; text-align: center; margin-bottom: 14px; }
  </style>
</head>
<body>
  <div class="card">
    <span class="badge">ESP32-C3 TinyML</span>
    <h2>Cài đặt Wi-Fi Nhà Mô Hình</h2>
    <p>Chọn mạng Wi-Fi và nhập mật khẩu để thiết bị kết nối vào Cloud.</p>
    <form method="post" action="/save">
      <label for="ssid">Tên Wi-Fi (SSID)</label>
      <select id="ssid" name="ssid" required>
        <option value="">Đang quét mạng Wi-Fi...</option>
      </select>
      <button class="btn-scan" type="button" onclick="quetWifi()">Quét lại mạng Wi-Fi</button>
      <p class="status" id="trangThai">Đang tìm các mạng Wi-Fi ở gần...</p>
      <label for="password">Mật khẩu Wi-Fi</label>
      <input id="password" name="password" type="password" maxlength="63" placeholder="Nhập mật khẩu Wi-Fi">
      <button class="btn-submit" type="submit">Lưu cấu hình và Kết nối</button>
    </form>
  </div>
  <script>
    const danhSach = document.getElementById('ssid');
    const thongBao = document.getElementById('trangThai');
    let dangQuet = false;
    async function capNhatDanhSach() {
      try {
        const phanHoi = await fetch('/networks', {cache: 'no-store'});
        const duLieu = await phanHoi.json();
        if (duLieu.scanning) {
          dangQuet = true;
          thongBao.textContent = 'Đang quét mạng Wi-Fi ở gần...';
          setTimeout(capNhatDanhSach, 1200);
          return;
        }
        dangQuet = false;
        const giaTriCu = danhSach.value;
        danhSach.replaceChildren();
        if (!duLieu.networks || duLieu.networks.length === 0) {
          danhSach.add(new Option('Không tìm thấy mạng Wi-Fi', ''));
          thongBao.textContent = 'Không tìm thấy mạng.';
          return;
        }
        danhSach.add(new Option('--- Chọn mạng Wi-Fi ---', ''));
        for (const mang of duLieu.networks) {
          const baoMat = mang.secure ? ' [Có mật khẩu]' : ' [Không mật khẩu]';
          danhSach.add(new Option(`${mang.ssid} (${mang.rssi} dBm${baoMat})`, mang.ssid));
        }
        if (giaTriCu) danhSach.value = giaTriCu;
        thongBao.textContent = `Đã tìm thấy ${duLieu.networks.length} mạng Wi-Fi.`;
      } catch (loi) {
        dangQuet = false;
        danhSach.replaceChildren(new Option('Không kết nối được với ESP32', ''));
        thongBao.textContent = 'Hãy chắc chắn bạn đã kết nối vào Wi-Fi của ESP32.';
      }
    }
    async function quetWifi() {
      if (dangQuet) return;
      danhSach.replaceChildren(new Option('Đang quét mạng Wi-Fi...', ''));
      thongBao.textContent = 'Đang kích hoạt quét mạng...';
      try {
        await fetch('/rescan', {cache: 'no-store'});
        dangQuet = true;
        setTimeout(capNhatDanhSach, 600);
      } catch (loi) {
        thongBao.textContent = 'Không bắt đầu được quá trình quét.';
      }
    }
    capNhatDanhSach();
  </script>
</body>
</html>
)HTML";

// ==============================================================================
// 5. CÁC HÀM XỬ LÝ WI-FI VÀ WEB SERVER
// ==============================================================================
void bat_che_do_cai_dat() {
  dang_o_che_do_cai_dat = true;
  WiFi.disconnect(true, false);
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(TEN_WIFI_CAI_DAT, MAT_KHAU_WIFI_CAI_DAT);
  const IPAddress ip = WiFi.softAPIP();

  may_chu_cai_dat.on("/", HTTP_GET, []() {
    if (WiFi.scanComplete() == -2) {
      WiFi.scanNetworks(true, true);
    }
    may_chu_cai_dat.send_P(200, "text/html; charset=utf-8", TRANG_CAI_DAT);
  });

  may_chu_cai_dat.on("/networks", HTTP_GET, []() {
    const int so_mang = WiFi.scanComplete();
    if (so_mang == -2 || so_mang == -1) {
      WiFi.scanNetworks(true, true);
      may_chu_cai_dat.send(200, "application/json; charset=utf-8", "{\"scanning\":true,\"networks\":[]}");
      return;
    }

    String json = "{\"scanning\":false,\"networks\":[";
    for (int i = 0; i < so_mang; ++i) {
      if (i > 0) json += ",";
      String ten_mang = WiFi.SSID(i);
      ten_mang.replace("\\", "\\\\");
      ten_mang.replace("\"", "\\\"");
      json += "{\"ssid\":\"" + ten_mang + "\",\"rssi\":" + String(WiFi.RSSI(i));
      json += ",\"secure\":" + String(WiFi.encryptionType(i) == WIFI_AUTH_OPEN ? "false" : "true") + "}";
    }
    json += "]}";
    may_chu_cai_dat.send(200, "application/json; charset=utf-8", json);
  });

  may_chu_cai_dat.on("/rescan", HTTP_GET, []() {
    WiFi.scanDelete();
    const int ket_qua = WiFi.scanNetworks(true, true);
    may_chu_cai_dat.send(200, "application/json; charset=utf-8",
      (ket_qua == -1) ? "{\"scanning\":true}" : "{\"scanning\":false}");
  });

  may_chu_cai_dat.on("/save", HTTP_POST, []() {
    String ssid = may_chu_cai_dat.arg("ssid");
    String password = may_chu_cai_dat.arg("password");
    ssid.trim();
    if (ssid.length() == 0 || ssid.length() > 32 || password.length() > 63) {
      may_chu_cai_dat.send(400, "text/plain; charset=utf-8", "Tên mạng hoặc mật khẩu không hợp lệ.");
      return;
    }
    bo_nho_wifi.begin("wifi-config", false);
    bo_nho_wifi.putString("ssid", ssid);
    bo_nho_wifi.putString("password", password);
    bo_nho_wifi.end();
    may_chu_cai_dat.send(200, "text/html; charset=utf-8",
      "<meta charset='utf-8'><h2 style='text-align:center;margin-top:40px;color:#2563eb;'>"
      "Đã lưu Wi-Fi thành công!<br><span style='font-size:16px;color:#64748b;'>Thiết bị đang khởi động lại...</span></h2>");
    delay(1200);
    ESP.restart();
  });

  may_chu_cai_dat.onNotFound([]() {
    may_chu_cai_dat.sendHeader("Location", String("http://") + WiFi.softAPIP().toString(), true);
    may_chu_cai_dat.send(302, "text/plain", "");
  });
  
  may_chu_cai_dat.begin();
  may_chu_dns.start(53, "*", ip);

  Serial.println("[Wi-Fi Setup] Đang phát Wi-Fi cài đặt. Truy cập: 192.168.4.1");
}

void ket_noi_mqtt() {
  if (client_mqtt.connected()) return;
  Serial.print("[MQTT] Đang kết nối Broker... ");
  if (client_mqtt.connect(MQTT_CLIENT_ID)) {
    Serial.println("[OK]");
  } else {
    Serial.printf("[Thất bại, rc = %d]\r\n", client_mqtt.state());
  }
}

void ket_noi_wifi_hoac_mo_trang_cai_dat() {
  bo_nho_wifi.begin("wifi-config", true);
  String ssid = bo_nho_wifi.getString("ssid", "");
  String password = bo_nho_wifi.getString("password", "");
  bo_nho_wifi.end();

  if (ssid.length() == 0) {
    bat_che_do_cai_dat();
    return;
  }

  Serial.printf("[Wi-Fi] Đang kết nối tới: %s\r\n", ssid.c_str());
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid.c_str(), password.c_str());
  const unsigned long bat_dau = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - bat_dau < 20000UL) {
    delay(300);
    Serial.print(".");
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("[Wi-Fi] Đã kết nối! IP: ");
    Serial.println(WiFi.localIP());
    dang_o_che_do_cai_dat = false;
    ket_noi_mqtt();
  } else {
    Serial.println("[Wi-Fi] Không thể kết nối sau 20s -> Mở trang cài đặt.");
    bat_che_do_cai_dat();
  }
}

void xu_ly_che_do_cai_dat() {
  may_chu_dns.processNextRequest();
  may_chu_cai_dat.handleClient();
}

void kiem_tra_nut_cai_dat() {
  if (digitalRead(CHAN_BOOT_CAI_WIFI) == LOW) {
    if (luc_bat_dau_giu_nut == 0) {
      luc_bat_dau_giu_nut = millis();
    } else if (millis() - luc_bat_dau_giu_nut >= THOI_GIAN_GIU_NUT_MS) {
      luc_bat_dau_giu_nut = 0;
      Serial.println("[Nút BOOT] Giữ > 5s -> Mở trang cài đặt Wi-Fi.");
      bat_che_do_cai_dat();
    }
  } else {
    luc_bat_dau_giu_nut = 0;
  }
}

// ==============================================================================
// 6. CÁC HÀM XỬ LÝ MÔ HÌNH TINYML TENSORFLOW LITE MICRO
// ==============================================================================
bool khoi_tao_model() {
  mo_hinh = tflite::GetModel(g_model);
  if (mo_hinh == nullptr) {
    Serial.println("[LỖI] Không thể nạp mô hình g_model!");
    return false;
  }

  // Đăng ký 3 toán tử: FullyConnected, ReLU, Softmax
  if (bo_toan_tu.AddFullyConnected() != kTfLiteOk ||
      bo_toan_tu.AddRelu() != kTfLiteOk ||
      bo_toan_tu.AddSoftmax() != kTfLiteOk) {
    Serial.println("[LỖI] Đăng ký toán tử resolver thất bại!");
    return false;
  }

#ifdef HAS_MICRO_ERROR_REPORTER
  static tflite::MicroErrorReporter micro_error_reporter;
  static tflite::MicroInterpreter bo_chay_tinh(
      mo_hinh, bo_toan_tu, vung_nho_tensor, sizeof(vung_nho_tensor), &micro_error_reporter);
#else
  static tflite::MicroInterpreter bo_chay_tinh(
      mo_hinh, bo_toan_tu, vung_nho_tensor, sizeof(vung_nho_tensor));
#endif
  
  bo_chay_model = &bo_chay_tinh;

  if (bo_chay_model->AllocateTensors() != kTfLiteOk) {
    Serial.println("[LỖI] Không thể cấp phát vùng nhớ tensor arena!");
    return false;
  }

  dau_vao = bo_chay_model->input(0);
  dau_ra = bo_chay_model->output(0);
  
  if (dau_vao == nullptr || dau_ra == nullptr ||
      dau_vao->type != kTfLiteInt8 || dau_ra->type != kTfLiteInt8) {
    Serial.println("[LỖI] Kích thước hoặc kiểu tensor không khớp INT8!");
    return false;
  }

  so_dac_trung_model = dau_vao->dims->data[dau_vao->dims->size - 1];
  Serial.printf("[OK] TinyML đã khởi tạo thành công! Đầu vào: %d đặc trưng\r\n", so_dac_trung_model);
  return true;
}

bool doc_goc_nghieng(float& goc_deg) {
  sensors_event_t su_kien_gia_toc, su_kien_gyro, su_kien_nhiet_do;
  mpu.getEvent(&su_kien_gia_toc, &su_kien_gyro, &su_kien_nhiet_do);

  const float ax = su_kien_gia_toc.acceleration.x;
  const float ay = su_kien_gia_toc.acceleration.y;
  const float az = su_kien_gia_toc.acceleration.z;

  // Tính góc nghiêng bằng atan2f để tránh tràn số
  const float gia_toc_ngang = sqrtf(ax * ax + ay * ay);
  const float goc_thu = atan2f(gia_toc_ngang, fabsf(az)) * 180.0f / (float)M_PI;
  if (!isfinite(goc_thu)) return false;

  // Trừ mốc hiệu chuẩn 0° — chỉ lấy phần nghiêng tương đối
  goc_deg = fabsf(goc_thu - goc_hieu_chinh_zero);
  return true;
}

void cap_nhat_toc_do(float goc_hien_tai, unsigned long hien_tai_ms) {
  if (!da_co_moc_toc_do) {
    goc_moc_toc_do = goc_hien_tai;
    moc_toc_do_ms = hien_tai_ms;
    goc_truoc_ms = goc_hien_tai;
    da_co_moc_toc_do = true;
    toc_do_doi_goc = 0.0f;
    return;
  }

  const unsigned long thoi_gian_da_qua = hien_tai_ms - moc_toc_do_ms;
  if (thoi_gian_da_qua >= CUA_SO_TOC_DO_MS) {
    const float so_phut = thoi_gian_da_qua / 60000.0f;
    if (so_phut > 0.0f) {
      toc_do_doi_goc = fabsf(goc_hien_tai - goc_moc_toc_do) / so_phut;
    }
    goc_moc_toc_do = goc_hien_tai;
    moc_toc_do_ms = hien_tai_ms;
  } else {
    // Phản hồi nhanh khi có chuyển động góc rõ rệt
    float delta_goc = fabsf(goc_hien_tai - goc_truoc_ms);
    if (delta_goc > 0.2f) {
      float dt_phut = 1.0f / 60.0f;
      float toc_do_tuc_thoi = delta_goc / dt_phut;
      toc_do_doi_goc = 0.7f * toc_do_doi_goc + 0.3f * toc_do_tuc_thoi;
    }
  }
  goc_truoc_ms = goc_hien_tai;
}

int phan_loai(float goc_deg, float toc_do_deg_moi_phut, float xac_suat_out[3]) {
  if (so_dac_trung_model == 2) {
    // Chuẩn hóa 2 đặc trưng: [goc_deg, toc_do_doi_goc]
    const float dac_trung[2] = { goc_deg, toc_do_deg_moi_phut };
    for (int i = 0; i < 2; ++i) {
      const float do_lech_chuan = (NORM_STD[i] == 0.0f) ? 1.0f : NORM_STD[i];
      const float da_chuan_hoa = (dac_trung[i] - NORM_MEAN[i]) / do_lech_chuan;
      const float da_luong_tu = roundf(da_chuan_hoa / INPUT_SCALE + INPUT_ZERO_POINT);
      dau_vao->data.int8[i] = (int8_t)constrain(da_luong_tu, -128.0f, 127.0f);
    }
  } else {
    // Tương thích mô hình 3 đặc trưng cũ: [do_am_mac_dinh, goc_deg, toc_do_doi_goc]
    const float dac_trung[3] = { 47.411724f, goc_deg, toc_do_deg_moi_phut };
    const float mean_3[3] = { 47.411724f, 5.565814f, 0.218661f };
    const float std_3[3]  = { 16.465240f, 5.037641f, 0.224276f };
    for (int i = 0; i < 3; ++i) {
      const float da_chuan_hoa = (dac_trung[i] - mean_3[i]) / std_3[i];
      const float da_luong_tu = roundf(da_chuan_hoa / INPUT_SCALE + INPUT_ZERO_POINT);
      dau_vao->data.int8[i] = (int8_t)constrain(da_luong_tu, -128.0f, 127.0f);
    }
  }

  if (bo_chay_model->Invoke() != kTfLiteOk) return -1;

  int lop_tot_nhat = 0;
  int8_t diem_cao_nhat = dau_ra->data.int8[0];
  float tong_xac_suat = 0.0f;
  float scale = dau_ra->params.scale;
  int zero_point = dau_ra->params.zero_point;

  for (int i = 0; i < 3; ++i) {
    if (dau_ra->data.int8[i] > diem_cao_nhat) {
      diem_cao_nhat = dau_ra->data.int8[i];
      lop_tot_nhat = i;
    }
    float p = (dau_ra->data.int8[i] - zero_point) * scale;
    xac_suat_out[i] = (p < 0.0f) ? 0.0f : p;
    tong_xac_suat += xac_suat_out[i];
  }

  if (tong_xac_suat > 0.0f) {
    for (int i = 0; i < 3; ++i) xac_suat_out[i] /= tong_xac_suat;
  }

  return lop_tot_nhat;
}

void cap_nhat_den_coi(int trang_thai) {
  digitalWrite(CHAN_DEN_XANH, trang_thai == 0 ? HIGH : LOW);
  digitalWrite(CHAN_DEN_VANG, trang_thai == 1 ? HIGH : LOW);
  digitalWrite(CHAN_DEN_DO, trang_thai == 2 ? HIGH : LOW);

  if (trang_thai == 2) {
    tone(CHAN_COI, 2000);
  } else {
    noTone(CHAN_COI);
  }
}

void gui_telemetry_mqtt(float goc, float toc_do, int trang_thai, const float prob[3]) {
  if (!client_mqtt.connected()) return;

  char payload[320];
  snprintf(payload, sizeof(payload),
    "{\"node\":\"%s\",\"tilt_angle\":%.2f,\"tilt_rate\":%.3f,\"soil_moisture\":0.0,"
    "\"risk_level\":%d,\"status\":\"%s\",\"prob_normal\":%.2f,\"prob_attention\":%.2f,\"prob_high_risk\":%.2f}",
    MQTT_CLIENT_ID, goc, toc_do, trang_thai, ten_trang_thai[trang_thai],
    prob[0], prob[1], prob[2]);

  client_mqtt.publish(MQTT_TOPIC_DATA, payload);
}

// ==============================================================================
// 7. SETUP VÀ VÒNG LẶP CHÍNH (LOOP)
// ==============================================================================
void setup() {
  Serial.begin(115200);
  delay(300);

  Serial.println("\r\n=======================================================");
  Serial.println("   ESP32-C3 SUPER MINI - TINYML GIÁM SÁT NHÀ MÔ HÌNH   ");
  Serial.println("=======================================================");

  pinMode(CHAN_DEN_XANH, OUTPUT);
  pinMode(CHAN_DEN_VANG, OUTPUT);
  pinMode(CHAN_DEN_DO, OUTPUT);
  pinMode(CHAN_COI, OUTPUT);
  pinMode(CHAN_BOOT_CAI_WIFI, INPUT_PULLUP);
  cap_nhat_den_coi(0);

  client_mqtt.setServer(MQTT_BROKER, MQTT_PORT);
  ket_noi_wifi_hoac_mo_trang_cai_dat();
  
  // Khởi tạo I2C với chân SDA=4, SCL=5
  Wire.begin(CHAN_SDA, CHAN_SCL);
  Wire.setClock(400000);
  delay(50);
  
  bool mpu_ok = false;
  for (int retry = 0; retry < 5; ++retry) {
    if (mpu.begin(0x68, &Wire)) {
      mpu_ok = true;
      break;
    }
    delay(100);
  }

  if (!mpu_ok) {
    Serial.println("[CẢNH BÁO] Không tìm thấy MPU-6050!");
  } else {
    mpu.setAccelerometerRange(MPU6050_RANGE_2_G);
    mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);

    // Tự động hiệu chuẩn góc 0° theo mặt phẳng đặt mô hình ban đầu
    float tong_goc = 0.0f;
    int so_mau = 0;
    for (int i = 0; i < 15; i++) {
      sensors_event_t a, g, t;
      if (mpu.getEvent(&a, &g, &t)) {
        float ha = sqrtf(a.acceleration.x * a.acceleration.x + a.acceleration.y * a.acceleration.y);
        tong_goc += atan2f(ha, fabsf(a.acceleration.z)) * 180.0f / (float)M_PI;
        so_mau++;
      }
      delay(15);
    }
    goc_hieu_chinh_zero = (so_mau > 0) ? (tong_goc / so_mau) : 0.0f;
    Serial.printf("[OK] Cảm biến MPU-6050 đã sẵn sàng! Mốc góc gốc: %.2f°\r\n", goc_hieu_chinh_zero);
  }

  if (khoi_tao_model()) {
    he_thong_san_sang = true;
    Serial.println("[OK] Hệ thống đã sẵn sàng bước vào vòng lặp giám sát!");
  }
}

void loop() {
  if (dang_o_che_do_cai_dat) {
    xu_ly_che_do_cai_dat();
    return;
  }

  kiem_tra_nut_cai_dat();
  if (dang_o_che_do_cai_dat) {
    xu_ly_che_do_cai_dat();
    return;
  }

  // Duy trì kết nối MQTT
  if (WiFi.status() == WL_CONNECTED) {
    if (!client_mqtt.connected()) {
      unsigned long now = millis();
      if (now - lan_cuoi_reconnect_mqtt_ms > 5000UL) {
        lan_cuoi_reconnect_mqtt_ms = now;
        ket_noi_mqtt();
      }
    } else {
      client_mqtt.loop();
    }
  }

  if (!he_thong_san_sang) {
    delay(500);
    return;
  }

  const unsigned long hien_tai_ms = millis();
  if (hien_tai_ms - lan_doc_truoc_ms < CHU_KY_DOC_MS) {
    delay(5);
    return;
  }
  lan_doc_truoc_ms = hien_tai_ms;

  float goc_deg = 0.0f;
  if (!doc_goc_nghieng(goc_deg)) return;

  cap_nhat_toc_do(goc_deg, hien_tai_ms);
  float prob[3] = {0.0f, 0.0f, 0.0f};
  const int trang_thai = phan_loai(goc_deg, toc_do_doi_goc, prob);
  if (trang_thai < 0) return;

  cap_nhat_den_coi(trang_thai);
  gui_telemetry_mqtt(goc_deg, toc_do_doi_goc, trang_thai, prob);

  Serial.printf("[GIÁM SÁT] Góc: %5.2f° | Tốc độ: %6.3f°/phút | Đánh giá: %s (Cấp %d)\r\n",
                goc_deg, toc_do_doi_goc, ten_trang_thai[trang_thai], trang_thai);
}

