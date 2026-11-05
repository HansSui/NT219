# Lab 2 - Missing / Lacking

**Audit date:** 2026-11-02 (updated 2026-11-05)
**Implementation pass:** 2026-11-02; Windows benchmark audit 2026-11-05
**Sources:** `docs/Lab2.txt`, `docs/lab2_reference.md`,
`docs/lab requirement.md`, `docs/GradingSystem.txt`,
`docs/blueprint/labs/lab2.md`, `lab02/`
**Method:** inspected the existing Ubuntu artifacts, rebuilt and tested Lab 2
on Windows with MSVC 19.51, MinGW GCC 16.2.0 and MinGW Clang 22.1.8, and
reviewed the code, tests, scripts and committed benchmark CSVs. On
2026-11-05 I validated the new Windows MSVC benchmark CSVs, generated summary
and plots, and wrote `lab02/output/windows/benchmarks/AUDIT.md`. Ubuntu was
not re-run in this pass because this host has no WSL/Linux distribution.

Status legend: **OK** = present and verified, **Partial** = present but
incomplete, **Missing** = not present, **Not verified** = no evidence here.

## Verdict

The required AES-128 core, CTR mode, KAT/cross-validation, CLI and Windows
test evidence are now in place. Lab 2 is **code-complete and
test-complete on Windows**, with bonus A (AES-192/256) and bonus B (XTS)
working. The Windows MSVC benchmark now exists for all three sizes with
summary and plots. It is **not final-submission-complete**: the Ubuntu test
logs are still missing, the Ubuntu benchmark data uses the old warm-up and
`powersave` governor, the Windows run uses the chosen 1000/100/30 counts (the
blueprint's nominal 500/50 is not required) and has no run-time environment
log, the Ubuntu 1 GiB plot and Crypto++ throughput comparison are missing,
and there is no PDF/DOCX report.

## Baseline before this pass

| Check | Ubuntu | Windows |
|---|---|---|
| Build and `ctest` | Pass, 3/3 (GCC, no committed log) | Missing / not verified |
| CLI KAT | 4/4 pass | Missing / not verified |
| Cross-validation | Pass, 3/3 | Missing / not verified |
| Benchmark data | 1 MiB/1000, 100 MiB/100, 1 GiB/30 CSV + summary | Missing |
| Test evidence logs | Directory empty | Directory empty |
| Report | No report file | No report file |

Chosen benchmark rows: 1 MiB = 1000, 100 MiB = 100, 1 GiB = 30. The
blueprint's nominal 500/50 is a target, but the chosen 100/30 is within the
course's N >= 30 band and is the accepted run for this submission.

Current Windows benchmark (2026-11-05, MSVC): also 1000/100/30, throughput
6.74 / 7.02 / 7.09 MiB/s, CV 6.3% / 1.3% / 0.7%, all `ok`. It has summary and
six plots but no run-time environment log; see
`lab02/output/windows/benchmarks/AUDIT.md`.

## Implementation log - 2026-11-02

All commands below were run from `E:\NT219\lab02`; logs are in
`lab02/output/windows/test_results/`.

| Step | Command | Result |
|---|---|---|
| MSVC build | `cmd /c scripts\build_windows.bat msvc` | Release build OK; Crypto++ oracle found at `D:/cryptolibrary` |
| MSVC KAT | `build\windows-msvc\Release\aestool.exe --kat kat\vectors.json` | `9 passed, 0 failed` |
| MSVC `ctest` | `ctest -C Release --output-on-failure` in `build/windows-msvc` | `4/4` passed |
| MSVC unit tests | `build\windows-msvc\Release\test_core.exe` | 351 assertions in 11 test cases |
| MSVC cross-validation | `build\windows-msvc\Release\test_crossval.exe` | 22 assertions in 3 test cases |
| MinGW GCC | `scripts\build_windows.bat gcc` with `D:\mys64\mingw64\bin` first on `PATH` | `9/9` KAT, `4/4` CTest |
| MinGW Clang | `scripts\build_windows.bat clang` with the same `PATH` | `9/9` KAT, `4/4` CTest |
| Bench progress | `aestool bench ... --verbose` | `progress: i/N (%, elapsed, ETA)` and `bench: completed ...` on stderr; CSV unchanged |
| Evidence files | KAT/CTest redirects and `environment.txt` under `output/windows/test_results/` | written |

The MinGW Clang build links the GCC-built Crypto++ archive. The local
`D:\cryptolibrary\libs\cryptopp\clang\libcryptopp.a` produced a heap-corruption
crash in `test_crossval` with MinGW clang; the GCC archive links and passes.
The build script now selects the GCC archive for both MinGW compilers. This is
a local oracle-build issue, not an `aestool` issue.

Build directories are ignored with `lab02/.gitignore` (`build/`); only source,
tests, docs and evidence are intended to be committed.

## Status after this pass

| ID | Gap | Status | Evidence / remaining |
|---|---|---|---|
| L2-01 | No Windows build/test/KAT evidence | **OK** | MSVC + MinGW GCC + MinGW Clang KAT/CTest logs exist; Windows benchmark CSV, summary and plots added |
| L2-02 | No committed Ubuntu test/KAT logs | **Partial** | Commands documented; no Linux host here |
| L2-03 | No final report | **Partial** | `lab02/REPORT.md` written; PDF/DOCX and identity fields missing |
| L2-04 | Thin KAT coverage; skips hidden | **OK** | 9 KATs; unsupported mode counts as skip and exits nonzero |
| L2-05 | Cross-validation optional | **OK** | CMake fails if `ENABLE_CROSSVAL=ON` and Crypto++ is missing |
| L2-06 | Benchmark protocol not required | **Partial** | Time-based warm-up, 4 threads and Windows High Performance/affinity implemented; Windows data is 1000/100/30 and Ubuntu still old |
| L2-07 | No Crypto++ performance or S-box analysis | **Partial** | S-box computed/verified and report analysis written; Crypto++ throughput benchmark still missing |
| L2-08 | CLI not course-uniform | **OK** | `--encode`, `--decode`, `--nonce`, `--threads`, `--verbose`; conflicts and unknown bench options rejected; `bench --verbose` reports progress/ETA |
| L2-09 | Windows UTF-8 paths/text broken | **OK** | `GetCommandLineW` + UTF-8 + `u8path`; Windows CLI test covers `thư mục` and Vietnamese text |
| L2-10 | Whole-file buffering | **Partial** | Documented; CTR streaming not implemented |
| L2-11 | No key wiping / IV-reuse discussion | **Partial** | Key/round-key wipe and report security section added; no stateful IV registry |
| L2-12 | Docs/plots/counts disagree | **Partial** | README/BENCHMARKS aligned with actual counts; Windows 1 GiB plots exist, Ubuntu 1 GiB plot still missing |

### L2-01 - Windows evidence

Now present:

- `output/windows/test_results/kat_msvc.txt`
- `output/windows/test_results/ctest_msvc.txt`
- `output/windows/test_results/kat_mingw_gcc.txt`
- `output/windows/test_results/ctest_mingw_gcc.txt`
- `output/windows/test_results/kat_mingw_clang.txt`
- `output/windows/test_results/ctest_mingw_clang.txt`
- `output/windows/test_results/unit_*.txt` (351 assertions / 11 cases)
- `output/windows/test_results/crossval_*.txt` (22 assertions / 3 cases)
- `output/windows/test_results/environment.txt`

All three compilers pass `9/9` KAT and `4/4` CTest. `ctest` includes unit
tests, KAT, CLI and cross-validation. The Windows MSVC benchmark now has
three CSVs, `summary.csv`, `summary_robust.csv`, six plots and `AUDIT.md`
under `output/windows/benchmarks/`. Row counts are 1000/100/30; the CSV schema
does not record power plan, affinity or thread count.

### L2-02 - Ubuntu test/KAT logs

Still missing because this machine has no Linux/WSL. The commands are:

```bash
cd lab02
scripts/build_linux.sh gcc
./build/ubuntu-gcc/aestool --kat kat/vectors.json \
  | tee output/ubuntu/test_results/kat_gcc.txt
(cd build/ubuntu-gcc && ctest --output-on-failure) \
  | tee output/ubuntu/test_results/ctest_gcc.txt
```

Expected counts after the changes: `9 passed, 0 failed` and `4/4` CTest.
Do not fabricate Ubuntu logs.

### L2-03 - Report

`lab02/REPORT.md` now covers objective, design, KATs, cross-validation,
negative tests, benchmark method, the committed performance table, S-box/cache
analysis, nonce reuse, malleability, key wiping, AES-NI, rubric self-grade and
remaining work.

Still missing: student name/MSSV/class, PDF/DOCX export, and a final package.
The report explicitly labels the Ubuntu numbers as old-protocol data.

### L2-04 - KAT coverage and skips

`lab02/kat/vectors.json` now has 9 vectors:

- NIST SP 800-38A AES-128/AES-192/AES-256 CTR, four blocks each.
- NIST SP 800-38A AES-128 CTR truncated to 20 bytes (partial final block).
- FIPS-197 C.1/C.2/C.3 ECB single blocks.
- IEEE P1619/D16 XTS-AES-128 vector 1.
- NIST CAVP XTSGenAES128 COUNT 1.

The KAT runner handles `ctr`, `ecb` and `xts`; an unsupported mode increments
`skipped` and makes the process exit nonzero. The CLI test checks both the
9/9 success and the unsupported-mode failure.

### L2-05 - Cross-validation

`CMakeLists.txt` now fails configuration when `ENABLE_CROSSVAL=ON` and either
the Crypto++ headers or library is missing. It also searches `cryptlib` (the
MSVC static archive) and the local `libs/cryptopp/{msvc,gcc}` directories.
`-DENABLE_CROSSVAL=OFF` is an explicit opt-out that prints a warning; such a
build cannot claim the cross-validation requirement.

### L2-06 - Benchmark protocol

Implemented:

- `bench --warmup <seconds>` with a default 1.0 s, using at most a 1 MiB
  prefix so a 1 GiB case is not delayed by a minute-long warm-up.
- `bench --threads <n>` with a worker-thread split of the payload.
- Linux `run_benchmarks.sh` uses `taskset -c ${PIN_CPU:-0}` when available,
  logs governor/pinning, and passes `--warmup` and `--threads`.
- Windows `run_benchmarks.bat` defaults to the chosen 1 MiB/1000,
  100 MiB/100, 1 GiB/30 matrix, uses High Performance, `/high /affinity`,
  and passes `--warmup`/`--threads`. The nominal 500/50 can be set with
  `RUNS_100MB=500 RUNS_1GB=50`.
- Both scripts default to `THREADS=4`; `THREADS=1` gives the single-thread
  baseline. Windows uses `PIN_MASK=F` (CPUs 0-3) for 4 threads and
  `PIN_MASK=4` for one thread; the script warns when affinity selects only
  one CPU.
- Windows benchmark data now exists at 1000/100/30, with summary and plots.

Not done: the Windows run uses the chosen 1000/100/30 counts but has no
run-time environment log. Ubuntu still has the old 5-iteration warm-up,
`powersave`, one thread and no pinning; it must be rerun.

Ubuntu rerun:

```bash
cd lab02
scripts/build_linux.sh gcc
THREADS=4 PIN_CPU=0-3 scripts/run_benchmarks.sh gcc
python3 scripts/summarize_bench.py --dir output/ubuntu/benchmarks
for f in output/ubuntu/benchmarks/bench_ubuntu_gcc_ctr_*.csv; do
  python3 scripts/plot_bench.py "$f" \
    --out-dir output/ubuntu/benchmarks/graphs --platform gcc
done
```

Windows rerun (only if a different matrix is required):

```bat
cd lab02
scripts\build_windows.bat msvc
scripts\run_benchmarks.bat msvc
python scripts\summarize_bench.py --dir output\windows\benchmarks
```

### L2-07 - Crypto++ performance and S-box analysis

Added `AES::sbox_computed()`, which computes the S-box from GF(2^8) inversion
and the affine transform. A unit test checks all 256 entries against the
FIPS-197 table. `REPORT.md` discusses the cache-timing risk of the table,
why the computed version is not automatically constant-time, and the
table-vs-computed trade-off.

Still missing: a test-only Crypto++ **throughput** benchmark. Cross-validation
proves byte equality only. A fair comparison needs the same payload sizes,
compiler, flags, CPU pinning and governor, and must not link Crypto++ into
`aestool`.

### L2-08 - CLI uniformity

Added:

- `--encode hex|base64|raw` and `--decode hex|base64|raw`.
- `--nonce` / `--nonce-hex` aliases for the CTR counter / XTS data unit.
- `--threads N` (bench only; rejected for encrypt/decrypt).
- `--verbose` (prints mode, key bits, IV, sizes to stderr; never the key).
  For `bench` it also prints `progress: i/N (percent%, elapsed, ETA)` and a
  final completion line, so long 100 MiB/1 GiB runs are not a black box.
- Rejection of `--key` + `--key-hex`, multiple IV sources, unknown options,
  and unknown `bench` options.
- `bench --mode` accepts only `ctr`; `--warmup` and `--threads` are parsed.
- `run_benchmarks.sh` and `run_benchmarks.bat` pass `--verbose` automatically.

The CLI tests cover base64 round-trip, nonce alias, XTS CTS, UTF-8 path/text,
conflicting keys, threads-on-encrypt, unknown bench options, and
`bench --verbose` progress/completion output.

### L2-09 - Windows UTF-8

`main.cpp` now has a Windows `main()` wrapper using `GetCommandLineW()` and
`CommandLineToArgvW()`; it converts every argument to UTF-8 with
`WideCharToMultiByte(CP_UTF8, ...)`. File I/O uses
`std::filesystem::u8path()`. `cli_test.cmake` creates `thư mục`, writes
Vietnamese text, encrypts/decrypts through that path and compares the result.
This passes on MSVC, MinGW GCC and MinGW Clang.

### L2-10 - Whole-file buffering

Still whole-file. The report states the ~3 GiB peak for a 1 GiB input and
lists chunked CTR as future work. XTS with ciphertext stealing is harder to
stream and is kept whole-file.

### L2-11 - Key handling and IV reuse

- The raw `key_` buffer is wiped and released immediately after key expansion;
  `AES::~AES()` wipes every round-key word with a volatile write.
- The report documents that this is best-effort (copies, swap, core dumps).
- The report documents the CTR/XTS IV uniqueness contract and explains why a
  stateless CLI cannot enforce it.
- No stateful nonce registry was added; that would need a deliberate design
  for lock files/registry scope, and is not required by the Lab 2 brief.

### L2-12 - Docs, plots and counts

Fixed in docs:

- README now says 9 KATs, 4 CTest tests, and lists the new flags.
- `BENCHMARKS.md` separates the actual committed counts from the target
  matrix and explains the old warm-up/governor.
- `output/README.md` maps every evidence file and gives rerun commands.

Still missing:

- Ubuntu 1 GiB plot (Windows 1 GiB plots are present).
- Windows benchmark run-time environment log; the chosen 1000/100/30 counts
  are accepted, so the nominal 500/50 is not required.
- A PDF/DOCX report.
- A final archive with student identity.

## Regrade

Rubric: 100 core + 20 bonus (Bonus A +10, B +5, C +5).

| Criterion | Max | Before | After | Reason |
|---|---:|---:|---:|---|
| FIPS-197 AES-128 core | 30 | 30 | 30 | manual core, vectors pass |
| CTR correctness | 15 | 15 | 15 | big-endian, partial, overflow fail-closed |
| KAT and cross-validation | 15 | 10 | 14 | 9 KATs + 3 parity tests; Ubuntu logs still missing |
| Engineering and documentation | 10 | 7 | 9 | CMake, tests, CLI, README/report; no PDF |
| Performance evaluation | 10 | 4 | 8 | Windows 3 sizes, summary + 6 plots; no Crypto++ perf, no run-time log; chosen 100/30 (N >= 30) |
| Security discussion | 20 | 8 | 16 | report analysis + key wipe; no constant-time core/IV registry |
| **Core** | **100** | **74** | **92** | |
| Bonus A: AES-192/256 | +10 | +10 | +10 | schedules, KATs, parity |
| Bonus B: XTS | +5 | +5 | +5 | GF tweak, CTS, KATs, parity |
| Bonus C: AES-NI/bitsliced | +5 | 0 | 0 | not implemented |
| **Total** | **120** | **89** | **107 -> 100** | bonus cap at 100 |

Current self-grade: **92/100 core + 15 bonus = 107, capped at 100**. The
conservative core score is 92 because the Ubuntu test logs, Windows run-time
environment log and Crypto++ throughput comparison are still missing. The
chosen 1000/100/30 matrix is accepted (N >= 30); the nominal 500/50 is only a
blueprint target. If the missing evidence is refreshed, the KAT and
performance criteria can move up.

## Remaining actions

1. On Ubuntu: build, run `--kat`, `ctest`, and save logs under
   `output/ubuntu/test_results/`.
2. Rerun the Ubuntu benchmark with `THREADS=4`, `PIN_CPU=0-3`, a
   `performance` governor, and the new warm-up; regenerate summaries and all
   three plots.
3. Add a test-only Crypto++ throughput target for the same sizes and compare.
4. Optional: rerun Windows with the console environment header; the current
   1000/100/30 data is the chosen matrix.
5. Add a PDF/DOCX export with student identity.
6. Optional: stream CTR in chunks; optional Bonus C (AES-NI/bitsliced).
7. Optional: add a stateful nonce/IV registry if the course wants enforcement
   rather than documentation.

## Benchmark commands

Windows data already exists; the command below is for a future rerun.

Chosen matrix (1000/100/30), which is now the script default:

```bash
cd lab02
scripts/build_linux.sh gcc
THREADS=4 PIN_CPU=0-3 scripts/run_benchmarks.sh gcc
python3 scripts/summarize_bench.py --dir output/ubuntu/benchmarks
for f in output/ubuntu/benchmarks/bench_ubuntu_gcc_ctr_*.csv; do
  python3 scripts/plot_bench.py "$f" \
    --out-dir output/ubuntu/benchmarks/graphs --platform gcc
done
```

Optional nominal 1000/500/50 if a grader explicitly requires it:

```bash
cd lab02
scripts/build_linux.sh gcc
THREADS=4 PIN_CPU=0-3 RUNS_100MB=500 RUNS_1GB=50 \
  scripts/run_benchmarks.sh gcc
```

```bat
rem Windows: chosen 1000/100/30 (default); add RUNS_100MB=500 RUNS_1GB=50 for 500/50.
cd lab02
scripts\build_windows.bat msvc
scripts\run_benchmarks.bat msvc
python scripts\summarize_bench.py --dir output\windows\benchmarks
```
