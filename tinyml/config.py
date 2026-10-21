from pathlib import Path #thư viện dùng làm việc với đường dẫn file


SEED = 42 #cố định bộ sinh số ngẫu nhiên

#3 future đầu vào của mô hình: độ ẩm đất (%), góc nghiêng (độ), tốc độ biến thiên goc nghiên theo thời gian (độ/phút)
FEATURES = ["soil_moisture_pct", "tilt_angle_deg", "tilt_rate_deg_per_min"]
#3 lớp đầu ra: 0-normal (bình thường), 1-attention (có bất thường), 2-high_risk_in_model (nguy cơ cao)
CLASS_NAMES = ["normal", "attention", "high_risk_in_model"]

#cấu hình đường dẫn
ROOT = Path(__file__).resolve().parent
DATASET_PATH = ROOT / "data" / "data.csv"
MODEL_DIR = ROOT / "model"
FIRMWARE_INCLUDE_DIR = ROOT.parent / "firmware" / "include"

#ánh xạ đổi tên cột
CSV_COLUMN_ALIASES = {
    "do_am": "soil_moisture_pct",
    "goc_nghieng": "tilt_angle_deg",
    "toc_do_doi_goc": "tilt_rate_deg_per_min",
    "nhan": "label",
}
