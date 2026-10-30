# Cap nguon nang luong mat troi

## So do nguon cho phan cung that

```text
Tam pin mat troi 6 V
        |
        v
Mach sac solar cho pin Li-ion 1 cell
(co bao ve pin va dau ra cap tai/power-path)
        |
        +---- Pin Li-ion 3.7 V co mach bao ve
        |
        v
Bo tang ap DC-DC len 5 V, dong ra du phong toi thieu 1 A
        |
        +---- 5V va GND tren ESP32-C3 DevKitM-1
                    |
                    +---- Chan 3V3 cua ESP32 cap cho MPU6050
                    +---- GPIO cap cho LED va buzzer theo diagram.json

Noi chung GND giua bo tang ap, ESP32 va cac linh kien.
```

## Cach dau

1. Tam pin mat troi noi vao dau vao tam pin cua mach sac solar, khong noi thang vao ESP32.
2. Pin Li-ion noi vao dau pin cua mach sac. Chon pin va mach sac cung loai, dung dien ap; uu tien pin co bao ve qua sac, qua xa va ngan mach.
3. Dau ra tai cua mach sac cap cho bo tang ap 5 V. Neu mach sac co dau `LOAD`/power-path thi dung dau nay; bo sac phai duoc thiet ke cho cap tai dong thoi sac pin.
4. Dau ra 5 V da on dinh noi vao chan `5V` va `GND` cua ESP32-C3 DevKitM-1. MPU6050 tiep tuc dung 3V3 tu ESP32; khong dua 5 V vao cac chan logic cua cam bien.
5. Khi lap that, khong cap USB va nguon ngoai dong thoi. Espressif liet ke USB, chan 5V/GND va chan 3V3/GND la cac cach cap nguon rieng biet; chi chon mot cach tai mot thoi diem.

## Trong Wokwi

`diagram.json` mo phong ESP32-C3, MPU6050, LED va buzzer. Wokwi hien khong liet ke tam pin mat troi hay bo sac pin trong danh sach linh kien mo phong, nen phan nguon mat troi khong the chay mo phong dien that. Khi mo phong, dung nguon ao/USB cua Wokwi; khi lap phan cung that, bo sung chuoi tam pin -> mach sac solar -> pin -> bo tang ap 5 V nhu so do tren.

## Luu y cho do an

- Mach sac phai ghi ro ho tro pin Li-ion 1 cell va dau vao tam pin mat troi; khong thay bang mach sac bat ky chi vi cung co dau USB.
- Bo tang ap can giu duoc 5 V khi Wi-Fi phat dong va khi buzzer bat. Pin la nguon dem cho cac dinh dong tuc thoi; tam pin sac lai pin khi co nang.
- Sketch khong can thay cong thuc TinyML khi doi sang nguon mat troi. Hien sketch chua doc dien ap pin, chua co che do ngu tiet kiem va chua gui telemetery ve dashboard.
- He thong nay la mo hinh do an, khong phai thiet bi canh bao an toan cho cong trinh that.
