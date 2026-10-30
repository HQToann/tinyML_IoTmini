"""Buoc 1: doc CSV va tao dac trung theo cong thuc accelerometer 2 truc."""

import numpy as np
import pandas as pd

from config import CLASS_NAMES, DATASET_PATH


CSV_COLUMN_ALIASES = {
    "goc_nghieng": "tilt_angle_deg",
    "toc_do_doi_goc": "tilt_rate_deg_per_min",
    "nhan": "label",
}


def _calculate_features(dataframe):
    """Tinh goc va toc do tu ax/ay/az, khong tin gia tri tinh san trong CSV."""
    required = {"run_id", "minute", "ax_g", "ay_g", "az_g", "label"}
    missing = sorted(required - set(dataframe.columns))
    if missing:
        raise ValueError("CSV thieu cac cot bat buoc: " + ", ".join(missing))

    dataframe = dataframe.copy()
    axis_values = dataframe[["ax_g", "ay_g", "az_g"]].to_numpy(dtype=np.float64)
    if not np.isfinite(axis_values).all():
        raise ValueError("Gia tri ax_g, ay_g, az_g phai la so huu han.")

    ax, ay, az = axis_values.T
    # Cung cong thuc voi firmware: goc nghieng tong hop tu hai truc ngang.
    horizontal_acceleration = np.sqrt(ax * ax + ay * ay)
    dataframe["tilt_angle_deg"] = np.degrees(
        np.arctan2(horizontal_acceleration, np.abs(az))
    )

    dataframe["minute"] = pd.to_numeric(dataframe["minute"], errors="raise")
    dataframe["label"] = pd.to_numeric(dataframe["label"], errors="raise").astype(int)
    dataframe = dataframe.sort_values(["run_id", "minute"]).reset_index(drop=True)
    if dataframe.duplicated(["run_id", "minute"]).any():
        raise ValueError("Moi run_id chi duoc co mot mau tai moi minute.")

    # So mau lien tiep cach nhau bao nhieu phut thi quy doi rate theo khoang do.
    elapsed_minutes = dataframe.groupby("run_id")["minute"].diff()
    angle_change = dataframe.groupby("run_id")["tilt_angle_deg"].diff().abs()
    dataframe["tilt_rate_deg_per_min"] = (
        angle_change / elapsed_minutes
    ).where(elapsed_minutes > 0, 0.0).fillna(0.0)

    if (elapsed_minutes.dropna() <= 0).any():
        raise ValueError("Gia tri minute phai tang trong tung run_id.")
    return dataframe


def load_dataset(csv_path=DATASET_PATH):
    if not csv_path.exists():
        raise FileNotFoundError(
            f"Khong tim thay dataset: {csv_path}. Dat CSV tai tinyml/data/data.csv."
        )

    dataframe = pd.read_csv(csv_path).rename(columns=CSV_COLUMN_ALIASES)
    dataframe = _calculate_features(dataframe)
    labels = dataframe["label"].to_numpy(dtype=np.int64)
    if not set(np.unique(labels)).issubset({0, 1, 2}):
        raise ValueError("Nhan phai la so nguyen 0, 1 hoac 2.")
    if len(np.unique(labels)) != len(CLASS_NAMES):
        raise ValueError("CSV phai co du ca ba nhan 0, 1 va 2.")

    features = dataframe[["tilt_angle_deg", "tilt_rate_deg_per_min"]]
    X = features.to_numpy(dtype=np.float32)
    groups = dataframe["run_id"].astype(str).to_numpy()
    if not np.isfinite(X).all():
        raise ValueError("Cac dac trung sau khi tinh phai la so huu han.")
    if len(labels) < 30:
        raise ValueError("Can it nhat 30 mau co nhan de chia train/validation/test.")
    return X, labels, groups
