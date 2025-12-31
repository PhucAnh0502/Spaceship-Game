# Giao Tranh Liên Đội

Trò chơi bắn tàu vũ trụ theo đội (tối đa 3 người/đội). Đội thua khi toàn bộ tàu bị hạ. Mã nguồn gồm client, server, giao thức chung và thư viện tiện ích.

## Luật chơi
- Đội: tối đa 3 người; thua khi toàn bộ tàu bị phá hủy.
- Chỉ số khởi đầu: HP 1000, Armor 0, 1 pháo tự động 30mm với 50 viên (Dam 10 mỗi viên).
- Giới hạn trang bị: tối đa 4 pháo, 4 tên lửa, 2 lớp giáp cho mỗi tàu.
- Tính sát thương: mỗi 1 điểm Dam trừ 1 Armor (nếu còn giáp), nếu hết giáp thì trừ 1 HP.

## Kinh tế và rương
- Trả lời câu hỏi (trắc nghiệm) để mở rương.
- Phần thưởng: rương đồng 100 coin, rương bạc 500 coin, rương vàng 2000 coin.
- Rương xuất hiện ngẫu nhiên; chỉ người trả lời đúng đầu tiên nhận coin.

## Trang bị tấn công
- Đạn pháo tự động: 100 coin/thùng 50 viên (Dam 10 mỗi viên).
- Pháo laze: 1000 coin/khẩu, Dam 100 mỗi phát; cần bộ nguồn 100 coin/bộ (bắn 10 lần).
- Tên lửa: 2000 coin/quả, Dam 800 mỗi quả.

## Trang bị phòng thủ
- Giáp giảm sát thương theo tỉ lệ 1 Dam -> 1 Armor; có thể thay thế lớp giáp.
- Giáp cơ bản: 1000 coin, Amor 500.
- Giáp tăng cường: 2000 coin, Amor 1500.

## Dịch vụ sửa chữa
- Phục hồi thân tàu: 1 coin cho 1 HP.

## Cấu trúc dự án
- Client/: mã phía client (C)
- Server/: mã server và các handler
- Common/: định nghĩa giao thức dùng chung
- Lib/: thư viện đi kèm (ví dụ cJSON)
- Makefile: điểm vào build cho client/server

## Build và chạy
- Cài toolchain C (gcc/clang trên Linux/macOS, MinGW/MSVC trên Windows).
- Tại thư mục gốc, xem Makefile để biết target khả dụng; quy trình thường gặp:
  - Build: `make` hoặc target cụ thể cho server/client.
  - Chạy: thực thi binary server/client sau khi build (đường dẫn phụ thuộc Makefile).

## Ghi chú
- Tuân thủ giới hạn slot (4 pháo, 4 tên lửa, 2 giáp).
- Cân đối chi tiêu coin giữa đạn, vũ khí, giáp và sửa chữa để giữ đội ở trạng thái tốt nhất.
