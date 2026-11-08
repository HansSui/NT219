# Lab 3 benchmarks

## Environment

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

- Build: `scripts/build_linux.sh gcc` (requires and links OpenSSL 4.0.2).
- Affinity: `taskset -c 0-3` (four logical CPUs) by default.
- Threads: 4 worker threads; each worker times independent operations.
- Warm-up: 1 s per worker before each case.
- Runs: **RSA keygen/encrypt/decrypt 1000**, **AES/hybrid 1 KiB and 1 MiB
  1000**, **AES/hybrid 100 MiB 100**.
- RSA: one operation per timed sample; message 32 bytes, OAEP-SHA256.
- AES-GCM/Hybrid: one operation per timed sample at the stated size.
- Metrics: mean, median, sample standard deviation and 95% CI
  (`1.96 * sd / sqrt(n)`). `elapsed_ms` is per operation under four-way
  concurrency; aggregate throughput is `per-op MB/s × 4`.
- CSV: `output/ubuntu/benchmarks/bench_ubuntu_gcc_rsa_hybrid.csv`.
- Summary: `output/ubuntu/benchmarks/summary_gcc.csv`.
- Plots: `output/ubuntu/benchmarks/graph/`.
- Runtime evidence: `output/ubuntu/test_results/environment.txt`.

Reproduce:

```bash
cd lab03
PIN_CPU=0-3 THREADS=4 RUNS_RSA=1000 RUNS_SMALL=1000 RUNS_LARGE=100 \
  WARMUP=1 scripts/run_benchmarks.sh gcc
```

The script defaults are the same; the environment variables are shown explicitly.
Key generation is inherently variable (prime search); its standard deviation is
large. Numbers are single-host and indicative, not a cross-machine record.

## Measured results

`elapsed` is per operation; `MB/s` is per-operation throughput; aggregate is for
all four worker threads.

| Case | Bits | Size | Threads | Runs | Mean ms | Median ms | SD ms | 95% CI ms | Mean MB/s | Aggregate MB/s |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| rsa_keygen | 3072 | — | 4 | 1000 | 207.036 | 170.745 | 138.860 | 8.607 | — | — |
| rsa_keygen | 4096 | — | 4 | 1000 | 687.465 | 599.651 | 455.906 | 28.257 | — | — |
| rsa_encrypt | 3072 | 32 B | 4 | 1000 | 0.055 | 0.054 | 0.005 | 0.000 | 0.6 | 2.3 |
| rsa_encrypt | 4096 | 32 B | 4 | 1000 | 0.108 | 0.096 | 0.052 | 0.003 | 0.3 | 1.2 |
| rsa_decrypt | 3072 | 32 B | 4 | 1000 | 1.316 | 1.205 | 0.261 | 0.016 | 0.0 | 0.1 |
| rsa_decrypt | 4096 | 32 B | 4 | 1000 | 3.090 | 2.806 | 0.651 | 0.040 | 0.0 | 0.0 |
| aes_gcm | 256 | 1 KiB | 4 | 1000 | 0.002 | 0.002 | 0.001 | 0.000 | 553.3 | 2213.2 |
| aes_gcm | 256 | 1 MiB | 4 | 1000 | 0.396 | 0.370 | 0.248 | 0.015 | 2814.4 | 11257.6 |
| aes_gcm | 256 | 100 MiB | 4 | 100 | 172.950 | 169.262 | 16.977 | 3.328 | 611.4 | 2445.7 |
| aes_gcm_decrypt | 256 | 1 KiB | 4 | 1000 | 0.001 | 0.001 | 0.001 | 0.000 | 706.3 | 2825.0 |
| aes_gcm_decrypt | 256 | 1 MiB | 4 | 1000 | 0.204 | 0.195 | 0.022 | 0.001 | 5195.0 | 20779.9 |
| aes_gcm_decrypt | 256 | 100 MiB | 4 | 100 | 119.207 | 119.690 | 14.048 | 2.753 | 892.3 | 3569.0 |
| hybrid_encrypt | 3072 | 1 KiB | 4 | 1000 | 0.081 | 0.075 | 0.015 | 0.001 | 13.0 | 52.0 |
| hybrid_encrypt | 3072 | 1 MiB | 4 | 1000 | 1.887 | 1.806 | 0.221 | 0.014 | 562.5 | 2250.1 |
| hybrid_encrypt | 3072 | 100 MiB | 4 | 100 | 274.844 | 268.183 | 25.341 | 4.967 | 384.6 | 1538.4 |
| hybrid_decrypt | 3072 | 1 KiB | 4 | 1000 | 1.421 | 1.304 | 0.278 | 0.017 | 0.7 | 3.0 |
| hybrid_decrypt | 3072 | 1 MiB | 4 | 1000 | 2.017 | 1.737 | 0.585 | 0.036 | 551.1 | 2204.5 |
| hybrid_decrypt | 3072 | 100 MiB | 4 | 100 | 185.481 | 182.949 | 14.457 | 2.834 | 568.6 | 2274.3 |

## Interpretation

- **RSA decryption is much slower than encryption** at the same key size
  (1.316 ms vs 0.055 ms at 3072 under four-way concurrency). Encryption uses the
  public exponent; private operations use the secret exponent/CRT and are
  dominated by modular exponentiation.
- **4096-bit RSA is roughly 2.0–3.3× slower** than 3072 for keygen, encrypt and
  decrypt in this run. The larger modulus also raises the direct plaintext limit
  from 318 to 446 bytes, but the safety margin and performance cost favor 3072
  unless a longer lifetime is required.
- **AES-GCM throughput is in the GB/s range** because AES-NI and GCM run in
  hardware; four threads reach about 11.3 GB/s aggregate for 1 MiB encryption
  and 20.8 GB/s for 1 MiB decryption on this host. RSA processes one small block
  per operation. This is why bulk data uses a symmetric cipher.
- **Hybrid overhead is dominated by RSA at small sizes** (1 KiB: ~81 µs encrypt,
  ~1.42 ms decrypt) and by AES-GCM at large sizes (100 MiB: ~275 ms encrypt,
  ~185 ms decrypt per op under four-way concurrency). The crossover is expected:
  one RSA wrap/unwrap plus a fast symmetric pass.
- **Four-way concurrency increases per-op latency for large payloads** because
  the workers share memory bandwidth; aggregate throughput still rises. The
  100 MiB rows are the stable comparison, not the 1 KiB timer resolution.
- **Memory/CPU:** the tool holds the plaintext and ciphertext in memory, so peak
  memory is several times the payload (100 MiB case; four concurrent operations
  increase it further); a streaming envelope would reduce this. RSA key
  generation is the only CPU-expensive setup step.
