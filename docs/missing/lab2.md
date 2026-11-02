# Lab 2 - Missing / Lacking

**Audit date:** 2026-11-02
**Sources:** `docs/Lab2.txt`, `docs/lab2_reference.md`,
`docs/lab requirement.md`, `docs/GradingSystem.txt`,
`docs/blueprint/labs/lab2.md`, `lab02/`
**Method:** rebuilt and ran Lab 2 on Ubuntu with GCC 15.2; reviewed the scripts,
committed benchmark artifacts, and output directories. Windows was not run in
this audit.

Status legend: **OK** = present and verified, **Partial** = present but
incomplete, **Missing** = not present, **Not verified** = no evidence here.

## Verdict

The pure C++ AES core, CTR mode, unit tests, cross-validation, and the 1 MiB /
100 MiB / 1 GiB benchmark data are working on Ubuntu. Lab 2 is **not**
submission-complete: there is no Windows evidence, no committed Ubuntu test
logs, no report, and the KAT/benchmark coverage is thinner than the brief
requires. The Windows CLI also has a likely UTF-8 path/argument problem that
Lab 1 already solved.

## Verified baseline

| Check | Ubuntu | Windows |
|---|---|---|
| Build and `ctest` | Pass, 3/3 (GCC) | Missing / not verified |
| CLI KAT | 4/4 pass | Missing / not verified |
| Cross-validation | Pass, 3/3 | Missing / not verified |
| Benchmark data | 1 MiB, 100 MiB, 1 GiB CSVs + summary | Missing |
| Test evidence logs | Directory empty | Directory empty |
| Report | No report file | No report file |

Ubuntu commands used:

```bash
cd lab02
scripts/build_linux.sh gcc
./build/ubuntu-gcc/aestool --kat kat/vectors.json
(cd build/ubuntu-gcc && ctest --output-on-failure)
```

Committed Ubuntu benchmark rows: 1 MiB = 1000 runs, 100 MiB = 100 runs,
1 GiB = 30 runs.

`docs/lab2_reference.md` section 8 is partly stale: the 1 GiB run, summaries,
and some plots now exist. The Windows evidence, report, Crypto++ performance
comparison, and full statistical write-up are still missing.

## Gaps

| ID | Gap | Requirement | Ubuntu | Windows | Severity |
|---|---|---|---|---|---|
| L2-01 | No Windows build/test/KAT/bench evidence | C-01, C-10, C-13 | N/A | Missing | High |
| L2-02 | No committed Ubuntu test/KAT logs | C-10, C-13 | Partial | N/A | Medium |
| L2-03 | No final report | C-14 | Missing | Missing | High (submission) |
| L2-04 | KAT coverage is thin; unsupported vectors are silently skipped | C-08 | Partial | Partial | Medium |
| L2-05 | Cross-validation is optional at configure time | C-08 | Partial | Partial | Medium |
| L2-06 | Benchmark protocol is not the required one | C-11 | Partial | Partial | High/Medium |
| L2-07 | No Crypto++ performance comparison or S-box/cache analysis | C-11, Lab 2 section 2.5 | Missing | Missing | Medium |
| L2-08 | CLI is not the course-uniform CLI | C-03, C-04 | Partial | Partial | Medium |
| L2-09 | Windows UTF-8 paths and `--text` are likely broken | C-04 | N/A | Not verified | High (probable) |
| L2-10 | Whole-file buffering limits large inputs | C-11 (large payload) | Partial | Partial | Low/Medium |
| L2-11 | No key wiping, no CTR IV-reuse guard, security section missing | C-12 | Partial | Partial | Medium |
| L2-12 | Docs, plots, and actual benchmark counts disagree | C-02, C-11 | Partial | Partial | Low |

### L2-01 - No Windows evidence

`lab02/output/windows/` exists but contains no files. There is no
`build/windows-*` directory, no `ctest` log, no KAT log, and no Windows
benchmark CSV. `scripts/build_windows.bat` and
`scripts/run_benchmarks.bat` exist, but they have not been demonstrated here.

Requirement: C-01, C-10, C-13; Lab 2 section 2.1 requires Ubuntu and Windows
MSVC/MinGW64.

Fix: run `scripts\build_windows.bat msvc` (and MinGW if available), save
`ctest`, KAT, and benchmark logs under `output/windows/`, and add the Windows
CSVs to the comparison. If no Windows host is available, state that limitation
explicitly rather than implying cross-platform verification.

### L2-02 - No committed Ubuntu test/KAT logs

`lab02/output/ubuntu/test_results/` is empty even though the README directory
tree advertises it. The tests pass locally, but a grader cannot see the
evidence without rebuilding.

Requirement: C-10 and C-13.

Fix:

```bash
(cd build/ubuntu-gcc && ctest --output-on-failure) \
  | tee output/ubuntu/test_results/ctest_gcc.txt
./build/ubuntu-gcc/aestool --kat kat/vectors.json \
  | tee output/ubuntu/test_results/kat_gcc.txt
```

### L2-03 - No final report

There is no `lab02/REPORT.md`, PDF, or DOCX. `docs/lab2_reference.md` is an
implementation guide, not the submitted report.

Requirement: C-14; Lab 2 rubric has 10 points for engineering/documentation and
20 for security discussion.

Fix: write the one report section for Lab 2 using the existing benchmark CSVs.
Cover the AES round structure, CTR overflow, KAT/cross-validation, benchmark
method, S-box/cache/timing analysis, two-time pad, malleability, and AES-NI.
Do not create a separate report format for every lab.

### L2-04 - KAT coverage is thin and skips are hidden

`lab02/kat/vectors.json` has only four vectors:

- FIPS-197 AES-128/192/256 ECB single blocks.
- One NIST SP 800-38A AES-128 CTR vector.

Missing coverage:

- AES-192 and AES-256 CTR KATs.
- Explicit partial-final-block cases in the KAT runner (unit tests cover them,
  but the JSON KAT does not).
- An official IEEE P1619 XTS vector, even though XTS is claimed as Bonus B.

`runKat()` also prints `SKIP` and `continue`s for unsupported modes without
counting them as failures, so a KAT file with an unsupported mode can still end
with `0 failed`.

Requirement: C-08; Lab 2 sections 2.4 and 2.7.

Fix: add the missing CTR/192/256 vectors and at least one XTS vector. Count
skips in the summary, or make unsupported modes a failure. Unit tests already
cover partial blocks and counter overflow, but they do not replace official
KATs.

### L2-05 - Cross-validation can silently disappear

`lab02/CMakeLists.txt` enables `ENABLE_CROSSVAL` by default, but if Crypto++ is
not found it prints "skipping optional cross-validation test target" and
continues. `ctest` then passes 2/2 without proving byte equality. The brief says
cross-validation is required.

Requirement: C-08; Lab 2 section 2.4.

Fix: if `ENABLE_CROSSVAL=ON` and Crypto++ is missing, fail configuration or
mark the requirement explicitly as not met. On Windows, point
`CRYPTOPP_ROOT` at the installed oracle; do not link Crypto++ into `aestool`.

### L2-06 - Benchmark protocol is not the required one

`runBench()` warms up with five iterations, not 1-2 seconds of wall time. At
1 MiB this is under a second; at 1 GiB it is several minutes. The committed
Ubuntu logs also show `cpu_governor=powersave` and no CPU pinning. Lab 1's
benchmarks used `performance` and `taskset`; Lab 2 did not.

`BENCHMARKS.md` says 100 MiB = 500 runs and 1 GiB = 50; the committed summary
has 100 and 30. Those counts are still inside the course's 30-100 repeat band,
so the data is usable, but the documentation is wrong.

The Windows script only covers 1 MiB and 100 MiB, both with 100 runs. It has no
1 GiB run, no system log, no summary, and no plots.

Requirement: C-11; Lab 2 section 2.5.

Fix:

1. Replace the five-iteration warm-up with a time-based 1-2 s warm-up.
2. Run on `performance` and pin the process, or record why not.
3. Make Windows run the full matrix and generate the same summary/plots.
4. Correct `BENCHMARKS.md` to the actual counts, or re-run at the documented
   counts.

`shortcut: warm-up is five iterations; upgrade to time-based warm-up before
comparing with Lab 1 or Crypto++.`

### L2-07 - No performance comparison or S-box/cache analysis

`test_crossval` proves correctness only. There is no Crypto++ throughput
benchmark, no table-based-vs-computed S-box measurement, no memory-footprint or
cache-line measurement, and no branch-prediction analysis.

Requirement: C-11 and Lab 2 section 2.5; rubric gives 10 points to performance
evaluation.

Fix: add a test-only Crypto++ CTR benchmark target with the same payload sizes,
and add either a second S-box/T-table implementation or a clearly cited
analytical comparison. Put the numbers and interpretation in the report. Do not
put Crypto++ in the production `aestool` binary.

### L2-08 - CLI is not the course-uniform CLI

Present: `encrypt`, `decrypt`, `bench`, `--kat`, `--mode`, `--key`,
`--key-hex`, `--iv`, `--iv-hex`, `--in`, `--text`, `--out`, `--out-hex`.

Missing or divergent:

- No `--encode hex|base64|raw`; only `--out-hex`.
- No `--threads N`.
- No `--verbose`.
- No `--nonce`; only `--iv`/`--iv-hex`.
- `--key` plus `--key-hex` is not rejected; `--key-hex` silently wins.
- `--iv` plus `--iv-hex` has the same problem.
- `bench` silently ignores unknown options such as `--warmup` or `--mode`.

Requirement: C-03/C-04. Lab 2's required CTR flow still works.

Fix: add the course flags or document the deviations; reject conflicting key/IV
options; make `bench` fail on unknown options so a mistyped benchmark flag does
not silently change the run.

### L2-09 - Windows UTF-8 paths and `--text` are likely broken

`lab02/src/main.cpp` uses `main(int argc, char* argv[])` and
`std::ifstream(std::string)` / `std::ofstream(std::string)`. It has no `wmain`,
no `std::filesystem::u8path`, and no `WideCharToMultiByte` conversion. On
Windows, non-ASCII paths and Vietnamese `--text` are interpreted through the
ANSI code page and can be mangled or fail to open.

Lab 1 already has the correct pattern: `wmain` plus UTF-8 argument conversion
and `std::filesystem::u8path` on file paths. Lab 2's Linux CLI test covers a
Vietnamese directory, but `lab02/tests/cli_test.cmake` does not.

Requirement: C-04 (UTF-8 input/output). Windows status is "not verified here",
but the code path is clearly platform-dependent and likely wrong.

Fix: copy Lab 1's `wmain` argument conversion and use `u8path` in
`readFile`/`writeFile`; add a Windows non-ASCII CLI case. This is the main
Windows blocker after the missing evidence.

### L2-10 - Whole-file buffering

`encrypt`/`decrypt` read the complete input into a `std::vector`, allocate the
ciphertext, and allocate the recovered plaintext. A 1 GiB run therefore needs
roughly 3 GiB at peak by construction. That works on this host but is not
streaming, and the brief describes arbitrary-length plaintext.

This is not a hard CTR correctness failure, but it is a real ceiling.

`shortcut: whole-file buffering; upgrade to chunked CTR if inputs can exceed
available RAM.`

Fix: stream CTR in chunks (the counter increment is already sequential). XTS
with ciphertext stealing is harder; keep XTS whole-file if only the bonus uses
it. Record the limit in the README either way.

### L2-11 - Security hardening and analysis are incomplete

- `AES` stores the key and expanded round keys; there is no secure wipe.
- CTR accepts any supplied IV and does not warn about reuse; the brief treats
  reuse as catastrophic, so the report must at least discuss it. A local tool
  cannot guarantee uniqueness without state, but it can document the rule.
- There is no report section on S-box cache/timing leakage, constant-time
  coding, or AES-NI.

Requirement: C-12; Lab 2 section 2.6 and the 20-point security rubric.

Fix: add key zeroization for the core object (portable `SecureZeroMemory` /
`explicit_bzero` wrapper), document the IV contract, and write the security
section. AES-NI remains optional Bonus C; not implementing it is not a gap.

### L2-12 - Docs, plots, and counts disagree

- `BENCHMARKS.md` says 100 MiB = 500 runs and 1 GiB = 50; the summary has 100
  and 30.
- `output/ubuntu/benchmarks/graphs/` has 1 MiB and 100 MiB plots, but no 1 GiB
  plots.
- `lab02/README.md` advertises `output/ubuntu/test_results/` and
  `output/windows/test_results/`; both are empty.
- The Windows benchmark script has no 1 GiB run.

Requirement: C-02 and C-11. Low impact compared with L2-01/L2-06.

Fix: make the docs match the actual artifacts, generate the 1 GiB plots, or
mark the 1 GiB run as optional and not part of the documented matrix.

## Per-environment actions

### Ubuntu

1. Save `ctest` and KAT logs under `output/ubuntu/test_results/`.
2. Add the missing KAT vectors and make skips visible.
3. Fix the benchmark warm-up, governor/pinning, and count documentation.
4. Add the Crypto++ performance comparison and 1 GiB plots.
5. Write the Lab 2 report section.
6. Decide whether to stream large inputs (L2-10).

### Windows

1. Run `scripts\build_windows.bat msvc` and, if available, the MinGW build.
2. Save `ctest`, KAT, and benchmark logs; run the full 1 MiB/100 MiB/1 GiB
   matrix.
3. Fix the UTF-8 `wmain`/`u8path` issue before claiming Windows support.
4. Ensure Crypto++ is found for cross-validation, or mark it as not met.
5. Add the Windows CSVs to the cross-platform comparison and report.

## Minimal fix order

1. Fix Windows UTF-8 and produce Windows evidence (L2-09, L2-01).
2. Save Ubuntu test/KAT logs and write the report (L2-02, L2-03).
3. Enforce cross-validation and expand KATs (L2-05, L2-04).
4. Fix benchmark protocol and add the Crypto++ comparison (L2-06, L2-07).
5. Align the CLI and documentation (L2-08, L2-12).
6. Add key wiping and document IV reuse (L2-11).
7. Add streaming only if large inputs are a real requirement (L2-10).

No source code was changed by this audit.
