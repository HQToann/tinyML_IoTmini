#pragma once

// Số lượng đặc trưng đầu vào cho mô hình TinyML
constexpr int FEATURE_COUNT = 2;

// Trung bình và độ lệch chuẩn của các đặc trưng: [goc_nghieng (độ), toc_do_doi_goc (độ/phút)]
// Được trích xuất chuẩn hóa từ tập dữ liệu mô phỏng
const float NORM_MEAN[FEATURE_COUNT] = {5.565814f, 0.218661f};
const float NORM_STD[FEATURE_COUNT]  = {5.037641f, 0.224276f};

// Tham số lượng tử hóa đầu vào (Int8)
const float INPUT_SCALE = 0.0191148650f;
const int INPUT_ZERO_POINT = -38;

// Nhãn các lớp phân loại
// 0 = Bình thường, 1 = Chú ý, 2 = Nguy cơ cao trong mô hình
const char* const CLASS_NAMES[3] = {
    "Bình thường",
    "Chú ý",
    "Nguy cơ cao trong mô hình"
};
