"""Chia du lieu theo phien do va chuan hoa hai dau vao."""

import numpy as np

from config import SEED


def _normalize(values, mean, std):
    std = np.where(std == 0, 1.0, std)
    return ((values - mean) / std).astype(np.float32)


def prepare_data(X, y, groups):
    """Chia 60/20/20 theo run_id de khong tach mot phien sang nhieu tap."""
    rng = np.random.default_rng(SEED)
    assignments = {}

    for label in np.unique(y):
        label_groups = np.array(sorted(set(groups[y == label])))
        rng.shuffle(label_groups)
        if len(label_groups) < 5:
            raise ValueError(f"Nhan {label} can it nhat 5 phien do doc lap.")

        train_end = int(len(label_groups) * 0.60)
        validation_end = int(len(label_groups) * 0.80)
        for group in label_groups[:train_end]:
            assignments[group] = "train"
        for group in label_groups[train_end:validation_end]:
            assignments[group] = "validation"
        for group in label_groups[validation_end:]:
            assignments[group] = "test"

    split = np.array([assignments[group] for group in groups])
    train = split == "train"
    validation = split == "validation"
    test = split == "test"

    mean = X[train].mean(axis=0)
    std = X[train].std(axis=0)

    return {
        "X_train": _normalize(X[train], mean, std),
        "X_validation": _normalize(X[validation], mean, std),
        "X_test": _normalize(X[test], mean, std),
        "y_train": y[train],
        "y_validation": y[validation],
        "y_test": y[test],
        "mean": mean,
        "std": std,
        "train_runs": len(set(groups[train])),
        "validation_runs": len(set(groups[validation])),
        "test_runs": len(set(groups[test])),
    }
