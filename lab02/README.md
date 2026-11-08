# Lab 2 — AES-128 Implementation in Pure C++ (CTR Mode)

`aestool` is a pure C++17 implementation of AES (128/192/256) and the NIST
SP 800-38A CTR mode. CTR is the required Lab 2 mode; XTS is implemented as
Bonus B. No external cryptographic library is linked into `aes_core` or
`aestool`; OpenSSL is used only by the test-only `test_crossval` and
`bench_openssl` targets.

> **Status:** the code, Ubuntu KAT/CTest logs and benchmarks are current.
> Ubuntu GCC 15.2 KAT/CTest logs and the 1000/100/30 benchmark (4 threads,
> CPUs 0-3) were refreshed on 2026-11-05. Windows KAT/unit logs are valid, but
> the Windows CTest/cross-validation logs predate the OpenSSL switch and need a
> rerun. The script is console-only, so the Ubuntu run has no environment log.
> No benchmark numbers are fabricated.

---

## Directory structure

```
lab02/
├── CMakeLists.txt              CMake C++17, MSVC / GCC / Clang
├── README.md                   this file
├── REPORT.md                   Vietnamese lab report and self-grade
├── BENCHMARKS.md               benchmark matrix, protocol and CSV format
├── src/
│   ├── AES.hpp / AES.cpp       pure C++ FIPS-197 AES core
│   ├── modes.hpp / modes.cpp   CTR (required) + XTS (bonus B)
│   ├── minijson.hpp            zero-dependency JSON parser
│   └── main.cpp                aestool CLI
├── tests/
│   ├── catch.hpp               Catch2 v2.13.10 single header
│   ├── test_aes.cpp            FIPS-197, S-box, key validation
│   ├── test_ctr.cpp            SP 800-38A CTR, overflow, XTS
│   ├── test_crossval.cpp       OpenSSL oracle parity
│   ├── bench_openssl.cpp       OpenSSL throughput oracle (test-only)
│   ├── cli_test.sh             Linux CLI tests
│   └── cli_test.cmake          Windows CLI tests
├── kat/vectors.json            9 official/derived KAT vectors
├── scripts/                    build, benchmark, summarise, plot
└── output/
    ├── ubuntu/benchmarks/      Ubuntu GCC benchmark data, summary and combined plot
    ├── windows/test_results/   MSVC / MinGW GCC / MinGW Clang evidence
    └── windows/benchmarks/     Windows MSVC benchmark, summary, plots, audit
```

---

## Dependencies

| Dependency | Version | Notes |
|---|---|---|
| C++ standard | C++17 | GCC 15.2, Clang 21.1, MSVC 19.51, MinGW GCC 16.2 |
| CMake | ≥ 3.16 | verified with 3.29 on Windows and 4.x on Ubuntu |
| OpenSSL | 3.5.5+ | **oracle only**, linked to `test_crossval` and `bench_openssl` |
| Catch2 | 2.13.10 | vendored, offline |
| Python | 3.8+ | optional statistics/plots |

`ENABLE_CROSSVAL=ON` (default) makes CMake **fail** if OpenSSL is missing.
Use `-DENABLE_CROSSVAL=OFF` only for a correctness-only build; that build
does not satisfy the cross-validation requirement.

---

## Build and test

### Ubuntu

```bash
cd lab02
scripts/build_linux.sh gcc          # or: scripts/build_linux.sh clang
./build/ubuntu-gcc/aestool --kat kat/vectors.json
(cd build/ubuntu-gcc && ctest --output-on-failure)
```

Expected current counts: `9 passed, 0 failed` and `4/4` CTest tests.

### Windows

From “x64 Native Tools Command Prompt for VS”:

```bat
cd lab02
scripts\build_windows.bat msvc      rem or: gcc / clang
cd build\windows-msvc
ctest -C Release --output-on-failure
```

`build_windows.bat` auto-detects `D:\cryptolibrary` when present and passes
`-DOPENSSL_ROOT_DIR`. Headers live under `include\openssl`; the `libcrypto`
import library and DLL are found by CMake, and `bin` is added to `PATH` for
CTest. Override `OPENSSL_ROOT_DIR` if your install differs.

Windows evidence: MSVC, MinGW GCC and MinGW Clang each passed `9/9` KAT and
`4/4` CTest in the pre-OpenSSL run. Rebuild and rerun after the oracle switch;
see `output/windows/test_results/`.

---

## CLI

### CTR encryption and decryption

```bash
./build/ubuntu-gcc/aestool encrypt --mode ctr \
  --key-hex 2b7e151628aed2a6abf7158809cf4f3c \
  --iv-hex f0f1f2f3f4f5f6f7f8f9fafbfcfdfeff \
  --text "Hello World" --out ct.bin

./build/ubuntu-gcc/aestool decrypt --mode ctr \
  --key-hex 2b7e151628aed2a6abf7158809cf4f3c \
  --iv-hex f0f1f2f3f4f5f6f7f8f9fafbfcfdfeff \
  --in ct.bin --out msg.bin
```

`--nonce` and `--nonce-hex` are aliases for the CTR counter / XTS data unit.
`--key` + `--key-hex` together is rejected, as are multiple IV sources.

### Encodings

```bash
# Write base64 ciphertext, then decode it before decryption.
aestool encrypt --mode ctr --key-hex ... --iv-hex ... \
  --text "data" --encode base64 --out ct.b64
aestool decrypt --mode ctr --key-hex ... --iv-hex ... \
  --decode base64 --in ct.b64 --out pt.bin
```

With `--out` the default is raw bytes; without `--out` the default is hex.
`--out-hex` still prints hex to stdout.

### XTS (Bonus B)

The XTS key is `K1 || K2` (32/48/64 bytes); `--iv` is the 16-byte data unit.

```bash
./build/ubuntu-gcc/aestool encrypt --mode xts \
  --key-hex 000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f \
  --iv-hex 01000000000000000000000000000000 \
  --in msg.bin --out ct.bin
```

Sizes that are not a multiple of 16 bytes use IEEE P1619 ciphertext stealing.

### KAT

```bash
./build/ubuntu-gcc/aestool --kat kat/vectors.json
```

Expected: `9 passed, 0 failed`. Unsupported KAT modes are counted as skipped
and make the command exit nonzero.

### Benchmark

```bash
./build/ubuntu-gcc/aestool bench --mode ctr --size 1048576 --runs 1000 \
  --warmup 1.0 --threads 4 --verbose --out bench_1MB.csv
```

`--warmup` is wall-clock seconds on a cache-resident prefix; `--threads` only
applies to `bench` and defaults to 4 in the benchmark scripts (`THREADS=1`
gives the single-thread baseline). Unknown bench options are rejected. `--verbose` prints a
`progress:` line to **stderr** with runs completed, percentage, elapsed time
and ETA, plus a final `bench: completed ... in ...` line; the CSV on stdout or
`--out` is unchanged. The benchmark scripts enable `--verbose` automatically.

---

## Correctness and security summary

- FIPS-197 AES-128/192/256 transformations are manual; the state is
  column-major.
- CTR uses a 128-bit big-endian counter and fails closed on overflow.
- Partial final blocks are XORed without padding.
- The raw key is wiped and released after key expansion; `AES::~AES()` wipes
  the round keys with volatile writes.
- The S-box table is not constant-time; `AES::sbox_computed()` provides a
  table-free reference for analysis but is not used on the main path.
- CTR has no authentication and is malleable; use an AEAD or
  Encrypt-then-MAC when integrity is required.
- CTR/XTS IVs must be unique per key. The CLI cannot enforce this without
  shared state; the contract is documented in `REPORT.md` §7.2.
- CLI inputs are read whole-file. A 1 GiB input needs roughly 3 GiB peak;
  streaming is listed as remaining work.

See `REPORT.md` for the full design, test methodology, performance tables,
security analysis and self-grade.

---

## Evidence and limitations

| Item | Status |
|---|---|
| Windows MSVC / MinGW GCC / MinGW Clang KAT | 9/9 logged; valid (oracle-independent) |
| Windows MSVC / MinGW GCC / MinGW Clang CTest + cross-validation | pre-OpenSSL logs; rerun with OpenSSL |
| Windows MSVC benchmark | 1000/100/30 rows, summary + 6 plots, `AUDIT.md`; no run-time env log |
| Ubuntu test/KAT logs | committed, 9/9 KAT and 4/4 CTest on GCC 15.2 (2026-11-05) |
| Ubuntu benchmark CSV | 1000/100/30 on GCC 15.2 with 4 threads and CPUs 0-3; no environment log; 1 MiB CV 14.4% |
| OpenSSL throughput comparison | Ubuntu 4-thread data under `output/ubuntu/benchmarks/openssl/`; Windows not run |
| Plots | Windows per-size plots; Ubuntu one combined summary plot (all sizes) |
| PDF/DOCX report | not generated; identity fields blank; academic-integrity statement in `REPORT.md` Appendix A |

The Ubuntu run uses CPU pinning, 4 threads and the new time-based warm-up;
cite the 1 MiB point with its 14.4% CV caveat. The Windows matrix uses the
chosen 1000/100/30 counts; the blueprint's nominal 500/50 is optional.
