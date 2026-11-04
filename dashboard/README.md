# 🌐 TINYML LANDSLIDE MONITOR - DJANGO WEB DASHBOARD

Hệ thống Dashboard thời gian thực thu thập dữ liệu quan trắc từ vi điều khiển ESP32-C3 (hoặc mô phỏng Wokwi) qua giao thức MQTT và hiển thị kết quả phân loại rủi ro sạt lở đất bằng mô hình TinyML Edge AI.

---

## 🚀 HƯỚNG DẪN KHỞI CHẠY BẰNG PIPENV

Môi trường ảo `pipenv` đã được cấu hình sẵn các thư viện `django` và `paho-mqtt`.

### 1. Mở Terminal tại thư mục gốc dự án:
```powershell
# Chuyển vào thư mục dashboard
cd "d:\Visual Studio Code\tinyML_IoTmini\dashboard"

# Khởi chạy server Django bằng môi trường ảo pipenv:
pipenv run python manage.py runserver
```

*(Hoặc nếu bạn đang ở trong terminal đã kích hoạt virtualenv sẵn `(tinyML_IoTmini)`, chỉ cần chạy:)*
```powershell
python manage.py runserver
```

### 2. Mở trình duyệt Web:
Truy cập địa chỉ: **[http://127.0.0.1:8000/](http://127.0.0.1:8000/)**

---

## ⚙️ KIẾN TRÚC HỆ THỐNG

1. **Backend (Django & MQTT Daemon)**:
   * **MQTT Worker**: Tự động chạy nền kết nối tới broker `broker.emqx.io:1883`, lắng nghe topic `landslide/telemetry`.
   * **Database SQLite (`db.sqlite3`)**: Tự động lưu trữ lịch sử các gói tin telemetry theo thời gian thực.
   * **REST APIs**:
     * `GET /api/latest/`: Trả về dữ liệu mới nhất tức thì từ bộ đệm RAM.
     * `GET /api/history/`: Trả về 30 điểm đo gần nhất phục vụ vẽ biểu đồ thời gian thực.
     * `GET /api/stats/`: Thống kê tổng gói tin, số lần cảnh báo, góc nghiêng cực đại.
     * `POST /api/simulate/`: Giả lập dữ liệu trực tiếp từ web khi không bật phần cứng.
     * `POST /api/clear/`: Xóa lịch sử lưu trữ.

2. **Frontend (Glassmorphic Dark UI + Chart.js)**:
   * **Giao diện chuẩn Dark Mode hiện đại**: Thẻ kính mờ Glassmorphism, viền phát sáng, font chữ Google Fonts (Outfit & Inter).
   * **4 Thẻ thông số tức thời**:
     * 🟢/🟡/🔴 **Đánh giá rủi ro TinyML**: Badge động phát sáng, độ tin cậy AI %.
     * 💧 **Độ ẩm đất (%)**: Thanh tiến trình động theo thời gian thực.
     * 📐 **Góc nghiêng (°)**: Cảnh báo sườn dốc nguy hiểm khi vượt ngưỡng 20°.
     * ⚡ **Vận tốc đổi góc (°/phút)**: Đo tốc độ trượt sạt tức thời.
   * **Phân bố xác suất AI Softmax**: 3 thanh đo % An toàn, Chú ý, Nguy cơ cao.
   * **2 Biểu đồ thời gian thực (Chart.js)**:
     * Biểu đồ đa trục: Độ ẩm đất vs Góc nghiêng.
     * Biểu đồ xu hướng xác suất AI.
   * **Âm thanh cảnh báo**: Tích hợp còi hú Web Audio API trên trình duyệt khi xảy ra nguy cơ sạt lở cấp bách (Cấp 2).
