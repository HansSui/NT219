# NT219 - Cryptography Labs

Course laboratory work for NT219. Each lab is self-contained and builds with
CMake. Lab 1 uses Crypto++ for its primitives; Lab 2 implements AES/CTR in pure
C++ and uses OpenSSL only as a test oracle.

## Layout

```
NT219/
  README.md           this file
  docs/               course briefs, blueprint and audit notes
  lab01/              Lab 1 - symmetric encryption CLI (aestool)
  lab02/              Lab 2 - pure C++ AES/CTR
  lab03/              Lab 3 - RSA-OAEP + hybrid AES-GCM (rsatool)
```

### lab01

```
CMakeLists.txt        build (MSVC, MinGW GCC/Clang, Linux GCC/Clang)
src/                  aescore (modes + container), C ABI and CLI
tests/                unit tests (Catch2, vendored) and CLI test
kat/                  NIST/RFC known-answer vectors
gui/                  Tkinter GUI calling the shared library
scripts/              build, benchmark, summarise and plot scripts
output/               benchmark evidence and generated charts
README.md             CLI, container format and security design
BENCHMARKS.md         full benchmark matrix and cross-platform charts
REPORT.md             Vietnamese lab report (refresh pending)
```

### docs

```
lab1.txt                    official Lab 1 brief, constraints and rubric
Lab2.txt ... Lab6.txt       Lab 2-6 briefs
lab requirement.md          course/toolchain core
GradingSystem.txt           performance, report and submission rules
blueprint/                  plan-only blueprint and lab briefs
missing/                    implementation audit for Labs 1-2
memory/                     repo memory (Index, architecture, decisions, history)
```

Use `lab01/README.md` and `lab01/BENCHMARKS.md` for the current build and output
layout.

## Quick start (Lab 1)

Windows, from a "x64 Native Tools Command Prompt for VS 2026":

```bat
cd /d E:\NT219\lab01
scripts\build_windows.bat msvc
scripts\bench_windows_controlled.bat
```

Use `scripts\run_benchmarks.bat msvc` only for a quick, less-controlled run.

Linux:

```bash
cd lab01
scripts/build_linux.sh gcc
scripts/run_benchmarks.sh gcc
```

Graphs:

```bat
cd lab01
python scripts\plot_crossplatform.py
```

GUI (bonus, shared library):

```bat
cd lab01
gui\build_gui.bat
gui\run_gui.bat
```

Linux: `./gui/build_gui.sh` then `./gui/run_gui.sh` after
`sudo apt install python3-tk libcryptopp-dev`.

The build, KAT (29/29 on Windows), `ctest` and benchmark matrix are verified
with MSVC 19.51 x64 on Windows (logs in `lab01/output/windows/test_results/` and
`lab01/output/windows/neg_results/`). MinGW GCC 16.2.0 and MinGW Clang 22.1.8
also build and pass the 29-vector KAT and `ctest` 2/2. Ubuntu GCC 15.2 and
Clang 21.1.8 also pass 29/29 and `ctest` 2/2 (logs refreshed 2026-11-05). The Tkinter GUI
bonus is built against the shared library and passes its eight-mode self-test on
Windows and
Linux. The Ubuntu benchmark data is the 48-row GCC 15.2 matrix from
`lab01/output/ubuntu/benchmarks/`; Ubuntu Clang was built and tested but not
benchmarked. The Windows benchmark matrix is the controlled 25/10 run, and
`summary_robust.csv` records its remaining noisy configurations. See
`lab01/README.md` for the CLI, `lab01/BENCHMARKS.md` for the benchmark method and
the completed Vietnamese report in `lab01/REPORT.md` for results, statistics and
analysis. The report is being refreshed separately for the new Windows run.

## Quick start (Lab 2)

Windows, from a "x64 Native Tools Command Prompt for VS 2026":

```bat
cd /d E:\NT219\lab02
scripts\build_windows.bat msvc
cd build\windows-msvc
ctest -C Release --output-on-failure
cd ..\..
scripts\run_benchmarks.bat msvc
```

Linux:

```bash
cd lab02
scripts/build_linux.sh gcc
THREADS=4 PIN_CPU=0-3 scripts/run_benchmarks.sh gcc
```

Lab 2 implements AES-128/192/256 and CTR in pure C++17; OpenSSL is linked only
into `test_crossval` and `bench_openssl`. The current KAT has 9 vectors and
CTest has 4 tests. Windows MSVC 19.51, MinGW GCC 16.2.0 and MinGW Clang 22.1.8
each passed 9/9 KAT and 4/4 CTest before the oracle switch; rerun Windows with
OpenSSL. Windows MSVC benchmark data exists at the chosen 1000/100/30 rows with
summary, six plots and an audit under `lab02/output/windows/benchmarks/`.
Ubuntu GCC 15.2 logs show 9/9 KAT, 4/4 CTest with OpenSSL, and the new
1000/100/30 benchmark; OpenSSL throughput comparison is in
`lab02/output/ubuntu/benchmarks/openssl/`; see `docs/missing/lab2.md`. The
Vietnamese report is `lab02/REPORT.md`; benchmark details are in
`lab02/BENCHMARKS.md`.

## Quick start (Lab 3)

Linux:

```bash
cd lab03
# requires OpenSSL 4.0.2; set OPENSSL_ROOT_DIR if auto-detection misses it
scripts/build_linux.sh gcc
./build/ubuntu-gcc/rsatool keygen --bits 3072 --pub pub.pem --priv priv.pem
./build/ubuntu-gcc/rsatool encrypt --in msg.bin --pub pub.pem --out ct.bin --label ctx
./build/ubuntu-gcc/rsatool decrypt --in ct.bin --priv priv.pem --out msg.bin --label ctx
./build/ubuntu-gcc/rsatool --kat kat/oaep_vectors.json
(cd build/ubuntu-gcc && ctest --output-on-failure)
```

Windows:

```bat
cd lab03
scripts\build_windows.bat msvc
```

Lab 3 uses OpenSSL 4.0.2 for RSA-OAEP-SHA256 and AES-256-GCM. It writes PEM,
DER and metadata for 3072/4096-bit keys, switches to a versioned hybrid envelope
above the direct OAEP limit, and fails closed on tampered ciphertext, tags,
headers, labels, keys and encodings. See `lab03/README.md` for the format and
`lab03/REPORT.md` / `lab03/BENCHMARKS.md` for the report and measured results.
Ubuntu GCC 15.2 + OpenSSL 4.0.2: KAT 2/2, `ctest` 5/5, CPU-pinned 4-thread
benchmark (1000/100 runs) with summary and three plots under
`lab03/output/ubuntu/benchmarks/`.
