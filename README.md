# NT219 - Cryptography Labs

Course laboratory work for NT219. Each lab is self-contained, builds with
CMake, and uses Crypto++ for the cryptographic primitives.

## Layout

```
NT219/
  README.md           this file
  docs/               course brief, objectives and execution guides
  lab01/              Lab 1 - symmetric encryption CLI (aestool)
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
REPORT.md             completed Vietnamese lab report (measured results)
```

### docs

```
course-objectives.txt       learning outcomes for Labs 1-6
lab01-brief.txt             official Lab 1 brief, constraints and rubric
guides/ubuntu-run-guide.txt Ubuntu command guide for collecting lab data
guides/report-guide-vi.txt  Vietnamese report/data-collection guide
```

The guides keep the paths they were written with; use `lab01/README.md` and
`lab01/BENCHMARKS.md` for the current build and output layout.

## Quick start (Lab 1)

Windows, from a "x64 Native Tools Command Prompt for VS 2026":

```bat
cd /d E:\NT219\lab01
scripts\build_windows.bat msvc
scripts\run_benchmarks.bat msvc
```

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

The build, KAT, `ctest` and benchmark matrix are verified with MSVC 19.51 x64 on
Windows (logs in `lab01/output/windows/test_results/` and
`lab01/output/windows/neg_results/`). The Tkinter GUI bonus is built against the
shared library and passes its eight-mode self-test on Windows. The Ubuntu data
comes from earlier GCC 15.2 and Clang 21.1.8 runs; MinGW builds and the Linux GUI
path are provided by script but were not re-verified in this environment. See
`lab01/README.md` for the CLI, `lab01/BENCHMARKS.md` for the benchmark method and
the completed Vietnamese report in `lab01/REPORT.md` for results, statistics and
analysis.
