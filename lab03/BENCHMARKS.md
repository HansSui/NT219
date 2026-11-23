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
| rsa_keygen | 3072 | — | 4 | 1000 | 180.157 | 155.273 | 117.502 | 7.283 | — | — |
| rsa_keygen | 4096 | — | 4 | 1000 | 606.081 | 512.946 | 413.817 | 25.649 | — | — |
| rsa_encrypt | 3072 | 32 B | 4 | 1000 | 0.053 | 0.052 | 0.004 | 0.000 | 0.6 | 2.4 |
| rsa_encrypt | 4096 | 32 B | 4 | 1000 | 0.097 | 0.091 | 0.014 | 0.001 | 0.3 | 1.3 |
| rsa_decrypt | 3072 | 32 B | 4 | 1000 | 1.217 | 1.139 | 0.349 | 0.022 | 0.0 | 0.1 |
| rsa_decrypt | 4096 | 32 B | 4 | 1000 | 2.659 | 2.524 | 0.473 | 0.029 | 0.0 | 0.0 |
| aes_gcm | 256 | 1 KiB | 4 | 1000 | 0.002 | 0.002 | 0.001 | 0.000 | 531.0 | 2124.1 |
| aes_gcm | 256 | 1 MiB | 4 | 1000 | 0.369 | 0.339 | 0.189 | 0.012 | 3021.5 | 12086.1 |
| aes_gcm | 256 | 100 MiB | 4 | 100 | 150.492 | 146.686 | 21.751 | 4.263 | 708.1 | 2832.5 |
| aes_gcm_decrypt | 256 | 1 KiB | 4 | 1000 | 0.002 | 0.002 | 0.001 | 0.000 | 613.9 | 2455.4 |
| aes_gcm_decrypt | 256 | 1 MiB | 4 | 1000 | 0.196 | 0.187 | 0.021 | 0.001 | 5395.6 | 21582.5 |
| aes_gcm_decrypt | 256 | 100 MiB | 4 | 100 | 81.777 | 80.274 | 5.129 | 1.005 | 1287.0 | 5148.1 |
| hybrid_encrypt | 3072 | 1 KiB | 4 | 1000 | 0.070 | 0.066 | 0.011 | 0.001 | 14.8 | 59.3 |
| hybrid_encrypt | 3072 | 1 MiB | 4 | 1000 | 1.817 | 1.728 | 0.272 | 0.017 | 586.3 | 2345.1 |
| hybrid_encrypt | 3072 | 100 MiB | 4 | 100 | 219.221 | 217.102 | 13.365 | 2.620 | 480.1 | 1920.2 |
| hybrid_decrypt | 3072 | 1 KiB | 4 | 1000 | 1.297 | 1.237 | 0.201 | 0.012 | 0.8 | 3.2 |
| hybrid_decrypt | 3072 | 1 MiB | 4 | 1000 | 1.674 | 1.578 | 0.260 | 0.016 | 637.5 | 2549.8 |
| hybrid_decrypt | 3072 | 100 MiB | 4 | 100 | 152.524 | 150.847 | 10.226 | 2.004 | 690.4 | 2761.7 |

## Measured results (Windows MSVC)

Sources: `output/windows/benchmarks/bench_windows_msvc_rsa_hybrid.csv`,
`summary_msvc.csv`, `graph/`. Same matrix as Ubuntu (1000/1000/100).

| Case | Bits | Size | Threads | Runs | Mean ms | Median ms | SD ms | 95% CI ms | Mean MB/s | Aggregate MB/s |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| rsa_keygen | 3072 | — | 4 | 1000 | 369.274 | 309.459 | 238.596 | 14.788 | — | — |
| rsa_keygen | 4096 | — | 4 | 1000 | 1081.614 | 901.519 | 721.901 | 44.744 | — | — |
| rsa_encrypt | 3072 | 32 B | 4 | 1000 | 0.107 | 0.106 | 0.015 | 0.001 | 0.3 | 1.2 |
| rsa_encrypt | 4096 | 32 B | 4 | 1000 | 0.180 | 0.177 | 0.046 | 0.003 | 0.2 | 0.7 |
| rsa_decrypt | 3072 | 32 B | 4 | 1000 | 2.229 | 2.189 | 0.393 | 0.024 | 0.0 | 0.1 |
| rsa_decrypt | 4096 | 32 B | 4 | 1000 | 4.710 | 4.541 | 0.658 | 0.041 | 0.0 | 0.0 |
| aes_gcm | 256 | 1 KiB | 4 | 1000 | 0.005 | 0.004 | 0.010 | 0.001 | 249.8 | 999.3 |
| aes_gcm | 256 | 1 MiB | 4 | 1000 | 1.590 | 1.474 | 0.456 | 0.028 | 691.5 | 2765.9 |
| aes_gcm | 256 | 100 MiB | 4 | 100 | 174.884 | 174.810 | 15.537 | 3.045 | 604.3 | 2417.3 |
| aes_gcm_decrypt | 256 | 1 KiB | 4 | 1000 | 0.003 | 0.003 | 0.001 | 0.000 | 314.1 | 1256.2 |
| aes_gcm_decrypt | 256 | 1 MiB | 4 | 1000 | 0.893 | 0.845 | 0.213 | 0.013 | 1227.0 | 4908.0 |
| aes_gcm_decrypt | 256 | 100 MiB | 4 | 100 | 105.640 | 105.602 | 9.063 | 1.776 | 1001.3 | 4005.1 |
| hybrid_encrypt | 3072 | 1 KiB | 4 | 1000 | 0.161 | 0.159 | 0.063 | 0.004 | 6.5 | 26.0 |
| hybrid_encrypt | 3072 | 1 MiB | 4 | 1000 | 2.805 | 2.701 | 0.504 | 0.031 | 383.8 | 1535.1 |
| hybrid_encrypt | 3072 | 100 MiB | 4 | 100 | 248.051 | 248.589 | 8.298 | 1.626 | 423.2 | 1692.8 |
| hybrid_decrypt | 3072 | 1 KiB | 4 | 1000 | 2.419 | 2.326 | 0.396 | 0.025 | 0.4 | 1.7 |
| hybrid_decrypt | 3072 | 1 MiB | 4 | 1000 | 4.579 | 3.688 | 2.403 | 0.149 | 258.4 | 1033.5 |
| hybrid_decrypt | 3072 | 100 MiB | 4 | 100 | 161.401 | 161.691 | 6.079 | 1.191 | 650.6 | 2602.4 |

## Interpretation

- **RSA decryption is much slower than encryption** at the same key size
  (1.217 ms vs 0.053 ms at 3072 under four-way concurrency). Encryption uses the
  public exponent; private operations use the secret exponent/CRT and are
  dominated by modular exponentiation.
- **4096-bit RSA is roughly 1.7–3.4× slower** than 3072 for keygen, encrypt and
  decrypt in this run. The larger modulus also raises the direct plaintext limit
  from 318 to 446 bytes, but the safety margin and performance cost favor 3072
  unless a longer lifetime is required.
- **AES-GCM throughput is in the GB/s range** because AES-NI and GCM run in
  hardware; four threads reach about 12.1 GB/s aggregate for 1 MiB encryption
  and 21.6 GB/s for 1 MiB decryption on this host. RSA processes one small block
  per operation. This is why bulk data uses a symmetric cipher.
- **Hybrid overhead is dominated by RSA at small sizes** (1 KiB: ~70 µs encrypt,
  ~1.30 ms decrypt) and by AES-GCM at large sizes (100 MiB: ~219 ms encrypt,
  ~153 ms decrypt per op under four-way concurrency). The crossover is expected:
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
  absolute numbers differ. At 1 MiB, AES-GCM encrypt is 691.5 MB/s on Windows
  and 3021.5 MB/s on Ubuntu; RSA-3072 keygen is 369.3 ms vs 180.2 ms. Different
  OS, compiler and OpenSSL build mean these are observations, not a controlled
  cross-platform comparison. The Windows run has no environment log.
