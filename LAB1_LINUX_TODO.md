# Lab 1 — Checklist cho Linux (chưa làm)

> **Cập nhật 24/10/2026:** benchmark GCC đã chạy lại có kiểm soát — governor
> `performance`, `PIN_CPUS=2`, `THREADS=1`, 1.000 lần/cấu hình. Có 48 CSV GCC,
> `summary.csv` / `summary_classes.csv` / `summary_tag_overhead.csv` và 8 PNG
> (thêm `comparison_latency_hist_16KB.png`). Clang đã bỏ khỏi CSV và plot.
> Việc còn lại: build lại `build/ubuntu-gcc` từ commit hiện tại rồi làm mới log
> KAT/ctest (log hiện tại từ 21/10, trước khi thêm sidecar/zeroize), cập nhật số
> liệu trong `REPORT.md`, điền thông tin sinh viên, xuất PDF/DOCX, đóng gói.

> Hoàn thành toàn bộ Lab 1 trên Ubuntu. Tất cả lệnh chạy từ thư mục `lab01/`.

---

## 0. Chuẩn bị môi trường

```bash
cd /path/to/NT219/lab01
```

### Cài đặt dependency

**Ubuntu / Debian:**

```bash
sudo apt update
sudo apt install -y build-essential cmake libcryptopp-dev python3 \
    python3-tk python3-matplotlib
```

- `build-essential`: gcc, g++, make
- `cmake`: ≥ 3.16
- `libcryptopp-dev`: Crypto++ headers + static libs
- `python3-tk`: cần cho GUI (Tkinter)
- `python3-matplotlib`: cần cho vẽ đồ thị

Nếu Crypto++ build ở custom prefix:

```bash
cmake -S . -B build -DCRYPTOPP_ROOT=/path/to/cryptopp
```

---

## 1. Build lệnh dòng lệnh (aestool)

Build tĩnh (mặc định):

```bash
scripts/build_linux.sh gcc   # GCC 15.2
scripts/build_linux.sh clang # Clang 21.1.8 (nếu có)
```

Hoặc thủ công:

```bash
mkdir -p build/ubuntu-gcc && cd build/ubuntu-gcc
cmake ../..
cmake --build .
cd ../..
```

Binary sẽ xuất hiện tại: `build/ubuntu-gcc/aestool` (CMake single-config).

Kiểm tra:

```bash
./build/ubuntu-gcc/aestool help
```

---

## 2. Build thư viện chia sẻ (bonus library)

Cần cho GUI:

```bash
cmake -S . -B build/ubuntu-gcc-shared -DBUILD_SHARED_LIBS=ON
cmake --build build/ubuntu-gcc-shared --config Release
```

Thư viện mong đợi: `build/ubuntu-gcc-shared/libaescore.so` (CMake single-config;
với generator multi-config thì nằm trong `Release/`).

---

## 3. Kiểm chứng tính đúng đắn

### 3.1 KAT (Known Answer Tests)

```bash
./build/ubuntu-gcc/aestool --kat kat/vectors.json
```

Kết quả mong đợi:

```text
PASS NIST SP 800-38A AES-128 ECB
PASS NIST SP 800-38A AES-128 CBC
PASS NIST SP 800-38A AES-128 CFB128
PASS NIST SP 800-38A AES-128 OFB
PASS NIST SP 800-38A AES-128 CTR
PASS NIST GCM AES-128 test case 3
PASS NIST GCM AES-128 test case 4 with AAD
PASS RFC 3610 CCM packet vector 1
PASS RFC 3610 CCM packet vector 2
PASS NIST SP 800-38A AES-192 ECB (first block)
PASS NIST SP 800-38A AES-192 CBC (first block)
PASS NIST SP 800-38A AES-192 CTR (first block)
PASS NIST SP 800-38A AES-256 ECB (first block)
PASS NIST SP 800-38A AES-256 CBC (first block)
PASS NIST SP 800-38A AES-256 CTR (first block)
PASS IEEE P1619/D16 XTS-AES-128 vector 1
16 passed, 0 failed
```

Lưu log:

```bash
./build/ubuntu-gcc/aestool --kat kat/vectors.json | tee output/ubuntu/test_results/kat_gcc.txt
```

### 3.2 Unit test (Catch2 + ctest)

```bash
cd build/ubuntu-gcc && ctest --output-on-failure
cd ../..
```

Lưu log:

```bash
cd build/ubuntu-gcc && ctest --output-on-failure | tee ../../output/ubuntu/test_results/ctest_gcc.txt
cd ../..
```

Mong đợi: `2/2 tests passed` (suite `aescore` + CLI test `cli`).

### 3.3 Kiểm thử âm (negative tests)

Chạy thủ công các trường hợp và lưu log vào `output/ubuntu/neg_results/`:

| Trường hợp | Lệnh mẫu | Kết quả mong đợi |
|---|---|---|
| GCM sai khoá | `aestool decrypt --mode gcm --aead --key-hex ...` | exit 1, `authentication failed` |
| GCM sửa tag | sửa 1 byte tag rồi decrypt | exit 1, không tạo output |
| GCM sửa ciphertext | sửa 1 byte ciphertext rồi decrypt | exit 1, không tạo output |
| CTR sai khoá | decrypt CTR với key khác | exit 0, plaintext hỏng |
| CTR sửa ciphertext | sửa 1 byte ciphertext rồi decrypt | exit 0, plaintext hỏng |
| CBC sai IV | sửa IV trong header | exit 0, plaintext hỏng |
| IV ngắn 8 byte | `--iv 0001020304050607` | exit 1, `invalid IV/nonce length` |
| ECB > 16 KiB | `--mode ecb` payload > 16384 byte | exit 1, `ECB is blocked` |
| CTR reuse nonce | chạy 2 lần cùng key+nonce | exit 1, `nonce reuse rejected` |
| Hex lẻ | `--key-hex abc` | exit 1, `odd number of digits` |
| XTS < 16 byte | payload 10 byte | exit 1, `XTS requires at least 16 bytes` |

Tổng hợp bảng vào `output/ubuntu/neg_results/SUMMARY.md` (mẫu tham khảo từ `output/windows/neg_results/SUMMARY.md`).

---

## 4. Chạy benchmark

Ma trận đầy đủ: 8 mode × 6 kích thước × 1000 lần chạy = 48 CSV mỗi compiler.

```bash
scripts/run_benchmarks.sh gcc
scripts/run_benchmarks.sh clang
```

`--threads N` (mặc định 1) chạy song song các lần đo trong `aestool bench`. Giữ
mặc định khi lấy số liệu báo cáo vì N > 1 có tranh chấp luồng.

Output: `output/ubuntu/benchmarks/bench_ubuntu_gcc_<mode>_<size>.csv`

Mỗi CSV có header + 1000 dòng:

```csv
run,mode,size_bytes,encrypt_us,decrypt_us,throughput_encrypt_MBps,throughput_decrypt_MBps
```

### 4.1 Tổng hợp thống kê

```bash
python scripts/summarize_bench.py --dir output/ubuntu/benchmarks
```

Sinh ra: `summary.csv`, `summary_tag_overhead.csv`, `summary_classes.csv`

### 4.2 Vẽ đồ thị

```bash
python scripts/plot_compare.py --dir output/ubuntu/benchmarks \
    --out-dir output/ubuntu/benchmarks/graph
```

Sinh 8 PNG:
- `comparison_throughput_by_mode_1MB.png`
- `comparison_throughput_vs_size.png`
- `comparison_latency_vs_size.png`
- `comparison_latency_box_16KB.png`
- `comparison_latency_hist_16KB.png`
- `comparison_stream_vs_block.png`
- `comparison_aead_vs_nonaead.png`
- `comparison_tag_overhead.png`

---

## 5. Chạy GUI (bonus)

Cần shared library đã build ở bước 2:

```bash
python gui/aestool_gui.py --self-test
```

Kết quả mong đợi:

```text
self-test OK: build/ubuntu-gcc-shared/libaescore.so
  modes tested: ECB CBC OFB CFB CTR XTS CCM GCM
  round-trip, header mode check and wrong-key rejection passed
```

Chạy cửa sổ:

```bash
python gui/aestool_gui.py
```

Hoặc launcher:

```bash
./gui/run_gui.sh
```

Kiểm tra thủ công:
- Mã hoá UTF-8 (`Xin chào Việt Nam`) bằng `--text` hoặc file
- Giải mã ngược, kiểm tra khớp
- Sai khoá → dialog lỗi, không tạo output

---

## 6. So sánh cross-platform (Windows vs Ubuntu)

Sau khi có cả 2 thư mục benchmark:

```bash
python scripts/plot_crossplatform.py \
    --linux-dir output/ubuntu/benchmarks \
    --windows-dir output/windows/benchmarks \
    --out-dir output/compare/graph
```

Sinh ra: `output/compare/cross_platform_summary.csv` + heatmaps.

---

## 7. Hoàn thiện báo cáo (REPORT.md)

### 7.1 Điền thông tin sinh viên

Mở `lab01/REPORT.md`, thay dòng đầu:

```markdown
**Sinh viên:** <Họ tên của bạn>
**MSSV:** <Mã số sinh viên>
**Lớp:** <Lớp của bạn>
```

### 7.2 Cập nhật bằng chứng Linux

Trong REPORT.md:
- Thêm log KAT Linux: `output/ubuntu/test_results/kat_gcc.txt`
- Thêm log ctest Linux: `output/ubuntu/test_results/ctest_gcc.txt`
- Thêm log GUI self-test Linux nếu chạy được
- Cập nhật bảng self-grade nếu muốn (hiện đang 92 + 5 bonus)

### 7.3 Xuất PDF/DOCX

```bash
# Pandoc (nên dùng)
sudo apt install -y pandoc
pandoc lab01/REPORT.md -o lab01/REPORT.pdf --pdf-engine=xelatex \
    -V mainfont="DejaVu Serif" -V geometry:margin=1in

# Hoặc DOCX
pandoc lab01/REPORT.md -o lab01/REPORT.docx
```

---

## 8. Checklist nộp bài (Linux)

| Mục | Trạng thái |
|---|---|
| Build GCC (static) | ☑ |
| Build Clang (static) | ☑ (giữ để kiểm chứng build; không dùng cho benchmark) |
| Build shared library | ☑ |
| KAT 16/16 | ☑ |
| ctest 2/2 | ☑ |
| Negative tests (Ubuntu) | ☑ (12 case) |
| Benchmark 48 CSV (GCC) | ☑ có kiểm soát: governor `performance`, `PIN_CPUS=2`, `THREADS=1` |
| Benchmark 48 CSV (Clang) | ☑ bỏ theo quyết định — chỉ dùng GCC cho báo cáo |
| summary.csv / tag overhead / classes | ☑ |
| Đồ thị Ubuntu (8 PNG) | ☑ gồm box plot và histogram latency |
| GUI self-test | ☑ |
| Build lại `build/ubuntu-gcc` từ commit hiện tại | ☑ binary rebuild 24/10 |
| Làm mới log KAT/ctest Ubuntu | ☑ `output/ubuntu/test_results/kat_gcc.txt` và `ctest_gcc.txt` từ 24/10 |
| Cập nhật số liệu benchmark mới vào REPORT.md | ☐ số liệu cũ vẫn còn trong báo cáo |
| Chạy `plot_crossplatform.py` sau khi Windows ổn định | ☐ |
| Điền Sinh viên / MSSV / Lớp | ☐ |
| Xuất PDF/DOCX | ☐ |
| Đóng gói `MSV.All6labs.rar` | ☐ |

---

## 9. Lệnh tắt (copy-paste)

```bash
cd /path/to/NT219/lab01

# Build
scripts/build_linux.sh gcc
scripts/build_linux.sh clang

# KAT
./build/ubuntu-gcc/aestool --kat kat/vectors.json | tee output/ubuntu/test_results/kat_gcc.txt
./build/ubuntu-clang/aestool --kat kat/vectors.json | tee output/ubuntu/test_results/kat_clang.txt

# ctest
(cd build/ubuntu-gcc && ctest --output-on-failure) | tee output/ubuntu/test_results/ctest_gcc.txt
(cd build/ubuntu-clang && ctest --output-on-failure) | tee output/ubuntu/test_results/ctest_clang.txt

# Benchmark
scripts/run_benchmarks.sh gcc
scripts/run_benchmarks.sh clang

# Thống kê + đồ thị
python3 scripts/summarize_bench.py --dir output/ubuntu/benchmarks
python3 scripts/plot_compare.py --dir output/ubuntu/benchmarks --out-dir output/ubuntu/benchmarks/graph

# GUI
python3 gui/aestool_gui.py --self-test
```

---

## 10. Lưu ý quan trọng

- **Không để `.aestool_nonces` trong repo root.** Negative tests nên chạy trong thư mục tạm.
- **Không có binary Linux trong repo hiện tại.** Sau khi build trên Linux, bạn có thể commit binary hoặc chỉ lưu lại CSV/ log làm bằng chứng.
- **MinGW trên Windows đã xác minh** — GCC 16.2.0 và Clang 22.1.8 build được,
  KAT 16/16 và ctest 2/2; xem `output/windows/test_results/*_mingw_*.txt`.
- UTF-8 trên Linux làm việc bình thường qua stdin/file, không cần xử lý đặc biệt như Windows.
