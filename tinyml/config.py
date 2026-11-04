from pathlib import Path

# Cố định bộ sinh số ngẫu nhiên
SEED = 42

# 2 đặc trưng đầu vào cho nhà mô hình: góc nghiêng (độ), tốc độ biến thiên góc nghiêng (độ/phút)
# Đã lược bỏ cảm biến độ ẩm đất
FEATURES = ["tilt_angle_deg", "tilt_rate_deg_per_min"]

# 3 lớp đầu ra: 0-normal (bình thường), 1-attention (chú ý/bất thường), 2-high_risk_in_model (nguy cơ cao)
CLASS_NAMES = ["normal", "attention", "high_risk_in_model"]

# Cấu hình đường dẫn
ROOT = Path(__file__).resolve().parent
DATASET_PATH = ROOT / "data" / "data.csv"
MODEL_DIR = ROOT / "model"
FIRMWARE_INCLUDE_DIR = ROOT.parent / "firmware" / "include"

# Ánh xạ đổi tên cột từ CSV
CSV_COLUMN_ALIASES = {
    "do_am": "soil_moisture_pct",
    "goc_nghieng": "tilt_angle_deg",
    "toc_do_doi_goc": "tilt_rate_deg_per_min",
    "nhan": "label",
}
