# BÁO CÁO LAB 2 — AES-128 THUẦN C++ VÀ CHẾ ĐỘ CTR

**Học phần:** NT219 — An toàn thông tin  
**Bài thực hành:** Lab 2 — AES-128 Implementation in Pure C++ (CTR Mode, FIPS-197)  
**Sinh viên:** ................................................  
**MSSV:** ................................................  
**Lớp:** ................................................  
**Ngày cập nhật:** 05/11/2026  

> Báo cáo dùng số liệu Ubuntu trong `lab02/output/ubuntu/benchmarks/`, số liệu
> Windows trong `lab02/output/windows/benchmarks/`, và nhật ký kiểm thử trong
> `lab02/output/windows/test_results/`. Không có số liệu hiệu năng nào được
> tạo thủ công. CSVs Ubuntu được đo bằng GCC 15.2 trước lần hoàn thiện này;
> chưa có nhật ký `ctest`/KAT Ubuntu mới. CSVs Windows được đo ngày 05/11/2026
> với MSVC; xem `output/windows/benchmarks/AUDIT.md` cho kiểm tra tính hợp lệ
> và các giới hạn (không có run-time environment log, số lần chạy 100/30).

---

## Mục lục

1. Tóm tắt điều hành
2. Mục tiêu và phạm vi
3. Thiết kế và cài đặt
4. Kiểm chứng tính đúng đắn
5. Phương pháp đo hiệu năng
6. Kết quả hiệu năng hiện có
7. Phân tích bảo mật
8. Đối chiếu rubric và tự chấm
9. Hạn chế và việc còn lại
10. Tài liệu tham khảo

---

## 1. Tóm tắt điều hành

Lab 2 cài đặt AES-128 từ đầu bằng C++17 chỉ với STL, tuân theo FIPS-197:
10 vòng, 44 word khoá, state ánh xạ theo cột. Chế độ CTR tuân theo NIST
SP 800-38A: bộ đếm 128-bit tăng big-endian, xử lý khối cuối không đầy đủ,
không padding, và dừng an toàn khi bộ đếm tràn thay vì quay vòng. Công cụ
`aestool` hỗ trợ `encrypt`, `decrypt`, `bench`, `--kat`, cùng các cờ thống
nhất như `--encode`, `--decode`, `--threads`, `--verbose` và `--nonce`.

Phần thưởng đã làm:

- **Bonus A — AES-192/AES-256:** lịch khoá mở rộng đầy đủ, KAT FIPS-197
  Appendix C.2/C.3 và KAT CTR tương ứng.
- **Bonus B — AES-XTS:** hai khoá, nhân tweak trong GF(2^128), ciphertext
  stealing theo IEEE P1619, KAT IEEE P1619 và CAVP, cùng kiểm tra parity với
  Crypto++.
- **Bonus C — AES-NI/bitsliced:** không cài đặt. Đây là bonus, không phải
  yêu cầu bắt buộc.

Kết quả kiểm thử Windows trong lần hoàn thiện: MSVC 19.51, MinGW GCC 16.2.0
và MinGW Clang 22.1.8 đều đạt **9/9 KAT**, **4/4 CTest** (unit, KAT, CLI,
cross-validation). Bộ unit test có 351 assertion trong 11 test case; bộ
cross-validation có 22 assertion trong 3 test case.

Benchmark Windows MSVC mới (05/11/2026) có đủ 3 kích thước, throughput trung
bình **6.74 / 7.02 / 7.09 MiB/s** cho 1 MiB / 100 MiB / 1 GiB; CV lần lượt
6.3% / 1.3% / 0.7%, tất cả được xếp `ok`. Số lần chạy là 1000 / 100 / 30,
không phải 1000 / 500 / 50 của blueprint; chưa có run-time environment log
cho benchmark Windows.

Điểm chưa hoàn thiện: chưa có nhật ký `ctest`/KAT Ubuntu mới, benchmark
Ubuntu vẫn là dữ liệu cũ (`powersave`, không pin, warm-up 5 vòng), chưa có
benchmark hiệu năng Crypto++, và chưa có PDF/identity. Báo cáo ghi rõ những
điểm này thay vì suy diễn số liệu.

---

## 2. Mục tiêu và phạm vi

### 2.1 Mục tiêu

1. Cài đặt AES-128 theo FIPS-197: `AddRoundKey`, `SubBytes`, `ShiftRows`,
   `MixColumns`, `KeyExpansion`; 10 vòng; state 4×4 theo cột.
2. Cài đặt CTR theo NIST SP 800-38A: IV 128-bit, bộ đếm big-endian, dữ liệu
   dài tùy ý, khối cuối không đầy đủ, không padding, phát hiện tràn bộ đếm.
3. Kiểm chứng bằng vector chính thức và đối chiếu byte-for-byte với Crypto++.
4. Đo hiệu năng ở 1 MiB, 100 MiB và 1 GiB; thảo luận cache, bộ nhớ và
   branch prediction.
5. Phân tích các rủi ro: tái sử dụng nonce, tính dễ uốn (malleability),
   thiếu xác thực, cache-timing của S-box, và giới hạn của mã không hằng số
   thời gian.

### 2.2 Phạm vi

- Bắt buộc: AES-128 + CTR.
- Bonus A: AES-192/AES-256.
- Bonus B: XTS-AES.
- Không làm Bonus C (AES-NI/bitsliced).
- Không dùng OpenSSL, Crypto++ hay thư viện mật mã nào trong `aes_core` hoặc
  `aestool`. Crypto++ chỉ xuất hiện trong target kiểm thử `test_crossval`.

---

## 3. Thiết kế và cài đặt

### 3.1 Cấu trúc mã nguồn

```
lab02/
├── CMakeLists.txt          CMake C++17, static CRT cho MSVC khi cần
├── src/
│   ├── AES.hpp/.cpp        AES core: 128/192/256, encrypt/decrypt, key schedule
│   ├── modes.hpp/.cpp      CTR bắt buộc; XTS bonus
│   ├── minijson.hpp        JSON parser tối giản, không phụ thuộc
│   └── main.cpp            CLI: encrypt, decrypt, bench, --kat
├── tests/
│   ├── test_aes.cpp        FIPS-197, S-box, key validation
│   ├── test_ctr.cpp        SP 800-38A, partial block, overflow, XTS
│   ├── test_crossval.cpp   Parity với Crypto++ (CTR-128/192/256 + XTS)
│   ├── cli_test.sh         CLI Linux
│   └── cli_test.cmake      CLI Windows
├── kat/vectors.json        9 vector KAT chính thức
└── scripts/                build, benchmark, summarize, plot
```

### 3.2 AES core

- State là `std::array<std::array<uint8_t,4>,4>` với quy ước
  `state[col][row] = block[col * 4 + row]`, đúng FIPS-197.
- Key expansion sinh `4 * (Nr + 1)` word: 44 word cho AES-128, 52 cho
  AES-192, 60 cho AES-256.
- `SubBytes` dùng bảng S-box 256 byte. Hàm `AES::sbox_computed()` cài đặt
  S-box từ phép nghịch đảo GF(2^8) và biến đổi affine; unit test xác nhận
  cả 256 giá trị khớp bảng. Hàm này chỉ dùng để kiểm chứng/phân tích, không
  nằm trên đường mã hoá chính.
- `MixColumns` dùng phép nhân GF(2^8) với đa thức rút gọn
  `x^8 + x^4 + x^3 + x + 1` (`0x11B`).
- Giải mã dùng `InvSubBytes`, `InvShiftRows`, `InvMixColumns` và thứ tự
  round key ngược.
- Khoá gốc `key_` được ghi đè và giải phóng ngay sau khi mở rộng khoá; huỷ
  `AES` ghi đè toàn bộ `round_keys_` bằng `volatile` write để giảm khả năng
  khoá còn trong bộ nhớ sau khi đối tượng chết.

### 3.3 CTR mode

- Bộ đếm là 128-bit, tăng từ byte cuối (big-endian).
- Mỗi khối: `keystream = AES_encrypt(counter)`, XOR với dữ liệu, rồi tăng.
- Khối cuối chỉ XOR `data.size() % 16` byte, không padding.
- Nếu bộ đếm tràn từ `FF..FF` về `00..00` mà vẫn còn dữ liệu, hàm ném
  `std::runtime_error` thay vì tái sử dụng keystream.
- Encrypt và decrypt dùng cùng một hàm `ctr_crypt`; đây là tính chất của
  stream cipher.

### 3.4 XTS (bonus B)

- Khoá XTS là `K1 || K2`; mỗi nửa là một AES key (16/24/32 byte).
- Tweak ban đầu là `AES_K2(data_unit)`, nhân với alpha trong GF(2^128)
  theo đa thức `x^128 + x^7 + x^2 + x + 1`.
- Với dữ liệu không chia hết 16 byte, dùng ciphertext stealing theo
  IEEE P1619.
- XTS không xác thực; nó chỉ bảo vệ tính bí mật và tính duy nhất của sector.

### 3.5 CLI và fail-closed

Các cờ chính:

| Cờ | Ý nghĩa |
|---|---|
| `--mode ctr\|xts` | Chế độ mã hoá |
| `--key FILE` / `--key-hex HEX` | Khoá nhị phân hoặc hex |
| `--iv FILE` / `--iv-hex HEX` / `--nonce ...` | Bộ đếm / data unit 16 byte |
| `--in FILE` / `--text STRING` | Đầu vào |
| `--out FILE` / `--out-hex` | Đầu ra |
| `--encode hex\|base64\|raw` | Mã hoá đầu ra |
| `--decode hex\|base64\|raw` | Giải mã đầu vào trước khi decrypt |
| `--threads N` | Chỉ dùng cho `bench` |
| `--verbose` | In metadata, tiến độ và ETA của `bench` ra stderr |
| `--kat FILE` | Chạy Known Answer Tests |

Các kiểm tra fail-closed:

- Khoá sai độ dài bị từ chối.
- IV khác 16 byte bị từ chối.
- `--key` đồng thời với `--key-hex` bị từ chối.
- `--iv`, `--iv-hex`, `--nonce`, `--nonce-hex` chỉ được dùng một nguồn.
- `--threads` ngoài `bench` bị từ chối.
- `bench` từ chối option lạ thay vì bỏ qua im lặng.
- KAT gặp mode không hỗ trợ sẽ đếm `skipped` và trả mã lỗi; không còn
  trường hợp "0 failed" nhưng thực ra đã bỏ qua vector.

### 3.6 UTF-8 trên Windows

`main()` trên Windows dùng `GetCommandLineW()` +
`CommandLineToArgvW()`, chuyển từng đối số sang UTF-8 bằng
`WideCharToMultiByte(CP_UTF8, ...)`. Mọi đường dẫn tệp đi qua
`std::filesystem::u8path()`. Vì vậy đường dẫn có tiếng Việt và `--text`
tiếng Việt không bị mã hoá lại theo ANSI code page. `cli_test.cmake` kiểm
tra cả thư mục `thư mục` và chuỗi tiếng Việt.

---

## 4. Kiểm chứng tính đúng đắn

### 4.1 Known Answer Tests

`kat/vectors.json` có 9 vector:

| Nguồn | Nội dung |
|---|---|
| NIST SP 800-38A | AES-128 CTR, 4 khối |
| NIST SP 800-38A | AES-192 CTR, 4 khối (bonus A) |
| NIST SP 800-38A | AES-256 CTR, 4 khối (bonus A) |
| NIST SP 800-38A | AES-128 CTR cắt còn 20 byte (khối cuối không đầy đủ) |
| FIPS-197 C.1 | AES-128 ECB một khối |
| FIPS-197 C.2 | AES-192 ECB một khối (bonus A) |
| FIPS-197 C.3 | AES-256 ECB một khối (bonus A) |
| IEEE P1619/D16 | XTS-AES-128 vector 1, 2 khối (bonus B) |
| NIST CAVP XTSGenAES128 | COUNT 1, 1 khối (bonus B) |

Lệnh:

```bash
./build/ubuntu-gcc/aestool --kat kat/vectors.json
```

Kết quả Windows đã lưu:

- `output/windows/test_results/kat_msvc.txt`: `9 passed, 0 failed`
- `output/windows/test_results/kat_mingw_gcc.txt`: `9 passed, 0 failed`
- `output/windows/test_results/kat_mingw_clang.txt`: `9 passed, 0 failed`

### 4.2 Unit tests

`test_core` kiểm tra:

- FIPS-197 AES-128/192/256.
- S-box tính toán khớp bảng 256/256.
- Key schedule và key validation.
- CTR vector SP 800-38A cho AES-128/192/256.
- Khối cuối không đầy đủ.
- Tràn bộ đếm: một khối `FF..FF` hợp lệ; hai khối phải ném lỗi.
- XTS full block và ciphertext stealing round-trip.
- Các trường hợp lỗi đầu vào.

Kết quả Windows: `All tests passed (351 assertions in 11 test cases)` trên
cả MSVC, MinGW GCC và MinGW Clang.

### 4.3 Cross-validation với Crypto++

`test_crossval` chỉ link Crypto++ trong target kiểm thử, không link vào
`aestool`. Nó so sánh:

- AES-128-CTR với Crypto++ trên dữ liệu 87 byte.
- AES-192-CTR và AES-256-CTR trên dữ liệu 257 byte (có khối cuối lẻ).
- XTS-AES-128 trên dữ liệu không chia hết block.

Kết quả Windows: `All tests passed (22 assertions in 3 test cases)`.

Nếu `ENABLE_CROSSVAL=ON` mà CMake không tìm thấy Crypto++, cấu hình **thất
bại**. Muốn bỏ oracle phải chỉ định rõ `-DENABLE_CROSSVAL=OFF`; khi đó
README/report phải ghi rằng yêu cầu cross-validation chưa đạt.

### 4.4 Negative và CLI tests

`cli_test.sh` / `cli_test.cmake` kiểm tra:

- Round-trip CTR.
- Đường dẫn và văn bản tiếng Việt.
- Khoá 15 byte và IV 8 byte bị từ chối.
- XTS round-trip có ciphertext stealing.
- `--nonce` / `--nonce-hex`.
- Base64 encode/decode round-trip.
- Xung đột `--key` + `--key-hex`.
- `--threads` bị từ chối ngoài `bench`.
- `bench` từ chối option lạ.
- `bench --verbose` in tiến độ/ETA ra stderr và vẫn ghi CSV đúng.
- CSV benchmark có header + đúng số dòng.
- KAT 9/9 và KAT mode không hỗ trợ phải trả mã lỗi.

Kết quả Windows: `ctest` 4/4 trên MSVC, MinGW GCC, MinGW Clang. Nhật ký nằm
trong `output/windows/test_results/ctest_*.txt`.

### 4.5 Ubuntu

Các lệnh cần chạy lại trên Ubuntu:

```bash
cd lab02
scripts/build_linux.sh gcc
./build/ubuntu-gcc/aestool --kat kat/vectors.json \
  | tee output/ubuntu/test_results/kat_gcc.txt
(cd build/ubuntu-gcc && ctest --output-on-failure) \
  | tee output/ubuntu/test_results/ctest_gcc.txt
```

Hiện chưa có nhật ký Ubuntu mới trong repo. Các CSVs Ubuntu bên dưới là dữ
liệu benchmark cũ, không phải bằng chứng test mới.

---

## 5. Phương pháp đo hiệu năng

### 5.1 Môi trường đo hiện có

**Ubuntu (dữ liệu cũ):**

| Thành phần | Giá trị |
|---|---|
| Host | `han-sui-Vostro-3500` |
| OS | Ubuntu, kernel `7.0.0-38-generic x86_64` |
| CPU | 11th Gen Intel Core i5-1135G7 @ 2.40 GHz, 8 logical CPU |
| RAM | 23,796,968 KiB (~22.7 GiB) |
| Swap | 8,388,604 KiB |
| Compiler | GCC 15.2.0 (`g++ (Ubuntu 15.2.0-16ubuntu1) 15.2.0`) |
| Governor lúc đo | `powersave` |
| CPU pinning | Không |
| Binary | `build/ubuntu-gcc/aestool`, commit `857cb1b` |

**Windows (benchmark mới, 05/11/2026):**

| Thành phần | Giá trị |
|---|---|
| Host | Dell Vostro 3500 |
| OS | Windows 11 Home 10.0.26300, x64 |
| CPU | 11th Gen Intel Core i5-1135G7 @ 2.40 GHz, 4 cores / 8 logical |
| Compiler | MSVC 19.51 (Visual Studio 18 2026) |
| Binary | `build/windows-msvc/Release/aestool.exe` |
| Power plan hiện tại | High performance (kiểm tra sau khi chạy, không phải run-time capture) |
| Threads | Script mặc định 4; CSV không có cột thread nên không chứng minh được từ file |
| Run-time log | **Không có**; script chỉ in ra console, không tee ra file |

Các bảng trên là môi trường quan sát được, không phải môi trường lý tưởng cho
kết luận nhân quả. Thiếu run-time log Windows là một hạn chế của bằng chứng,
không phải lý do để suy diễn governor/affinity/priority.

### 5.2 Giao thức hiện có

Dữ liệu đã commit (cả Ubuntu cũ và Windows mới đều dùng 1000/100/30):

| Nền tảng | 1 MiB | 100 MiB | 1 GiB | Ghi chú |
|---|---:|---:|---:|---|
| Ubuntu GCC 15.2 (cũ) | 1000 | 100 | 30 | `powersave`, không pin, warm-up 5 vòng |
| Windows MSVC 19.51 (05/11/2026) | 1000 | 100 | 30 | Script 4 thread; thiếu run-time log |

Blueprint ghi nominal 1000/500/50. Bài này chốt dùng 1000/100/30: 100 MiB và
1 GiB vẫn có N >= 30 và CV thấp, đủ để phân tích thống kê.

Mỗi lần chạy đo riêng encrypt và decrypt bằng `std::chrono::high_resolution_clock`,
ghi `encrypt_us`, `decrypt_us`, rồi tính throughput MiB/s. CSV có header:

```csv
run,mode,size_bytes,encrypt_us,decrypt_us,throughput_encrypt_MBps,throughput_decrypt_MBps
```

`summary.csv` tính mean, median, standard deviation, 95% CI và coefficient
of variation. `summary_robust.csv` bổ sung trimmed mean, outlier percentage
và stability.

### 5.3 Giao thức đã sửa trong mã

- Warm-up cũ: 5 vòng cố định. Điều này quá ngắn ở 1 MiB và quá tốn ở 1 GiB.
- Warm-up mới: theo thời gian, mặc định 1.0 giây (`--warmup`), dùng prefix
  tối đa 1 MiB để không biến 1 GiB thành một phút warm-up.
- Script Linux mới dùng `taskset -c ${PIN_CPU:-0}` nếu có, ghi lại
  governor và cảnh báo nếu không phải `performance`.
- Script Windows mặc định 1 MiB/1000, 100 MiB/100, 1 GiB/30, dùng High
  Performance, `start /high /affinity`, và hỗ trợ `RUNS`, `RUNS_1MB`,
  `RUNS_100MB`, `RUNS_1GB`, `WARMUP`, `THREADS`, `PIN_MASK`, `PRIORITY`.
  Đây là ma trận đã chọn; nominal 500/50 vẫn có thể đặt qua `RUNS_*`.
- Cả hai script mặc định `THREADS=4` và truyền `--threads 4`; có thể đổi về
  `THREADS=1` để lấy baseline đơn luồng. Windows dùng `PIN_MASK=F` (CPU 0-3)
  cho 4 thread và `PIN_MASK=4` cho 1 thread; nếu affinity chỉ chọn 1 CPU thì
  script in cảnh báo vì 4 thread sẽ tranh nhau 1 core.

Các dữ liệu Ubuntu hiện có được đo bằng mã cũ (warm-up 5 vòng), nên so sánh
với Lab 1/Crypto++ chỉ mang tính tham khảo cho đến khi chạy lại.

### 5.4 Lệnh chạy lại

Ubuntu, giao thức chốt (1000/100/30, 4 thread, có pin CPU):

```bash
cd lab02
scripts/build_linux.sh gcc
THREADS=4 PIN_CPU=0-3 scripts/run_benchmarks.sh gcc
python3 scripts/summarize_bench.py --dir output/ubuntu/benchmarks
for f in output/ubuntu/benchmarks/bench_ubuntu_gcc_ctr_*.csv; do
  python3 scripts/plot_bench.py "$f" \
    --out-dir output/ubuntu/benchmarks/graphs --platform gcc
done
```

Đây là mặc định hiện tại của script. Nếu chỉ muốn nominal 500/50 của blueprint,
thêm `RUNS_100MB=500 RUNS_1GB=50`; không cần thiết cho bài này.

Windows — **đã chạy ngày 05/11/2026**; dữ liệu và audit ở
`output/windows/benchmarks/`. Lần chạy đã commit dùng 1000/100/30, khớp ma trận
đã chọn; không cần chạy lại chỉ để đổi sang 500/50:

```bat
cd lab02
scripts\build_windows.bat msvc
scripts\run_benchmarks.bat msvc
python scripts\summarize_bench.py --dir output\windows\benchmarks
```

Nếu muốn so sánh với Crypto++ ở cùng kích thước, cần thêm target benchmark
Crypto++ (chưa có trong lần này) và chạy cùng payload, cùng compiler, cùng
CPU pinning. Không dùng số liệu cross-validation (chỉ đo đúng/sai) để suy ra
throughput.

---

## 6. Kết quả hiệu năng hiện có

### 6.1 Ubuntu (dữ liệu cũ)

Nguồn: `output/ubuntu/benchmarks/summary.csv`.

| Kích thước | Runs | Enc mean (µs) | Enc median (µs) | Enc SD (µs) | Enc 95% CI (µs) | Enc MiB/s | Dec mean (µs) | Dec median (µs) | Dec SD (µs) | Dec 95% CI (µs) | Dec MiB/s |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| 1 MiB | 1000 | 55,612.259 | 54,593.925 | 2,767.611 | 171.538 | 18.020 | 55,681.962 | 54,547.095 | 2,778.929 | 172.240 | 17.998 |
| 100 MiB | 100 | 5,588,411.281 | 5,482,736.905 | 340,985.128 | 66,833.085 | 17.949 | 5,589,545.739 | 5,517,173.505 | 290,452.718 | 56,928.733 | 17.933 |
| 1 GiB | 30 | 59,386,377.908 | 59,128,999.550 | 1,527,787.668 | 546,711.796 | 17.254 | 59,565,180.960 | 59,234,784.430 | 1,510,169.555 | 540,407.235 | 17.201 |

Ghi chú: cột `MBps` trong CSV thực chất là MiB/s vì mẫu số dùng
`1024 * 1024`. Báo cáo giữ nhãn MiB/s để tránh nhầm với MB/s thập phân.

### 6.2 Windows MSVC (05/11/2026)

Nguồn: `output/windows/benchmarks/summary.csv`; kiểm tra đầy đủ ở
`output/windows/benchmarks/AUDIT.md`.

| Kích thước | Runs | Enc mean (µs) | Enc median (µs) | Enc SD (µs) | Enc 95% CI (µs) | Enc MiB/s | Dec mean (µs) | Dec median (µs) | Dec SD (µs) | Dec 95% CI (µs) | Dec MiB/s | CV enc/dec |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| 1 MiB | 1000 | 148,837.743 | 146,208.300 | 9,403.395 | 582.828 | 6.741 | 147,471.127 | 145,860.650 | 9,197.615 | 570.074 | 6.801 | 6.3% / 6.2% |
| 100 MiB | 100 | 14,254,751.949 | 14,204,528.500 | 190,003.671 | 37,240.720 | 7.016 | 14,263,820.188 | 14,194,372.900 | 196,034.703 | 38,422.802 | 7.012 | 1.3% / 1.4% |
| 1 GiB | 30 | 144,547,416.503 | 144,153,493.400 | 997,913.478 | 357,098.752 | 7.085 | 144,574,139.337 | 144,385,364.750 | 931,980.253 | 333,504.851 | 7.083 | 0.7% / 0.6% |

`summary_robust.csv` đánh dấu cả ba cấu hình `ok`: CV <= 25% và outlier
<= 5%. Ma trận chốt là 1000/100/30; blueprint nominal 1000/500/50.

### 6.3 Nhận xét

- Ubuntu cũ: throughput gần như phẳng theo kích thước, khoảng 18 MiB/s ở
  1 MiB và 100 MiB, giảm còn khoảng 17.2 MiB/s ở 1 GiB. Mức giảm nhỏ cho thấy
  chi phí chính là phép biến đổi AES table-based, không phải băng thông bộ nhớ.
- Windows mới: throughput tăng nhẹ từ 6.74 MiB/s (1 MiB) lên 7.09 MiB/s
  (1 GiB). 1 MiB có CV 6.3% vì payload nhỏ, thời gian mỗi lần chạy chỉ khoảng
  0.15 s nên dao động timer/cache rõ hơn; 100 MiB và 1 GiB có CV 1.3% và 0.7%.
- Encrypt và decrypt gần như bằng nhau trên cả hai nền tảng, đúng với CTR
  (cùng hàm).
- Không so sánh trực tiếp 18 MiB/s Ubuntu với 7 MiB/s Windows: khác compiler
  (GCC 15.2 so với MSVC 19.51), khác OS, khác số thread (Ubuntu cũ 1 thread,
  Windows 4 thread), khác power plan/pinning. Chênh lệch này là dữ liệu để
  điều tra, không phải kết luận về chất lượng code.
- Throughput tuyệt đối thấp so với AES-NI (hàng trăm MiB/s đến GB/s) là dự
  kiến: đây là AES bảng tra cứu, mỗi khối tạo `std::array`, cộng thêm chi phí
  cấp phát vector. Báo cáo không so sánh trực tiếp với Crypto++ vì chưa có
  phép đo Crypto++ cùng điều kiện.

### 6.4 Biểu đồ

Ubuntu cũ đã có:

- `graphs/bench_ubuntu_gcc_ctr_1048576_throughput.png`
- `graphs/bench_ubuntu_gcc_ctr_1048576_latency.png`
- `graphs/bench_ubuntu_gcc_ctr_104857600_throughput.png`
- `graphs/bench_ubuntu_gcc_ctr_104857600_latency.png`

Ubuntu chưa có biểu đồ 1 GiB. Lệnh tạo:

```bash
python3 scripts/plot_bench.py \
  output/ubuntu/benchmarks/bench_ubuntu_gcc_ctr_1073741824.csv \
  --out-dir output/ubuntu/benchmarks/graphs --platform gcc
```

Windows mới đã có đủ 6 biểu đồ trong
`output/windows/benchmarks/graphs/`: throughput và latency cho 1 MiB, 100 MiB,
1 GiB.

---

## 7. Phân tích bảo mật

### 7.1 S-box bảng tra và cache-timing

AES core dùng bảng S-box 256 byte, tương đương 4 cache line 64 byte trên
x86-64. Chỉ số truy cập phụ thuộc vào byte trung gian, tức phụ thuộc khoá và
plaintext. Trên CPU có cache chia sẻ, kẻ tấn công cùng máy có thể quan sát
cache set bị chiếm để suy đoán chỉ số, dẫn tới tấn công cache-timing
(Bernstein 2005; Osvik–Shamir–Tromer 2006). Bảng T-table 32-bit còn tệ hơn
vì chiếm khoảng 4 KiB và tạo nhiều cache line hơn.

`AES::sbox_computed()` được thêm để đối chiếu: nó tính nghịch đảo trong
GF(2^8) bằng lũy thừa 254 và biến đổi affine, không tra bảng. Unit test xác
nhận 256/256 giá trị khớp. Tuy nhiên đây không phải cài đặt constant-time
hoàn chỉnh: vòng lặp square-and-multiply có nhánh theo bit và phụ thuộc dữ
liệu; nó chỉ minh hoạ hướng tránh tra cứu bảng. Muốn constant-time thực sự
cần bit-slicing hoặc AES-NI, và cần đo lại hiệu năng.

### 7.2 Tái sử dụng nonce/IV

CTR biến AES thành stream cipher: `C = P XOR keystream`. Nếu cùng khoá và
cùng IV được dùng cho hai plaintext, `C1 XOR C2 = P1 XOR P2`, làm lộ quan hệ
giữa hai bản rõ. Với XTS, dùng lại data unit trong cùng sector cũng phá tính
bảo mật. CLI hiện nhận IV từ người dùng và không có registry trạng thái, nên
không thể tự bảo đảm duy nhất. Hợp đồng sử dụng là:

- Mỗi khoá CTR chỉ dùng một IV một lần; IV ngẫu nhiên 96–128 bit hoặc bộ đếm
  không lặp.
- Với XTS, data unit phải duy nhất trong phạm vi một khoá.
- Không dùng lại khoá giữa các phiên/lab.

Đây là giới hạn cần ghi trong README và báo cáo, không phải lỗi có thể sửa
chỉ bằng một cờ CLI.

### 7.3 Malleability và thiếu xác thực

CTR không có tag xác thực. Kẻ tấn công có thể lật bit bất kỳ trong ciphertext
và bit tương ứng trong plaintext sẽ lật, mà không cần biết khoá. Vì vậy CTR
chỉ phù hợp khi có lớp xác thực (Encrypt-then-MAC, GCM, CCM) hoặc khi dữ
liệu không cần toàn vẹn. Lab 2 chỉ yêu cầu CTR nên đây là giới hạn cần nêu,
không phải lỗi cài đặt.

### 7.4 Quản lý khoá

- Khoá gốc bị ghi đè và giải phóng ngay sau khi mở rộng; `AES::~AES()` ghi
  đè toàn bộ round keys bằng volatile write. Đây là best-effort: bản sao, bộ
  nhớ tạm của vector, swap, core dump và tối ưu compiler vẫn có thể để lại
  dấu vết.
- Không có `mlock`/`VirtualLock`, nên khoá có thể bị swap.
- Không có cơ chế xoá khoá sau khi dùng xong ở cấp CLI ngoài destructor.
- Khoá không được in ra stdout; `--verbose` chỉ in độ dài khoá và IV, không
  in khoá.

### 7.5 AES-NI và bitslicing

AES-NI thực hiện AES trong phần cứng, thường nhanh hơn hàng chục lần và
không có tra cứu bảng phụ thuộc dữ liệu. Đây là Bonus C, không bắt buộc.
Nếu làm, cần:

- Phát hiện CPUID (`AES` bit) tại runtime.
- Có fallback phần mềm cho CPU không hỗ trợ.
- Đo lại throughput và so sánh với bản table-based.
- Phân tích rằng AES-NI loại bỏ cache-timing của S-box nhưng vẫn cần bảo vệ
  nonce và xác thực.

### 7.6 So sánh với Lab 1

Lab 1 dùng Crypto++ và có AEAD (GCM/CCM), registry nonce, kiểm tra biên và
benchmark có pinning. Lab 2 cố ý không dùng thư viện mật mã, nên phải tự
chịu trách nhiệm về:

- Tính đúng đắn của AES/CTR.
- Kiểm tra tràn bộ đếm.
- Quản lý khoá.
- Giới hạn side-channel của bảng tra.

Lab 2 không có AEAD; vì vậy không thể so sánh trực tiếp độ an toàn với GCM
của Lab 1. Đây là khác biệt về mục tiêu, không phải lỗi.

---

## 8. Đối chiếu rubric và tự chấm

Rubric từ `docs/blueprint/labs/lab2.md`: 100 điểm core + 20 điểm bonus.

| Tiêu chí | Điểm tối đa | Tự chấm | Căn cứ |
|---|---:|---:|---|
| FIPS-197 AES-128 core | 30 | 30 | Cài đặt thủ công, đúng state cột, key expansion, KAT FIPS-197 |
| CTR correctness | 15 | 15 | Big-endian, partial block, không padding, overflow fail-closed |
| KAT và cross-validation | 15 | 14 | 9 KAT, Crypto++ parity 3 test; Ubuntu log chưa commit |
| Engineering và documentation | 10 | 9 | CMake, module, CLI fail-closed, CTest, README/report; chưa có PDF |
| Performance evaluation | 10 | 8 | Windows đủ 3 kích thước, summary + 6 biểu đồ; thiếu Crypto++ perf và run-time log; chốt 100/30 thay vì nominal 500/50 |
| Security discussion | 20 | 16 | Có S-box/cache, nonce, malleability, key wipe, AES-NI; chưa constant-time thực |
| **Core** | **100** | **92** | |
| Bonus A: AES-192/256 | +10 | +10 | Key schedule + KAT + crossval |
| Bonus B: XTS | +5 | +5 | GF tweak, CTS, KAT + crossval |
| Bonus C: AES-NI/bitsliced | +5 | 0 | Không làm |
| **Tổng** | **120** | **107 → 100** | Điểm bonus vượt 100 nhưng bị cap 100 |

Tự chấm bảo thủ: **92/100 core + 15 bonus = 107, cap 100**. Ma trận chốt là
1000/100/30 (N >= 30); nếu giảng viên bắt buộc đúng nominal 1000/500/50 hoặc
run-time environment log, điểm performance có thể thấp hơn. Các mục này đã
được ghi rõ để có thể chạy lại.

---

## 9. Hạn chế và việc còn lại

### 9.1 Đã hoàn thành trong lần sửa này

- Windows UTF-8 (`GetCommandLineW`, `u8path`) và test đường dẫn tiếng Việt.
- Windows evidence cho MSVC, MinGW GCC, MinGW Clang: KAT 9/9, CTest 4/4.
- KAT mở rộng lên 9 vector, gồm AES-192/256 CTR, partial block, XTS.
- KAT không hỗ trợ mode bị tính là skip-failure, không còn im lặng.
- Cross-validation bắt buộc khi `ENABLE_CROSSVAL=ON`.
- CLI thống nhất: `--encode`, `--decode`, `--nonce`, `--threads`,
  `--verbose`; từ chối xung đột và option lạ.
- `bench --verbose` in tiến độ, phần trăm, thời gian đã chạy và ETA ra stderr;
  script Linux/Windows tự bật `--verbose` để biết khi nào benchmark xong.
- Key zeroization, S-box computed để đối chiếu.
- Warm-up theo thời gian, script pin CPU/governor, Windows đủ 3 kích thước.
- Benchmark Windows MSVC 1000/100/30, summary + 6 biểu đồ; audit ghi rõ
  giới hạn số lần chạy và thiếu run-time log.
- Report này và cập nhật README/BENCHMARKS.

### 9.2 Chưa hoàn thành

1. **Ubuntu test logs:** chưa chạy lại trên Ubuntu; cần lệnh ở §4.5.
2. **Ubuntu benchmark mới:** dữ liệu Ubuntu hiện có dùng warm-up cũ, governor
   `powersave`, không pin CPU, 1 thread. Cần chạy lại theo §5.4 trước khi so
   sánh với Lab 1/Windows.
3. **Windows run-time log:** dữ liệu đã có là 1000/100/30 theo ma trận chốt;
   vẫn chưa có log ghi power plan/affinity/priority/threads.
4. **Crypto++ performance comparison:** chưa có target đo throughput
   Crypto++; cross-validation chỉ chứng minh đúng/sai.
5. **Ubuntu 1 GiB plot:** chưa tạo; lệnh ở §6.4. Windows đã có đủ 6 plot.
6. **PDF/DOCX:** chưa có; cần điền tên/MSSV/lớp và công cụ chuyển đổi.
7. **Streaming:** CLI vẫn đọc toàn bộ file vào bộ nhớ; 1 GiB cần khoảng 3 GiB
   peak. Chưa stream từng chunk.
8. **AES-NI/bitsliced (Bonus C):** không làm.
9. **Constant-time đầy đủ:** bảng S-box vẫn phụ thuộc dữ liệu; computed S-box
   chỉ là tham chiếu, chưa dùng trong core.

### 9.3 Kết luận

Lab 2 đã hoàn thành phần bắt buộc AES-128/CTR và hai bonus A/B, với kiểm
chứng KAT/cross-validation và bằng chứng Windows đầy đủ. Benchmark Windows
MSVC đã có đủ 3 kích thước, summary và biểu đồ; throughput 6.74–7.09 MiB/s,
CV 0.7%–6.3%. Điểm mạnh là core thuần C++ rõ ràng, CTR fail-closed, CLI thống
nhất, UTF-8 Windows và bộ KAT mở rộng. Điểm yếu là bằng chứng Ubuntu chưa được
làm mới, Windows thiếu run-time log và chưa có so sánh hiệu năng Crypto++,
chưa có phân tích constant-time thực nghiệm. Các việc này đã được liệt kê
kèm lệnh chạy lại để có thể hoàn thiện mà không tạo lại số liệu.

---

## 10. Tài liệu tham khảo

1. NIST, *FIPS 197 — Advanced Encryption Standard (AES)*, 2001 (updated 2023).
2. NIST, *SP 800-38A — Recommendation for Block Cipher Modes of Operation:
   Methods and Techniques*, 2001.
3. NIST, *SP 800-38A Addendum — Ciphertext Stealing*, 2010.
4. IEEE, *IEEE Std 1619-2007 — Standard for Cryptographic Protection of
   Data at Rest*, 2008.
5. NIST CAVP, *XTSGenAES128* test vectors.
6. D. J. Bernstein, “Cache-timing attacks on AES,” 2005.
7. D. A. Osvik, A. Shamir, E. Tromer, “Cache Attacks and Countermeasures:
   The Case of AES,” CT-RSA 2006.
8. Crypto++ Library 8.9.0, https://www.cryptopp.com/.
9. Catch2 v2.13.10, https://github.com/catchorg/Catch2.
10. `docs/Lab2.txt`, `docs/lab2_reference.md`, `docs/blueprint/labs/lab2.md`.
