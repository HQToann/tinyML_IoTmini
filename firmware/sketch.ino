#include <Arduino.h>
#include <Wire.h>
#include <math.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <TensorFlowLite.h>
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"
#include "tensorflow/lite/schema/schema_generated.h"
#include "include/model_data.h"
#include "include/norm_params.h"

// Chân nối theo diagram.json dành cho ESP32-C3.
constexpr int CHAN_SDA = 8;
constexpr int CHAN_SCL = 9;
constexpr int CHAN_COI = 3;
constexpr int CHAN_DEN_XANH = 5;
constexpr int CHAN_DEN_VANG = 6;
constexpr int CHAN_DEN_DO = 7;

// Dataset lấy mẫu mỗi phút nên tốc độ cũng được tính trên cửa sổ một phút.
constexpr unsigned long CHU_KY_DOC_MS = 1000UL;
constexpr unsigned long CUA_SO_TOC_DO_MS = 60000UL;

// Bộ nhớ tạm cho TensorFlow Lite Micro.
alignas(16) uint8_t vung_nho_tensor[16 * 1024];

Adafruit_MPU6050 mpu;
tflite::MicroMutableOpResolver<3> bo_toan_tu;
const tflite::Model* mo_hinh = nullptr;
tflite::MicroInterpreter* bo_chay_model = nullptr;
TfLiteTensor* dau_vao = nullptr;
TfLiteTensor* dau_ra = nullptr;

float goc_moc_toc_do = 0.0f;
float toc_do_doi_goc = 0.0f;
unsigned long moc_toc_do_ms = 0;
unsigned long lan_doc_truoc_ms = 0;
bool da_co_moc_toc_do = false;
bool he_thong_san_sang = false;

const char* ten_trang_thai[] = {
  "Binh thuong",
  "Chu y",
  "Nguy co cao trong mo hinh"
};

bool khoi_tao_model() {
  mo_hinh = tflite::GetModel(g_model);
  if (mo_hinh == nullptr) {
    Serial.println("Khong doc duoc model trong model_data.h");
    return false;
  }

  // Model MLP gom cac phep toan FullyConnected, Relu va Softmax.
  if (bo_toan_tu.AddFullyConnected() != kTfLiteOk ||
      bo_toan_tu.AddRelu() != kTfLiteOk ||
      bo_toan_tu.AddSoftmax() != kTfLiteOk) {
    Serial.println("Khong dang ky duoc cac phep toan TinyML");
    return false;
  }

  static tflite::MicroInterpreter bo_chay_tinh(
      mo_hinh, bo_toan_tu, vung_nho_tensor, sizeof(vung_nho_tensor));
  bo_chay_model = &bo_chay_tinh;

  if (bo_chay_model->AllocateTensors() != kTfLiteOk) {
    Serial.println("Khong du bo nho de nap model TinyML");
    return false;
  }

  dau_vao = bo_chay_model->input(0);
  dau_ra = bo_chay_model->output(0);
  if (dau_vao == nullptr || dau_ra == nullptr ||
      dau_vao->type != kTfLiteInt8 || dau_ra->type != kTfLiteInt8 ||
      dau_vao->dims->data[dau_vao->dims->size - 1] != FEATURE_COUNT) {
    Serial.println("Kich thuoc hoac kieu tensor khong khop model INT8");
    return false;
  }
  return true;
}

bool doc_goc_nghieng(float& goc_deg) {
  sensors_event_t su_kien_gia_toc;
  sensors_event_t su_kien_gyro;
  sensors_event_t su_kien_nhiet_do;
  mpu.getEvent(&su_kien_gia_toc, &su_kien_gyro, &su_kien_nhiet_do);

  // Gia toc theo m/s^2 cung dung duoc: cung he so tren ax, ay, az se tu rut gon.
  const float ax = su_kien_gia_toc.acceleration.x;
  const float ay = su_kien_gia_toc.acceleration.y;
  const float az = su_kien_gia_toc.acceleration.z;

  // Goc nghieng tong hop theo cong thuc accelerometer da thong nhat.
  const float gia_toc_ngang = sqrtf(ax * ax + ay * ay);
  goc_deg = atan2f(gia_toc_ngang, fabsf(az)) * 180.0f / PI;
  return isfinite(goc_deg);
}

void cap_nhat_toc_do(float goc_hien_tai, unsigned long hien_tai_ms) {
  if (!da_co_moc_toc_do) {
    goc_moc_toc_do = goc_hien_tai;
    moc_toc_do_ms = hien_tai_ms;
    da_co_moc_toc_do = true;
    toc_do_doi_goc = 0.0f;
    return;
  }

  const unsigned long thoi_gian_da_qua = hien_tai_ms - moc_toc_do_ms;
  if (thoi_gian_da_qua >= CUA_SO_TOC_DO_MS) {
    const float so_phut = thoi_gian_da_qua / 60000.0f;
    if (so_phut > 0.0f) {
      toc_do_doi_goc = fabsf(goc_hien_tai - goc_moc_toc_do) / so_phut;
    }
    goc_moc_toc_do = goc_hien_tai;
    moc_toc_do_ms = hien_tai_ms;
  }
}

int phan_loai(float goc_deg, float toc_do_deg_moi_phut) {
  // Thu tu nay phai trung voi FEATURES trong config.py.
  const float dac_trung[FEATURE_COUNT] = {
    goc_deg,
    toc_do_deg_moi_phut
  };

  for (int i = 0; i < FEATURE_COUNT; ++i) {
    const float do_lech_chuan = (NORM_STD[i] == 0.0f) ? 1.0f : NORM_STD[i];
    const float da_chuan_hoa = (dac_trung[i] - NORM_MEAN[i]) / do_lech_chuan;
    const float da_luong_tu = roundf(da_chuan_hoa / INPUT_SCALE + INPUT_ZERO_POINT);
    dau_vao->data.int8[i] = (int8_t)constrain(da_luong_tu, -128.0f, 127.0f);
  }

  if (bo_chay_model->Invoke() != kTfLiteOk) {
    Serial.println("Loi khi chay model TinyML");
    return -1;
  }

  int lop_tot_nhat = 0;
  int8_t diem_cao_nhat = dau_ra->data.int8[0];
  for (int i = 1; i < 3; ++i) {
    if (dau_ra->data.int8[i] > diem_cao_nhat) {
      diem_cao_nhat = dau_ra->data.int8[i];
      lop_tot_nhat = i;
    }
  }
  return lop_tot_nhat;
}

void cap_nhat_den_coi(int trang_thai) {
  digitalWrite(CHAN_DEN_XANH, trang_thai == 0 ? HIGH : LOW);
  digitalWrite(CHAN_DEN_VANG, trang_thai == 1 ? HIGH : LOW);
  digitalWrite(CHAN_DEN_DO, trang_thai == 2 ? HIGH : LOW);

  if (trang_thai == 2) {
    tone(CHAN_COI, 2000);
  } else {
    noTone(CHAN_COI);
  }
}

void setup() {
  Serial.begin(115200);
  delay(300);

  pinMode(CHAN_DEN_XANH, OUTPUT);
  pinMode(CHAN_DEN_VANG, OUTPUT);
  pinMode(CHAN_DEN_DO, OUTPUT);
  pinMode(CHAN_COI, OUTPUT);
  cap_nhat_den_coi(0);

  Wire.begin(CHAN_SDA, CHAN_SCL);
  Wire.setClock(400000);
  if (!mpu.begin(0x68, &Wire)) {
    Serial.println("Khong tim thay MPU6050; kiem tra SDA/SCL va nguon.");
    return;
  }
  mpu.setAccelerometerRange(MPU6050_RANGE_2_G);
  mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);

  if (!khoi_tao_model()) {
    return;
  }

  he_thong_san_sang = true;
  Serial.println("Da khoi tao MPU6050 va model INT8.");
  Serial.println("Dau vao model: goc nghieng (do), toc do (do/phut).");
}

void loop() {
  if (!he_thong_san_sang) {
    delay(1000);
    return;
  }

  const unsigned long hien_tai_ms = millis();
  if (hien_tai_ms - lan_doc_truoc_ms < CHU_KY_DOC_MS) {
    delay(5);
    return;
  }
  lan_doc_truoc_ms = hien_tai_ms;

  float goc_deg = 0.0f;
  if (!doc_goc_nghieng(goc_deg)) {
    Serial.println("Gia tri cam bien khong hop le.");
    return;
  }

  cap_nhat_toc_do(goc_deg, hien_tai_ms);
  const int trang_thai = phan_loai(goc_deg, toc_do_doi_goc);
  if (trang_thai < 0) {
    return;
  }

  cap_nhat_den_coi(trang_thai);
  Serial.print("Goc: ");
  Serial.print(goc_deg, 2);
  Serial.print(" do | Toc do: ");
  Serial.print(toc_do_doi_goc, 3);
  Serial.print(" do/phut | TinyML: ");
  Serial.println(ten_trang_thai[trang_thai]);
}
