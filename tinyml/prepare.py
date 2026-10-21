#Bước 2 chia và chuẩn hoá dữa liệu 

import numpy as np

from config import SEED

#đưa về phân hoá trung bình có giá trị xấp xỉ bằng 0 và độ lệch chuẩn bằng 1
def _standardize(values, mean, std):
    safe_std = np.where(std == 0, 1.0, std)
    return ((values - mean) / safe_std).astype(np.float32)


#hàm chia dữ liệu huấn luyện theo cơ chế chia tách phân tầng theo từng đợt đo
# Tỷ lệ chia: 60% huấn luyện, 20% kiểm định và 20% kiểm tra.
def prepare_data(X, y, groups):
    assignments = {}
    rng = np.random.default_rng(SEED)
    #duyệt từng nhãn 0, 1, 2
    for label in np.unique(y):
        
        # Lấy danh sách phiên đo theo thứ tự ổn định, sau đó xáo trộn có thể lặp lại.
        label_groups = sorted(set(groups[y == label]))
        rng.shuffle(label_groups)
        if len(label_groups) < 5:
            raise ValueError(
                f"Nhãn {label} cần phải có 5 đợt đo độc lập {len(label_groups)}."
            )
        train_end = int(len(label_groups) * 0.60)  # 60% phiên đo để huấn luyện
        val_end = int(len(label_groups) * 0.80)  # 20% phiên đo để kiểm định
        for group in label_groups[:train_end]:
            assignments[group] = "train"
        for group in label_groups[train_end:val_end]:
            assignments[group] = "validation"
        for group in label_groups[val_end:]:
            assignments[group] = "test"

    #ánh xạ để biết từng dòng dwux liệu thuộc tập nào
    split = np.array([assignments[group] for group in groups])
    #Tạo các mảng giá trị True/False tương ứng với từng dòng dữ liệu.
    train_mask, val_mask, test_mask = split == "train", split == "validation", split == "test"
    #lọc trực tiếp bằng mảng boolean
    X_train, y_train = X[train_mask], y[train_mask]
    X_val, y_val = X[val_mask], y[val_mask]
    X_test, y_test = X[test_mask], y[test_mask]

    # Tính trung bình và độ lệch chuẩn chỉ từ tập huấn luyện để tránh rò rỉ dữ liệu.
    mean = X_train.mean(axis=0)
    std = X_train.std(axis=0)
    return {
        #dữa liệu đặc trưng đã chuẩn hoá
        "X_train_norm": _standardize(X_train, mean, std),
        "X_val_norm": _standardize(X_val, mean, std),
        "X_test_norm": _standardize(X_test, mean, std),
        #nhãn tương ứng
        "y_train": y_train,
        "y_val": y_val,
        "y_test": y_test,
        #danh sách các đợt đo theo từng nhóm
        "train_groups": sorted(set(groups[train_mask])),
        "val_groups": sorted(set(groups[val_mask])),
        "test_groups": sorted(set(groups[test_mask])),
        #các tham số tính đc
        "mean": mean,
        "std": std,
    }
