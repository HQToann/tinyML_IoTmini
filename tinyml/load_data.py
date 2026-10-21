#bước 1 lode file data

import numpy as np
import pandas as pd

from config import CSV_COLUMN_ALIASES, CLASS_NAMES, DATASET_PATH, FEATURES

#hàm load dataset
def load_dataset(csv_path=DATASET_PATH):
    #kiểm tra file có tồn tại không
    if not csv_path.exists():
        raise FileNotFoundError(
            f"không tìm thấy: {csv_path}, hãy kiểm tra lại"
        )

    #đọc file csv bằng pandas và đổi tên các cột
    dataframe = pd.read_csv(csv_path).rename(columns=CSV_COLUMN_ALIASES)

    #kiểm tra 3 future bắt buộc và nhãn đầu ra là phải có
    required_columns = FEATURES + ["label"]

    #kiểm tra tên cột có tồn tại hay không
    missing = []
    for name in required_columns:
        if name not in dataframe.columns:
            missing.append(name)
    if missing:
        raise ValueError("CSV đang thiếu các cột bắt buộc: " + ", ".join(missing))

    #kiểm tra cột run_id có tồn tại
    if "run_id" not in dataframe.columns:
        raise ValueError("File CSV phải có cột run_id để các dữ liệu đo thuộc cùng một lần đo được nhóm lại với nhau.")
    
    #lọc dữ liệu các hàng nào có null/nan bị xoá bỏ
    dataframe = dataframe[required_columns + ["run_id"]].dropna()
    #chuyển đổi đầu vào future
    X = dataframe[FEATURES].to_numpy(dtype=np.float32)
    #chuẩn đổi đầu ra nhãn
    y = dataframe["label"].to_numpy(dtype=np.int64)

    if not np.isfinite(X).all():
        raise ValueError("dữ liệu rỗng")
    if not set(np.unique(y)).issubset({0, 1, 2}):
        raise ValueError("nhãn hiện tại không đúng")
    if len(np.unique(y)) != len(CLASS_NAMES):
        raise ValueError("nhãn đang thiếu")
    if len(y) < 30:
        raise ValueError("số lượng mẫu nhỏ hơn 30")

    #ép kiểu của cột run_id (định danh nhóm) sang mảng num_py 1 chiều
    groups = dataframe["run_id"].astype(str).to_numpy()
    #trả về giá trị
    return X, y, groups
