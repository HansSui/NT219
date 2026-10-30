# Lab 2 — AES-128 Implementation in Pure C++ (CTR Mode)

`aestool` is a pure C++ implementation of the AES block cipher (AES-128, AES-192, AES-256) and the NIST SP 800-38A CTR (Counter) mode stream cipher. CTR is the only mode required by Lab 2; XTS is implemented as Bonus B.

In strict adherence to the `Lab2.txt` assignment specification:
* **Zero external cryptographic libraries are used for primitives.** All core logic (`SubBytes`, `ShiftRows`, `MixColumns`, `AddRoundKey`, `KeyExpansion`, and `CTR` keystream generation) is implemented manually in standard C++17.
* **Cross-validation target (`test_crossval`):** Automatically links against Crypto++ (when available) strictly as an external test oracle to mathematically verify byte-for-byte ciphertext identity.

---

## Directory Structure

```
lab02/
├── CMakeLists.txt              # CMake configuration (C++17, MSVC / GCC / Clang)
├── README.md                   # Project overview & CLI documentation
├── BENCHMARKS.md               # Benchmark matrix, methodology & CSV formats
├── src/
│   ├── AES.hpp / AES.cpp       # Pure C++ FIPS-197 AES block cipher core
│   ├── modes.hpp / modes.cpp   # CTR (required) + XTS (bonus) in one file
│   ├── minijson.hpp            # Header-only zero-dependency JSON parser
│   └── main.cpp                # aestool CLI (encrypt, decrypt, bench, --kat)
├── tests/
│   ├── catch.hpp               # Catch2 v2.13.10 single-header test framework
│   ├── test_aes.cpp            # FIPS-197 unit tests (Appendix C vectors)
│   ├── test_ctr.cpp            # CTR stream cipher & negative tests
│   ├── test_crossval.cpp       # Oracle cross-validation against Crypto++
│   ├── cli_test.sh             # Linux CLI smoke test
│   └── cli_test.cmake          # Windows CLI smoke test
├── kat/
│   └── vectors.json            # Official NIST SP 800-38A & FIPS-197 vectors
├── scripts/
│   ├── build_linux.sh          # Linux GCC/Clang automated build & CTest runner
│   ├── build_windows.bat       # Windows MSVC/MinGW build & CTest runner
│   ├── run_benchmarks.sh       # Linux benchmark automation (1 MiB, 100 MiB)
│   ├── run_benchmarks.bat      # Windows benchmark automation
│   ├── summarize_bench.py      # Statistical aggregator (mean, median, SD, CI95)
│   └── plot_bench.py           # Matplotlib latency/throughput chart generator
└── output/
    ├── ubuntu/test_results/    # Linux test evidence & KAT logs
    ├── ubuntu/benchmarks/      # Linux benchmark CSVs and charts
    ├── windows/test_results/   # Windows MSVC logs
    └── windows/benchmarks/     # Windows benchmark CSVs
```

---

## Dependencies

| Dependency | Minimum Version | Notes |
|---|---|---|
| **C++ Standard** | C++17 | Verified with GCC 15.2, Clang 21.1, MSVC 19.51 |
| **CMake** | ≥ 3.16 | Verified with 4.2.3 (Ubuntu) & 4.3.1 (Windows VS bundled) |
| **Crypto Primitive Core** | **Pure C++ STL** | **Strictly NO external crypto libraries in core primitives** |
| **Catch2** | 2.13.10 | Vendored in `tests/catch.hpp` (zero internet access needed) |
| **Crypto++ (Optional Oracle)** | 8.9.0 | Used strictly for `test_crossval` oracle parity testing |
| **Python** | 3.8+ | Optional, for benchmark statistics and graph plotting |

---

## Build & Test Instructions

### Ubuntu Linux

```bash
cd lab02

# Build with GCC (default):
scripts/build_linux.sh gcc

# Build with Clang:
scripts/build_linux.sh clang
```

Run test suite manually:

```bash
cd build/ubuntu-gcc
ctest --output-on-failure
```

### Windows (MSVC & MinGW)

Open **"x64 Native Tools Command Prompt for VS 2026/2022"**:

```bat
cd lab02
scripts\build_windows.bat msvc
```

For MinGW64 from MSYS2 shell:

```bat
scripts\build_windows.bat gcc
scripts\build_windows.bat clang
```

---

## CLI Usage (`aestool`)

### 1. Encryption (CTR Mode)

```bash
# Using binary key and IV files
./build/ubuntu-gcc/aestool encrypt --mode ctr --key key.bin --iv iv.bin --in msg.bin --out ct.bin

# Using hex arguments and text input
./build/ubuntu-gcc/aestool encrypt --mode ctr \
    --key-hex 2b7e151628aed2a6abf7158809cf4f3c \
    --iv-hex f0f1f2f3f4f5f6f7f8f9fafbfcfdfeff \
    --text "Hello World" --out ct.bin
```

### 2. Decryption (CTR Mode)

```bash
./build/ubuntu-gcc/aestool decrypt --mode ctr --key key.bin --iv iv.bin --in ct.bin --out msg.bin
```

### 3. XTS Mode (Bonus B)

The XTS key is two equal AES keys concatenated (`K1 || K2`), so use 32 bytes for
AES-128-XTS. `--iv` is the 16-byte data-unit number (little-endian sector).

```bash
./build/ubuntu-gcc/aestool encrypt --mode xts \
    --key-hex 000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f \
    --iv-hex 01000000000000000000000000000000 \
    --in msg.bin --out ct.bin

./build/ubuntu-gcc/aestool decrypt --mode xts \
    --key-hex 000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f \
    --iv-hex 01000000000000000000000000000000 \
    --in ct.bin --out msg.bin
```

Sizes that are not a multiple of 16 bytes use IEEE P1619 ciphertext stealing.

### 4. Known Answer Tests (KAT)

```bash
./build/ubuntu-gcc/aestool --kat kat/vectors.json
```

Expected output: `4 passed, 0 failed`.

### 5. Throughput Benchmarking

```bash
./build/ubuntu-gcc/aestool bench --size 1048576 --runs 100 --out bench_1MB.csv
```
