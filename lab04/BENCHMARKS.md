# Lab 4 benchmarks

## Environment (Ubuntu)

| Item | Value |
|---|---|
| CPU | 11th Gen Intel Core i5-1135G7 @ 2.40 GHz, 4 cores / 8 threads |
| RAM | 22 GiB |
| OS | Ubuntu 26.04.1 LTS, kernel 7.0.0-43-generic |
| Compiler | GCC 15.2.0, CMake Release (`-O3`) |
| OpenSSL | 4.0.2 at `$HOME/CryptoLib/lib/openssl/gcc` (`libcrypto.so.4`) |
| CMake | 4.2.3 |
| Date | 2026-11-18 |

The same source also builds and passes `ctest` 5/5 with Clang 21.1.8 and with
Ubuntu's system OpenSSL 3.5.5. Only the GCC/OpenSSL 4.0.2 run is benchmarked.

## Method

- Command:

  ```bash
  cd lab04
  OPENSSL_ROOT_DIR="$HOME/CryptoLib/lib/openssl/gcc" scripts/run_benchmarks.sh gcc
  ```

- Algorithms: SHA-256, SHA-512, SHA3-256, SHA3-512.
- Sizes: 1 MiB, 100 MiB, 1024 MiB (approximately 1 GiB).
- I/O paths: `stream` (64 KiB read buffer) and `mmap` (map once, update in
  1 MiB chunks).
- Warm-up: 1 second per combination.
- Runs per combination: 1000 (1 MiB), 100 (100 MiB), 30 (1 GiB).
- Single-threaded; no CPU pinning and no power-plan control.
- Timing: `std::chrono::steady_clock`; CPU: `std::clock`.
- MB = 10^6 bytes; 1 MiB ≈ 1.048576 MB.
- Statistics: mean, median, sample standard deviation and 95% CI
  (`1.96 * sd / sqrt(n)`).
- The three benchmark files are created once under `/tmp/hashtool_bench` and
  reused. `/tmp` is tmpfs on this host (12 GiB), so every run reads from RAM;
  these are warm-cache, CPU-bound numbers, not storage measurements.

Raw data: `output/ubuntu/benchmarks/bench_ubuntu_gcc.csv`.
Summary: `output/ubuntu/benchmarks/summary_ubuntu_gcc.csv`.
Run log: `output/ubuntu/benchmarks/bench_run_log.txt`.
Plots: `output/ubuntu/benchmarks/graphs/`.

## Interactive analysis

`analysis/benchmark_analysis.ipynb` loads the summary CSV and renders the table,
throughput ranking and the four plots interactively. It imports `load` and
`plot_metric` from `scripts/plot_bench.py`, so the notebook and the committed
PNGs use the same code. It needs Jupyter and matplotlib; pandas is not used.
The committed copy contains the executed outputs from the 2026-11-18 data.

## Results (Ubuntu, GCC, OpenSSL 4.0.2)

Mean throughput with 95% confidence interval; MB = 10^6 bytes.

| Algo | Size | stream MB/s | mmap MB/s | mmap vs stream | CPU % (max) |
|---|---:|---:|---:|---:|---:|
| SHA256 | 1 MiB | 1519 ± 7 | 1510 ± 8 | -0.6% | 100.0 |
| SHA256 | 100 MiB | 1421 ± 5 | 1536 ± 7 | +8.1% | 100.0 |
| SHA256 | 1024 MiB | 1370 ± 19 | 1440 ± 20 | +5.1% | 100.0 |
| SHA512 | 1 MiB | 685 ± 3 | 674 ± 3 | -1.7% | 99.9 |
| SHA512 | 100 MiB | 672 ± 3 | 701 ± 2 | +4.3% | 100.0 |
| SHA512 | 1024 MiB | 639 ± 8 | 663 ± 6 | +3.9% | 100.0 |
| SHA3-256 | 1 MiB | 430 ± 2 | 437 ± 2 | +1.7% | 100.0 |
| SHA3-256 | 100 MiB | 402 ± 4 | 402 ± 3 | -0.2% | 100.0 |
| SHA3-256 | 1024 MiB | 386 ± 3 | 405 ± 4 | +4.9% | 100.0 |
| SHA3-512 | 1 MiB | 234 ± 1 | 238 ± 1 | +1.9% | 100.0 |
| SHA3-512 | 100 MiB | 224 ± 2 | 222 ± 1 | -1.1% | 99.9 |
| SHA3-512 | 1024 MiB | 216 ± 2 | 216 ± 2 | +0.1% | 99.9 |

## Analysis

**Ranking.** SHA-256 is the fastest at every size, about 1.37–1.44 GB/s on the
1 GiB case. SHA-512 reaches 0.64–0.66 GB/s (about 0.46× SHA-256), SHA3-256
0.39–0.40 GB/s (about 0.28×), and SHA3-512 0.216 GB/s (about 0.16×). The
SHA-256 advantage is not only algorithmic: OpenSSL uses the x86 SHA extensions
(`sha_ni`) for SHA-256, while SHA-3 is Keccak-f[1600] with no comparable
instruction set on this CPU.

**Stream vs mmap.** At 1 MiB the two paths differ by less than 2%, which is
the size of the measured intervals and therefore not a meaningful difference.
At 100 MiB and 1 GiB, mmap is 4–8% faster for SHA-256 and SHA-512 because the
stream path copies every 64 KiB block into the user buffer while mmap hashes
the page-cache pages directly. SHA-3 is largely flat (within about 5%): the
Keccak permutation dominates, so removing one memory copy barely moves the
total. The 1 GiB SHA3-256 stream case (386 MB/s vs 405 MB/s mmap) is the
largest gap in the SHA-3 rows and is consistent with that copy cost.

**CPU.** Every combination shows about 100% of one core. The tool hashes on the
calling thread only, so this confirms there is no hidden parallelism and that
the numbers are single-core throughput.

**Interpretation limits.** `/tmp` is tmpfs, so all reads come from RAM; these
are warm-cache, CPU-bound results and say nothing about disk throughput. The
host was shared and the CPU frequency was not pinned, so treat absolute MB/s
as indicative. The CI95 values show that within a run the mean is stable, but
they do not capture run-to-run frequency scaling.

## Windows

No Windows benchmark is committed. `scripts/build_windows.bat` writes CTest and
KAT logs under `output/windows/test_results/`, and
`scripts/run_benchmarks.bat` writes the CSV/summary/plots under
`output/windows/benchmarks/`. Run both on the Windows host to complete the
cross-OS comparison; the Ubuntu and Windows numbers would still be different
hosts, not a controlled experiment.

## Known limits

- Warm page cache and a shared host mean the absolute MB/s values are
  indicative, not a hardware peak.
- No pinning or frequency control: the CPU scaling governor and background
  load can shift individual runs, which is why the summary reports CI95.
- `mmap` still calls `EVP_DigestUpdate` in 1 MiB chunks; it removes the read
  copy but not the hashing cost.
- SHA-256 benefits from x86 SHA extensions in OpenSSL; SHA-3 has no comparable
  instruction set, so the gap is structural as well as algorithmic.
