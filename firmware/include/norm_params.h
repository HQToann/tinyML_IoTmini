#pragma once
// Tạo bởi tinyml/train_tinyml.py. Giữ nguyên thứ tự các đặc trưng.
// Thứ tự đầu vào: độ ẩm đất (%), góc nghiêng (độ), tốc độ đổi góc (độ/phút).
const float NORM_MEAN[3] = {47.411724f, 5.565814f, 0.218661f};
const float NORM_STD[3] = {16.465240f, 5.037641f, 0.224276f};
const float INPUT_SCALE = 0.0191148650f;
const int INPUT_ZERO_POINT = -38;
// Các lớp: 0=bình thường, 1=chú ý, 2=nguy cơ cao trong mô hình.
