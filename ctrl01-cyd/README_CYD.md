# CTRL 01 — ESP32-CYD (ESP32-2432S028R, ILI9341)

Bản chuyển từ LilyGO T-Display sang CYD: màn hình ngang 320×240,
cảm ứng XPT2046, LED RGB trên board và nút BOOT. Tên Bluetooth giữ là
**CTRL 01**; thiết bị vẫn dùng Apple Media Service của project gốc.

## Build và nạp

Mở **thư mục project chứa platformio.ini** trong VS Code / PlatformIO,
chọn environment **esp32-cyd**, rồi Build / Upload.

```powershell
pio run -e esp32-cyd
pio run -e esp32-cyd -t upload
pio device monitor -b 115200
```

Nếu cần chọn cổng, thêm `--upload-port COMx` vào lệnh upload.
Platform được cố định ở espressif32 6.12.0 / Arduino ESP32 2.0.17 để
tương thích mã BLE và TFT_eSPI có sẵn trong project. Board: esp32dev,
flash 4 MB, partition Huge APP, không dùng PSRAM. PlatformIO tự tải
OneButton 2.6.2 và XPT2046 v1.4 từ kho chính thức.

Cấu hình TFT được áp dụng cho cả thư viện qua `-include include/cyd_tft_setup.h`;
không cần chỉnh TFT_eSPI trong thư viện Arduino hay thư mục `lib`.
Sprite 8 bit dùng khoảng 76,8 KB heap để giảm RAM dành cho màn hình.
Số RAM trong báo cáo linker chưa bao gồm sprite, BLE và stack cấp phát lúc chạy.

## Điều khiển

- Cảm ứng: **PREV / PLAY hoặc PAUSE / NEXT / VOL− / VOL+**.
- BOOT: nhấn một lần để phát/dừng, hai lần để chuyển bài tiếp,
  ba lần về bài trước; giữ để giảm âm lượng.
- Sau 30 giây không thao tác: tắt màn hình và LED. Chạm hoặc nhấn BOOT
  để bật lại; lần chạm đánh thức màn hình không gửi lệnh cảm ứng.
- Sau 3 phút không thao tác, khi không phát nhạc hoặc mất kết nối:
  deep sleep. Nhấn BOOT để đánh thức; cảm ứng không đánh thức deep sleep.

LED: xanh dương nhấp nháy khi chưa kết nối, xanh dương khi đang phát,
xanh lá khi đã kết nối và không phát. GPIO15 chỉ dùng cho TFT CS.
GPIO34 là cảm biến ánh sáng của CYD, nên module đo pin TTGO được loại
khỏi build. Giao diện hiển thị USB, không hiển thị điện áp giả.
GPIO35 không dùng làm nút hoặc nguồn đánh thức vì trên CYD đây là chân
input rời, không có pull-up nội.

## Giao diện nghe nhạc

Giao diện dùng nền tối, khung bài hát xanh navy và điểm nhấn cyan/hồng/tím.
Header hiển thị trạng thái kết nối và âm lượng iPhone; thanh tiến độ có thời
gian đã phát/tổng thời lượng. Nút có icon và nhãn, đổi màu khi giữ cảm ứng
và sáng thêm khoảng 550 ms sau khi thả. BOOT cũng làm nút tương ứng sáng.
Phản hồi màu báo thao tác trên thiết bị, không phải xác nhận lệnh từ iPhone.
Tên bài dài dừng khoảng 1,2 giây trước khi cuộn; font tiếng Việt và dữ liệu
calib NVS tiếp tục được sử dụng. Đường xuất màn hình vẫn là SPI trực tiếp 10 MHz.

## Xuất hình và rotation

Phần vẽ và font vẫn dùng sprite 8 bit; phần gửi ảnh ra ILI9341 dùng
`CydPanel`, chuyển sang RGB565 và gửi từng dòng bằng SPI 10 MHz.
Không dùng đường xuất `pushSprite` của TFT_eSPI 2.2.20.
Đây là thay đổi xử lý hiện tượng sọc/xáo trộn hình cần xác nhận trên board.
Rotation 0/2 dùng 240×320; rotation 1/3 dùng 320×240.
Serial lúc khởi động bản mới in `CYD RAW SPI 10MHz RGB565` cùng rotation
và kích thước để xác nhận đúng firmware đã được nạp.

## Hiệu chỉnh cảm ứng

Chương trình một file nằm ở `src/touch_calibration.cpp`.
Trong PlatformIO chọn **esp32-cyd-calibration** rồi Upload; hoặc:

```powershell
pio run -e esp32-cyd-calibration -t upload
```

Mở Serial Monitor 115200. Dùng bút cảm ứng giữ vào tâm dấu cộng trắng
ở lần lượt bốn góc. Khi hiện **OK - RELEASE NOW**, nhấc bút để chuyển
điểm tiếp theo. Sau bốn góc, chạm dấu cộng ở giữa để kiểm tra độ chính xác.
Nếu timeout hoặc tiếp xúc không ổn định, chương trình không lưu kết quả sai;
nhấn BOOT hoặc gửi ký tự R qua Serial để hiệu chỉnh lại.

Khi hiện **CALIBRATION SAVED**, thử cả năm nút bên dưới. Nút đúng vị trí
sẽ sáng xanh lá. Hệ số được in ra Serial và lưu NVS namespace `cyd-touch`.
Ánh xạ affine xử lý cả hoán đổi trục và đảo chiều, không chỉ min/max.
Sau đó nạp lại ứng dụng nhạc:

```powershell
pio run -e esp32-cyd -t upload
```

Ứng dụng đọc hệ số đã lưu tự động; upload thông thường giữ lại NVS.
Hai environment dùng cùng rotation và partition. Nếu đổi rotation hoặc xóa
flash, cần hiệu chỉnh lại. Serial ứng dụng in **Touch: using saved calibration**
khi chạm lần đầu nếu đã đọc được kết quả phù hợp.

## Font tiếng Việt

Tên bài hát dùng DejaVu Sans Bold 28 px; nghệ sĩ và album dùng DejaVu Sans
20 px. Font nhúng trong flash có 299 glyph Unicode, gồm đầy đủ chữ Việt
hoa/thường và dấu thanh. Không cần tải font lên SD hay SPIFFS.
Chuỗi UTF-8 được chuẩn hóa dấu Việt tách rời (NFD) thành ký tự dựng sẵn (NFC)
trước khi đo độ rộng và cuộn chữ. Font được tạo bằng
`python tools/generate_vietnamese_fonts.py` (Pillow và fonttools);
font nguồn và giấy phép nằm trong `tools/font_sources`.

## Chân board

| Phần | Chân GPIO |
| --- | --- |
| ILI9341 SCLK / MOSI / MISO | 14 / 13 / 12 |
| ILI9341 CS / DC / BL | 15 / 2 / 21 |
| XPT2046 CLK / MOSI / MISO | 25 / 32 / 39 |
| XPT2046 CS / IRQ | 33 / 36 |
| LED đỏ / xanh lá / xanh dương (active LOW) | 4 / 16 / 17 |
| BOOT | 0 |

Màn hình dùng HSPI; cảm ứng dùng VSPI trên chân riêng. SD không được sử dụng.
Nguồn pinout: https://github.com/witnessmenow/ESP32-Cheap-Yellow-Display/blob/main/PINS.md

## Kiểm tra trên phần cứng

Build đã được kiểm tra trên máy; chưa nạp và kiểm tra với board/iPhone.
Sau upload, kiểm tra logo, màu sắc, ghép đôi Bluetooth với iPhone,
thông tin bài hát, cả 5 nút cảm ứng, BOOT và chế độ tắt/ngủ.
Chỉnh `TouchMinX`, `TouchMaxX`, `TouchMinY`, `TouchMaxY` trong
`include/cyd_board.h` nếu vùng chạm lệch trên board của bạn. Các endpoint
200–3800 hiện là giá trị ban đầu, chưa hiệu chỉnh trên phần cứng.
Mã này dành cho CYD ESP32-2432S028R với ILI9341, không phải ESP32-S3.
