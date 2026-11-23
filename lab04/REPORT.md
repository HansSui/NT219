# BÁO CÁO LAB 4 — HÀM BĂM, PKI VÀ CÁC TẤN CÔNG THỰC TẾ

**Học phần:** NT219 — An toàn thông tin  
**Bài thực hành:** Lab 4 — Hashing, PKI and Practical Attacks  
**Sinh viên:** Nguyễn Công Tuấn  
**MSSV:** 25522012  
**Lớp:** NT219.R11.ANTT  
**Ngày hoàn thành:** 18/11/2026  
**Ngày cập nhật:** 19/11/2026

> Báo cáo chỉ ghi nhận những gì đã được chạy và lưu bằng chứng trong kho lưu
> trữ. Ba phần chưa có bằng chứng tại chỗ được nêu rõ: triển khai TLS cần
> hostname/khóa của người dùng, build Windows cần chạy trên máy Windows, và
> bonus chosen-prefix đầy đủ không khả thi trên CPU hiện tại. Không có kết quả
> nào được suy diễn thay cho phép đo.

---

## Mục lục

1. Tóm tắt điều hành
2. Mục tiêu và phạm vi
3. Thiết kế và cài đặt
4. Kiểm chứng tính đúng đắn
5. Phương pháp đo hiệu năng
6. Kết quả và phân tích hiệu năng
7. Phân tích bảo mật
8. Đối chiếu yêu cầu, rubric và tự chấm điểm
9. Bài học và kết luận
10. Tài liệu tham khảo
11. Phụ lục

---

## 1. Tóm tắt điều hành

Lab 4 yêu cầu một công cụ băm hỗ trợ toàn bộ SHA-2, SHA-3 và SHAKE, đọc tệp
theo luồng, kiểm tra vector KAT và xử lý lỗi theo hướng fail-closed; một thành
phần phân tích X.509 có xác minh chữ ký; cùng ba minh họa ngoại tuyến: va chạm
MD5, tấn công length-extension trên `MAC = H(k || m)`, và (tùy chọn) va chạm
chứng chỉ.

Sản phẩm là dự án C++17/CMake `lab04/` với CLI `hashtool`:

- **Băm:** SHA-224/256/384/512, SHA3-224/256/384/512, SHAKE128/256 với
  `--outlen` tính bằng bit. Primitive lấy từ OpenSSL EVP; SHA-256 được viết lại
  ở mức nhỏ để phục vụ demo length-extension.
- **I/O:** luôn đọc theo luồng 64 KiB; hỗ trợ thêm memory-mapped I/O cho
  benchmark. Stdout mặc định hex, ghi tệp mặc định raw.
- **X.509:** trích xuất subject, issuer, SPKI (thuật toán + tham số), thuật toán
  chữ ký, hiệu lực, key usage, SAN, thuật toán chữ ký trong TBS; xác minh một
  mắt xích khi có khóa issuer; báo `UNVERIFIED`/`NOT TRUSTED` khi không có.
- **Demo MD5:** hai ảnh PNG 64×64 hợp lệ, giống hệt điểm ảnh, có cùng MD5
  `c1a8659eeb61d1d1258f1a6e1e1b55e1`.
- **Demo length-extension:** MAC gốc `8c916ae4…e780`, MAC giả `082e543e…d5fe`,
  thông điệp giả 59 byte, không cần khóa.
- **Benchmark:** SHA-256 nhanh nhất, rồi SHA-512, SHA3-256, SHA3-512; CPU một
  lõi gần 100%; stream và mmap chênh lệch nhỏ trên dữ liệu đã nằm trong page
  cache.

Kết quả kiểm thử: `ctest` 5/5, KAT 31/31, CLI/negative đạt, trên Ubuntu GCC
15.2 với OpenSSL 4.0.2; bản Clang 21.1.8 cũng đạt 5/5. Chưa có bằng chứng
Windows và chưa triển khai TLS thật.

Điểm chưa hoàn thiện: TLS deployment, build Windows, bonus chosen-prefix đầy
đủ (+10). Bonus tự viết length-extension (+5) đã hoàn thành.

---

## 2. Mục tiêu và phạm vi

### 2.1 Mục tiêu

1. Cài đặt/sử dụng đúng SHA-2, SHA-3 và SHAKE, kể cả đầu ra biến thiên.
2. Băm tệp lớn theo luồng, không nạp toàn bộ vào RAM.
3. Kiểm tra vector KAT và từ chối tham số không hợp lệ.
4. Phân tích X.509 và xác minh chữ ký theo mô hình fail-closed.
5. Minh họa va chạm MD5 trong môi trường cô lập.
6. Minh họa length-extension trên một MAC ngây thơ và giải thích HMAC.
7. Đo hiệu năng bốn hàm băm, so sánh stream/mmap và SHA-2/SHA-3.
8. Thảo luận Merkle–Damgård vs sponge, collision vs preimage, XOF, X.509,
   chuỗi tin cậy, CT/CA-B, và lý do loại bỏ MD5/SHA-1.

### 2.2 Phạm vi

**Trong phạm vi:** CLI băm, KAT, X.509 parse/verify một mắt xích, demo MD5 bằng
HashClash, demo length-extension tự viết, benchmark và báo cáo.

**Ngoài phạm vi:** xây dựng toàn bộ chuỗi tin cậy/CRL/OCSP, kiểm tra hostname
và thời hạn theo policy, triển khai TLS thật (cần host), build Windows tại chỗ,
và tấn công chosen-prefix đầy đủ.

### 2.3 Yêu cầu kỹ thuật và cách đáp ứng

| Yêu cầu | Cài đặt | Bằng chứng | Trạng thái |
|---|---|---|---|
| SHA-2 đủ 4 biến thể | `hash_core.cpp`, EVP fetch | KAT, `test_hashes` | Đạt |
| SHA-3 đủ 4 biến thể | `hash_core.cpp` | KAT, `test_hashes` | Đạt |
| SHAKE128/256 + `--outlen` | `outputBits`, `EVP_DigestFinalXOF` | KAT, CLI test | Đạt |
| KAT | `kat/hash_vectors.json` 31 vector | `kat_2026-11-18.txt` | Đạt |
| Streamed I/O | đọc 64 KiB trong `hashStream` | `test_hashes`, CLI 1 MiB | Đạt |
| Hex console / raw file | `writeResult` | `cli_test.sh` | Đạt |
| Validate algorithm | `find`, `outputBits` | CLI negative | Đạt |
| Fail closed | ném lỗi, exit 1, không ghi output dở | `cli_test.sh` | Đạt |
| X.509 extract đủ trường | `x509_inspect.cpp` | `test_x509`, log cert | Đạt |
| Verify với issuer | `X509_verify` + so tên | `test_x509`, CLI | Đạt |
| Không có issuer → unverified | `UNVERIFIED`, `NOT TRUSTED` | `test_x509` | Đạt |
| Fail closed khi cần verified | `--require-verified` | CLI test | Đạt |
| TLS Apache/Nginx | snippet + script | `demos/tls/` | **Chưa triển khai** |
| MD5 collision | PNG pair + HashClash | `demos/md5_collision/` | Đạt |
| Length-extension | `sha256_ext.cpp` | `test_lenext`, demo | Đạt |
| Benchmark 4 hàm, 3 cỡ | `bench` + scripts | CSV/summary/plots | Đạt |
| Windows + Linux | script Windows | Ubuntu có log; Windows chưa chạy | Một phần |
| Bonus +5 length-extension tự viết | SHA-256 state continuation | `test_lenext` | Đạt |
| Bonus +10 chosen-prefix | fallback identical-prefix | `demos/chosen_prefix_cert/` | **Không đạt** |

---

## 3. Thiết kế và cài đặt

### 3.1 Cấu trúc

```
lab04/
├── CMakeLists.txt          CMake C++17; OpenSSL ≥ 3.0
├── README.md               build, CLI, hành vi, giới hạn
├── BENCHMARKS.md           môi trường, bảng đo, phân tích
├── REPORT.md               báo cáo này
├── src/
│   ├── hash_core.hpp/.cpp  SHA-2/SHA-3/SHAKE qua EVP, KAT, stream
│   ├── sha256_ext.hpp/.cpp SHA-256 tối giản + length extension
│   ├── x509_inspect.*      parse X.509, trích trường, verify
│   ├── io.hpp              mmap đọc-only (POSIX/Windows)
│   ├── util.hpp            hex/base64/file helper
│   ├── minijson.hpp        parser JSON tối giản cho KAT
│   └── main.cpp            CLI: hash, cert, lenext, bench, --kat
├── tests/
│   ├── test_hashes.cpp     thuật toán, outlen, stream/mmap
│   ├── test_x509.cpp       trường, verify, tamper, sai issuer
│   ├── test_lenext.cpp     vector SHA-256, biên padding
│   ├── cli_test.sh         CLI + negative trên Linux
│   ├── cli_test.cmake      bản tương đương cho Windows
│   └── catch.hpp           Catch2 2.13.10 vendored
├── kat/hash_vectors.json   31 vector FIPS
├── demos/
│   ├── md5_collision/      PNG pair, README, run_demo.sh
│   ├── length_extension/   MAC gốc/giả, padding diagram, script
│   ├── chosen_prefix_cert/ fallback, make_certs.cpp, script
│   └── tls/                snippet nginx/Apache, collector
├── scripts/
│   ├── build_linux.sh      GCC/Clang + ctest
│   ├── build_windows.bat   MSVC/MinGW + ctest + log
│   ├── run_benchmarks.*    benchmark + summary + plots
│   ├── summarize_bench.py  mean/median/SD/CI95
│   └── plot_bench.py       grouped bar charts
└── output/                 log và CSV (Ubuntu; Windows do script tạo)
```

### 3.2 Lõi băm

Mỗi thuật toán được ánh xạ sang tên OpenSSL (`SHA2-256`, `SHA3-256`,
`SHAKE128`, …), lấy bằng `EVP_MD_fetch` để dùng provider hiện hành. Tên người
dùng được chuẩn hóa: không phân biệt hoa thường, `_` tương đương `-`. MD5 và
SHA-1 không có trong bảng nên bị từ chối ở tầng tên thuật toán.

- Hàm cố định: `EVP_DigestFinal_ex`, kích thước đầu ra do thuật toán quyết
  định; truyền `--outlen` cho hàm cố định là lỗi.
- SHAKE: bắt buộc `--outlen`, là bội số của 8 trong khoảng 1…8.388.608 bit;
  `EVP_DigestFinalXOF` sinh đúng số byte yêu cầu.
- `hashStream` đọc từng khối 64 KiB, nên bộ nhớ không tăng theo kích thước
  tệp. `--stream` được chấp nhận như dạng tường minh của cùng đường xử lý.
- `hashFile(..., mmap=true)` dùng `MappedFile` chỉ đọc, cập nhật theo khối
  1 MiB. Trên Windows dùng `CreateFileW`/`MapViewOfFile` với đường dẫn UTF-8
  được đổi sang wide; trên POSIX dùng `open`/`mmap`.

`--in` là nhị phân; `--text` dành cho chuỗi UTF-8. Đúng một trong hai được
phép. Kết quả ra stdout là hex; `--out` mặc định raw; `--encode
hex|base64|raw` ghi đè. Cách này giữ cho digest 32 byte không bị in thành ký
tự rác khi ghi tệp.

### 3.3 X.509

`parseCertificate` thử PEM trước, rồi DER, và từ chối dữ liệu thừa sau DER
(kiểm tra con trỏ cuối). Các trường được lấy bằng API OpenSSL:

| Trường | API |
|---|---|
| Subject / Issuer | `X509_get_subject_name`, `X509_get_issuer_name` + `XN_FLAG_RFC2253` |
| SPKI algorithm | `X509_PUBKEY_get0_param` → OID |
| SPKI parameters | RSA: `EVP_PKEY_get_bn_param` (N, E); EC: group name |
| Signature algorithm | `X509_get0_signature` → `X509_ALGOR` |
| Validity | `X509_get0_notBefore/notAfter` |
| Key usage | `X509_get_ext_d2i(NID_key_usage)` |
| SAN | `X509_get_ext_d2i(NID_subject_alt_name)` + `GENERAL_NAME_print` |
| TBS signature alg | `X509_get0_tbs_sigalg` |

Kiểm tra cấu trúc gồm: có đủ thuật toán chữ ký ngoài và trong TBS; hai
`X509_ALGOR` phải giống nhau (`X509_ALGOR_cmp`); OID chữ ký phải tương thích
loại khóa công khai (RSA/RSA-PSS, EC, Ed25519/Ed448, DSA); OID lạ bị từ chối
theo hướng fail-closed.

Khi có `--issuer`:

1. Nếu issuer là chứng chỉ, tên issuer của leaf phải bằng subject của issuer.
2. `X509_verify(leaf, issuer_key)` phải trả 1.
3. Báo `issuer_name: match` và `signature: verified against supplied issuer key`.

Khi không có issuer: exit 0 nhưng báo `signature: UNVERIFIED`, `trust: NOT
TRUSTED`; `--require-verified` biến trạng thái này thành lỗi. Đây là điểm quan
trọng: parse được không đồng nghĩa với tin được. Chain/root/CRL/OCSP/hostname
không được đánh giá và báo cáo nói rõ.

### 3.4 Length-extension

`sha256_ext.cpp` cài SHA-256 chuẩn (64 vòng, hằng số K, padding Merkle–Damgård)
để tái tạo trạng thái. Với `MAC = SHA256(k || m)`:

1. MAC chính là 8 từ trạng thái sau khi xử lý khối đã pad của `k || m`.
2. Tính padding của `k || m` theo độ dài `key_len + len(m)`.
3. Đặt `state = MAC`, `bit_length = (key_len + len(m) + len(pad)) * 8`.
4. Nén tiếp `m'`, rồi pad và kết thúc như SHA-256 bình thường.

Kết quả là `SHA256(k || m || pad || m')` mà không cần biết `k`. Test kiểm tra
các biên padding `0, 1, 55, 56, 63, 64, 120` byte và so với SHA-256 thật trên
`k || forged`.

### 3.5 Demo MD5

`md5_fastcoll` (HashClash v1.2b) sinh hai khối 64 byte va chạm cho một prefix
cố định. Ta tạo một PNG 64×64 RGBA hợp lệ, pad đến bội số 64 byte, rồi để
fastcoll nối hai khối va chạm. Hai tệp kết quả:

- giống nhau đến byte cuối của `IEND`;
- khác nhau 128 byte sau `IEND` (PNG decoder bỏ qua);
- có cùng MD5.

Đây là **identical-prefix collision**: prefix phải giống nhau. Với chứng chỉ,
`make_certs.cpp` căn TBS sao cho vùng extension bắt đầu ở offset chia hết cho
64, gọi fastcoll với chính prefix TBS đó, rồi ký MD5/RSA. Hai TBS khác nhau
nhưng cùng MD5, nên cùng một chữ ký RSA hợp lệ cho cả hai.

### 3.6 CLI và fail-closed

`parseOptions` từ chối lệnh/flag lạ. Các kiểm tra chính:

- `hash`: đúng một `--in`/`--text`; `--algo` bắt buộc.
- `lenext`: `--mac` hex 32 byte; `--key-len` bắt buộc; đúng một `--in`/
  `--text` và một `--append`/`--append-text`.
- `cert`: `--in` bắt buộc; issuer sai tên/chữ ký → lỗi.
- `bench`: kích thước 1…4096 MiB, `--io` chỉ `stream`/`mmap`, `--runs` dương.
- Mọi lỗi in `error: …` ra stderr và trả mã 1; không ghi tệp kết quả dở dang.

---

## 4. Kiểm chứng tính đúng đắn

### 4.1 KAT

`kat/hash_vectors.json` có 31 vector từ ví dụ FIPS 180-4/FIPS 202 (sinh độc
lập bằng Python `hashlib`) cho: rỗng, `abc`, câu fox, khối biên SHA-2, dữ liệu
nhị phân 0…255, và SHAKE với nhiều độ dài. Kết quả:

```
31 passed, 0 failed
```

Log: `output/ubuntu/test_results/kat_2026-11-18.txt`.

### 4.2 Unit test và CLI

`ctest` 5/5:

| Test | Nội dung | Kết quả |
|---|---|---|
| `hashes` | kích thước đầu ra, vector NIST, tên thuật toán, outlen, stream/mmap, lỗi tệp | Pass |
| `x509` | trích đủ trường, verify PEM/DER/pubkey, tamper, sai issuer, malformed | Pass |
| `lenext` | vector SHA-256, biên padding, MAC sai độ dài | Pass |
| `kat` | chạy file vector | Pass |
| `cli` | hash/encode/outlen/lenext/cert/bench + negative | Pass |

Log: `output/ubuntu/test_results/ctest_2026-11-18.txt`. Test âm bao gồm:
`md5`, `sha1`, `--outlen` cho hàm cố định, SHAKE thiếu outlen, outlen 7, tệp
không tồn tại, encoding lạ, MAC ngắn, chứng chỉ hỏng, issuer sai, và
`--require-verified` khi không có khóa.

### 4.3 X.509 thủ công

Với chứng chỉ tự ký `CN=lab4.example`:

```
signature: UNVERIFIED (issuer key not provided)
trust: NOT TRUSTED (signature unverified)
```

Khi thêm `--issuer cert.pem --require-verified`:

```
signature: verified against supplied issuer key
issuer_name: match
trust: signature verified; chain/root trust NOT evaluated
```

Issuer khác tên hoặc khóa sai → exit 1. Chứng chỉ bị sửa → exit 1. Log nằm
trong `cli_2026-11-18.txt` (phần CLI test tự sinh chứng chỉ bằng OpenSSL CLI).

### 4.4 Demo

- **MD5:** `md5sum` hai PNG giống nhau; script kiểm tra CRC từng chunk PNG và
  CRC qua `IEND`, xác nhận 128 byte dư bị bỏ qua.
- **Length-extension:** MAC giả được tính lại từ `key || forged_message` bằng
  `hashtool --algo sha256` và khớp từng byte.
- **Chứng chỉ MD5:** hai TBS có MD5 `775c78b6…ef4c`, chữ ký RSA bắt đầu
  `5b:99:3f:b6…` giống nhau; `openssl x509 -text` chỉ khác vùng extension.
  `hashtool cert --in certB.pem --issuer certA.pem --require-verified` báo
  `md5WithRSAEncryption`, `algorithm_consistency: match` và
  `signature: verified against supplied issuer key` — đúng hệ quả nguy hiểm
  cần chứng minh. Đây là fallback identical-prefix, không phải chosen-prefix.

### 4.5 Build

| Môi trường | Kết quả |
|---|---|
| Ubuntu 26.04, GCC 15.2.0, OpenSSL 4.0.2 | build + `ctest` 5/5 |
| Ubuntu 26.04, Clang 21.1.8, OpenSSL 4.0.2 | build + `ctest` 5/5 |
| Ubuntu, GCC, OpenSSL 3.5.5 (system) | build + `ctest` 5/5 (kiểm tra tương thích) |
| Windows MSVC/MinGW | script sẵn sàng; **chưa chạy trên máy Windows** |

---

## 5. Phương pháp đo hiệu năng

### 5.1 Môi trường

| Thành phần | Giá trị |
|---|---|
| CPU | 11th Gen Intel Core i5-1135G7 @ 2.40 GHz, 4 nhân / 8 luồng |
| RAM | 22 GiB |
| OS | Ubuntu 26.04.1 LTS, kernel 7.0.0-43-generic |
| Compiler | GCC 15.2.0, `-O3` (CMake Release) |
| OpenSSL | 4.0.2 (`libcrypto.so.4`, prefix `~/CryptoLib/lib/openssl/gcc`) |
| CMake | 4.2.3 |
| Ngày đo | 18/11/2026 |

### 5.2 Cấu hình

- Thuật toán: `sha256,sha512,sha3-256,sha3-512`.
- Kích thước: 1 MiB, 100 MiB, 1024 MiB (xấp xỉ 1 GiB).
- I/O: `stream` (đọc 64 KiB) và `mmap` (ánh xạ rồi cập nhật 1 MiB).
- Warm-up 1 giây cho mỗi tổ hợp; đo 1000 lần (1 MiB), 100 lần (100 MiB),
  30 lần (1 GiB). Chạy đơn luồng, không ghim CPU.
- Mỗi lần đo dùng `steady_clock` cho thời gian thực và `std::clock` cho CPU
  process; throughput = kích thước / thời gian.
- Thống kê: mean, median, độ lệch chuẩn mẫu, CI95 = `1.96·sd/√n`.
- Lệnh:

```bash
cd lab04
OPENSSL_ROOT_DIR="$HOME/CryptoLib/lib/openssl/gcc" scripts/run_benchmarks.sh gcc
```

Script ghi CSV thô, CSV tóm tắt và bốn đồ thị vào
`output/ubuntu/benchmarks/`. Tệp benchmark nằm trong `/tmp/hashtool_bench`;
`/tmp` là tmpfs (12 GiB) trên host này, được tạo một lần và dùng lại, nên mọi
lần đo đều đọc từ RAM. Đây là benchmark **CPU-bound, cache ấm**, không phải đo
cold storage hay tốc độ ổ đĩa.

### 5.3 Cách đọc kết quả

- `elapsed_ms` là thời gian một lần băm toàn bộ tệp.
- `mbps` dùng MB = 10^6 byte, nên 1 MiB ≈ 1.048576 MB.
- `cpu_pct` gần 100% nghĩa là một lõi bận gần như toàn bộ; không có song song
  trong một lần băm.
- So sánh stream/mmap chỉ có ý nghĩa trong cùng tổ hợp; chênh lệch nhỏ hơn
  khoảng 5–10% nên được coi là nhiễu hoặc chi phí copy.

---

## 6. Kết quả và phân tích hiệu năng

Bảng dưới là throughput trung bình (MB/s) kèm CI95; MB = 10^6 byte.

| Thuật toán | Cỡ | stream MB/s | mmap MB/s | mmap so với stream | CPU % (cao nhất) |
|---|---:|---:|---:|---:|---:|
| SHA-256 | 1 MiB | 1519 ± 7 | 1510 ± 8 | -0.6% | 100.0 |
| SHA-256 | 100 MiB | 1421 ± 5 | 1536 ± 7 | +8.1% | 100.0 |
| SHA-256 | 1024 MiB | 1370 ± 19 | 1440 ± 20 | +5.1% | 100.0 |
| SHA-512 | 1 MiB | 685 ± 3 | 674 ± 3 | -1.7% | 99.9 |
| SHA-512 | 100 MiB | 672 ± 3 | 701 ± 2 | +4.3% | 100.0 |
| SHA-512 | 1024 MiB | 639 ± 8 | 663 ± 6 | +3.9% | 100.0 |
| SHA3-256 | 1 MiB | 430 ± 2 | 437 ± 2 | +1.7% | 100.0 |
| SHA3-256 | 100 MiB | 402 ± 4 | 402 ± 3 | -0.2% | 100.0 |
| SHA3-256 | 1024 MiB | 386 ± 3 | 405 ± 4 | +4.9% | 100.0 |
| SHA3-512 | 1 MiB | 234 ± 1 | 238 ± 1 | +1.9% | 100.0 |
| SHA3-512 | 100 MiB | 224 ± 2 | 222 ± 1 | -1.1% | 99.9 |
| SHA3-512 | 1024 MiB | 216 ± 2 | 216 ± 2 | +0.1% | 99.9 |

**Thứ hạng.** SHA-256 nhanh nhất ở mọi cỡ, khoảng 1,37–1,44 GB/s ở trường hợp
1 GiB. SHA-512 đạt 0,64–0,66 GB/s (khoảng 0,46 lần SHA-256), SHA3-256
0,39–0,40 GB/s (0,28 lần), SHA3-512 0,216 GB/s (0,16 lần). Lợi thế của SHA-256
không chỉ do thuật toán: OpenSSL dùng tập lệnh SHA của x86 (`sha_ni`) cho
SHA-256, còn SHA-3 là hoán vị Keccak-f[1600] không có tập lệnh tương đương
trên CPU này.

**Stream so với mmap.** Ở 1 MiB, hai đường chênh dưới 2% — nhỏ hơn khoảng CI95
nên không coi là khác biệt thật. Ở 100 MiB và 1 GiB, mmap nhanh hơn 4–8% với
SHA-256 và SHA-512 vì đường stream phải copy từng khối 64 KiB vào buffer,
trong khi mmap băm trực tiếp trên trang page cache. SHA-3 gần như phẳng
(trong khoảng 5%): hoán vị Keccak chiếm phần lớn thời gian nên bỏ một lần copy
không thay đổi đáng kể. Trường hợp SHA3-256 1 GiB stream (386 so với 405 MB/s
mmap) là khoảng cách lớn nhất trong các dòng SHA-3, phù hợp với chi phí copy
này.

**CPU.** Mọi tổ hợp đều dùng khoảng 100% một lõi. Công cụ băm trên luồng gọi,
nên con số này xác nhận không có song song ẩn và kết quả là throughput một
lõi.

**Giới hạn diễn giải.** `/tmp` là tmpfs nên mọi lần đọc đều từ RAM; đây là kết
quả cache ấm, thiên về CPU, không nói gì về tốc độ đĩa. Host dùng chung và
không ghim tần số CPU, nên MB/s tuyệt đối chỉ mang tính tham khảo. CI95 cho
thấy trung bình trong một lần chạy ổn định, nhưng không phản ánh dao động tần
số giữa các lần chạy.

Bốn đồ thị trong `output/ubuntu/benchmarks/graphs/` vẽ cùng dữ liệu này theo
throughput và latency cho từng đường I/O. Bảng trên và các đồ thị đều sinh
trực tiếp từ `summary_ubuntu_gcc.csv` / `bench_ubuntu_gcc.csv`, không chỉnh
tay.

---

## 7. Phân tích bảo mật

### 7.1 Merkle–Damgård và sponge

SHA-2 dùng cấu trúc Merkle–Damgård: chia thông điệp thành khối, nén từng khối
vào một state cố định, và state cuối (sau padding) chính là digest (hoặc một
phần của nó). Cấu trúc này cho phép xử lý streaming tự nhiên, nhưng cũng tạo
ra length-extension: kẻ tấn công đã biết state cuối có thể nối thêm dữ liệu.

SHA-3 dùng sponge: state lớn (1600 bit), chia thành rate và capacity; hấp thụ
từng khối vào rate rồi hoán vị bằng Keccak-f[1600], sau đó vắt ra đầu ra.
Capacity quyết định độ an toàn; rate quyết định tốc độ. Vì state không bị lộ
trực tiếp như digest Merkle–Damgård và có domain separation, sponge không cho
length-extension kiểu `H(k || m)`.

### 7.2 Collision, preimage và second-preimage

- **Preimage:** cho `h`, tìm `m` sao cho `H(m) = h`. Với hash `n` bit, chi phí
  kỳ vọng `2^n`.
- **Second-preimage:** cho `m`, tìm `m' ≠ m` cùng digest. Cũng khoảng `2^n`.
- **Collision:** tìm bất kỳ cặp `m ≠ m'` cùng digest. Do birthday bound, chi
  phí khoảng `2^(n/2)`.

MD5 có collision thực tế từ 2004 (khoảng `2^39` phép nén), dù preimage vẫn
khó. SHA-1 có collision thực tế (SHAttered, 2017). SHA-256/384/512 và SHA-3
chưa có collision công khai; giới hạn an toàn tương ứng 128/192/256 bit với
collision.

### 7.3 Vì sao MD5/SHA-1 bị cấm

MD5 và SHA-1 không còn đáp ứng collision resistance. Hệ quả:

- Chứng chỉ và chữ ký số: hai TBS khác nhau có thể dùng chung một chữ ký, như
  demo trong `demos/chosen_prefix_cert/`.
- Toàn vẹn tệp: kẻ tấn công có thể thay nội dung mà giữ nguyên checksum MD5.
- Giao thức: TLS, S/MIME, code signing và CA/B Forum đã loại MD5/SHA-1 khỏi
  chữ ký; trình duyệt từ chối chứng chỉ SHA-1.

Bài học: không dùng MD5/SHA-1 cho mục đích chống giả mạo, kể cả khi chỉ thấy
chúng trong checksum.

### 7.4 XOF và domain separation

SHAKE là extendable-output function: đầu ra dài tùy ý, an toàn như một hàm băm
với cùng capacity. Ứng dụng:

- Sinh khóa/nonce/IV từ một seed với độ dài khác nhau (KDF).
- Tạo nhiều đầu ra độc lập từ một bí mật, kèm nhãn domain separation
  (`SHAKE128("enc" || seed)`, `SHAKE128("mac" || seed)`), tránh tái sử dụng
  cùng dòng bit cho hai mục đích.
- Cam kết/merkle tree, tạo salt và thử nghiệm hậu lượng tử.

`--outlen` là bit, không phải byte; điều này khớp với đặc tả FIPS 202 và tránh
nhầm lẫn khi tài liệu dùng bit.

### 7.5 X.509: TBS và chữ ký

Chứng chỉ X.509 gồm:

```
Certificate
  tbsCertificate        <- subject, issuer, validity, SPKI, extensions
  signatureAlgorithm    <- OID + parameters
  signatureValue        <- Sign(issuer_private_key, DER(tbsCertificate))
```

Bên xác minh băm phần TBS (theo thuật toán trong `signatureAlgorithm`) rồi
kiểm tra chữ ký bằng khóa công khai của issuer. Vì vậy mọi thay đổi trong TBS
(subject, SAN, key usage, extension) đều làm chữ ký sai. Chữ ký không bảo vệ
chính trường `signatureAlgorithm` nếu kẻ tấn công thay đổi nó mà không làm
TBS thay đổi — do đó cần kiểm tra thuật toán ngoài và trong TBS khớp nhau, như
`hashtool` làm.

### 7.6 Chuỗi tin cậy

Trình duyệt/OS có một tập root CA tin cậy. Server gửi leaf + intermediate
(thường không gửi root). Xác minh:

1. Chữ ký leaf bằng khóa intermediate.
2. Chữ ký intermediate bằng khóa root.
3. Root nằm trong trust store.
4. Tên khớp, thời hạn hợp lệ, SAN khớp hostname.
5. Không bị thu hồi (CRL/OCSP) và tuân thủ CA/B Baseline Requirements.

`hashtool cert --issuer` chỉ làm bước 1 (và so tên issuer). Báo cáo ghi rõ
`chain/root trust NOT evaluated`; đây là lý do một chứng chỉ "verify thành
công" trong công cụ vẫn chưa chắc được trình duyệt chấp nhận.

### 7.7 Certificate Transparency và CA/B

Certificate Transparency (CT) ghi chứng chỉ vào các log công khai chỉ-append,
cho phép giám sát và phát hiện chứng chỉ giả. CA/Browser Forum Baseline
Requirements quy định thuật toán, thời hạn, key usage, validation và logging.
Các biện pháp này không sửa được hash yếu, nhưng làm giảm khả năng kẻ tấn công
dùng collision để tạo chứng chỉ thứ hai mà không bị phát hiện: chứng chỉ thứ
hai sẽ xuất hiện trong log hoặc không có SCT hợp lệ.

### 7.8 Length-extension và HMAC

Với `MAC = H(k || m)` trên Merkle–Damgård, MAC chính là state sau khi xử lý
`k || m || pad`. Kẻ tấn công biết MAC, biết `len(k)` (hoặc thử vài giá trị),
có thể đặt state đó làm IV và nén tiếp `m'`. Kết quả là MAC hợp lệ cho
`m || pad || m'` mà không cần khóa.

HMAC sửa lỗi này bằng hai lớp:

```
HMAC(k, m) = H((k' ⊕ opad) || H((k' ⊕ ipad) || m))
```

Lớp trong che state sau khi xử lý khóa; lớp ngoài băm lại toàn bộ, nên không
thể nối thêm. Ngoài ra có thể dùng:

- Keyed sponge/KMAC: khóa được hấp thụ trước, có domain separation.
- Prefix-free encoding hoặc MAC dựa trên PRF (AES-CMAC).
- Encrypt-then-MAC với MAC độc lập.

### 7.9 Chosen-prefix và giới hạn của demo

Demo chứng chỉ đã nộp là **identical-prefix**: hai TBS có cùng prefix đến hết
vùng extension, chỉ khác khối va chạm. Nó chứng minh hậu quả của MD5 (một chữ
ký cho hai TBS) nhưng không chứng minh khả năng chọn hai danh tính khác nhau.

Chosen-prefix MD5 cần tìm hai prefix tùy ý dẫn đến cùng state sau vài khối
near-collision; chi phí thực tế khoảng `2^39` phép nén và cần công cụ chuyên
dụng (HashClash `cpc.sh`, có thể cần GPU/CPU lâu). Trên host 4 nhân này không
khả thi trong thời gian lab, nên bonus +10 **không được yêu cầu**. Báo cáo
không tuyên bố đã đạt bonus này.

### 7.10 Mô hình đe dọa của công cụ

- `hashtool` không nhận khóa bí mật qua CLI (trừ demo MAC giả lập); không ghi
  khóa ra đĩa.
- `cert` chỉ đọc; không kết nối mạng.
- Demo MD5/length-extension chạy trên tệp do người dùng tạo, không nhắm mục
  tiêu thật.
- `bench` tạo tệp tạm dung lượng lớn; cần quyền ghi thư mục tạm.
- Đầu vào X.509 không đáng tin được parse bằng OpenSSL; mọi lỗi cấu trúc đều
  dẫn đến exit 1. Tuy vậy, đây không phải bộ xác minh chuỗi đầy đủ.

---

## 8. Đối chiếu yêu cầu, rubric và tự chấm điểm

### 8.1 Bảng tự chấm

| Tiêu chí | Điểm tối đa | Tự chấm | Căn cứ |
|---|---:|---:|---|
| Hash suite đúng + KAT | 25 | 25 | 10 thuật toán, KAT 31/31, test outlen, CLI negative |
| Streaming + hiệu năng tệp lớn | 10 | 10 | đọc 64 KiB, benchmark 1 GiB, phân tích stream/mmap |
| X.509 parse + verify | 25 | 17 | đủ trường, verify/fail-closed, test; **thiếu TLS deployment** |
| MD5 collision + giải thích | 20 | 20 | PNG pair, digest, README, tái lập |
| Length-extension + mitigation | 20 | 20 | MAC giả, padding diagram, HMAC, test |
| **Cộng** | **100** | **92** | |
| Bonus: tự viết length-extension | +5 | +5 | SHA-256 state reconstruction, test biên |
| Bonus: chosen-prefix certificate | +10 | 0 | chỉ có fallback identical-prefix, không tuyên bố |
| **Tổng** | **115** | **97** | |

Việc trừ 8 điểm ở tiêu chí X.509 phản ánh đúng thực tế: mã parse/verify và
kiểm thử đã có, nhưng rubric yêu cầu "TLS deployment delivered" và hiện chưa
có hostname/khóa để triển khai. Khi bổ sung log TLS thật, phần này có thể đạt
25/25 và tổng là 105 (100 core + 5 bonus).

### 8.2 Checklist definition of done

- [x] Build Ubuntu, đủ thuật toán, SHAKE outlen.
- [x] KAT đủ thuật toán; `ctest` 5/5.
- [x] Stream tệp lớn (1 GiB trong benchmark; không nạp toàn bộ).
- [x] X.509 trích đủ trường; verify hoặc fail closed.
- [ ] TLS deployment + screenshot/log + config + trust chain.
- [x] MD5 collision: hai tệp, digest, giải thích.
- [x] Length-extension: thông điệp gốc/giả, MAC, padding, mitigation.
- [x] Benchmark 4 hàm, 3 cỡ, đồ thị.
- [x] README CLI/build/dependency/limitations.
- [x] Self-grade + integrity statement.
- [ ] Windows build/test evidence.
- [ ] Bonus chosen-prefix đầy đủ.

### 8.3 Tuyên bố liêm chính

Các tệp PNG, chứng chỉ, khóa demo và MAC được tạo cục bộ cho bài lab này. Không
sử dụng dữ liệu của bên thứ ba, không tấn công hệ thống thật. HashClash được
dùng ở chế độ ngoại tuyến trên tệp do người viết tạo. Mã nguồn tham khảo cấu
trúc Lab 1–3 của cùng tác giả; phần SHA-256 length-extension được viết cho bài
này. Các số liệu benchmark lấy từ CSV do `hashtool` sinh, không chỉnh sửa.

---

## 9. Bài học và kết luận

1. **Hash không chỉ là "băm ra chuỗi hex".** Cùng một primitive nhưng cách
   dùng (MAC ngây thơ, chữ ký trên TBS, checksum tệp) quyết định an toàn.
2. **Streaming là yêu cầu thiết kế, không phải tối ưu nhỏ.** Băm 1 GiB với
   `EVP_DigestUpdate` từng khối giữ RAM ổn định; mmap chỉ giúp giảm copy.
3. **SHA-3 không nhanh hơn SHA-2 trên CPU này.** SHA-256 được tăng tốc bằng
   SHA-NI; SHA-3 dựa trên hoán vị Keccak và chậm hơn, đặc biệt SHA3-512 do
   rate nhỏ hơn.
4. **X.509 là bài toán tin cậy, không chỉ parse.** Trích đủ trường vẫn chưa
   đủ; phải xác minh chữ ký, kiểm tra thuật toán, và biết mình chưa kiểm tra
   chain/root.
5. **MD5 chết vì collision, không phải vì preimage.** Demo hai ảnh và hai
   chứng chỉ cho thấy hậu quả trực tiếp.
6. **Length-extension là lỗi của cách xây MAC.** SHA-256 vẫn tốt; `H(k||m)`
   mới là sai. HMAC là câu trả lời chuẩn.
7. **Trung thực về giới hạn:** không có TLS, không có Windows, không có
   chosen-prefix đầy đủ. Báo cáo và điểm tự chấm phản ánh đúng các khoảng
   trống này.

---

## 10. Tài liệu tham khảo

1. NIST FIPS 180-4 — Secure Hash Standard (SHA-1/2).
2. NIST FIPS 202 — SHA-3 Standard: Permutation-Based Hash and Extendable-Output
   Functions.
3. NIST SP 800-185 — SHA-3 Derived Functions (cSHAKE, KMAC, TupleHash).
4. RFC 2104 — HMAC: Keyed-Hashing for Message Authentication.
5. RFC 5280 — Internet X.509 Public Key Infrastructure Certificate and CRL
   Profile.
6. RFC 8446 — TLS 1.3.
7. CA/Browser Forum Baseline Requirements.
8. Wang, Yin, Yu — "Finding Collisions in the Full SHA-1", CRYPTO 2005.
9. Wang, Feng, Lai, Yu — "Collisions for Hash Functions MD4, MD5, HAVAL-128 and
   RIPEMD", CRYPTO 2004.
10. Stevens, Lenstra, de Weger — "Chosen-Prefix Collisions for MD5 and
    Colliding X.509 Certificates for Different Identities", EUROCRYPT 2007.
11. Stevens, Bursztein, Karpman, Albertini, Markov — "The first collision for
    full SHA-1" (SHAttered), 2017.
12. OpenSSL 3.x/4.x documentation — EVP, X509, PEM, BIO.
13. HashClash — https://github.com/cr-marcstevens/hashclash.

---

## 11. Phụ lục

### 11.1 Lệnh tái lập nhanh

```bash
cd lab04
export OPENSSL_ROOT_DIR="$HOME/CryptoLib/lib/openssl/gcc"
scripts/build_linux.sh gcc

./build/ubuntu-gcc/hashtool --algo sha256 --text abc
./build/ubuntu-gcc/hashtool --algo shake256 --outlen 64 --text abc
./build/ubuntu-gcc/hashtool --kat kat/hash_vectors.json

./demos/md5_collision/run_demo.sh
./demos/length_extension/run_demo.sh
./demos/chosen_prefix_cert/run_demo.sh

scripts/run_benchmarks.sh gcc
```

### 11.2 Danh mục bằng chứng

| Tệp | Nội dung |
|---|---|
| `output/ubuntu/test_results/ctest_2026-11-18.txt` | CTest 5/5 |
| `output/ubuntu/test_results/kat_2026-11-18.txt` | KAT 31/31 |
| `output/ubuntu/test_results/cli_2026-11-18.txt` | CLI, negative, edge cases |
| `output/ubuntu/test_results/demos_2026-11-18.txt` | log ba demo + xác minh cặp chứng chỉ đã nộp |
| `output/ubuntu/test_results/build_gcc.log`, `build_clang.log`, `build_gcc_system_openssl.log` | log build/CTest GCC, Clang, system OpenSSL 3.5.5 |
| `output/ubuntu/test_results/environment.txt` | CPU/OS/compiler/OpenSSL |
| `output/ubuntu/benchmarks/bench_run_log.txt` | log lần chạy benchmark cuối |
| `analysis/benchmark_analysis.ipynb` | notebook Jupyter (bảng, xếp hạng, 4 đồ thị; đã chạy) |
| `output/ubuntu/benchmarks/bench_ubuntu_gcc.csv` | CSV thô từng lần đo |
| `output/ubuntu/benchmarks/summary_ubuntu_gcc.csv` | mean/median/SD/CI95 |
| `output/ubuntu/benchmarks/graphs/*.png` | 4 đồ thị throughput/latency |
| `demos/md5_collision/png1.png`, `png2.png` | cặp va chạm MD5 |
| `demos/length_extension/*` | MAC gốc/giả, thông điệp, báo cáo |
| `demos/chosen_prefix_cert/certA.pem`, `certB.pem` | fallback chứng chỉ MD5 |
| `demos/tls/*` | snippet + script thu bằng chứng (chưa chạy) |

### 11.3 Ghi chú về môi trường

- `OPENSSL_CONF` của shell hiện tại trỏ tới đường dẫn không tồn tại; các lệnh
  OpenSSL CLI trong test đặt `OPENSSL_CONF=/etc/ssl/openssl.cnf`. `hashtool`
  không đọc biến này.
- Bản Ubuntu chính dùng OpenSSL 4.0.2 tại `$HOME/CryptoLib/lib/openssl/gcc`;
  bản system 3.5.5 cũng được build thử để kiểm tra CMake ≥ 3.0.
- Benchmark không ghim CPU; các số là quan sát trên một host, không phải so
  sánh có kiểm soát với Windows.
