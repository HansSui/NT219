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
- **Output:** one CSV per size plus a timestamped system log on Linux.
- **Progress:** `--verbose` prints `progress:` lines to stderr with run count,
  percentage, elapsed time and ETA, then a final `bench: completed ...` line.
  Both benchmark scripts pass `--verbose` automatically; the CSV is unaffected.

### Actual committed data vs documented matrix

Ubuntu CSVs are from an earlier run; Windows CSVs were measured on
2026-11-05 with MSVC 19.51:

| Platform | Size | Committed rows | Blueprint nominal | Notes |
|---|---|---:|---:|---|
| Ubuntu GCC 15.2 | 1 MiB | 1000 | 1000 | old 5-iteration warm-up, `powersave`, no pin |
| Ubuntu GCC 15.2 | 100 MiB | 100 | 500 | old protocol |
| Ubuntu GCC 15.2 | 1 GiB | 30 | 50 | old protocol |
| Windows MSVC 19.51 | 1 MiB | 1000 | 1000 | 4 threads |
| Windows MSVC 19.51 | 100 MiB | 100 | 500 | 4 threads |
| Windows MSVC 19.51 | 1 GiB | 30 | 50 | 4 threads |

The chosen matrix is 1000/100/30. Both 100 MiB/100 and 1 GiB/30 are inside
the course's “N >= 30” band, so the data is usable for observation; the
blueprint's 500/50 is a nominal target, not the chosen run. Ubuntu also used
the old 5-iteration warm-up, so rerun it with the current protocol before the
final comparison. Windows has no run-time environment log, so treat
power/affinity/thread details as unverified from the CSV alone. See
`output/windows/benchmarks/AUDIT.md`.

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

## System log

`scripts/run_benchmarks.sh` writes a timestamped log next to the CSVs with:

- header: host, kernel, CPU model/cores/governor/MHz, RAM, swap, compiler,
  git commit, binary path, log path;
- `warmup_s`, `pin_cpu`, `taskset` availability;
- governor warning when it is not `performance`;
- exact CLI and benchmark commands;
- `SYS ...` samples every `SAMPLE_INTERVAL` seconds (default 2): CPU busy,
  load, RAM, swap, process CPU/RSS;
- row count and elapsed seconds per size.

The Windows batch script prints the same kind of header to the console but
does **not** tee it to a file. The committed Windows benchmark therefore has
no run-time environment log; do not reconstruct one from a later `powercfg`
query.

Environment overrides: `BENCH_1GB=0` skips 1 GiB, `OUT_DIR=...` changes the
output directory, `SAMPLE_INTERVAL=1` samples every second, `WARMUP=2`
changes the warm-up, `THREADS=4` sets worker threads, `PIN_CPU=0-3` pins to
four logical CPUs.

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

# Statistics and plots:
python3 scripts/summarize_bench.py --dir output/ubuntu/benchmarks
for f in output/ubuntu/benchmarks/bench_ubuntu_gcc_ctr_*.csv; do
  python3 scripts/plot_bench.py "$f" \
    --out-dir output/ubuntu/benchmarks/graphs --platform gcc
done
```

For a `performance` governor:

```bash
sudo cpupower frequency-set -g performance
# ... run benchmarks ...
sudo cpupower frequency-set -g powersave
```

If `cpupower` is unavailable or sudo is not permitted, record the actual
governor in the report; do not claim `performance`.

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

## Crypto++ performance comparison

Cross-validation (`test_crossval`) only proves byte equality; it does not
measure throughput. A fair comparison needs a separate test-only benchmark
that links Crypto++ and uses the same payloads, compiler, flags, CPU pinning
and governor. This is still an open item; do not infer performance from the
correctness oracle.

## Limitations

- The committed Ubuntu numbers used `powersave`, no CPU pinning, one thread,
  and the old 5-iteration warm-up.
- The Ubuntu 1 GiB plot has not been generated.
- The Windows benchmark uses the chosen 1000/100/30 counts but has no
  run-time environment log.
- No Crypto++ throughput comparison exists yet.
- Pure C++ table-based AES is expected to be far slower than AES-NI; report
  the absolute numbers, not a claim of “fast AES”.
