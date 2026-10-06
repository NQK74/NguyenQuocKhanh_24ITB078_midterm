# HƯỚNG DẪN SỬ DỤNG CHƯƠNG TRÌNH `myls`

Chương trình **`myls`** là phiên bản clone của lệnh `ls(1)` trên hệ điều hành **NetBSD**, được viết bằng ngôn ngữ C (chuẩn C99 và POSIX APIs) phục vụ môn học **Lập trình hệ thống (LTHT)**.

---

## 1. Yêu cầu môi trường

- **Hệ điều hành:** NetBSD, FreeBSD hoặc Linux (do chương trình sử dụng các system call POSIX như `lstat`, `opendir`, `readdir`, `readlink`, `pwd.h`, `grp.h` không có sẵn trên Windows).
- **Trình biên dịch:** `cc` / `gcc` hoặc `clang`.
- **Công cụ build:** `make` (hỗ trợ cả BSD make và GNU make).

---

## 2. Cách đưa mã nguồn lên máy ảo & Đăng nhập

Từ máy chủ Windows, mở PowerShell/CMD và sử dụng lệnh `scp` và `ssh`:

### Bước 2.1: Chuyển mã nguồn lên máy ảo (chạy trên Windows)
```powershell
# Chuyển thư mục code qua cổng SSH 2222
scp -P 2222 -r d:\workspace\LTHT nqk@127.0.0.1:~/

# Hoặc chuyển file zip
scp -P 2222 D:\LTHT_netbsd.zip nqk@127.0.0.1:~/
```

### Bước 2.2: Đăng nhập SSH vào máy ảo
```powershell
ssh nqk@127.0.0.1 -p 2222
```

---

## 3. Biên dịch chương trình

Khi đã ở trong terminal của máy ảo NetBSD:

```bash
# 1. Đi vào thư mục chứa mã nguồn
cd ~/LTHT

# 2. Xóa các file build cũ (nếu có)
make clean

# 3. Biên dịch chương trình
make
```

Sau khi hoàn tất, file thực thi nhị phân `./myls` sẽ được tạo ra trong cùng thư mục.

---

## 4. Cú pháp sử dụng

```bash
./myls [-AacdFfhiklnqRrSstuw] [file ...]
```

- Nếu **không truyền tham số**: Chương trình liệt kê nội dung của thư mục hiện tại (`.`).
- Nếu **truyền nhiều tham số hỗn hợp**: File thường sẽ được in trước, thư mục in sau và có tiêu đề riêng biệt.

---

## 5. Bảng tra cứu các tùy chọn (Flags)

| Cờ | Tên gọi | Chức năng | Ví dụ |
| :---: | :--- | :--- | :--- |
| **`-a`** | All | Liệt kê tất cả file, bao gồm file ẩn bắt đầu bằng dấu chấm (`.` và `..`) | `./myls -a` |
| **`-A`** | Almost All | Liệt kê file ẩn nhưng loại trừ `.` và `..` *(mặc định bật cho super-user/root)* | `./myls -A` |
| **`-l`** | Long Format | Hiển thị dạng danh sách chi tiết (mode, link, owner, group, size, date, name) | `./myls -l` |
| **`-n`** | Numeric | Tương tự `-l` nhưng hiển thị UID và GID dạng số | `./myls -n` |
| **`-h`** | Human-readable | Hiển thị dung lượng dạng dễ đọc (`K`, `M`, `G`), ghi đè `-k` | `./myls -lh` |
| **`-k`** | Kilobytes | Hiển thị kích thước/block theo đơn vị KB (1024 bytes), ghi đè `-h` | `./myls -lk` |
| **`-i`** | Inode | Hiển thị số hiệu inode trước mỗi file | `./myls -i` |
| **`-s`** | Size (Blocks) | Hiển thị số block thực tế cấp phát trên ổ cứng | `./myls -s` |
| **`-F`** | Type Indicator | Thêm ký hiệu sau tên file: `/` (thư mục), `*` (thực thi), `@` (symlink), `\|` (FIFO), `=` (socket), `%` (whiteout) | `./myls -F` |
| **`-d`** | Directory only | Xem thư mục như file thường, không mở liệt kê nội dung bên trong | `./myls -ld tests` |
| **`-R`** | Recursive | Duyệt đệ quy tất cả các thư mục con | `./myls -R` |
| **`-r`** | Reverse | Đảo ngược thứ tự sắp xếp | `./myls -lr` |
| **`-S`** | Sort by Size | Sắp xếp các file theo kích thước giảm dần | `./myls -lS` |
| **`-t`** | Sort by Time | Sắp xếp theo thời gian sửa đổi (mới nhất lên đầu) | `./myls -lt` |
| **`-u`** | Access Time | Sử dụng thời gian truy cập gần nhất (atime) để hiển thị/sắp xếp | `./myls -lu` |
| **`-c`** | Status Change Time | Sử dụng thời gian thay đổi trạng thái inode (ctime) để hiển thị/sắp xếp | `./myls -lc` |
| **`-f`** | Unsorted | Không sắp xếp kết quả (tự động bật cờ `-a`) | `./myls -f` |
| **`-q`** | Question Mark | Chuyển các ký tự không in được thành dấu `?` (hỗ trợ UTF-8) | `./myls -q` |
| **`-w`** | Raw Print | In nguyên bản ký tự thô (không thay thế ký tự lạ) | `./myls -w` |

---

## 6. Quy tắc ưu tiên & Biến môi trường

### 6.1. Quy tắc ghi đè (Cờ xuất hiện sau cùng sẽ thắng)
- **`-w`** và **`-q`**: Cờ nào đứng sau sẽ quyết định cách in ký tự.
- **`-l`** và **`-n`**: Cờ nào đứng sau sẽ quyết định hiển thị tên hay ID số.
- **`-h`** và **`-k`**: Cờ nào đứng sau sẽ quyết định đơn vị dung lượng.
- **`-c`** và **`-u`**: Cờ nào đứng sau sẽ quyết định mốc thời gian lấy theo ctime hay atime.
- **`-R`** và **`-d`**: Cờ nào đứng sau sẽ quyết định duyệt đệ quy hay chỉ xem thư mục như file.

### 6.2. Biến môi trường
- **`BLOCKSIZE`**: Xác định đơn vị khối tính cho `-s` và dòng `total`. Hỗ trợ các hậu tố `k`, `m`, `g` (Ví dụ: `BLOCKSIZE=1K ./myls -s` hoặc `BLOCKSIZE=4096 ./myls -s`).
- **`TZ`**: Điều khiển múi giờ khi hiển thị cột ngày giờ ở dạng chi tiết `-l`.

---

## 7. Các ví dụ minh họa thường dùng

```bash
# Xem toàn bộ file bao gồm file ẩn với chi tiết quyền và dung lượng dễ đọc:
./myls -lah

# Xem file sắp xếp theo kích thước giảm dần:
./myls -lSh

# Xem file kèm số inode và ký hiệu phân loại file:
./myls -iF

# Liệt kê đệ quy một thư mục cụ thể:
./myls -laR tests/

# Kiểm tra symlink và xem thông tin thư mục như một file:
./myls -ld tests
```

---

## 8. Chạy bộ kiểm thử tự động (Test Suite)

Dự án tích hợp sẵn kịch bản kiểm thử tự động gồm 41 test case so sánh với lệnh `ls` chuẩn của hệ điều hành:

```bash
# Cấp quyền thực thi cho script test (chỉ cần chạy lần đầu)
chmod +x tests/run_tests.sh

# Chạy kiểm thử
./tests/run_tests.sh
```

---

## 9. Mã thoát (Exit Codes)

- **`0`**: Thực thi thành công không có lỗi.
- **`1`**: Lỗi khi đọc file/thư mục (ví dụ file không tồn tại, không đủ quyền đọc).
- **`2`**: Lỗi cú pháp dòng lệnh (truyền tham số tùy chọn không hợp lệ).
