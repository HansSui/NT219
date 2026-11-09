# Lab 3 - Missing / Lacking

**Audit date:** 2026-11-09
**Sources:** `docs/Lab3.txt`, `docs/lab requirement.md`,
`docs/GradingSystem.txt`, `docs/blueprint/labs/lab3.md`, `lab03/`
**Method:** rebuilt Lab 3 on Ubuntu 26.04 with GCC 15.2 and OpenSSL 4.0.2,
ran `--kat`, all CTest targets and the report tables against the committed
benchmark CSV; reviewed the CLI, negative tests, build scripts and plot
pipeline; regenerated the three plots as grouped bars. No Windows host was
available, so Windows is **not verified** here.

Status legend: **OK** = present and verified, **Partial** = present but
incomplete, **Missing** = not present, **Not verified** = no evidence here.

## Verdict

The Lab 3 implementation meets the functional brief: RSA-3072/4096 OAEP with
SHA-256 and an optional label, PEM/DER/metadata key output, automatic
AES-256-GCM hybrid envelope, fail-closed negative tests, and a complete
benchmark matrix. The 2026-11-09 regrade is **97/100 core** plus **6/10
optional hybrid-analysis bonus** (manual OAEP bonus not attempted). The
unscored gaps are submission work and Windows evidence, not a broken RSA or
envelope path.

Two real documentation defects were fixed in this pass: the report/benchmark
tables had been written before the final benchmark rerun and disagreed with
`summary_gcc.csv`, and the line-chart plots hid the RSA encrypt/decrypt and
hybrid-vs-AES comparison.

## Verified state after fixes

| Check | Ubuntu | Windows |
|---|---|---|
| Build and `ctest` | Pass, 5/5 (GCC 15.2; committed Clang log also 5/5) | Script present; not run |
| CLI KAT | `2 passed, 0 failed` (GCC and Clang logs) | Not verified |
| Unit/negative tests | OAEP 24 assertions/7 cases; hybrid 27/5; negative 20/5 | Not verified |
| CLI negative suite | Pass (label, wrong key, tampered payload/header, malformed input) | Not verified |
| Benchmark data | 18-case CSV, summary, 3 plots (1000/1000/100, 4 threads) | Missing |
| Docs vs evidence | Tables now match `summary_gcc.csv` | N/A |

Ubuntu commands used:

```bash
cd lab03
scripts/build_linux.sh gcc
./build/ubuntu-gcc/rsatool --kat kat/oaep_vectors.json
(cd build/ubuntu-gcc && ctest --output-on-failure)
```

## Gaps

| ID | Gap | Requirement | Ubuntu | Windows | Severity |
|---|---|---|---|---|---|
| L3-01 | No Windows build/test/KAT evidence | C-13; rubric cross-platform 5 | N/A | Not verified | High |
| L3-02 | Final report identity, PDF/DOCX and archive absent | C-13, C-14; report 10 | Partial | Partial | High (submission) |
| L3-03 | No Windows benchmark run or benchmark script | Performance evidence | N/A | Missing | Medium |
| L3-04 | Report/BENCHMARKS tables disagreed with the committed CSV | Report quality | Fixed 2026-11-09 | N/A | Closed |
| L3-05 | Line plots obscured the required comparisons | Performance plots | Fixed 2026-11-09 | N/A | Closed |
| L3-06 | Manual OAEP padding bonus not implemented | Bonus +5 | Missing | Missing | Optional |
| L3-07 | Hybrid security analysis is partial | Bonus +10 | Partial (6/10) | N/A | Optional |

### L3-01 - Windows build, tests and KAT are not evidenced

`scripts/build_windows.bat` supports MSVC, MinGW GCC and MinGW Clang and pins
OpenSSL 4.0.2, but `lab03/output/windows/` is empty. The rubric awards 5 points
for cross-platform build and documentation; Ubuntu GCC and Clang are verified,
so 4/5 is held pending a Windows run. Do not claim Windows parity until
`output/windows/` contains build/`ctest`/KAT logs.

Fix on a Windows host:

```bat
cd lab03
set "OPENSSL_ROOT_DIR=D:\cryptolibrary"
scripts\build_windows.bat msvc
build\windows-msvc\Release\rsatool.exe --kat kat\oaep_vectors.json
pushd build\windows-msvc
ctest -C Release --output-on-failure
popd
```

### L3-02 - Final report package is not produced

`REPORT.md`, `README.md` and `BENCHMARKS.md` contain the objectives, design,
security discussion, test evidence, performance analysis and self-grade, but:

- Student name, student ID and class are blank.
- No PDF/DOCX export exists.
- No submission archive exists.

This is the only core deduction besides Windows evidence: report quality
9/10. The rubric content itself is present.

### L3-03 - No Windows benchmark data

There is no `run_benchmarks.bat` for Lab 3. The benchmark is not required on
both OSes by the brief, but controlled Windows data would strengthen the
performance evidence. The command is in "Benchmark commands" below; the CSV
schema, summary script and plot script are cross-platform.

### L3-04 - Documentation tables were stale (fixed)

`BENCHMARKS.md` and `REPORT.md` were written at 14:02 on 2026-11-07, then the
benchmark was rerun at 14:38. For example, the docs said RSA-3072 keygen
207.04 ms while `summary_gcc.csv` says 186.41 ms; AES-GCM 1 MiB encryption was
2814.4 MB/s in the docs and 3756.7 MB/s in the committed summary. The tables,
ratio statements and GB/s figures were corrected against the committed CSV on
2026-11-09. The CSV/summary are the evidence of record; do not hand-edit them.

### L3-05 - Line-chart plots hid the comparisons (fixed)

The old `plot_bench.py` drew lines between 3072 and 4096 bits. RSA keygen
(~186/605 ms) swamped encrypt (0.05/0.11 ms) and decrypt (1.20/2.64 ms) on a
linear scale, and the AES/hybrid throughput lines connected discrete payload
sizes. The script now draws grouped bars with 95% CI whiskers and value labels,
matching the Lab 1/Lab 2 comparison style:

- `rsa_latency.png`: 3072 vs 4096, grouped keygen/encrypt/decrypt, log scale.
- `hybrid_throughput.png`: 1 KiB/1 MiB/100 MiB, grouped AES-GCM and hybrid
  encrypt/decrypt, aggregate MB/s.
- `hybrid_latency.png`: same groups, per-operation ms on a log scale.

Regenerate with:

```bash
cd lab03
python3 scripts/plot_bench.py \
  --input output/ubuntu/benchmarks/bench_ubuntu_gcc_rsa_hybrid.csv \
  --out-dir output/ubuntu/benchmarks/graph --platform ubuntu-gcc
```

### L3-06 - Manual OAEP bonus not attempted

The implementation uses OpenSSL's OAEP (`EVP_PKEY_encrypt`/`decrypt`) rather
than hand-written MGF1, XOR masking and constant-time padding validation. The
brief makes this optional (+5); not claiming it is correct.

### L3-07 - Hybrid security analysis bonus is partial

`REPORT.md` compares the envelope with direct RSA, explains the TLS hybrid
design and the forward-secrecy limitation, and proposes ECDHE/ML-KEM. It does
not include the optional AES-CTR+HMAC comparison or authentication-failure
timing variance, so 6/10 of the bonus is claimed.

## Rubric regrade 2026-11-09

Rubric from `docs/Lab3.txt`: 100 core + 15 bonus.

| Criterion | Max | Score | Reason |
|---|---:|---:|---|
| Correct RSA-OAEP (SHA-256) | 20 | 20 | 3072/4096, MGF1-SHA-256, optional label, limit test, KAT 2/2 |
| Correct hybrid envelope | 20 | 20 | Auto-switch, AES-256-GCM, 96-bit IV, RSA-wrapped key, authenticated decrypt |
| Key management and formats | 15 | 15 | PEM + DER + metadata; <3072 rejected; 3072/4096 tested |
| Negative tests and secure errors | 15 | 15 | Altered ciphertext/tag/IV/wrapped key/header, wrong key/label, malformed input; no output on failure |
| Performance study and analysis | 15 | 14 | Full matrix, CI, grouped plots and interpretation; single host, no Windows run |
| Cross-platform build and documentation | 5 | 4 | Ubuntu GCC + Clang verified; Windows script exists but is unrun |
| Report quality | 10 | 9 | Content complete; identity/PDF/archive missing |
| **Core** | **100** | **97** | |
| Bonus: manual OAEP | +5 | 0 | Library OAEP, no manual MGF1 |
| Bonus: hybrid security analysis | +10 | 6 | TLS/envelope/forward secrecy; no CTR+HMAC timing study |
| **Total** | **115** | **103** | |

A lenient grader who accepts the committed Windows build script in place of a
run, or does not deduct for a single-host benchmark, could reach 99/100 core.
The safe claim is 97 until Windows logs and the report package exist.

## Per-environment actions

### Ubuntu

1. Re-run `scripts/build_linux.sh gcc`, `--kat` and `ctest`; all pass
   2026-11-09.
2. Keep the committed benchmark CSV/summary; regenerate plots with
   `scripts/plot_bench.py` after any rerun.
3. Optionally regenerate `output/ubuntu/test_results/` logs if the submission
   needs fresh console evidence.

### Windows

1. Build with OpenSSL 4.0.2 and run KAT + `ctest` (see L3-01).
2. Run the benchmark command below and commit CSV/summary/plots under
   `output/windows/`.
3. Add the Windows result to `BENCHMARKS.md` or clearly mark it as Ubuntu-only.

### Submission

1. Fill student identity in `REPORT.md`.
2. Export PDF/DOCX and create the archive.
3. Keep the OpenSSL 4.0.2 requirement and Windows evidence statement accurate.

## Benchmark commands

Linux (already run; this is the reproducible command):

```bash
cd lab03
PIN_CPU=0-3 THREADS=4 RUNS_RSA=1000 RUNS_SMALL=1000 RUNS_LARGE=100 \
  WARMUP=1 scripts/run_benchmarks.sh gcc
```

Windows (not run here; there is no batch wrapper, so invoke the tool directly):

```bat
cd lab03
set "OPENSSL_ROOT_DIR=D:\cryptolibrary"
scripts\build_windows.bat msvc
build\windows-msvc\Release\rsatool.exe bench --runs-rsa 1000 --runs-small 1000 ^
  --runs-large 100 --threads 4 --warmup 1 ^
  --out output\windows\benchmarks\bench_windows_msvc_rsa_hybrid.csv --verbose
python scripts\summarize_bench.py ^
  --input output\windows\benchmarks\bench_windows_msvc_rsa_hybrid.csv ^
  --out output\windows\benchmarks\summary_msvc.csv
python scripts\plot_bench.py ^
  --input output\windows\benchmarks\bench_windows_msvc_rsa_hybrid.csv ^
  --out-dir output\windows\benchmarks\graph --platform windows-msvc
```

The Windows command does not pin affinity; use `start /affinity F /wait /b`
around `rsatool.exe` if a 4-CPU controlled run is required.

## Residual risk

- Ubuntu-only evidence; Windows build, KAT, CTest and benchmark are unverified.
- OpenSSL is the sole implementation; the two KAT vectors and OpenSSL CLI
  interoperability are the independent checks.
- Benchmarks are single-host and indicative; 100 MiB runs use 100 samples.
- The report/PDF/identity package is still missing, so the submission is not
  final.
