#include "network_mqtt.h"
#include "config.h"
#include <ArduinoJson.h>

// Trang web cài đặt Wi-Fi responsive tiếng Việt lưu trong PROGMEM
static const char TRANG_CAI_DAT_HTML[] PROGMEM = R"HTML(
<!doctype html>
<html lang="vi">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width,initial-scale=1">
  <title>Cài đặt Wi-Fi Nhà Mô Hình</title>
  <style>
    :root {
      --primary: #2563eb;
      --primary-hover: #1d4ed8;
      --bg: #f8fafc;
      --card-bg: #ffffff;
      --text: #1e293b;
      --text-muted: #64748b;
      --border: #e2e8f0;
      --radius: 12px;
    }
    * { box-sizing: border-box; margin: 0; padding: 0; font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif; }
    body { background: var(--bg); color: var(--text); display: flex; justify-content: center; align-items: center; min-height: 100vh; padding: 16px; }
    .card { background: var(--card-bg); width: 100%; max-width: 440px; padding: 32px 24px; border-radius: var(--radius); box-shadow: 0 10px 25px -5px rgba(0,0,0,0.08); border: 1px solid var(--border); }
    .header { text-align: center; margin-bottom: 24px; }
    .header h2 { font-size: 20px; font-weight: 700; color: #0f172a; margin-bottom: 6px; }
    .header p { font-size: 14px; color: var(--text-muted); }
    .form-group { margin-bottom: 18px; }
    label { display: block; font-size: 13px; font-weight: 600; margin-bottom: 6px; color: #334155; }
    select, input { width: 100%; padding: 12px; border: 1px solid var(--border); border-radius: 8px; font-size: 14px; background: #fff; outline: none; transition: border-color .2s; }
    select:focus, input:focus { border-color: var(--primary); }
    .btn-group { display: flex; flex-direction: column; gap: 10px; margin-top: 24px; }
    button { padding: 12px 20px; border-radius: 8px; font-size: 15px; font-weight: 600; cursor: pointer; border: none; transition: background .2s; }
    .btn-primary { background: var(--primary); color: #fff; }
    .btn-primary:hover { background: var(--primary-hover); }
    .btn-secondary { background: #f1f5f9; color: #334155; border: 1px solid var(--border); }
    .btn-secondary:hover { background: #e2e8f0; }
    .status { font-size: 13px; color: var(--text-muted); margin-top: 8px; text-align: center; }
    .badge { display: inline-block; padding: 4px 8px; border-radius: 6px; font-size: 12px; background: #eff6ff; color: var(--primary); font-weight: 500; margin-bottom: 12px; }
  </style>
</head>
<body>
  <div class="card">
    <div class="header">
      <span class="badge">ESP32-C3 TinyML</span>
      <h2>Cài Đặt Kết Nối Wi-Fi</h2>
      <p>Chọn mạng Wi-Fi tại vị trí lắp đặt để thiết bị truyền dữ liệu về hệ thống giám sát.</p>
    </div>
    <form method="post" action="/save">
      <div class="form-group">
        <label for="ssid">Tên Wi-Fi (SSID)</label>
        <select id="ssid" name="ssid" required>
          <option value="">Đang quét mạng Wi-Fi...</option>
        </select>
        <p class="status" id="trangThai">Đang tìm các mạng Wi-Fi ở gần...</p>
      </div>
      <button class="btn-secondary" type="button" onclick="quetWifi()" style="width:100%; margin-bottom:16px;">🔄 Quét lại mạng</button>
      <div class="form-group">
        <label for="password">Mật khẩu Wi-Fi</label>
        <input id="password" name="password" type="password" maxlength="63" placeholder="Nhập mật khẩu Wi-Fi">
      </div>
      <div class="btn-group">
        <button class="btn-primary" type="submit">Lưu cấu hình và Kết nối</button>
      </div>
    </form>
  </div>
  <script>
    const danhSach = document.getElementById('ssid');
    const thongBao = document.getElementById('trangThai');
    let dangQuet = false;
    async function capNhatDanhSach() {
      try {
        const res = await fetch('/networks', {cache: 'no-store'});
        const data = await res.json();
        if (data.scanning) {
          dangQuet = true;
          thongBao.textContent = 'Đang quét sóng Wi-Fi...';
          setTimeout(capNhatDanhSach, 1200);
          return;
        }
        dangQuet = false;
        const curVal = danhSach.value;
        danhSach.replaceChildren();
        if (!data.networks || data.networks.length === 0) {
          danhSach.add(new Option('Không tìm thấy mạng Wi-Fi nào', ''));
          thongBao.textContent = 'Không tìm thấy mạng.';
          return;
        }
        danhSach.add(new Option('--- Chọn mạng Wi-Fi ---', ''));
        for (const net of data.networks) {
          const sec = net.secure ? '🔒' : '🔓';
          danhSach.add(new Option(`${net.ssid} (${net.rssi} dBm) ${sec}`, net.ssid));
        }
        if (curVal) danhSach.value = curVal;
        thongBao.textContent = `Đã tìm thấy ${data.networks.length} mạng Wi-Fi.`;
      } catch (err) {
        dangQuet = false;
        danhSach.replaceChildren(new Option('Chưa kết nối được với ESP32', ''));
        thongBao.textContent = 'Hãy chắc chắn bạn đang kết nối Wi-Fi của ESP32.';
      }
    }
    async function quetWifi() {
      if (dangQuet) return;
      danhSach.replaceChildren(new Option('Đang quét...', ''));
      thongBao.textContent = 'Đang kích hoạt quét mạng...';
      try {
        await fetch('/rescan', {cache: 'no-store'});
        dangQuet = true;
        setTimeout(capNhatDanhSach, 600);
      } catch (e) {
        thongBao.textContent = 'Không kích hoạt được quá trình quét.';
      }
    }
    capNhatDanhSach();
  </script>
</body>
</html>
)HTML";

NetworkMQTT::NetworkMQTT()
    : _server(80), _mqttClient(_wifiClient), _setup_mode(false),
      _button_press_start_ms(0), _last_reconnect_attempt_ms(0) {}

void NetworkMQTT::begin() {
    _mqttClient.setServer(MQTT_BROKER, MQTT_PORT);
    _mqttClient.setBufferSize(512);

    _connectWiFiOrStartSetup();
}

void NetworkMQTT::_connectWiFiOrStartSetup() {
    _preferences.begin("wifi-config", true);
    String ssid = _preferences.getString("ssid", "");
    String password = _preferences.getString("password", "");
    _preferences.end();

    if (ssid.length() == 0) {
        Serial.println("[Wi-Fi] Chưa có thông tin cấu hình Wi-Fi -> Bật chế độ cài đặt.");
        startSetupMode();
        return;
    }

    Serial.printf("[Wi-Fi] Đang kết nối tới: %s ...\r\n", ssid.c_str());
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid.c_str(), password.c_str());

    unsigned long start_ms = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start_ms < 20000UL) {
        delay(300);
        Serial.print(".");
    }
    Serial.println();

    if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("[Wi-Fi] Kết nối thành công! IP: %s\r\n", WiFi.localIP().toString().c_str());
        _setup_mode = false;
        _reconnectMQTT();
    } else {
        Serial.println("[Wi-Fi] Không thể kết nối sau 20 giây -> Bật chế độ cài đặt.");
        startSetupMode();
    }
}

void NetworkMQTT::startSetupMode() {
    _setup_mode = true;
    WiFi.disconnect(true, false);
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAP(WIFI_AP_SSID, WIFI_AP_PASSWORD);

    IPAddress ip = WiFi.softAPIP();
    Serial.printf("[Setup Wi-Fi] Đang phát AP: %s | Mật khẩu: %s\r\n", WIFI_AP_SSID, WIFI_AP_PASSWORD);
    Serial.printf("[Setup Wi-Fi] Địa chỉ IP máy chủ: %s\r\n", ip.toString().c_str());

    _setupWebServerRoutes();
    _dnsServer.start(53, "*", ip);
    _server.begin();
}

void NetworkMQTT::_setupWebServerRoutes() {
    _server.on("/", HTTP_GET, [this]() {
        if (WiFi.scanComplete() == -2) {
            WiFi.scanNetworks(true, true);
        }
        _server.send_P(200, "text/html; charset=utf-8", TRANG_CAI_DAT_HTML);
    });

    _server.on("/networks", HTTP_GET, [this]() {
        int count = WiFi.scanComplete();
        if (count == -2) {
            WiFi.scanNetworks(true, true);
            _server.send(200, "application/json; charset=utf-8", "{\"scanning\":true,\"networks\":[]}");
            return;
        }
        if (count == -1) {
            _server.send(200, "application/json; charset=utf-8", "{\"scanning\":true,\"networks\":[]}");
            return;
        }

        String json = "{\"scanning\":false,\"networks\":[";
        for (int i = 0; i < count; ++i) {
            if (i > 0) json += ",";
            String ssid_name = WiFi.SSID(i);
            ssid_name.replace("\\", "\\\\");
            ssid_name.replace("\"", "\\\"");
            json += "{\"ssid\":\"" + ssid_name + "\",\"rssi\":" + String(WiFi.RSSI(i));
            json += ",\"secure\":" + String(WiFi.encryptionType(i) == WIFI_AUTH_OPEN ? "false" : "true") + "}";
        }
        json += "]}";
        _server.send(200, "application/json; charset=utf-8", json);
    });

    _server.on("/rescan", HTTP_GET, [this]() {
        WiFi.scanDelete();
        int res = WiFi.scanNetworks(true, true);
        bool scanning = (res == -1);
        _server.send(200, "application/json; charset=utf-8",
                     scanning ? "{\"scanning\":true}" : "{\"scanning\":false}");
    });

    _server.on("/save", HTTP_POST, [this]() {
        String ssid = _server.arg("ssid");
        String password = _server.arg("password");
        ssid.trim();

        if (ssid.length() == 0 || ssid.length() > 32 || password.length() > 63) {
            _server.send(400, "text/plain; charset=utf-8", "Tên mạng hoặc mật khẩu không hợp lệ.");
            return;
        }

        _preferences.begin("wifi-config", false);
        _preferences.putString("ssid", ssid);
        _preferences.putString("password", password);
        _preferences.end();

        _server.send(200, "text/html; charset=utf-8",
            "<meta charset='utf-8'><h2 style='text-align:center;margin-top:40px;color:#2563eb;'>"
            "Đã lưu Wi-Fi thành công!<br><span style='color:#64748b;font-size:16px;'>Thiết bị đang khởi động lại để kết nối...</span></h2>");
        delay(1200);
        ESP.restart();
    });

    _server.onNotFound([this]() {
        _server.sendHeader("Location", String("http://") + WiFi.softAPIP().toString(), true);
        _server.send(302, "text/plain", "");
    });
}

bool NetworkMQTT::isSetupMode() const {
    return _setup_mode;
}

void NetworkMQTT::checkSetupButton(int pin, unsigned long hold_time_ms) {
    if (digitalRead(pin) == LOW) {
        if (_button_press_start_ms == 0) {
            _button_press_start_ms = millis();
        } else if (millis() - _button_press_start_ms >= hold_time_ms) {
            _button_press_start_ms = 0;
            Serial.println("[Nút bấm] Giữ nút > 5 giây -> Chuyển sang chế độ cài đặt Wi-Fi.");
            startSetupMode();
        }
    } else {
        _button_press_start_ms = 0;
    }
}

void NetworkMQTT::_reconnectMQTT() {
    if (_mqttClient.connected()) return;

    Serial.printf("[MQTT] Đang kết nối tới Broker %s:%d ... ", MQTT_BROKER, MQTT_PORT);
    if (_mqttClient.connect(MQTT_CLIENT_ID)) {
        Serial.println("[OK]");
    } else {
        Serial.printf("[Thất bại, mã lỗi = %d]\r\n", _mqttClient.state());
    }
}

void NetworkMQTT::loop() {
    if (_setup_mode) {
        _dnsServer.processNextRequest();
        _server.handleClient();
        return;
    }

    // Nếu ở chế độ bình thường và có Wi-Fi, duy trì MQTT
    if (WiFi.status() == WL_CONNECTED) {
        if (!_mqttClient.connected()) {
            unsigned long now_ms = millis();
            if (now_ms - _last_reconnect_attempt_ms > 5000UL) {
                _last_reconnect_attempt_ms = now_ms;
                _reconnectMQTT();
            }
        } else {
            _mqttClient.loop();
        }
    }
}

bool NetworkMQTT::publishTelemetry(const SensorData& sensor_data, const PredictionResult& pred) {
    if (!_mqttClient.connected()) {
        return false;
    }

    StaticJsonDocument<384> doc;
    doc["node"]           = MQTT_CLIENT_ID;
    doc["tilt_angle"]     = roundf(sensor_data.tilt_angle_deg * 100.0f) / 100.0f;
    doc["tilt_rate"]      = roundf(sensor_data.tilt_rate_deg_per_min * 1000.0f) / 1000.0f;
    doc["soil_moisture"]  = 0.0; // Tương thích trường dữ liệu Dashboard
    doc["risk_level"]     = pred.predicted_class;
    doc["status"]         = pred.class_name;
    doc["prob_normal"]    = roundf(pred.probabilities[0] * 100.0f) / 100.0f;
    doc["prob_attention"] = roundf(pred.probabilities[1] * 100.0f) / 100.0f;
    doc["prob_high_risk"] = roundf(pred.probabilities[2] * 100.0f) / 100.0f;

    char payload[384];
    size_t len = serializeJson(doc, payload);

    return _mqttClient.publish(MQTT_TOPIC_DATA, payload, len);
}

bool NetworkMQTT::isWiFiConnected() {
    return WiFi.status() == WL_CONNECTED;
}

bool NetworkMQTT::isMQTTConnected() {
    return _mqttClient.connected();
}
