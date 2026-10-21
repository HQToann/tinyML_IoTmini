# Dashboard Django TinyML IoT

Django nhận số đo ESP32 qua HTTP, lưu vào SQLite và cập nhật trang web mỗi giây. TinyML vẫn chạy trên ESP32. Dashboard cho phép chỉnh một lớp ngưỡng bổ sung; ngưỡng chỉ nâng cảnh báo, không hạ kết quả model.

## Chạy trên Windows

Trong PowerShell tại thư mục `dashboard`:

```powershell
.\.venv\Scripts\Activate.ps1
python manage.py migrate
python manage.py runserver 0.0.0.0:8000
```

Mở `http://127.0.0.1:8000/`. Phần “Cấu hình môi trường” lưu ngưỡng vào SQLite. Các giá trị ban đầu chỉ để minh họa, không phải ngưỡng thực địa.

## API

- `GET /api/health/`: kiểm tra server.
- `GET /api/readings/?limit=60`: lấy lịch sử, tối đa 200 bản tin.
- `POST /api/readings/`: nhận số đo ESP32; yêu cầu `X-API-Key`.
- `GET /api/config/`: trả cấu hình ngưỡng hiện tại; yêu cầu `X-API-Key`.

ESP32 cần gọi `GET /api/config/` định kỳ và áp dụng lớp ngưỡng sau kết quả TinyML. Xem `ESP32_THRESHOLD_CONFIG.md` để thêm phần đồng bộ vào sketch Wokwi.

Các ngưỡng cần được hiệu chỉnh theo cảm biến, mô hình và dữ liệu của đồ án; không dùng cho cảnh báo an toàn thực địa.
