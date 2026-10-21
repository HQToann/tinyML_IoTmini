#Bước 4 chuyển đổi mô hình thành mô hình số nguyên int8 để chạy trên ESP32

from pathlib import Path

import numpy as np
import tensorflow as tf
from sklearn.metrics import classification_report, confusion_matrix

from config import CLASS_NAMES, FEATURES, FIRMWARE_INCLUDE_DIR, MODEL_DIR

#chuyển đổi mô hình nhận chuyển byte chuyển thành mảng hằng số c++
def _write_model_header(model_bytes: bytes, output_path: Path):
    lines = [
        #lệnh tiền xử lsy giúp file chỉ include 1 lần
        "#pragma once",
        #khai báo mảng byte g_model 
        "alignas(8) const unsigned char g_model[] = {",
    ]
    #cắt mảng thành các đoạn 12 byte
    for offset in range(0, len(model_bytes), 12):
        chunk = model_bytes[offset : offset + 12]
        lines.append("  " + ", ".join(f"0x{byte:02x}" for byte in chunk) + ",")
    lines.extend(["};", f"const unsigned int g_model_len = {len(model_bytes)};", ""])
    output_path.write_text("\n".join(lines), encoding="utf-8")


def export_and_evaluate(model, data):
    MODEL_DIR.mkdir(parents=True, exist_ok=True)
    FIRMWARE_INCLUDE_DIR.mkdir(parents=True, exist_ok=True)

    def representative_dataset():
        samples = data["X_train_norm"]
        for row in samples:
            yield [row.reshape(1, -1).astype(np.float32)]

    converter = tf.lite.TFLiteConverter.from_keras_model(model)
    converter.optimizations = [tf.lite.Optimize.DEFAULT]
    converter.representative_dataset = representative_dataset
    converter.target_spec.supported_ops = [tf.lite.OpsSet.TFLITE_BUILTINS_INT8]
    converter.inference_input_type = tf.int8
    converter.inference_output_type = tf.int8
    model_bytes = converter.convert()

    model_path = MODEL_DIR / "model_int8.tflite"
    model_path.write_bytes(model_bytes)
    _write_model_header(model_bytes, FIRMWARE_INCLUDE_DIR / "model_data.h")

    interpreter = tf.lite.Interpreter(model_content=model_bytes)
    interpreter.allocate_tensors()
    input_info = interpreter.get_input_details()[0]
    output_info = interpreter.get_output_details()[0]
    input_scale, input_zero_point = input_info["quantization"]
    if input_scale == 0:
        raise ValueError("Model input quantization scale is zero.")

    mean = data["mean"]
    std = data["std"]
    norm_lines = [
        "#pragma once",
        "// Tạo bởi tinyml/train_tinyml.py. Giữ nguyên thứ tự các đặc trưng.",
        "// Thứ tự đầu vào: độ ẩm đất (%), góc nghiêng (độ), tốc độ đổi góc (độ/phút).",
        "const float NORM_MEAN[3] = {" + ", ".join(f"{v:.6f}f" for v in mean) + "};",
        "const float NORM_STD[3] = {" + ", ".join(f"{v:.6f}f" for v in std) + "};",
        f"const float INPUT_SCALE = {input_scale:.10f}f;",
        f"const int INPUT_ZERO_POINT = {input_zero_point};",
        "// Các lớp: 0=bình thường, 1=chú ý, 2=nguy cơ cao trong mô hình.",
        "",
    ]
    norm_path = FIRMWARE_INCLUDE_DIR / "norm_params.h"
    norm_path.write_text("\n".join(norm_lines), encoding="utf-8")

    predictions = []
    for row in data["X_test_norm"]:
        quantized = np.clip(
            np.round(row / input_scale + input_zero_point), -128, 127
        ).astype(np.int8)
        interpreter.set_tensor(input_info["index"], quantized.reshape(1, -1))
        interpreter.invoke()
        scores = interpreter.get_tensor(output_info["index"])[0]
        predictions.append(int(np.argmax(scores)))

    print(f"Int8 model size: {len(model_bytes)} bytes")
    print("Classification report (test rows from the supplied CSV):")
    print(
        classification_report(
            data["y_test"],
            predictions,
            labels=[0, 1, 2],
            target_names=CLASS_NAMES,
            zero_division=0,
            digits=3,
        )
    )
    print("Confusion matrix:")
    print(confusion_matrix(data["y_test"], predictions, labels=[0, 1, 2]))
    print("Exported:")
    print(f"  {model_path}")
    print(f"  {FIRMWARE_INCLUDE_DIR / 'model_data.h'}")
    print(f"  {norm_path}")


