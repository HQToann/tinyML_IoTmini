"""Cau hinh cho model phan loai do nghieng mo hinh nha."""

from pathlib import Path

SEED = 42
FEATURES = ["tilt_angle_deg", "tilt_rate_deg_per_min"]
CLASS_NAMES = ["normal_house", "attention_house_tilt", "high_tilt_risk_in_model"]

ROOT = Path(__file__).resolve().parent
DATASET_PATH = ROOT / "data" / "data.csv"
MODEL_DIR = ROOT / "model"
FIRMWARE_INCLUDE_DIR = ROOT.parent / "firmware" / "include"
