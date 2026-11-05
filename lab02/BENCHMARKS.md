# Lab 2 — AES-128 CTR Mode Benchmarks

Benchmark suite for AES-128 CTR throughput and latency in pure C++.

## Matrix

- **Mode:** `ctr` (AES-128, NIST SP 800-38A)
- **Required sizes:**
  - `1048576` bytes (1 MiB) — cache-resident
  - `104857600` bytes (100 MiB) — exceeds L3
  - `1073741824` bytes (1 GiB) — sustained large-buffer run
- **Runs (chosen):** 1 MiB = 1000, 100 MiB = 100, 1 GiB = 30.
  Both large cases satisfy the course's N >= 30 rule. The blueprint's nominal
  1000/500/50 is available with `RUNS_100MB=500 RUNS_1GB=50`; override with
  `RUNS`, `RUNS_1MB`, `RUNS_100MB`, `RUNS_1GB`.
- **Warm-up:** wall-clock, default 1.0 s (`--warmup`, `WARMUP=...`). The
  warm-up uses at most the first 1 MiB so a 1 GiB case is not delayed by a
  minute-long warm-up.
- **Threads:** both scripts default to `THREADS=4` and pass `--threads 4`.
  Override with `THREADS=1` for a single-thread baseline. Windows defaults to
  `PIN_MASK=F` (logical CPUs 0-3) for 4 threads and `PIN_MASK=4` for a
  single thread; set `PIN_MASK=0` to disable pinning. Linux users should set
  `PIN_CPU=0-3` (or another set of four distinct cores); pinning 4 threads
  to one CPU gives no speedup.
- **CPU control:** Linux script pins to `${PIN_CPU:-0}` with `taskset` when
  available and records the governor. Windows script selects the High
  Performance power plan, runs with `start /high /affinity`, and restores the
  previous plan afterward. The Windows CSV has no metadata columns, so the
  run-time plan/affinity/priority must be kept in a separate console log.
- **Output:** one CSV per size; both scripts are console-only now.
- **Progress:** `--verbose` prints `progress:` lines to stderr with run count,
  percentage, elapsed time and ETA, then a final `bench: completed ...` line.
  Both benchmark scripts pass `--verbose` automatically; the CSV is unaffected.

### Actual committed data vs documented matrix

Ubuntu CSVs were measured on 2026-11-05 with GCC 15.2; Windows CSVs were
measured on 2026-11-05 with MSVC 19.51:

| Platform | Size | Committed rows | Blueprint nominal | Notes |
|---|---|---:|---:|---|
| Ubuntu GCC 15.2 | 1 MiB | 1000 | 1000 | 4 threads, CPUs 0-3, `performance`; CV 14.4% |
| Ubuntu GCC 15.2 | 100 MiB | 100 | 500 | 4 threads, CPUs 0-3; CV 5.4% |
| Ubuntu GCC 15.2 | 1 GiB | 30 | 50 | 4 threads, CPUs 0-3; CV 4.0% |
| Windows MSVC 19.51 | 1 MiB | 1000 | 1000 | 4 threads |
| Windows MSVC 19.51 | 100 MiB | 100 | 500 | 4 threads |
| Windows MSVC 19.51 | 1 GiB | 30 | 50 | 4 threads |

The chosen matrix is 1000/100/30. Both 100 MiB/100 and 1 GiB/30 are inside
the course's “N >= 30” band, so the data is usable for observation; the
blueprint's 500/50 is a nominal target, not the chosen run. Ubuntu now uses
the current warm-up/thread/pinning protocol. Windows has no run-time
environment log, so treat power/affinity/thread details as unverified from the
CSV alone. See `output/windows/benchmarks/AUDIT.md`.

Committed Windows means: **6.741 / 7.016 / 7.085 MiB/s** encrypt and
**6.801 / 7.012 / 7.083 MiB/s** decrypt for 1 MiB / 100 MiB / 1 GiB. CVs are
6.3% / 1.3% / 0.7%; `summary_robust.csv` marks all three `ok`.

## CSV format

```csv
run,mode,size_bytes,encrypt_us,decrypt_us,throughput_encrypt_MBps,throughput_decrypt_MBps
```

- `run`: iteration index (0..N-1)
- `mode`: always `ctr`
- `size_bytes`: payload size
- `encrypt_us`, `decrypt_us`: wall-clock microseconds
- `throughput_*_MBps`: MiB/s (the code divides by `1024 * 1024`)

## Console output

`scripts/run_benchmarks.sh` mirrors the Windows batch script: it prints the
matrix, each command and the `--verbose` progress to the console, and writes
only the per-size CSVs plus the summary. It creates **no** `.log` or
environment file. Old `.log` files, if present, are from the earlier script.

The script sets the `performance` governor via `sudo -n cpupower` when that
is possible and restores the previous governor on exit; otherwise it prints a
warning and continues. The governor is not written to a file, so capture the
console if you need run-time evidence.

Environment overrides: `BENCH_1GB=0` skips 1 GiB, `OUT_DIR=...` changes the
output directory, `WARMUP=2` changes the warm-up, `THREADS=4` sets worker
threads, `PIN_CPU=0-3` pins to four logical CPUs and `PIN_CPU=none` disables
pinning. `RUNS`, `RUNS_1MB`, `RUNS_100MB` or `RUNS_1GB` override the counts.

## Run

### Ubuntu

```bash
cd lab02
scripts/build_linux.sh gcc
THREADS=4 PIN_CPU=0-3 scripts/run_benchmarks.sh gcc

# Quick run without 1 GiB:
THREADS=4 PIN_CPU=0-3 BENCH_1GB=0 scripts/run_benchmarks.sh gcc

# Blueprint nominal 1000/500/50, if ever needed:
THREADS=4 PIN_CPU=0-3 RUNS_100MB=500 RUNS_1GB=50 \
  scripts/run_benchmarks.sh gcc

# Statistics and one combined summary plot:
python3 scripts/summarize_bench.py --dir output/ubuntu/benchmarks
python3 scripts/plot_bench.py output/ubuntu/benchmarks/bench_ubuntu_gcc_ctr_*.csv \
  --out-dir output/ubuntu/benchmarks/graphs --platform ubuntu-gcc
```

The script tries `sudo -n cpupower frequency-set -g performance` and restores
the previous governor on exit. If that is not permitted it prints a warning;
then set it manually and record the limitation:

```bash
sudo cpupower frequency-set -g performance
# ... run benchmarks ...
sudo cpupower frequency-set -g powersave
```

### Windows

From a “x64 Native Tools Command Prompt for VS”:

```bat
cd lab02
scripts\build_windows.bat msvc
scripts\run_benchmarks.bat msvc
python scripts\summarize_bench.py --dir output\windows\benchmarks
```

The batch file runs the chosen 1 MiB/1000, 100 MiB/100 and 1 GiB/30 matrix,
selects High Performance, uses `start /high /affinity`, and passes
`--threads 4`. Override with `RUNS`, `RUNS_1MB`, `RUNS_100MB`, `RUNS_1GB`,
`WARMUP`, `THREADS`, `PIN_MASK`, or `PRIORITY`.

The committed Windows run matches these chosen counts but has no environment
log.

## OpenSSL performance comparison

`test_crossval` proves byte equality; `bench_openssl` measures throughput with
the same AES-128-CTR primitive, payload sizes, run counts and thread count.
It is test-only and is never linked into `aestool`.

Ubuntu (GCC 15.2, 4 threads, 2026-11-05):

| Size | Pure C++ enc | OpenSSL enc | Ratio | Pure C++ dec | OpenSSL dec | Ratio |
|---|---:|---:|---:|---:|---:|---:|
| 1 MiB | 62.8 MiB/s | 2424.2 MiB/s | 38.6x | 62.4 MiB/s | 2193.4 MiB/s | 35.2x |
| 100 MiB | 46.9 MiB/s | 1251.4 MiB/s | 26.7x | 47.0 MiB/s | 1234.6 MiB/s | 26.3x |
| 1 GiB | 45.6 MiB/s | 1120.0 MiB/s | 24.6x | 46.0 MiB/s | 1105.8 MiB/s | 24.0x |

This is the expected gap: OpenSSL uses AES-NI, while the required `aes_core`
is table-based software AES. All three OpenSSL points are marked stable
(CV 8.1–8.9% at 1 MiB, 4.0–4.9% at 100 MiB/1 GiB).

Regenerate on Ubuntu:

```bash
mkdir -p output/ubuntu/benchmarks/openssl
for pair in "1048576 1000" "104857600 100" "1073741824 30"; do
  set -- $pair
  ./build/ubuntu-gcc/bench_openssl --size "$1" --runs "$2" --threads 4 \
    --out "output/ubuntu/benchmarks/openssl/bench_openssl_gcc_ctr_$1.csv"
done
python3 scripts/summarize_bench.py --dir output/ubuntu/benchmarks/openssl
```

Windows (MSVC example; use `gcc`/`clang` for the other builds):

```bat
cd lab02
scripts\build_windows.bat msvc
mkdir output\windows\benchmarks\openssl 2>nul
build\windows-msvc\Release\bench_openssl.exe --size 1048576 --runs 1000 --threads 4 --out output\windows\benchmarks\openssl\bench_openssl_msvc_ctr_1048576.csv
build\windows-msvc\Release\bench_openssl.exe --size 104857600 --runs 100 --threads 4 --out output\windows\benchmarks\openssl\bench_openssl_msvc_ctr_104857600.csv
build\windows-msvc\Release\bench_openssl.exe --size 1073741824 --runs 30 --threads 4 --out output\windows\benchmarks\openssl\bench_openssl_msvc_ctr_1073741824.csv
python scripts\summarize_bench.py --dir output\windows\benchmarks\openssl
```

## Limitations

- The Ubuntu run (2026-11-05) used 4 threads, CPUs 0-3 and the `performance`
  governor, but wrote no environment log (the script is console-only). The
  1 MiB point has CV 14.4%, so cite it with that noise caveat.
- A single combined summary plot covers all three Ubuntu sizes;
  `graphs/bench_ubuntu_gcc_ctr_summary.png`.
- The Windows benchmark uses the chosen 1000/100/30 counts but has no
  run-time environment log.
- OpenSSL throughput was measured on Ubuntu only; Windows has no comparison
  yet. OpenSSL is memory-bound here, so extra threads do not increase its
  throughput at 100 MiB/1 GiB.
- Pure C++ table-based AES is expected to be far slower than AES-NI; report
  the absolute numbers, not a claim of “fast AES”.
