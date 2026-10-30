# Ghi chú TinyML và firmware

Model hiện tại phân loại độ nghiêng mô hình ngôi nhà bằng hai đầu vào theo đúng thứ tự:

1. `tilt_angle_deg`: góc nghiêng MPU6050, đơn vị độ.
2. `tilt_rate_deg_per_min`: tốc độ đổi góc, đơn vị độ/phút.

Không dùng biến trở hay dữ liệu độ ẩm. Chạy `python train_tinyml.py` trong thư mục `tinyml` để tạo lại `model/model_int8.tflite` và hai header firmware `model_data.h`, `norm_params.h`.

Firmware phải dùng đúng hai giá trị trên, cùng công thức tính góc nghiêng và tốc độ như lúc tạo dataset. Sơ đồ Wokwi hiện chỉ giữ ESP32-C3, MPU6050, còi và đèn.

Tệp này thay thế hướng dẫn cũ cho sketch HTTP ba đầu vào. Firmware PlatformIO hiện tại gửi telemetry bằng MQTT; cấu hình MQTT và API Django cần dùng cùng một đường truyền nếu muốn hiển thị trực tiếp trên dashboard.
