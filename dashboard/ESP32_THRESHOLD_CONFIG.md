# Cấu hình ngưỡng TinyML từ dashboard xuống ESP32

Dashboard đã có form cấu hình và API đọc ngưỡng ở `GET /api/config/`. API cần header `X-API-Key` giống token ESP32. Để thay đổi có hiệu lực trên Wokwi, thêm các phần sau vào sketch đang chạy.

## 1. Khai báo cấu hình mặc định

Đặt sau các biến toàn cục. Các giá trị mặc định chỉ để trình diễn; chỉnh trên dashboard sau khi chạy migration.

```cpp
struct NguongMoiTruong {
  float doAmChuY = 70.0f;
  float gocChuY = 10.0f;
  float gocNguyCoCao = 20.0f;
  float tocDoChuY = 5.0f;
  float tocDoNguyCoCao = 15.0f;
};

NguongMoiTruong nguong;
unsigned long lanLayCauHinh = 0;
const unsigned long KHOANG_LAY_CAU_HINH_MS = 15000;
```

## 2. Thêm hàm đọc số từ JSON

Đặt trước `setup()`:

```cpp
float doc_so_json(const String& json, const char* ten, float macDinh) {
  String khoa = "\"" + String(ten) + "\"";
  int vi_tri = json.indexOf(khoa);
  if (vi_tri < 0) return macDinh;

  vi_tri = json.indexOf(':', vi_tri + khoa.length());
  if (vi_tri < 0) return macDinh;
  vi_tri++;

  while (vi_tri < json.length() &&
         (json[vi_tri] == ' ' || json[vi_tri] == '\t')) {
    vi_tri++;
  }

  int ket_thuc = vi_tri;
  while (ket_thuc < json.length()) {
    char c = json[ket_thuc];
    if (!((c >= '0' && c <= '9') || c == '.' || c == '-')) break;
    ket_thuc++;
  }

  if (ket_thuc == vi_tri) return macDinh;
  return json.substring(vi_tri, ket_thuc).toFloat();
}

bool lay_cau_hinh_nguong() {
  if (WiFi.status() != WL_CONNECTED) return false;

  wifiClient.setInsecure();
  HTTPClient http;
  http.setTimeout(5000);

  String url = String(API_URL);
  url.replace("/api/readings/", "/api/config/");

  if (!http.begin(wifiClient, url)) return false;
  http.addHeader("X-API-Key", API_KEY);

  int ma = http.GET();
  if (ma != 200) {
    Serial.print("Khong lay duoc cau hinh, HTTP: ");
    Serial.println(ma);
    http.end();
    return false;
  }

  String json = http.getString();
  http.end();

  int dau_nguong = json.indexOf("\"thresholds\"");
  if (dau_nguong < 0) return false;
  String phan_nguong = json.substring(dau_nguong);

  nguong.doAmChuY = doc_so_json(phan_nguong, "moisture_attention_pct", nguong.doAmChuY);
  nguong.gocChuY = doc_so_json(phan_nguong, "attention_angle_deg", nguong.gocChuY);
  nguong.gocNguyCoCao = doc_so_json(phan_nguong, "high_risk_angle_deg", nguong.gocNguyCoCao);
  nguong.tocDoChuY = doc_so_json(phan_nguong, "attention_rate_deg_per_min", nguong.tocDoChuY);
  nguong.tocDoNguyCoCao = doc_so_json(phan_nguong, "high_risk_rate_deg_per_min", nguong.tocDoNguyCoCao);

  Serial.println("Da cap nhat nguong tu dashboard:");
  Serial.print("Do am chu y: "); Serial.println(nguong.doAmChuY);
  Serial.print("Goc chu y/nguy co cao: ");
  Serial.print(nguong.gocChuY); Serial.print(" / "); Serial.println(nguong.gocNguyCoCao);
  Serial.print("Toc do chu y/nguy co cao: ");
  Serial.print(nguong.tocDoChuY); Serial.print(" / "); Serial.println(nguong.tocDoNguyCoCao);
  return true;
}

int nang_muc_canh_bao(int lop_model, float do_am, float goc, float toc_do) {
  int lop_nguong = 0;

  if (goc >= nguong.gocNguyCoCao || toc_do >= nguong.tocDoNguyCoCao) {
    lop_nguong = 2;
  } else if (do_am >= nguong.doAmChuY ||
             goc >= nguong.gocChuY ||
             toc_do >= nguong.tocDoChuY) {
    lop_nguong = 1;
  }

  // Chỉ nâng mức cảnh báo; không hạ mức model đã dự đoán.
  return lop_model > lop_nguong ? lop_model : lop_nguong;
}
```

## 3. Đồng bộ ngưỡng và áp dụng sau TinyML

Sau khi Wi-Fi kết nối ở `setup()`, gọi:

```cpp
lay_cau_hinh_nguong();
lanLayCauHinh = millis();
```

Trong `loop()`, trước khi đọc cảm biến, thêm đồng bộ định kỳ:

```cpp
if (millis() - lanLayCauHinh >= KHOANG_LAY_CAU_HINH_MS) {
  lanLayCauHinh = millis();
  lay_cau_hinh_nguong();
}
```

Ngay sau dòng chạy model:

```cpp
int lop = chay_suy_luan(do_am, goc, toc_do);
```

thêm:

```cpp
if (lop >= 0) {
  lop = nang_muc_canh_bao(lop, do_am, goc, toc_do);
}
```

ESP32 sẽ lấy cấu hình khoảng 15 giây một lần. TinyML vẫn phân loại chính; ngưỡng dashboard chỉ có thể nâng kết quả lên mức cảnh báo cao hơn.

