/**
* CÁC THƯ VIỆN HỆ THỐNG
* - Phần cứng kết nối: Arduino.h, Wire.h (I2C/MPU6050),
* WiFi.h và WiFiManager.h (quản lý và kết nối mạng), HTTPClient.h (gửi API lên server),
* Preferences.h (lưu trữ cấu hình lên bộ nhớ Flash).
* - Phần TinyML: TensorFlowLite_ESP32.h và các module bổ trợ
* (interpreter, resolver, error_reporter) để chạy mô hình ML trực tiếp trên chip.
* - Dữ liệu nhúng: model_data.h (trọng số mô hình) và norm_params.h
* (thông số chuẩn hóa dữ liệu đầu vào Mean/Std).
*/
#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <TensorFlowLite_ESP32.h>
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"
#include "tensorflow/lite/micro/micro_error_reporter.h"
#include "model_data.h"
#include "norm_params.h"

//cấu hình các chân phần cứng (BTN = GPIO9 chính là nút BOOT trên board)
const int XANH = 21, VANG = 20, DO = 10, COI = 8, BTN = 9;
//địa chỉ URL API server Django nhận dữ liệu (giá trị mặc định khi chưa cấu hình)
const char* SV_MAC_DINH = "http://192.168.1.187:8000/api/readings/";
//URL đang dùng, có thể bị thay bằng URL đã lưu trong Flash hoặc nhập ở trang cấu hình
String sv = SV_MAC_DINH;
//đối tượng quản lý việc đọc/ghi bộ nhớ Flash
Preferences pref;

/**
* - Cấu hình hướng trục cảm biến MPU6050 (X hướng lên, Y ngang, Z đâm vào trong)
* - zero_r và zero_p lưu trữ gốc cân bằng ban đầu Roll (nghiêng trái/phải), Pitch (nghiêng vào/ra)
*/
const float X_SIGN = 1.0f;
float zero_r = 0.0f, zero_p = 0.0f;

/**
* - Trạng thái hệ thống: da_calib (hiệu chỉnh), loi_hw (lỗi phần cứng), coi_dang_keu (trạng thái còi)
* - che_do_cau_hinh: đang mở trang cấu hình WiFi (đèn chạy đuổi, không đo, không cảnh báo)
* - cau_hinh_xong: cờ do WiFiManager bật lên khi người dùng đã cấu hình và nối WiFi thành công
* - Các biến ghi nhận mốc time: t_doc (thời điểm lần đọc cảm biến/gửi dữ liệu lên server gần nhất),
* t_wifi (thời điểm lần thử nối lại WiFi gần nhất)
* - so_loi_doc: đếm số lần đọc MPU6050 lỗi liên tiếp, vượt MAX_LOI_DOC thì báo lỗi phần cứng
* - Mảng chứa chuỗi các trạng thái
*/
bool da_calib = false, loi_hw = false, coi_dang_keu = false;
bool che_do_cau_hinh = false;
volatile bool cau_hinh_xong = false;
unsigned long t_doc = 0, t_wifi = 0;
int so_loi_doc = 0;
const int MAX_LOI_DOC = 5;
const char* tt_str[] = { "Binh thuong", "Chu y", "Nguy co cao" };

/**
 * - Vùng nhớ đệm (Tensor Arena) 16KB căn chỉnh 16-byte chứa trọng số và
 * dữ liệu trung gian cho mô hình.
 * - Bộ giải quyết toán tử (Op Resolver) khai báo tối đa 3 loại toán tử (layers) mà mô hình sử dụng
 * - Bộ thông dịch (Interpreter) điều phối và thực thi việc chạy suy luận trực tiếp trên chip
 * - Các con trỏ tensor đại diện cho cổng đầu vào (in) và cổng đầu ra (out) của mô hình
 */
alignas(16) uint8_t arena[16 * 1024];
tflite::MicroMutableOpResolver<3> res;
tflite::MicroInterpreter* interp = nullptr;
TfLiteTensor *in = nullptr, *out = nullptr;

/**
 * - Đối tượng WiFiManager dùng chung cho cả lúc bật trang cấu hình
 * - p_sv: ô nhập URL server trên trang cấu hình (tạo 1 lần khi mở trang cấu hình)
 */
WiFiManager wm;
WiFiManagerParameter* p_sv = nullptr;

/**
 * Hàm điều khiển còi hú:
 * - Chỉ gọi lệnh tone() hoặc noTone() KHI TRẠNG THÁI THAY ĐỔI.
 * - Tránh việc gọi lặp lại liên tục trong hàm loop() gây xung đột
 *   hoặc làm gián đoạn âm thanh trên chip ESP32.
 */
void set_coi(bool bat) {
  // Nếu yêu cầu bật còi và hiện tại còi đang tắt
  if (bat && !coi_dang_keu) {
    tone(COI, 3000);         // Phát ra âm thanh ở tần số 3000Hz tại chân COI
    coi_dang_keu = true;     // Đánh dấu trạng thái kêu
  }
  // Nếu yêu cầu tắt còi và hiện tại còi đang kêu
  else if (!bat && coi_dang_keu) {
    noTone(COI);             // Ngừng phát âm thanh tại chân COI
    coi_dang_keu = false;    // Đánh dấu trạng thái tắt
  }
}

/**
 * - Hàm cập nhật trạng thái hiển thị cảnh báo của thiết bị
 * - Mức độ trạng thái (0 = Bình thường, 1 = Chú ý, 2 = Nguy cơ cao)
 */
void set_st(int tt) {
  // Điều khiển các đèn LED sáng tương ứng với mức độ trạng thái:
  // - Nếu tt == 0 (Bình thường): LED XANH bật (true), LED VÀNG và ĐỎ tắt (false)
  // - Nếu tt == 1 (Chú ý): LED VÀNG bật (true), LED XANH và ĐỎ tắt (false)
  // - Nếu tt == 2 (Nguy cơ cao): LED ĐỎ bật (true), LED XANH và VÀNG tắt (false)
  digitalWrite(XANH, tt == 0);
  digitalWrite(VANG, tt == 1);
  digitalWrite(DO, tt == 2);

  set_coi(tt == 2);              // Chỉ bật còi khi ở mức Nguy cơ cao
}

/**
 * Hàm tính tốc độ nghiêng (độ/phút) dựa trên lịch sử các góc đã đo:
 * - Mỗi kênh (roll, pitch, tổng) có một bộ đệm vòng riêng, lưu tối đa RATE_WIN mẫu (góc + thời điểm đo).
 * - Tốc độ = |góc mới nhất - góc cũ nhất trong cửa sổ| / thời gian giữa 2 mẫu đó.
 * - Kết quả luôn không âm, khớp với việc model chỉ học giá trị dương.
 * - LƯU Ý: RATE_WIN phải khớp với cách tính tốc độ lúc train model, nếu lệch thì classify() sẽ ra kết quả sai.
 *
 * ch: Kênh cần tính (0 = roll, 1 = pitch, 2 = tổng); ch < 0 nghĩa là xóa sạch lịch sử
 * angle: Góc hiện tại của kênh đó (độ)
 * return tốc độ nghiêng (độ/phút), trả về 0 nếu chưa đủ dữ liệu
 */
#define RATE_WIN 60   // Cửa sổ tối đa 60 mẫu (~60 giây, vì mỗi giây đo 1 mẫu)

struct RateBuf {
  float a[RATE_WIN];            // Mảng lưu các góc đã đo
  unsigned long t[RATE_WIN];    // Mảng lưu thời điểm (ms) tương ứng với từng góc
  int head = 0, n = 0;          // head: vị trí ghi tiếp theo, n: số mẫu hiện có trong bộ đệm
};
static RateBuf rb[3];           // 3 bộ đệm cho 3 kênh: roll, pitch, tổng

float get_rate(int ch, float angle) {
  // 1. Nếu ch < 0: xóa sạch lịch sử của cả 3 kênh (dùng khi vừa đặt lại mốc 0)
  if (ch < 0) {
    for (int i = 0; i < 3; ++i) { rb[i].head = 0; rb[i].n = 0; }
    return 0;
  }

  // 2. Ghi góc mới và thời điểm hiện tại vào bộ đệm vòng, mẫu cũ nhất sẽ bị ghi đè khi bộ đệm đầy
  RateBuf& b = rb[ch];
  b.a[b.head] = angle;
  b.t[b.head] = millis();
  b.head = (b.head + 1) % RATE_WIN;
  if (b.n < RATE_WIN) b.n++;

  // 3. Chưa đủ 5 mẫu thì chưa tính được tốc độ có ý nghĩa
  if (b.n < 5) return 0;

  // 4. Xác định vị trí mẫu mới nhất và mẫu cũ nhất trong bộ đệm
  int newest = (b.head - 1 + RATE_WIN) % RATE_WIN;
  int oldest = (b.head - b.n + RATE_WIN) % RATE_WIN;

  // 5. Tính khoảng thời gian giữa 2 mẫu (đổi từ ms sang phút), dưới 3 giây thì bỏ qua để tránh chia cho số quá nhỏ
  float dt_min = (b.t[newest] - b.t[oldest]) / 60000.0f;
  if (dt_min < 0.05f) return 0;

  // 6. Tốc độ = độ chênh lệch góc (lấy trị tuyệt đối) chia cho thời gian (phút)
  return fabsf(b.a[newest] - b.a[oldest]) / dt_min;
}

/**
 * Hàm hỗ trợ ghi một giá trị cấu hình vào thanh ghi của cảm biến MPU6050 qua I2C
 * r: Địa chỉ thanh ghi (Register) trên MPU6050 cần ghi tới
 * v: Giá trị dữ liệu (Value) muốn ghi vào thanh ghi đó
 */
void w_i2c(uint8_t r, uint8_t v) {
  Wire.beginTransmission(0x68); // 1. Bắt đầu truyền dữ liệu I2C tới thiết bị có địa chỉ 0x68 (địa chỉ của MPU6050)
  Wire.write(r);                // 2. Gửi địa chỉ thanh ghi (register) cần thao tác vào bộ đệm
  Wire.write(v);                // 3. Gửi giá trị (value) cần ghi vào thanh ghi vừa chọn
  Wire.endTransmission();       // 4. Kết thúc truyền và gửi toàn bộ gói dữ liệu đi qua chân I2C
}

/**
 * Hàm khởi tạo và cấu hình cảm biến gia tốc/gyro MPU6050:
 * - Kiểm tra xem cảm biến có thực sự kết nối với ESP32 qua I2C hay không.
 * - Đánh thức cảm biến và cấu hình bộ lọc chống nhiễu, dải đo.
 *
 * return true nếu khởi tạo thành công, false nếu lỗi phần cứng (không tìm thấy cảm biến).
 */
bool init_mpu() {
  delay(100);                    // Chờ 100ms để ổn định nguồn điện và phần cứng cảm biến sau khi khởi động

  Wire.beginTransmission(0x68);  // Bắt đầu truyền dữ liệu kiểm tra sự hiện diện của MPU6050 (địa chỉ 0x68)

  // Nếu kết nối thất bại (mã trả về khác 0, tức là không có phản hồi từ chip), trả về false
  if (Wire.endTransmission() != 0) return false;

  //CẤU HÌNH CÁC THANH GHI CƠ BẢN CỦA MPU6050
  w_i2c(0x6B, 0);                // Thanh ghi 0x6B (PWR_MGMT_1): Ghi giá trị 0 để đánh thức chip (mặc định MPU6050 ở chế độ ngủ)
  w_i2c(0x1A, 4);                // Thanh ghi 0x1A (CONFIG): Cài đặt bộ lọc thông thấp kỹ thuật số (DLPF) để loại bỏ nhiễu rung động
  w_i2c(0x1C, 0);                // Thanh ghi 0x1C (ACCEL_CONFIG): Cài đặt dải đo gia tốc (mặc định là ±2g)

  return true;                   // Hoàn tất cấu hình suôn sẻ, trả về true
}

/**
 * Hàm hỗ trợ đọc 2 byte liên tiếp từ MPU6050 và ghép thành 1 số nguyên 16-bit có dấu:
 * - Tách từng lệnh Wire.read() ra dòng riêng để đảm bảo byte cao (High) luôn được đọc trước byte thấp (Low).
 * - Nếu viết gộp (Wire.read() << 8) | Wire.read() thì C++ không đảm bảo thứ tự tính toán, có thể đọc ngược byte làm góc nhảy lung tung.
 */
static int16_t rd16() {
  uint8_t h = Wire.read();                // Đọc byte cao (High byte) trước
  uint8_t l = Wire.read();                // Đọc byte thấp (Low byte) sau
  return (int16_t)((h << 8) | l);         // Ghép 2 byte thành số nguyên 16-bit có dấu
}

/**
 * Hàm đọc dữ liệu gia tốc thô từ MPU6050 và tính toán ra góc nghiêng Roll và Pitch
 * roll: Biến tham chiếu (reference) trả về góc nghiêng trái/phải (độ)
 * pitch: Biến tham chiếu trả về góc nghiêng vào/ra (độ)
 * true nếu đọc và tính toán thành công (dữ liệu hợp lệ), false nếu lỗi giao tiếp I2C
 */
bool read_angle(float& roll, float& pitch) {
  Wire.beginTransmission(0x68); // 1. Bắt đầu giao tiếp I2C với MPU6050
  Wire.write(0x3B);             // 2. Gửi địa chỉ thanh ghi bắt đầu đọc dữ liệu gia tốc (ACCEL_XOUT_H)

  // 3. Gửi lệnh yêu cầu dữ liệu (restart truyền động bằng false) và kiểm tra xem có đủ 6 byte trả về từ cảm biến không
  // (Mỗi trục X, Y, Z chiếm 2 byte, tổng cộng 3 trục = 6 byte)
  if (Wire.endTransmission(false) != 0 || Wire.requestFrom(0x68, 6) != 6) return false;

  // 4. Đọc ghép 2 byte cao (High byte) và thấp (Low byte) của từng trục thành số nguyên 16-bit (qua hàm rd16),
  // sau đó chia cho 16384.0f (hệ số nhạy ở dải ±2g) để quy đổi ra đơn vị gia tốc trọng trường (g).
  // Nhân với X_SIGN để có thể đảo chiều trục X nếu lắp ngược hướng phần cứng.
  float ax = rd16() / 16384.0f * X_SIGN;
  float ay = rd16() / 16384.0f;
  float az = rd16() / 16384.0f;

  // 5. Dùng hàm lượng giác atan2f kết hợp gia tốc các trục để tính ra góc nghiêng (đổi từ Radian sang Độ bằng cách nhân 180 / PI)
  roll  = atan2f(ay, ax) * 180.0f / PI; // Tính góc Roll (nghiêng trái/phải)
  pitch = atan2f(az, ax) * 180.0f / PI; // Tính góc Pitch (nghiêng vào/ra)

  // 6. Kiểm tra tính hợp lệ của kết quả (tránh lỗi số vô định NaN hoặc vô cực Infinity) rồi trả về kết quả
  return isfinite(roll) && isfinite(pitch);
}

/**
 * Hàm thiết lập và lưu trữ mốc 0 (góc cân bằng gốc) cho thiết bị:
 * - Cập nhật góc mốc mới cho trục Roll và Pitch.
 * - Lưu trạng thái hiệu chuẩn và giá trị góc vào bộ nhớ Flash (thông qua thư viện Preferences).
 * - Reset lại bộ đếm tốc độ và cập nhật trạng thái đèn LED, còi báo hiệu.
 *
 * r: Góc mốc 0 mới của trục Roll
 * p: Góc mốc 0 mới của trục Pitch
 * c: Trạng thái đã calib hay chưa (mặc định là true)
 */
void set_zero(float r, float p, bool c = true) {
  zero_r = r; zero_p = p; da_calib = c; // Cập nhật giá trị mốc 0 và cờ trạng thái hiệu chuẩn vào biến toàn cục

  // Mở bộ nhớ Flash (vùng nhớ "goc") ở chế độ ghi dữ liệu (false)
  pref.begin("goc", false);
  pref.putBool("c", c);          // Lưu trạng thái đã calib (true/false) vào Flash
  pref.putFloat("zr", r);        // Lưu giá trị góc mốc Roll vào Flash
  pref.putFloat("zp", p);        // Lưu giá trị góc mốc Pitch vào Flash
  pref.end();                    // Đóng bộ nhớ Flash sau khi ghi xong

  get_rate(-1, 0);               // Gọi hàm get_rate với tham số âm để xóa sạch lịch sử tốc độ cũ (làm mới bộ đếm)

  // Cập nhật trạng thái hiển thị đèn LED và còi dựa vào kết quả calib:
  if (c) {
    set_st(0);                   // Nếu đã calib thành công: Đưa về trạng thái bình thường (LED Xanh sáng)
  } else {
    digitalWrite(XANH, 0);       // Nếu chưa calib: Tắt cả 3 LED (hàm loop() sẽ lo việc nhấp nháy cả 3 đèn)
    digitalWrite(VANG, 0);
    digitalWrite(DO, 0);
    set_coi(false);              // Tắt còi báo động (chưa calib thì không cảnh báo)
  }

  // In thông báo chi tiết ra màn hình Serial Monitor để theo dõi
  Serial.printf(">>> MOC 0: roll %.2f | pitch %.2f (calib=%d)\n", r, p, c);
}

/**
 * Hàm hiệu chuẩn (tare) lấy vị trí hiện tại làm mốc 0:
 * - Đọc 20 mẫu liên tiếp (cách nhau 25ms) rồi lấy trung bình để giảm nhiễu.
 * - Chỉ chấp nhận khi có ít nhất 15/20 mẫu đọc thành công, nếu không thì bỏ qua và giữ mốc cũ.
 * - Được gọi khi nhấn nút BOOT 2 lần hoặc khi web gửi tare_command.
 */
void do_tare() {
  float sr = 0, sp = 0; int n = 0;   // sr, sp: tổng góc Roll và Pitch; n: số mẫu đọc thành công
  for (int i = 0; i < 20; ++i) {
    float r, p;
    if (read_angle(r, p)) { sr += r; sp += p; ++n; }   // Đọc thành công thì cộng dồn vào tổng
    delay(25);                                          // Chờ 25ms giữa 2 lần đọc
  }
  if (n >= 15) set_zero(sr / n, sp / n, true);          // Đủ mẫu thì lấy trung bình làm mốc 0 mới
  else Serial.println("Tare that bai: doc cam bien khong du mau");
}

/**
 * Hàm khởi tạo mô hình TinyML để chạy suy luận trực tiếp trên chip:
 * - Nạp mô hình từ g_model (model_data.h) và kiểm tra phiên bản schema có khớp thư viện không.
 * - Đăng ký 3 loại toán tử mà mô hình sử dụng: FullyConnected, ReLU, Softmax.
 * - Cấp phát bộ nhớ cho các tensor từ vùng đệm arena, rồi lấy con trỏ đầu vào/đầu ra.
 *
 * return true nếu khởi tạo thành công, false nếu mô hình lỗi hoặc không đủ bộ nhớ.
 */
bool init_ai() {
  const tflite::Model* m = tflite::GetModel(g_model);                  // 1. Nạp mô hình từ mảng dữ liệu nhúng
  if (!m || m->version() != TFLITE_SCHEMA_VERSION) return false;       // 2. Sai phiên bản schema thì không dùng được
  res.AddFullyConnected(); res.AddRelu(); res.AddSoftmax();            // 3. Khai báo các toán tử (layers) mô hình cần
  static tflite::MicroErrorReporter err;                               // 4. Bộ báo lỗi của TFLite (static để tồn tại suốt chương trình)
  static tflite::MicroInterpreter it(m, res, arena, sizeof(arena), &err); // 5. Tạo bộ thông dịch dùng vùng đệm arena
  interp = &it;
  if (interp->AllocateTensors() != kTfLiteOk) return false;            // 6. Cấp phát tensor, thất bại nếu arena quá nhỏ
  in = interp->input(0); out = interp->output(0);                      // 7. Lấy con trỏ cổng vào/ra của mô hình
  return (in && out);
}

/**
 * Hàm phân loại mức độ nguy hiểm bằng mô hình TinyML:
 * g: Độ lớn góc nghiêng (độ, luôn dương)
 * spd: Tốc độ nghiêng (độ/phút)
 * return 0 (Bình thường), 1 (Chú ý), 2 (Nguy cơ cao), hoặc -1 nếu chạy suy luận lỗi
 */
int classify(float g, float spd) {
  float f[2] = { g, spd };       // Gom 2 đặc trưng đầu vào thành mảng
  for (int i = 0; i < 2; ++i) {
    // 1. Tránh chia cho 0 nếu độ lệch chuẩn bằng 0
    float std_v = (NORM_STD[i] == 0) ? 1.0f : NORM_STD[i];
    // 2. Chuẩn hóa (trừ Mean, chia Std), rồi lượng tử hóa sang int8 theo INPUT_SCALE/INPUT_ZERO_POINT,
    // kẹp trong khoảng -128..127 để không bị tràn số
    in->data.int8[i] = (int8_t)constrain(roundf(((f[i] - NORM_MEAN[i]) / std_v) / INPUT_SCALE + INPUT_ZERO_POINT), -128, 127);
  }
  // 3. Chạy suy luận, lỗi thì trả về -1
  if (interp->Invoke() != kTfLiteOk) return -1;
  // 4. Tìm lớp có điểm số cao nhất trong 3 đầu ra (argmax) và trả về chỉ số của nó
  int b = 0; int8_t mx = out->data.int8[0];
  for (int i = 1; i < 3; ++i) if (out->data.int8[i] > mx) { mx = out->data.int8[i]; b = i; }
  return b;
}

/**
 * Hàm gửi dữ liệu đo được lên server Django và nhận lệnh điều khiển ngược lại:
 * - Gửi gói JSON gồm góc, tốc độ, trạng thái, tình trạng calib qua HTTP POST.
 * - Đọc phản hồi từ server: nếu có tare_command thì hiệu chuẩn lại, reset_tare_command thì xóa mốc 0.
 * - Đặt timeout ngắn (1,5 giây) để khi server tắt thì vòng lặp không bị treo lâu.
 * - Chưa kết nối WiFi thì KHÔNG gửi API, thiết bị vẫn cảnh báo bằng đèn và còi như bình thường.
 *
 * a: Tổng góc nghiêng (độ)
 * r: Tốc độ nghiêng (độ/phút)
 * s: Trạng thái (0, 1, 2; -1 nếu đang chờ hiệu chuẩn)
 * roll, pitch: Góc nghiêng từng trục (độ)
 */
void send_telemetry(float a, float r, int s, float roll, float pitch) {
  if (WiFi.status() != WL_CONNECTED) return;     // 1. Mất WiFi thì bỏ qua, thiết bị vẫn chạy offline
  HTTPClient http;
  if (!http.begin(sv)) return;                   // 2. Mở kết nối tới URL server, lỗi thì thoát
  http.setConnectTimeout(1500);                  // 3. Timeout khi kết nối: 1,5 giây
  http.setTimeout(1500);                         //    Timeout khi chờ phản hồi: 1,5 giây
  http.addHeader("Content-Type", "application/json");
  http.addHeader("X-API-Key", "demo-satlo-2026"); // Khóa API để server xác thực thiết bị

  // 4. Ghép chuỗi JSON chứa toàn bộ dữ liệu cần gửi
  String p = "{\"device_id\":\"esp32c3_node01\",\"tilt_angle_deg\":" + String(a, 2)
           + ",\"roll_deg\":" + String(roll, 2) + ",\"pitch_deg\":" + String(pitch, 2)
           + ",\"tilt_rate_deg_per_min\":" + String(r, 3) + ",\"status\":" + String(s)
           + ",\"is_calibrated\":" + (da_calib ? "true" : "false") + "}";
  int code = http.POST(p);                       // 5. Gửi POST, code > 0 nghĩa là server có phản hồi

  // 6. Kiểm tra phản hồi xem server có gửi lệnh điều khiển không (chấp nhận cả dạng có và không có dấu cách)
  bool c_tare = false, c_rst = false;
  if (code > 0) {
    String resp = http.getString();
    c_tare = (resp.indexOf("tare_command\":true") >= 0 || resp.indexOf("tare_command\": true") >= 0);
    c_rst = (resp.indexOf("reset_tare_command\":true") >= 0 || resp.indexOf("reset_tare_command\": true") >= 0);
  }
  http.end();                                    // 7. Đóng kết nối HTTP

  // 8. Thực hiện lệnh sau khi đã đóng kết nối (do_tare mất khoảng 0,5 giây)
  if (c_tare) do_tare();                         // Lệnh hiệu chuẩn: lấy vị trí hiện tại làm mốc 0
  else if (c_rst) set_zero(0, 0, false);         // Lệnh reset: xóa mốc 0, quay về chờ hiệu chuẩn
}

/**
 * ĐẾM SỐ LẦN NHẤN NÚT BOOT (dùng ngắt để không bỏ sót lần nhấn khi loop() đang bận gửi HTTP):
 * - nut_dem: số lần nhấn hợp lệ đã đếm trong chuỗi hiện tại
 * - nut_t_canh: thời điểm cạnh (nhấn/thả) hợp lệ gần nhất, dùng để chống dội phím và xác định hết chuỗi
 * - nut_xuong: trạng thái nút đã được lọc nhiễu (true = đang nhấn xuống)
 */
const unsigned long NUT_CHONG_DOI_MS = 30;     // Các cạnh xuất hiện trong 30ms được coi là rung phím, bỏ qua
const unsigned long NUT_KHOANG_CACH_MS = 600;  // Thả nút quá 600ms mà không nhấn thêm thì chốt chuỗi
volatile int nut_dem = 0;
volatile unsigned long nut_t_canh = 0;
volatile bool nut_xuong = false;

/**
 * Hàm ngắt chạy mỗi khi chân BTN đổi mức (CHANGE):
 * - Bỏ qua cạnh xuất hiện quá sát cạnh trước (chống dội phím 30ms).
 * - Chỉ tăng bộ đếm khi chuyển từ thả sang nhấn.
 */
void IRAM_ATTR isr_nut() {
  unsigned long now = millis();
  if (now - nut_t_canh < NUT_CHONG_DOI_MS) return;   // 1. Còn trong thời gian rung phím thì bỏ qua
  bool xuong = (digitalRead(BTN) == LOW);            // 2. Đọc mức hiện tại (nhấn xuống = LOW)
  if (xuong == nut_xuong) return;                    // 3. Trạng thái không đổi thì không phải cạnh thật
  nut_t_canh = now;                                  // 4. Ghi nhận cạnh hợp lệ
  nut_xuong = xuong;
  if (xuong) nut_dem++;                              // 5. Mỗi lần nhấn xuống tính là 1 lần
}

/**
 * Hàm lấy kết quả chuỗi nhấn nút (gọi liên tục trong loop()):
 * - Chỉ trả về khi nút đã thả và đã quá NUT_KHOANG_CACH_MS kể từ lần thả cuối (hết chuỗi nhấn).
 * return số lần nhấn của chuỗi vừa kết thúc, 0 nếu chưa có chuỗi nào hoàn tất
 */
int doc_nut() {
  if (nut_dem > 0 && !nut_xuong && millis() - nut_t_canh >= NUT_KHOANG_CACH_MS) {
    noInterrupts();              // Tạm khóa ngắt để đọc và xóa bộ đếm an toàn
    int n = nut_dem;
    nut_dem = 0;
    interrupts();
    return n;
  }
  return 0;
}

/**
 * Hàm callback do WiFiManager gọi khi người dùng đã lưu cấu hình và nối WiFi thành công:
 * - Chỉ bật cờ, việc lưu URL và khởi động lại do loop() xử lý.
 */
void on_cau_hinh_xong() {
  cau_hinh_xong = true;
}

/**
 * Hàm bật chế độ cấu hình WiFi (gọi khi nhấn nút BOOT 3 lần):
 * - Tắt còi, xóa WiFi đã lưu và URL server đã lưu trong Flash.
 * - Mở trang cấu hình "NhaMoHinh-Setup" ở chế độ không chặn (non-blocking) và KHÔNG có timeout.
 * - Trong lúc chờ, loop() chỉ chạy hiệu ứng đèn chạy đuổi và xử lý trang cấu hình.
 */
void bat_cau_hinh() {
  Serial.println(">>> NHAN 3 LAN: xoa WiFi, mo trang cau hinh NhaMoHinh-Setup");
  set_coi(false);                                       // 1. Tắt còi
  digitalWrite(XANH, 0); digitalWrite(VANG, 0); digitalWrite(DO, 0);

  wm.resetSettings();                                   // 2. Xóa thông tin WiFi đã lưu
  pref.begin("goc", false); pref.remove("sv"); pref.end(); // 3. Xóa URL server đã lưu
  sv = SV_MAC_DINH;                                     //    Quay về URL mặc định để điền sẵn vào ô nhập

  // 4. Tạo ô nhập URL server trên trang cấu hình (chỉ tạo 1 lần)
  if (!p_sv) {
    p_sv = new WiFiManagerParameter("server", "Dashboard URL", sv.c_str(), 100);
    wm.addParameter(p_sv);
  }

  wm.setConfigPortalTimeout(0);                         // 5. Không giới hạn thời gian, chờ đến khi cấu hình xong
  wm.setConfigPortalBlocking(false);                    //    Không chặn để loop() còn chạy được hiệu ứng đèn
  wm.setSaveConfigCallback(on_cau_hinh_xong);           //    Cấu hình xong thì bật cờ cau_hinh_xong
  cau_hinh_xong = false;
  wm.startConfigPortal("NhaMoHinh-Setup", "00000000");  // 6. Mở trang cấu hình
  che_do_cau_hinh = true;
}

/**
 * Hàm chạy trong lúc đang ở chế độ cấu hình WiFi (gọi liên tục từ loop()):
 * - Xử lý các yêu cầu từ trang cấu hình (wm.process()).
 * - Đèn chạy đuổi lần lượt Xanh -> Vàng -> Đỏ rồi lặp lại (mỗi đèn sáng 250ms).
 * - Khi cấu hình xong: lưu URL server mới vào Flash rồi khởi động lại, lúc này mới thoát chế độ đèn chạy đuổi.
 */
void chay_cau_hinh() {
  wm.process();                                         // 1. Phục vụ trang web cấu hình

  // 2. Đèn chạy đuổi: mỗi 250ms đổi sang đèn kế tiếp (0 = Xanh, 1 = Vàng, 2 = Đỏ)
  int buoc = (millis() / 250) % 3;
  digitalWrite(XANH, buoc == 0);
  digitalWrite(VANG, buoc == 1);
  digitalWrite(DO, buoc == 2);

  // 3. Cấu hình xong: lưu URL (nếu có nhập) rồi khởi động lại
  if (cau_hinh_xong) {
    if (p_sv && strlen(p_sv->getValue()) > 0) {
      pref.begin("goc", false); pref.putString("sv", String(p_sv->getValue())); pref.end();
    }
    Serial.println(">>> Cau hinh xong, khoi dong lai");
    delay(500);
    ESP.restart();
  }
}

/**
 * Hàm khởi động, chạy 1 lần khi cấp nguồn:
 * - Cấu hình chân I/O, ngắt nút BOOT, khởi tạo I2C, MPU6050 và mô hình AI (lỗi thì đặt cờ loi_hw).
 * - Đọc mốc 0 và URL server đã lưu từ bộ nhớ Flash.
 * - Bắt đầu nối WiFi đã lưu ở chế độ không chặn, KHÔNG mở trang cấu hình, không có WiFi thì chạy offline luôn.
 */
void setup() {
  Serial.begin(115200);
  pinMode(XANH, OUTPUT); pinMode(VANG, OUTPUT); pinMode(DO, OUTPUT); pinMode(COI, OUTPUT);
  pinMode(BTN, INPUT_PULLUP);    // Nút nhấn dùng điện trở kéo lên, nhấn xuống mức LOW
  attachInterrupt(digitalPinToInterrupt(BTN), isr_nut, CHANGE); // Đếm số lần nhấn bằng ngắt
  set_coi(false);                // Đảm bảo còi tắt khi vừa khởi động

  Wire.begin(0, 1);              // Khởi tạo I2C: SDA = GPIO0, SCL = GPIO1
  Wire.setClock(100000);         // Tốc độ I2C 100kHz
  bool mpu_ok = init_mpu();      // Khởi tạo cảm biến
  bool ai_ok = init_ai();        // Khởi tạo mô hình AI
  if (!mpu_ok || !ai_ok) loi_hw = true;   // Lỗi cảm biến hoặc mô hình thì báo lỗi phần cứng (vẫn cho nhấn nút 3 lần cấu hình WiFi)

  // Đọc cấu hình đã lưu trong Flash (mở ở chế độ chỉ đọc)
  pref.begin("goc", true); da_calib = pref.getBool("c", false);
  zero_r = pref.getFloat("zr", 0); zero_p = pref.getFloat("zp", 0);
  if (da_calib && !pref.isKey("zr")) da_calib = false;  // mốc 0 cũ (1 trục) không dùng được, cần tare lại
  String s_url = pref.getString("sv", "");              // URL server đã lưu (nếu có)
  if (s_url.length() > 0) sv = s_url;
  pref.end();
  if (da_calib && !loi_hw) set_st(0);                   // Đã calib: LED Xanh; chưa calib: loop() sẽ nháy cả 3 đèn

  // Nối WiFi đã lưu ở chế độ không chặn: không đợi, không mở trang cấu hình
  // Nối được thì send_telemetry() tự gửi, không nối được thì thiết bị vẫn cảnh báo offline bình thường
  WiFi.mode(WIFI_STA);
  WiFi.begin();                                         // Không tham số: dùng WiFi đã lưu trong Flash
  t_wifi = millis();                                    // Mốc để loop() thử nối lại sau mỗi 30 giây
}

/**
 * Vòng lặp chính, thứ tự ưu tiên từ trên xuống:
 * 1. Đang ở chế độ cấu hình WiFi: chỉ chạy đèn chạy đuổi và trang cấu hình, không làm việc khác.
 * 2. Xử lý nút BOOT: nhấn 3 lần = đặt lại WiFi, nhấn 2 lần = cân bằng góc.
 * 3. Lỗi phần cứng: chớp LED Vàng + Đỏ, tắt còi.
 * 4. Thử nối lại WiFi nếu bị mất (mỗi 30 giây).
 * 5. Chưa hiệu chuẩn: nháy cả 3 đèn, không cảnh báo, gửi dữ liệu thô (nếu có WiFi) và chờ tare.
 * 6. Hoạt động bình thường: mỗi giây đọc góc, tính tốc độ, phân loại bằng AI, hiển thị (có WiFi thì gửi lên server).
 */
void loop() {
  // 1. CHẾ ĐỘ CẤU HÌNH WIFI: đèn chạy đuổi cho đến khi cấu hình xong và khởi động lại
  if (che_do_cau_hinh) {
    chay_cau_hinh();
    return;
  }

  // 2. XỬ LÝ NÚT BOOT theo số lần nhấn
  int so_lan = doc_nut();
  if (so_lan == 3) {                                    // Nhấn 3 lần: đặt lại WiFi và mở trang cấu hình
    bat_cau_hinh();
    return;
  }
  if (so_lan == 2 && !loi_hw) {                         // Nhấn 2 lần: cân bằng góc (lấy vị trí hiện tại làm mốc 0)
    Serial.println(">>> NHAN 2 LAN: can bang goc");
    do_tare();
  }

  // 3. LỖI PHẦN CỨNG: chớp LED Vàng và Đỏ cùng lúc (chu kỳ 300ms), tắt LED Xanh và còi
  if (loi_hw) {
    set_coi(false);
    digitalWrite(XANH, 0);
    digitalWrite(VANG, (millis()/300)%2); digitalWrite(DO, (millis()/300)%2);
    return;
  }

  // 4. THỬ NỐI LẠI WIFI mỗi 30 giây nếu đang mất kết nối
  if (WiFi.status() != WL_CONNECTED && millis() - t_wifi >= 30000) {
    t_wifi = millis();
    WiFi.reconnect();
  }

  // 5. CHƯA HIỆU CHUẨN: nháy cả 3 đèn cùng lúc (chu kỳ 500ms), không chạy AI, không cảnh báo, còi tắt
  // Mỗi giây gửi góc thô lên web (nếu có WiFi) để chờ lệnh tare từ web
  if (!da_calib) {
    set_coi(false);
    digitalWrite(XANH, (millis()/500)%2);
    digitalWrite(VANG, (millis()/500)%2);
    digitalWrite(DO, (millis()/500)%2);
    if (millis() - t_doc >= 1000) {
      t_doc = millis(); float r = 0, p = 0; read_angle(r, p);
      send_telemetry(sqrtf(r * r + p * p), 0, -1, r, p);   // status = -1: chưa có trạng thái
      Serial.printf("Dang cho can bang goc (nhan BOOT 2 lan) | roll %.2f | pitch %.2f\n", r, p);
    }
    return;
  }

  // 6. HOẠT ĐỘNG BÌNH THƯỜNG: chạy 1 lần mỗi giây
  if (millis() - t_doc >= 1000) {
    t_doc = millis();

    // 6.1. Đọc góc, nếu lỗi thì đếm số lần lỗi liên tiếp, đủ MAX_LOI_DOC lần thì báo lỗi phần cứng
    float r, p;
    if (!read_angle(r, p)) {
      if (++so_loi_doc >= MAX_LOI_DOC) loi_hw = true;
      return;
    }
    so_loi_doc = 0;                                     // Đọc thành công thì xóa bộ đếm lỗi

    // 6.2. Trừ mốc 0 để ra góc lệch so với vị trí cân bằng, rồi tính tổng góc lệch (Pythagoras)
    float gr = r - zero_r, gp = p - zero_p;
    float gt = sqrtf(gr * gr + gp * gp);

    // 6.3. Tính tốc độ nghiêng cho 3 kênh: roll, pitch, tổng
    float sr = get_rate(0, gr), sp = get_rate(1, gp), st = get_rate(2, gt);

    // 6.4. Phân loại từng kênh bằng AI
    // Model chi hoc goc duong (0..90 do) nen dua do lon goc vao, khong dua goc co dau
    int t1 = classify(fabsf(gr), sr), t2 = classify(fabsf(gp), sp), t3 = classify(gt, st);
    if (t1 < 0 || t2 < 0 || t3 < 0) return;             // Có kênh lỗi suy luận thì bỏ qua lần này

    // 6.5. Lấy mức nguy hiểm cao nhất trong 3 kênh và tốc độ lớn nhất, rồi cập nhật đèn + còi
    // (cảnh báo này chạy độc lập với WiFi nên offline vẫn cảnh báo)
    int tt = max(t1, max(t2, t3));
    float spd = max(sr, max(sp, st));
    set_st(tt);

    // 6.6. In ra Serial Monitor: kèm hướng nghiêng (lệch dưới 0.5 độ coi như không nghiêng)
    const char* h_r = fabsf(gr) < 0.5f ? "-" : (gr > 0 ? "sang +Y" : "sang -Y");
    const char* h_p = fabsf(gp) < 0.5f ? "-" : (gp > 0 ? "vao trong" : "ra ngoai");
    Serial.printf("Roll %.2f (%s) | Pitch %.2f (%s) | Tong %.2f | Toc do %.2f do/phut | TT: %s | WiFi: %s\n",
                  gr, h_r, gp, h_p, gt, spd, tt_str[tt],
                  WiFi.status() == WL_CONNECTED ? "online" : "offline");

    // 6.7. Gửi dữ liệu lên server (chỉ khi có WiFi, có thể nhận lệnh tare/reset từ web)
    send_telemetry(gt, spd, tt, gr, gp);
  }
}