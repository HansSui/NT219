# Lab 3 benchmarks

## Environment (Ubuntu)

| Item | Value |
|---|---|
| CPU | 11th Gen Intel Core i5-1135G7 @ 2.40 GHz, 4 cores / 8 threads |
| RAM | host default (not constrained) |
| OS | Ubuntu 26.04 LTS, kernel 7.0.0 |
| Compiler | GCC 15.2.0, `-O3 -Wall -Wextra` |
| OpenSSL | 4.0.2 (`libcrypto.so.4`, custom prefix) |
| CMake | 4.2.3 |
| Date | 2026-11-07 |

## Method

- Build: `scripts/build_linux.sh gcc` (requires OpenSSL 4.0.x or newer; the
  committed run used 4.0.2).
- Affinity: `taskset -c 0-3` (four logical CPUs) by default.
- Threads: 4 worker threads; each worker times independent operations.
- Warm-up: 1 s per worker before each case.
- Runs: **RSA keygen/encrypt/decrypt 1000**, **AES/hybrid 1 KiB and 1 MiB
  1000**, **AES/hybrid 100 MiB 100**.
- RSA: one operation per timed sample; message 32 bytes, OAEP-SHA256.
- AES-GCM/Hybrid: one operation per timed sample at the stated size.
- Optional comparison: AES-256-CTR + HMAC-SHA256 is unit-tested but is not in
  this benchmark matrix; no CTR+HMAC timing claims are made.
- Metrics: mean, median, sample standard deviation and 95% CI
  (`1.96 * sd / sqrt(n)`). `elapsed_ms` is per operation under four-way
  concurrency; aggregate throughput is `per-op MB/s × 4`.
- CSV: `output/ubuntu/benchmarks/bench_ubuntu_gcc_rsa_hybrid.csv`.
- Summary: `output/ubuntu/benchmarks/summary_gcc.csv`.
- Plots: `output/ubuntu/benchmarks/graph/` (grouped bars, 95% CI).
- Runtime evidence: `output/ubuntu/test_results/environment.txt`.

Reproduce:

```bash
cd lab03
PIN_CPU=0-3 THREADS=4 RUNS_RSA=1000 RUNS_SMALL=1000 RUNS_LARGE=100 \
  WARMUP=1 scripts/run_benchmarks.sh gcc
```

The script defaults are the same; the environment variables are shown explicitly.
Key generation is inherently variable (prime search); its standard deviation is
large. Each platform's numbers are single-host and indicative; the two hosts
are not a controlled comparison.

Windows now has a batch wrapper, `scripts/run_benchmarks.bat`; it builds if
needed, pins CPUs 0-3, sets the High performance power plan, writes the CSV and
summary, and regenerates the plots. The committed run is from 2026-11-10:

```bat
cd lab03
set "OPENSSL_ROOT_DIR=D:\cryptolibrary"
scripts\run_benchmarks.bat msvc
```

Equivalent manual command:

```bat
build\windows-msvc\Release\rsatool.exe bench --runs-rsa 1000 --runs-small 1000 ^
  --runs-large 100 --threads 4 --warmup 1 ^
  --out output\windows\benchmarks\bench_windows_msvc_rsa_hybrid.csv --verbose
```

Override `RUNS_RSA`, `RUNS_SMALL`, `RUNS_LARGE`, `WARMUP`, `THREADS`, `PIN_MASK`
or `OUT_DIR` as needed. Use `PIN_MASK=0` to disable affinity.

### Environment (Windows)

| Item | Value |
|---|---|
| OS | Windows 11 host; no Lab 3 run-time log |
| Compiler | MSVC 19.51 (`build\windows-msvc`) |
| OpenSSL | 4.0.x (Windows host install; no Lab 3 version log) |
| Threads | 4 (from the CSV `threads` column) |
| Affinity / power | Script defaults: CPUs 0-3, High performance; not captured in a log |
| Date | 2026-11-10 |

The CSV/summary/plots are the evidence of record. The missing run-time log
means affinity, power plan and priority are script defaults, not verified
metadata.

## Measured results (Ubuntu)

`elapsed` is per operation; `MB/s` is per-operation throughput; aggregate is for
all four worker threads.

| Case | Bits | Size | Threads | Runs | Mean ms | Median ms | SD ms | 95% CI ms | Mean MB/s | Aggregate MB/s |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| rsa_keygen | 3072 | — | 4 | 1000 | 186.407 | 155.079 | 123.569 | 7.659 | — | — |
| rsa_keygen | 4096 | — | 4 | 1000 | 604.706 | 519.984 | 392.837 | 24.348 | — | — |
| rsa_encrypt | 3072 | 32 B | 4 | 1000 | 0.052 | 0.052 | 0.003 | 0.000 | 0.6 | 2.4 |
| rsa_encrypt | 4096 | 32 B | 4 | 1000 | 0.105 | 0.093 | 0.020 | 0.001 | 0.3 | 1.3 |
| rsa_decrypt | 3072 | 32 B | 4 | 1000 | 1.198 | 1.140 | 0.199 | 0.012 | 0.0 | 0.1 |
| rsa_decrypt | 4096 | 32 B | 4 | 1000 | 2.637 | 2.535 | 0.384 | 0.024 | 0.0 | 0.0 |
| aes_gcm | 256 | 1 KiB | 4 | 1000 | 0.002 | 0.002 | 0.000 | 0.000 | 601.8 | 2407.2 |
| aes_gcm | 256 | 1 MiB | 4 | 1000 | 0.281 | 0.272 | 0.028 | 0.002 | 3756.7 | 15026.8 |
| aes_gcm | 256 | 100 MiB | 4 | 100 | 159.752 | 158.424 | 8.089 | 1.585 | 658.0 | 2631.9 |
| aes_gcm_decrypt | 256 | 1 KiB | 4 | 1000 | 0.001 | 0.001 | 0.001 | 0.000 | 709.6 | 2838.5 |
| aes_gcm_decrypt | 256 | 1 MiB | 4 | 1000 | 0.186 | 0.184 | 0.009 | 0.001 | 5659.3 | 22637.2 |
| aes_gcm_decrypt | 256 | 100 MiB | 4 | 100 | 97.500 | 98.542 | 6.665 | 1.306 | 1080.4 | 4321.7 |
| hybrid_encrypt | 3072 | 1 KiB | 4 | 1000 | 0.065 | 0.063 | 0.023 | 0.001 | 16.1 | 64.3 |
| hybrid_encrypt | 3072 | 1 MiB | 4 | 1000 | 1.772 | 1.771 | 0.217 | 0.013 | 614.4 | 2457.7 |
| hybrid_encrypt | 3072 | 100 MiB | 4 | 100 | 251.621 | 249.409 | 17.730 | 3.475 | 418.8 | 1675.1 |
| hybrid_decrypt | 3072 | 1 KiB | 4 | 1000 | 1.316 | 1.264 | 0.207 | 0.013 | 0.8 | 3.2 |
| hybrid_decrypt | 3072 | 1 MiB | 4 | 1000 | 1.774 | 1.585 | 0.417 | 0.026 | 612.7 | 2450.7 |
| hybrid_decrypt | 3072 | 100 MiB | 4 | 100 | 171.222 | 170.623 | 13.572 | 2.660 | 616.3 | 2465.2 |

## Measured results (Windows MSVC)

Sources: `output/windows/benchmarks/bench_windows_msvc_rsa_hybrid.csv`,
`summary_msvc.csv`, `graph/`. Same matrix as Ubuntu (1000/1000/100).

| Case | Bits | Size | Threads | Runs | Mean ms | Median ms | SD ms | 95% CI ms | Mean MB/s | Aggregate MB/s |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| rsa_keygen | 3072 | — | 4 | 1000 | 330.932 | 277.948 | 218.385 | 13.536 | — | — |
| rsa_keygen | 4096 | — | 4 | 1000 | 829.217 | 703.324 | 550.214 | 34.103 | — | — |
| rsa_encrypt | 3072 | 32 B | 4 | 1000 | 0.079 | 0.079 | 0.003 | 0.000 | 0.4 | 1.6 |
| rsa_encrypt | 4096 | 32 B | 4 | 1000 | 0.109 | 0.109 | 0.003 | 0.000 | 0.3 | 1.2 |
| rsa_decrypt | 3072 | 32 B | 4 | 1000 | 1.765 | 1.702 | 0.306 | 0.019 | 0.0 | 0.1 |
| rsa_decrypt | 4096 | 32 B | 4 | 1000 | 3.409 | 3.184 | 0.528 | 0.033 | 0.0 | 0.0 |
| aes_gcm | 256 | 1 KiB | 4 | 1000 | 0.003 | 0.003 | 0.001 | 0.000 | 371.3 | 1485.1 |
| aes_gcm | 256 | 1 MiB | 4 | 1000 | 1.199 | 1.106 | 0.319 | 0.020 | 917.2 | 3668.6 |
| aes_gcm | 256 | 100 MiB | 4 | 100 | 123.306 | 122.613 | 6.866 | 1.346 | 853.1 | 3412.3 |
| aes_gcm_decrypt | 256 | 1 KiB | 4 | 1000 | 0.003 | 0.003 | 0.000 | 0.000 | 397.9 | 1591.5 |
| aes_gcm_decrypt | 256 | 1 MiB | 4 | 1000 | 0.638 | 0.570 | 0.452 | 0.028 | 1803.8 | 7215.4 |
| aes_gcm_decrypt | 256 | 100 MiB | 4 | 100 | 76.504 | 77.150 | 9.813 | 1.923 | 1402.9 | 5611.7 |
| hybrid_encrypt | 3072 | 1 KiB | 4 | 1000 | 0.113 | 0.114 | 0.006 | 0.000 | 9.1 | 36.3 |
| hybrid_encrypt | 3072 | 1 MiB | 4 | 1000 | 1.969 | 1.839 | 0.386 | 0.024 | 548.5 | 2194.1 |
| hybrid_encrypt | 3072 | 100 MiB | 4 | 100 | 194.323 | 194.293 | 16.833 | 3.299 | 545.7 | 2182.7 |
| hybrid_decrypt | 3072 | 1 KiB | 4 | 1000 | 1.865 | 1.825 | 0.300 | 0.019 | 0.6 | 2.2 |
| hybrid_decrypt | 3072 | 1 MiB | 4 | 1000 | 2.915 | 2.805 | 0.382 | 0.024 | 364.6 | 1458.4 |
| hybrid_decrypt | 3072 | 100 MiB | 4 | 100 | 136.146 | 137.209 | 12.499 | 2.450 | 779.0 | 3115.9 |

## Interpretation

- **RSA decryption is much slower than encryption** at the same key size
  (1.198 ms vs 0.052 ms at 3072 under four-way concurrency). Encryption uses the
  public exponent; private operations use the secret exponent/CRT and are
  dominated by modular exponentiation.
- **4096-bit RSA is roughly 2.0–3.2× slower** than 3072 for keygen, encrypt and
  decrypt in this run. The larger modulus also raises the direct plaintext limit
  from 318 to 446 bytes, but the safety margin and performance cost favor 3072
  unless a longer lifetime is required.
- **AES-GCM throughput is in the GB/s range** because AES-NI and GCM run in
  hardware; four threads reach about 15.0 GB/s aggregate for 1 MiB encryption
  and 22.6 GB/s for 1 MiB decryption on this host. RSA processes one small block
  per operation. This is why bulk data uses a symmetric cipher.
- **Hybrid overhead is dominated by RSA at small sizes** (1 KiB: ~65 µs encrypt,
  ~1.32 ms decrypt) and by AES-GCM at large sizes (100 MiB: ~252 ms encrypt,
  ~171 ms decrypt per op under four-way concurrency). The crossover is expected:
  one RSA wrap/unwrap plus a fast symmetric pass.
- **Four-way concurrency increases per-op latency for large payloads** because
  the workers share memory bandwidth; aggregate throughput still rises. The
  100 MiB rows are the stable comparison, not the 1 KiB timer resolution.
- **Memory/CPU:** the tool holds the plaintext and ciphertext in memory, so peak
  memory is several times the payload (100 MiB case; four concurrent operations
  increase it further); a streaming envelope would reduce this. RSA key
  generation is the only CPU-expensive setup step.
- **Windows MSVC vs Ubuntu GCC:** the same ordering holds (RSA private >
  public, hybrid dominated by RSA at small sizes and AES at large sizes), but
  absolute numbers differ. At 1 MiB, AES-GCM encrypt is 917 MB/s on Windows and
  3757 MB/s on Ubuntu; RSA-3072 keygen is 331 ms vs 186 ms. Different OS,
  compiler and OpenSSL build mean these are observations, not a controlled
  cross-platform comparison. The Windows run has no environment log.
