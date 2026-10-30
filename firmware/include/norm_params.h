#pragma once
// Tao boi tinyml/train_tinyml.py. Giu nguyen thu tu dac trung.
// Thu tu dau vao: goc nghieng nha (do), toc do doi goc (do/phut).
const int FEATURE_COUNT = 2;
const float NORM_MEAN[2] = {3.645876f, 0.214062f};
const float NORM_STD[2] = {3.619054f, 0.191106f};
const float INPUT_SCALE = 0.0107770581f;
const int INPUT_ZERO_POINT = -24;
// Lop: 0=nha on dinh, 1=chu y do nghieng, 2=nguy co cao trong mo hinh.
