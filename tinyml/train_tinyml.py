"""Chạy toàn bộ quy trình huấn luyện, đánh giá và xuất TinyML.

Chạy trong thư mục này bằng lệnh: python train_tinyml.py
CSV đầu vào là dữ liệu mô phỏng phục vụ đồ án, chưa được xác thực ngoài thực địa.
"""

from export_model import export_and_evaluate
from load_data import load_dataset
from prepare import prepare_data
from train_model import train_model


def main():
    print("Bước 1/4: Đọc và kiểm tra CSV")
    X, y, groups = load_dataset()
    print(f"Đã đọc {len(y)} dòng, {X.shape[1]} đặc trưng, {len(set(groups))} phiên đo.")

    print("Bước 2/4: Chia các phiên đo và chuẩn hóa dữ liệu")
    data = prepare_data(X, y, groups)
    print(
        f"Số dòng: huấn luyện={len(data['y_train'])}, "
        f"kiểm định={len(data['y_val'])}, kiểm tra={len(data['y_test'])}"
    )
    print(
        f"Số phiên: huấn luyện={len(data['train_groups'])}, "
        f"kiểm định={len(data['val_groups'])}, kiểm tra={len(data['test_groups'])}"
    )

    print("Bước 3/4: Huấn luyện bộ phân loại")
    model = train_model(data)
    print(f"Số tham số của mô hình: {model.count_params()}")

    print("Bước 4/4: Lượng tử hóa, đánh giá và xuất mô hình cho ESP32")
    export_and_evaluate(model, data)
    print("Hoàn tất. Kết quả chỉ áp dụng cho bộ dữ liệu mô phỏng đã cung cấp.")


if __name__ == "__main__":
    main()
