# NT219 - Cryptography Labs

Course laboratory work for NT219. Each lab is self-contained and builds with
CMake. Lab 1 uses Crypto++ for its primitives; Lab 2 implements AES/CTR in pure
C++ and uses Crypto++ only as a test oracle.

## Layout

```
NT219/
  README.md           this file
  docs/               course briefs, blueprint and audit notes
  lab01/              Lab 1 - symmetric encryption CLI (aestool)
  lab02/              Lab 2 - pure C++ AES/CTR
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
also build and pass the 29-vector KAT and `ctest` 2/2. The Ubuntu logs still show
the pre-expansion 20/20; rerun `--kat` on Linux to refresh them. The Tkinter GUI
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

Lab 2 implements AES-128/192/256 and CTR in pure C++17; Crypto++ is linked only
into `test_crossval`. The current KAT has 9 vectors and CTest has 4 tests.
Windows MSVC 19.51, MinGW GCC 16.2.0 and MinGW Clang 22.1.8 each pass 9/9 KAT and
4/4 CTest (logs in `lab02/output/windows/test_results/`). Windows MSVC
benchmark data now exists at the chosen 1000/100/30 rows with summary, six
plots and an audit under `lab02/output/windows/benchmarks/`. The Ubuntu test
logs, Ubuntu benchmark rerun and Crypto++ throughput comparison are still
open; see `docs/missing/lab2.md`. The Vietnamese report is
`lab02/REPORT.md`; benchmark details are in `lab02/BENCHMARKS.md`.
