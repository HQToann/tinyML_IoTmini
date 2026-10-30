# Dataset goc nghieng mo hinh nha

`data.csv` la du lieu mo phong co nhan cho do an TinyML. Ba cot `ax_g`, `ay_g`,
`az_g` mo phong thanh phan gia toc do trong luc theo don vi g. Dataset bao gom
nhieu huong nghieng quanh mat phang ngang, khong chi gia su nha nghieng tren truc X.

Pipeline tinh lai goc tu ba truc theo cung cong thuc voi firmware:

```text
goc_deg = atan2(sqrt(ax_g^2 + ay_g^2), abs(az_g)) * 180 / pi
```

Goc nay la do lon nghieng tong hop trong khoang 0..90 do. No bo qua huong nghieng
va khong phan biet nghieng ve phia nao. Toc do duoc tinh tu hai mau lien tiep:
`abs(goc_moi - goc_truoc) / thoi_gian_phut`.

| Nhan | Y nghia trong mo hinh demo |
| --- | --- |
| `0` | Nha mo hinh on dinh, goc nho va it thay doi. |
| `1` | Can chu y, goc tang tu tu. |
| `2` | Nguy co cao trong mo hinh, goc lon hon hoac tang nhanh hon. |

| Cot | Y nghia |
| --- | --- |
| `run_id` | Ma phien kich ban; tat ca mau cua mot phien nam cung mot tap. |
| `minute` | Thoi diem mau, don vi phut. |
| `ax_g`, `ay_g`, `az_g` | Gia toc do trong mo phong theo ba truc, don vi g. |
| `tilt_angle_deg` | Goc tinh tu ba truc bang cong thuc tren. |
| `tilt_rate_deg_per_min` | Toc do doi goc, do/phut. |
| `label` | Nhan 0, 1 hoac 2. |

CSV la du lieu da tao san; huan luyen va xuat model bang `python train_tinyml.py`. Model van nhan hai dac trung theo thu tu:
`tilt_angle_deg`, `tilt_rate_deg_per_min`. Khi thay doi tap du lieu, chay lai
huan luyen de cap nhat `model_int8.tflite`, `model_data.h` va `norm_params.h`.

Day la du lieu tong hop de trinh dien quy trinh, khong phai so lieu thu tu nha
that va khong the xac nhan an toan cong trinh. Khi lay mau MPU6050 that, can giu
cung quy uoc truc: cam bien gan sao cho truc Z thang dung luc nha can bang; tinh
goc tu ax/ay/az bang cong thuc tren; tinh rate bang thoi gian thuc giua hai mau.
Can hieu chuan offset va xac minh cach lap cam bien truoc khi so sanh ket qua.

