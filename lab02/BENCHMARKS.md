# Lab 2 — AES-128 CTR Mode Benchmarks

Automated benchmarking suite for measuring AES-128 CTR throughput and latency in pure C++, adhering to the `Lab2.txt` specification.

## Benchmark Matrix

- **Cipher Mode:** `ctr` (AES-128 Counter mode, NIST SP 800-38A)
- **Required Sizes:**
  - `1048576` bytes (1 MiB) — Cache-resident; measures raw algorithmic transformation speed.
  - `104857600` bytes (100 MiB) — Exceeds L3 cache; evaluates memory hierarchy streaming.
  - `1073741824` bytes (1 GiB) — Optional high-capacity sustained I/O throughput.
- **Iterations:** 100 runs per size (configurable via `--runs N`).
- **Warm-up:** 5 discarded iterations before recording to eliminate cold-cache jitter.
- **Output:** Structured CSV files located in `output/<platform>/benchmarks/`.

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

---

## Execution Instructions

### Ubuntu Linux

```bash
cd lab02

# 1. Build release binaries
scripts/build_linux.sh gcc
scripts/build_linux.sh clang

# 2. Run benchmark suite (GCC)
scripts/run_benchmarks.sh gcc

# 3. Run benchmark suite (Clang)
scripts/run_benchmarks.sh clang

# 4. Generate statistical summary & plots
python3 scripts/summarize_bench.py --dir output/ubuntu/benchmarks
python3 scripts/plot_bench.py --csv output/ubuntu/benchmarks/bench_ubuntu_gcc_ctr_1048576.csv \
    --out output/ubuntu/benchmarks/graph_ctr_1MB.png
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
