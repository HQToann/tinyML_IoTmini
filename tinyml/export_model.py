"""Buoc 4: luong tu hoa, danh gia va xuat mo hinh cho firmware ESP32."""

from pathlib import Path
import tempfile

import numpy as np
import tensorflow as tf
from sklearn.metrics import classification_report, confusion_matrix

from config import CLASS_NAMES, FEATURES, FIRMWARE_INCLUDE_DIR, MODEL_DIR


def _write_model_header(model_bytes: bytes, output_path: Path):
    lines = [
        "#pragma once",
        "// Tao boi tinyml/train_tinyml.py. Khong sua truc tiep.",
        "alignas(8) const unsigned char g_model[] = {",
    ]
    for offset in range(0, len(model_bytes), 12):
        chunk = model_bytes[offset : offset + 12]
        lines.append("  " + ", ".join(f"0x{byte:02x}" for byte in chunk) + ",")
    lines.extend(["};", f"const unsigned int g_model_len = {len(model_bytes)};", ""])
    output_path.write_text("\n".join(lines), encoding="utf-8")


def export_and_evaluate(model, data):
    MODEL_DIR.mkdir(parents=True, exist_ok=True)
    FIRMWARE_INCLUDE_DIR.mkdir(parents=True, exist_ok=True)

    def representative_dataset():
        samples = data["X_train"][: min(300, len(data["X_train"]))]
        for row in samples:
            yield [row.reshape(1, -1).astype(np.float32)]

    # Xuat SavedModel truoc de tranh loi tuong thich Keras moi voi
    # TFLiteConverter.from_keras_model tren mot so ban TensorFlow.
    with tempfile.TemporaryDirectory(prefix="saved_model_", dir=MODEL_DIR) as temp_dir:
        saved_model_path = Path(temp_dir) / "model"
        model.export(str(saved_model_path))
        converter = tf.lite.TFLiteConverter.from_saved_model(str(saved_model_path))
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
        "// Tao boi tinyml/train_tinyml.py. Giu nguyen thu tu dac trung.",
        "// Thu tu dau vao: goc nghieng nha (do), toc do doi goc (do/phut).",
        f"const int FEATURE_COUNT = {len(FEATURES)};",
        f"const float NORM_MEAN[{len(FEATURES)}] = {{" + ", ".join(f"{v:.6f}f" for v in mean) + "};",
        f"const float NORM_STD[{len(FEATURES)}] = {{" + ", ".join(f"{v:.6f}f" for v in std) + "};",
        f"const float INPUT_SCALE = {input_scale:.10f}f;",
        f"const int INPUT_ZERO_POINT = {input_zero_point};",
        "// Lop: 0=nha on dinh, 1=chu y do nghieng, 2=nguy co cao trong mo hinh.",
        "",
    ]
    norm_path = FIRMWARE_INCLUDE_DIR / "norm_params.h"
    norm_path.write_text("\n".join(norm_lines), encoding="utf-8")

    predictions = []
    for row in data["X_test"]:
        quantized = np.clip(
            np.round(row / input_scale + input_zero_point), -128, 127
        ).astype(np.int8)
        interpreter.set_tensor(input_info["index"], quantized.reshape(1, -1))
        interpreter.invoke()
        scores = interpreter.get_tensor(output_info["index"])[0]
        predictions.append(int(np.argmax(scores)))

    print(f"Kich thuoc mo hinh int8: {len(model_bytes)} byte")
    print("Bao cao phan loai tren tap kiem tra:")
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
    print("Ma tran nham lan:")
    print(confusion_matrix(data["y_test"], predictions, labels=[0, 1, 2]))
    print("Da xuat cac tep:")
    print(f"  {model_path}")
    print(f"  {FIRMWARE_INCLUDE_DIR / 'model_data.h'}")
    print(f"  {norm_path}")

