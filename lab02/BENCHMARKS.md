# Lab 2 — AES-128 CTR Mode Benchmarks

Automated benchmarking suite for measuring AES-128 CTR throughput and latency in pure C++, adhering to the `Lab2.txt` specification.

## Benchmark Matrix

- **Cipher Mode:** `ctr` (AES-128 Counter mode, NIST SP 800-38A)
- **Required Sizes:**
  - `1048576` bytes (1 MiB) — Cache-resident; measures raw algorithmic transformation speed.
  - `104857600` bytes (100 MiB) — Exceeds L3 cache; evaluates memory hierarchy streaming.
  - `1073741824` bytes (1 GiB) — Optional high-capacity sustained I/O throughput.
- **Iterations:** Per size: 1 MiB = 1000, 100 MiB = 500, 1 GiB = 50. Override with `RUNS_1MB`, `RUNS_100MB`, `RUNS_1GB`, or `RUNS` for all sizes.
- **Warm-up:** 5 discarded iterations before recording to eliminate cold-cache jitter.
- **Output:** Structured CSV files plus a timestamped system-tracking log in `output/<platform>/benchmarks/`.

### CSV Column Specification

Each CSV file contains a header row followed by one row per iteration:

```csv
run,mode,size_bytes,encrypt_us,decrypt_us,throughput_encrypt_MBps,throughput_decrypt_MBps
```

- `run`: Iteration index (0 to N-1)
- `mode`: Cipher mode (`ctr`)
- `size_bytes`: Payload size in bytes
- `encrypt_us`: Microseconds elapsed during encryption
- `decrypt_us`: Microseconds elapsed during decryption
- `throughput_encrypt_MBps`: Encryption throughput in Megabytes/sec (MiB/s)
- `throughput_decrypt_MBps`: Decryption throughput in Megabytes/sec (MiB/s)

### System Tracking Log

`scripts/run_benchmarks.sh` mirrors everything to `bench_ubuntu_<compiler>_<UTC timestamp>.log`:

- `===== BENCHMARK =====` header: host, kernel, CPU model/cores/governor/MHz, total/available RAM, swap, compiler version, git commit, binary path, log path.
- `--- CLI ---`: the exact `aestool --help` and every benchmark command line.
- `--- MATRIX ---`: the size/run pairs being executed.
- `SYS ...` sampled every `SAMPLE_INTERVAL` seconds (default 2): CPU busy %, load average, used/available RAM, swap used, and the benchmark process CPU % / RSS.
- `===== END size=... =====`, CSV row count, and total elapsed seconds.

Environment overrides: `BENCH_1GB=0` skips 1 GiB, `OUT_DIR=/path` changes the output directory, `SAMPLE_INTERVAL=1` samples every second.

---

## Execution Instructions

### Ubuntu Linux

```bash
cd lab02

# 1. Build release binaries
scripts/build_linux.sh gcc
scripts/build_linux.sh clang

# 2. Run full matrix (1 MiB/1000, 100 MiB/500, 1 GiB/50)
scripts/run_benchmarks.sh gcc

# Skip 1 GiB for a quick run
BENCH_1GB=0 scripts/run_benchmarks.sh gcc

# 3. Run benchmark suite (Clang)
scripts/run_benchmarks.sh clang

# 4. Generate statistical summary & plots
python3 scripts/summarize_bench.py --dir output/ubuntu/benchmarks
for f in output/ubuntu/benchmarks/bench_ubuntu_gcc_ctr_*.csv; do
    python3 scripts/plot_bench.py "$f" --out-dir output/ubuntu/benchmarks/graphs --platform gcc
done
```

### Windows (MSVC & MinGW)

From a "x64 Native Tools Command Prompt for VS 2026/2022":

```bat
cd lab02

rem 1. Build release binaries
scripts\build_windows.bat msvc

rem 2. Run benchmark suite
scripts\run_benchmarks.bat msvc

rem 3. Generate summaries
python scripts\summarize_bench.py --dir output\windows\benchmarks
```
